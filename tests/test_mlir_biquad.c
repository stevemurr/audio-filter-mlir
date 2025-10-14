#include "biquad.h"
#include "mlir_biquad.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EPSILON 1e-10
#define PASS "\033[32m✓\033[0m"
#define FAIL "\033[31m✗\033[0m"

// Test result tracking
static int tests_passed = 0;
static int tests_failed = 0;

void assert_double_eq(const char *test_name, double expected, double actual,
                      double epsilon) {
  if (fabs(expected - actual) < epsilon) {
    printf("  %s %s: expected=%.10f, actual=%.10f\n", PASS, test_name, expected,
           actual);
    tests_passed++;
  } else {
    printf("  %s %s: expected=%.10f, actual=%.10f (diff=%.10e)\n", FAIL,
           test_name, expected, actual, fabs(expected - actual));
    tests_failed++;
  }
}

void test_mlir_availability(void) {
  printf("\nTest 1: MLIR BiQuad Availability\n");

  int available = mlir_biquad_available();
  if (available) {
    printf("  %s MLIR BiQuad is available\n", PASS);
    tests_passed++;
  } else {
    printf("  %s MLIR BiQuad is NOT available\n", FAIL);
    tests_failed++;
  }
}

void test_jit_creation(void) {
  printf("\nTest 2: JIT Context Creation\n");

  // Create a simple BiQuad filter
  BiQuad bq;
  biquad_init(&bq);
  bq.a0 = 1.0;
  bq.a1 = 0.5;
  bq.a2 = 0.25;
  bq.b1 = 0.1;
  bq.b2 = 0.05;

  MLIRBiQuadJIT *jit = mlir_biquad_jit_create(&bq);

  if (jit != NULL) {
    printf("  %s JIT context created successfully\n", PASS);
    tests_passed++;
    mlir_biquad_jit_destroy(jit);
  } else {
    printf("  %s Failed to create JIT context\n", FAIL);
    tests_failed++;
  }
}

void test_single_sample_processing(void) {
  printf("\nTest 3: Single Sample Processing (C vs MLIR)\n");

  // Create two identical BiQuad filters
  BiQuad bq_c, bq_mlir;
  biquad_init(&bq_c);
  biquad_init(&bq_mlir);

  // Set same coefficients
  bq_c.a0 = bq_mlir.a0 = 1.0;
  bq_c.a1 = bq_mlir.a1 = 0.5;
  bq_c.a2 = bq_mlir.a2 = 0.25;
  bq_c.b1 = bq_mlir.b1 = 0.1;
  bq_c.b2 = bq_mlir.b2 = 0.05;

  // Create MLIR JIT
  MLIRBiQuadJIT *jit = mlir_biquad_jit_create(&bq_mlir);
  if (!jit) {
    printf("  %s Failed to create JIT context\n", FAIL);
    tests_failed++;
    return;
  }

  // Test with a single sample
  double input = 0.5;
  double output_c = biquad_process(&bq_c, input);
  double output_mlir = mlir_biquad_process(jit, &bq_mlir, input);

  assert_double_eq("Single sample output", output_c, output_mlir, EPSILON);

  // Check that delay states match
  assert_double_eq("xz1 state", bq_c.xz1, bq_mlir.xz1, EPSILON);
  assert_double_eq("xz2 state", bq_c.xz2, bq_mlir.xz2, EPSILON);
  assert_double_eq("yz1 state", bq_c.yz1, bq_mlir.yz1, EPSILON);
  assert_double_eq("yz2 state", bq_c.yz2, bq_mlir.yz2, EPSILON);

  mlir_biquad_jit_destroy(jit);
}

void test_multiple_samples(void) {
  printf("\nTest 4: Multiple Sample Processing\n");

  // Create two identical BiQuad filters
  BiQuad bq_c, bq_mlir;
  biquad_init(&bq_c);
  biquad_init(&bq_mlir);

  // Set same coefficients
  bq_c.a0 = bq_mlir.a0 = 0.8;
  bq_c.a1 = bq_mlir.a1 = -0.4;
  bq_c.a2 = bq_mlir.a2 = 0.2;
  bq_c.b1 = bq_mlir.b1 = -0.3;
  bq_c.b2 = bq_mlir.b2 = 0.15;

  // Create MLIR JIT
  MLIRBiQuadJIT *jit = mlir_biquad_jit_create(&bq_mlir);
  if (!jit) {
    printf("  %s Failed to create JIT context\n", FAIL);
    tests_failed++;
    return;
  }

  // Test sequence of samples
  double test_samples[] = {0.1, 0.5, -0.3, 0.8, -0.2, 0.0, 0.4, -0.6};
  int num_samples = sizeof(test_samples) / sizeof(test_samples[0]);

  int all_match = 1;
  for (int i = 0; i < num_samples; i++) {
    double output_c = biquad_process(&bq_c, test_samples[i]);
    double output_mlir = mlir_biquad_process(jit, &bq_mlir, test_samples[i]);

    if (fabs(output_c - output_mlir) > EPSILON) {
      printf("  %s Sample %d mismatch: C=%.10f, MLIR=%.10f\n", FAIL, i,
             output_c, output_mlir);
      all_match = 0;
      tests_failed++;
    }
  }

  if (all_match) {
    printf("  %s All %d samples match\n", PASS, num_samples);
    tests_passed++;
  }

  // Check final states
  assert_double_eq("Final xz1", bq_c.xz1, bq_mlir.xz1, EPSILON);
  assert_double_eq("Final yz1", bq_c.yz1, bq_mlir.yz1, EPSILON);

  mlir_biquad_jit_destroy(jit);
}

void test_buffer_processing(void) {
  printf("\nTest 5: Buffer Processing\n");

  // Create two identical BiQuad filters
  BiQuad bq_c, bq_mlir;
  biquad_init(&bq_c);
  biquad_init(&bq_mlir);

  // Set same coefficients
  bq_c.a0 = bq_mlir.a0 = 1.0;
  bq_c.a1 = bq_mlir.a1 = 0.6;
  bq_c.a2 = bq_mlir.a2 = 0.3;
  bq_c.b1 = bq_mlir.b1 = 0.2;
  bq_c.b2 = bq_mlir.b2 = 0.1;

  // Create MLIR JIT
  MLIRBiQuadJIT *jit = mlir_biquad_jit_create(&bq_mlir);
  if (!jit) {
    printf("  %s Failed to create JIT context\n", FAIL);
    tests_failed++;
    return;
  }

  // Create test buffer
  const int buffer_size = 100;
  double input[buffer_size];
  double output_c[buffer_size];
  double output_mlir[buffer_size];

  // Generate sine wave input
  for (int i = 0; i < buffer_size; i++) {
    input[i] = sin(2.0 * M_PI * i / 20.0) * 0.5;
  }

  // Process with C version
  for (int i = 0; i < buffer_size; i++) {
    output_c[i] = biquad_process(&bq_c, input[i]);
  }

  // Process with MLIR version
  mlir_biquad_process_buffer(jit, &bq_mlir, input, output_mlir, buffer_size);

  // Compare outputs
  int all_match = 1;
  double max_diff = 0.0;
  for (int i = 0; i < buffer_size; i++) {
    double diff = fabs(output_c[i] - output_mlir[i]);
    if (diff > max_diff)
      max_diff = diff;
    if (diff > EPSILON) {
      all_match = 0;
    }
  }

  if (all_match) {
    printf("  %s Buffer processing matches (max diff: %.2e)\n", PASS, max_diff);
    tests_passed++;
  } else {
    printf("  %s Buffer processing mismatch (max diff: %.2e)\n", FAIL,
           max_diff);
    tests_failed++;
  }

  mlir_biquad_jit_destroy(jit);
}

void test_zero_input(void) {
  printf("\nTest 6: Zero Input Handling\n");

  BiQuad bq_c, bq_mlir;
  biquad_init(&bq_c);
  biquad_init(&bq_mlir);

  bq_c.a0 = bq_mlir.a0 = 1.0;
  bq_c.a1 = bq_mlir.a1 = 0.5;
  bq_c.a2 = bq_mlir.a2 = 0.25;
  bq_c.b1 = bq_mlir.b1 = 0.1;
  bq_c.b2 = bq_mlir.b2 = 0.05;

  MLIRBiQuadJIT *jit = mlir_biquad_jit_create(&bq_mlir);
  if (!jit) {
    printf("  %s Failed to create JIT context\n", FAIL);
    tests_failed++;
    return;
  }

  // Process zeros
  for (int i = 0; i < 10; i++) {
    double output_c = biquad_process(&bq_c, 0.0);
    double output_mlir = mlir_biquad_process(jit, &bq_mlir, 0.0);

    if (fabs(output_c - output_mlir) > EPSILON) {
      printf("  %s Zero input mismatch at sample %d\n", FAIL, i);
      tests_failed++;
      mlir_biquad_jit_destroy(jit);
      return;
    }
  }

  printf("  %s Zero input handled correctly\n", PASS);
  tests_passed++;

  mlir_biquad_jit_destroy(jit);
}

int main(void) {
  printf("\n=== MLIR BiQuad Tests ===\n");

  test_mlir_availability();
  test_jit_creation();
  test_single_sample_processing();
  test_multiple_samples();
  test_buffer_processing();
  test_zero_input();

  printf("\n=== Test Summary ===\n");
  printf("Passed: %d\n", tests_passed);
  printf("Failed: %d\n", tests_failed);

  if (tests_failed == 0) {
    printf("\n%s All MLIR BiQuad tests passed!\n\n", PASS);
    return 0;
  } else {
    printf("\n%s Some tests failed.\n\n", FAIL);
    return 1;
  }
}
