#include "biquad.h"
#include "mlir_biquad.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BUFFER_SIZE 1000000 // 1 million samples
#define NUM_ITERATIONS 10

// Get time in seconds
static double get_time(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec + ts.tv_nsec / 1e9;
}

int main(void) {
  printf("\n=== MLIR BiQuad Performance Benchmark ===\n\n");

  // Allocate buffers
  double *input = malloc(BUFFER_SIZE * sizeof(double));
  double *output_c = malloc(BUFFER_SIZE * sizeof(double));
  double *output_mlir = malloc(BUFFER_SIZE * sizeof(double));

  if (!input || !output_c || !output_mlir) {
    fprintf(stderr, "Failed to allocate memory\n");
    return 1;
  }

  // Generate test signal (sine wave)
  for (int i = 0; i < BUFFER_SIZE; i++) {
    input[i] = sin(2.0 * M_PI * i / 100.0) * 0.5;
  }

  // Create BiQuad filters
  BiQuad bq_c, bq_mlir;
  biquad_init(&bq_c);
  biquad_init(&bq_mlir);

  // Set coefficients (simple low-pass)
  bq_c.a0 = bq_mlir.a0 = 0.05;
  bq_c.a1 = bq_mlir.a1 = 0.10;
  bq_c.a2 = bq_mlir.a2 = 0.05;
  bq_c.b1 = bq_mlir.b1 = -1.60;
  bq_c.b2 = bq_mlir.b2 = 0.80;

  // Create MLIR JIT context
  MLIRBiQuadJIT *jit = mlir_biquad_jit_create(&bq_mlir);
  if (!jit) {
    fprintf(stderr, "Failed to create MLIR JIT context\n");
    free(input);
    free(output_c);
    free(output_mlir);
    return 1;
  }

  printf("Buffer size: %d samples\n", BUFFER_SIZE);
  printf("Iterations: %d\n\n", NUM_ITERATIONS);

  // Benchmark C implementation
  printf("Benchmarking C implementation...\n");
  double c_total_time = 0.0;
  for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
    biquad_init(&bq_c); // Reset state
    bq_c.a0 = 0.05;
    bq_c.a1 = 0.10;
    bq_c.a2 = 0.05;
    bq_c.b1 = -1.60;
    bq_c.b2 = 0.80;

    double start = get_time();
    for (int i = 0; i < BUFFER_SIZE; i++) {
      output_c[i] = biquad_process(&bq_c, input[i]);
    }
    double end = get_time();
    c_total_time += (end - start);
  }
  double c_avg_time = c_total_time / NUM_ITERATIONS;
  double c_samples_per_sec = BUFFER_SIZE / c_avg_time;

  printf("  Average time: %.6f seconds\n", c_avg_time);
  printf("  Throughput: %.2f M samples/sec\n\n", c_samples_per_sec / 1e6);

  // Benchmark MLIR implementation
  printf("Benchmarking MLIR implementation...\n");
  double mlir_total_time = 0.0;
  for (int iter = 0; iter < NUM_ITERATIONS; iter++) {
    biquad_init(&bq_mlir); // Reset state
    bq_mlir.a0 = 0.05;
    bq_mlir.a1 = 0.10;
    bq_mlir.a2 = 0.05;
    bq_mlir.b1 = -1.60;
    bq_mlir.b2 = 0.80;

    double start = get_time();
    mlir_biquad_process_buffer(jit, &bq_mlir, input, output_mlir, BUFFER_SIZE);
    double end = get_time();
    mlir_total_time += (end - start);
  }
  double mlir_avg_time = mlir_total_time / NUM_ITERATIONS;
  double mlir_samples_per_sec = BUFFER_SIZE / mlir_avg_time;

  printf("  Average time: %.6f seconds\n", mlir_avg_time);
  printf("  Throughput: %.2f M samples/sec\n\n", mlir_samples_per_sec / 1e6);

  // Compute speedup
  double speedup = c_avg_time / mlir_avg_time;
  printf("=== Results ===\n");
  printf("MLIR vs C speedup: %.2fx\n", speedup);

  if (speedup >= 1.0) {
    printf("Status: ✓ MLIR is %.2fx faster\n", speedup);
  } else {
    printf("Status: ✓ Baseline established (MLIR calls C for now)\n");
    printf("Note: Phase 3.2 focuses on API and correctness.\n");
    printf("      Future phases will add MLIR optimization.\n");
  }

  // Verify correctness
  printf("\n=== Correctness Check ===\n");
  int mismatches = 0;
  double max_diff = 0.0;
  for (int i = 0; i < BUFFER_SIZE; i++) {
    double diff = fabs(output_c[i] - output_mlir[i]);
    if (diff > max_diff)
      max_diff = diff;
    if (diff > 1e-10)
      mismatches++;
  }

  if (mismatches == 0) {
    printf("✓ All samples match (max diff: %.2e)\n", max_diff);
  } else {
    printf("✗ Found %d mismatches (max diff: %.2e)\n", mismatches, max_diff);
  }

  // Cleanup
  mlir_biquad_jit_destroy(jit);
  free(input);
  free(output_c);
  free(output_mlir);

  printf("\n");
  return 0;
}
