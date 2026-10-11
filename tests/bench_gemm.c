/* SPDX-License-Identifier: GPL-3.0-or-later
   Copyright (C) 2026 Maurizio Cammalleri */
/*
 * bench_gemm.c - throughput of the quantized products over several vectors
 * (the tokens of a block), on random weights.
 *
 * Decode is bound by memory; a block of n tokens reads every weight once for
 * all of them, so beyond a few tokens the product is bound by arithmetic and
 * the figure of merit is multiply-accumulates per second.
 *
 * Usage: bench_gemm [rows cols q4k|q6k|q6kp1|q6kp2|q6kp3 threads [gpu]]
 * q6kpL is Q6_K in bit planes with L of the three planes in memory.
 * Defaults: 12288 x 2048 Q4_K (Qwen3-Next ssm_in), 12 threads on CPUs 0-11.
 * gpu: the products of a prompt's blocks on the GPU (the fast prompt's
 * tiled shaders, *_mmq.comp), 64 to 512 vectors, the weights imported.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/pool.h"
#include "llm/gpu.h"
#include "llm/matvec.h"
#include "llm/quant.h"

static double now(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

int main(int argc, char **argv)
{
    size_t rows = argc > 1 ? (size_t)atol(argv[1]) : 12288;
    size_t cols = argc > 2 ? (size_t)atol(argv[2]) : 2048;
    const char *kind = argc > 3 ? argv[3] : "q4k";
    int level = 0;
    int type = JANAS_Q4_K;
    if (!strcmp(kind, "q6k"))
        type = JANAS_Q6_K;
    else if (!strncmp(kind, "q6kp", 4)) {
        type = JANAS_Q6_K_P;
        level = kind[4] ? kind[4] - '0' : 3;
        if (level < 1 || level > 3)
            return 2;
    }
    int threads = argc > 4 ? atoi(argv[4]) : 12;
    int on_gpu = argc > 5 && !strcmp(argv[5], "gpu");
    int cpus[256];
    for (int i = 0; i < threads && i < 256; i++)
        cpus[i] = i;
    struct janas_pool *pool = janas_pool_create(threads, cpus);
    janas_pin_current_thread(0);
    size_t nb = cols / JANAS_QK, bsz = janas_qtype_block_size(type);
    size_t nvmax = on_gpu ? 512 : 64;
    /* page-sized and whole pages: what the GPU imports */
    size_t wbytes = (rows * nb * bsz + 4 + 4095) / 4096 * 4096;
    uint8_t *w = aligned_alloc(4096, wbytes);
    size_t plane_bytes = (rows * nb * JANAS_Q6KP_PLANE + 4095) / 4096 * 4096;
    uint8_t *pl[2] = {NULL, NULL};
    for (int i = 0; i < level - 1; i++) {
        pl[i] = aligned_alloc(4096, plane_bytes);
        if (!pl[i])
            return 1;
        for (size_t j = 0; j < plane_bytes; j++)
            pl[i][j] = (uint8_t)rand();
    }
    struct janas_block_q8k *x = malloc(nvmax * nb * sizeof(*x));
    float *xf = malloc(cols * sizeof(float));
    float *y = malloc(nvmax * rows * sizeof(float));
    if (!pool || !w || !x || !xf || !y)
        return 1;
    srand(1);
    for (size_t i = 0; i < wbytes; i++)
        w[i] = (uint8_t)rand();
    for (size_t b = 0; b < rows * nb; b++) {
        if (type == JANAS_Q6_K_P) {
            ((struct janas_block_q6kp *)w)[b].d = janas_fp32_to_fp16(0.001f);
        } else if (type == JANAS_Q6_K) {
            ((struct janas_block_q6k *)w)[b].d = janas_fp32_to_fp16(0.001f);
        } else {
            ((struct janas_block_q4k *)w)[b].d = janas_fp32_to_fp16(0.001f);
            ((struct janas_block_q4k *)w)[b].dmin = janas_fp32_to_fp16(0.0005f);
        }
    }
    for (size_t v = 0; v < nvmax; v++) {
        for (size_t i = 0; i < cols; i++)
            xf[i] = (float)((double)rand() / RAND_MAX) - 0.5f;
        janas_q8k_quantize(xf, x + v * nb, cols);
    }
    double mb =
        (rows * nb * bsz + (size_t)(level ? level - 1 : 0) * plane_bytes) / 1e6;
    struct janas_gpu *gpu = NULL;
    if (on_gpu) {
        char err[200];
        gpu = janas_gpu_create(err, sizeof(err));
        if (!gpu || janas_gpu_import(gpu, w, wbytes) != 0 ||
            (pl[0] && janas_gpu_import(gpu, pl[0], plane_bytes) != 0) ||
            (pl[1] && janas_gpu_import(gpu, pl[1], plane_bytes) != 0)) {
            fprintf(stderr, "bench_gemm: no GPU: %s\n", gpu ? "import" : err);
            return 1;
        }
    }
    printf("%s %zu x %zu (%.1f MB), %s\n", kind, rows, cols, mb,
           on_gpu ? janas_gpu_name(gpu) : "CPU");
    if (!on_gpu)
        printf("%d threads\n", threads);
    size_t nvs_cpu[] = {1, 2, 4, 8, 16, 32, 64},
           nvs_gpu[] = {64, 128, 256, 512};
    size_t *nvs = on_gpu ? nvs_gpu : nvs_cpu,
           n_nvs = on_gpu ? sizeof(nvs_gpu) / sizeof(*nvs_gpu)
                          : sizeof(nvs_cpu) / sizeof(*nvs_cpu);
    for (size_t k = 0; k < n_nvs; k++) {
        size_t nv = nvs[k];
        struct janas_matvec_task t = {.type = type,
                                      .w = w,
                                      .plane = {pl[0], pl[1]},
                                      .x = x,
                                      .y = y,
                                      .rows = rows,
                                      .cols = cols,
                                      .n_vec = nv};
        int reps = (int)(200 / nv) + 5;
        if (on_gpu) {
            janas_gpu_matvec_group(gpu, pool, &t, 1); /* warm-up */
            reps = (int)(2048 / nv) + 5;
        } else
            janas_matvec_q4k_group(pool, &t, 1);
        double t0 = now();
        for (int r = 0; r < reps; r++) {
            if (on_gpu)
                janas_gpu_matvec_group(gpu, pool, &t, 1);
            else
                janas_matvec_q4k_group(pool, &t, 1);
        }
        double dt = (now() - t0) / reps;
        /* the outputs' sum, to tell two versions of a shader apart */
        double sum = 0;
        for (size_t i = 0; i < nv * rows; i++)
            sum += y[i];
        printf("n %2zu: %8.3f ms, %7.3f ms per vector, %6.1f GMAC/s, "
               "%5.1f GB/s, sum %.17g\n",
               nv, dt * 1e3, dt * 1e3 / nv, rows * cols * nv / dt / 1e9,
               mb / 1e3 / dt, sum);
    }
    janas_gpu_destroy(gpu);
    janas_pool_destroy(pool);
    free(w);
    free(pl[0]);
    free(pl[1]);
    free(x);
    free(xf);
    free(y);
    return 0;
}
