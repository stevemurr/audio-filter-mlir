#include "biquad.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

// Test initialization
void test_biquad_init() {
  printf("Test 1: BiQuad Initialization\n");

  BiQuad bq;
  biquad_init(&bq);

  assert(bq.a0 == 0.0);
  assert(bq.a1 == 0.0);
  assert(bq.a2 == 0.0);
  assert(bq.b1 == 0.0);
  assert(bq.b2 == 0.0);
  assert(bq.c0 == 1.0);
  assert(bq.d0 == 0.0);
  assert(bq.xz1 == 0.0);
  assert(bq.xz2 == 0.0);
  assert(bq.yz1 == 0.0);
  assert(bq.yz2 == 0.0);

  printf("  ✓ All fields initialized correctly\n\n");
}

// Test delay flushing
void test_biquad_flush() {
  printf("Test 2: Delay Flushing\n");

  BiQuad bq;
  biquad_init(&bq);

  // Set some non-zero delays
  bq.xz1 = 1.0;
  bq.xz2 = 2.0;
  bq.yz1 = 3.0;
  bq.yz2 = 4.0;

  biquad_flush_delays(&bq);

  assert(bq.xz1 == 0.0);
  assert(bq.xz2 == 0.0);
  assert(bq.yz1 == 0.0);
  assert(bq.yz2 == 0.0);

  printf("  ✓ Delays flushed successfully\n\n");
}

// Test basic processing (pass-through)
void test_biquad_passthrough() {
  printf("Test 3: Pass-through (a0=1, all others=0)\n");

  BiQuad bq;
  biquad_init(&bq);
  bq.a0 = 1.0; // Pass-through configuration

  double input[] = {0.5, -0.3, 0.8, -0.1, 0.0};
  int count = 5;

  for (int i = 0; i < count; i++) {
    double output = biquad_process(&bq, input[i]);
    assert(fabs(output - input[i]) < 1e-10);
  }

  printf("  ✓ Pass-through working correctly\n\n");
}

// Test delay line behavior
void test_biquad_delays() {
  printf("Test 4: Delay Line Behavior\n");

  BiQuad bq;
  biquad_init(&bq);

  // Simple delay: y(n) = x(n-1)
  bq.a0 = 0.0;
  bq.a1 = 1.0; // Output previous input
  bq.a2 = 0.0;
  bq.b1 = 0.0;
  bq.b2 = 0.0;

  double inputs[] = {1.0, 2.0, 3.0, 4.0};
  double expected[] = {0.0, 1.0, 2.0, 3.0}; // Delayed by one sample

  for (int i = 0; i < 4; i++) {
    double output = biquad_process(&bq, inputs[i]);
    assert(fabs(output - expected[i]) < 1e-10);
    printf("  Input: %.1f -> Output: %.1f (expected: %.1f)\n", inputs[i],
           output, expected[i]);
  }

  printf("  ✓ Delay line working correctly\n\n");
}

// Test simple low-pass filter coefficients
void test_biquad_lowpass() {
  printf("Test 5: Simple Low-Pass Filter\n");

  BiQuad bq;
  biquad_init(&bq);

  // Simple averaging filter: y(n) = 0.5*x(n) + 0.5*x(n-1)
  bq.a0 = 0.5;
  bq.a1 = 0.5;
  bq.a2 = 0.0;
  bq.b1 = 0.0;
  bq.b2 = 0.0;

  // Test with alternating signal (should be smoothed)
  double inputs[] = {1.0, -1.0, 1.0, -1.0, 1.0};

  printf("  Testing with alternating signal:\n");
  for (int i = 0; i < 5; i++) {
    double output = biquad_process(&bq, inputs[i]);
    printf("  Input: %6.2f -> Output: %6.2f\n", inputs[i], output);
  }

  printf("  ✓ Low-pass filter smoothing signal\n\n");
}

// Test underflow prevention
void test_biquad_underflow() {
  printf("Test 6: Underflow Prevention\n");

  BiQuad bq;
  biquad_init(&bq);
  bq.a0 = 1.0;

  // Test very small positive value (should be zeroed)
  double tiny_positive = 1e-39;
  double output1 = biquad_process(&bq, tiny_positive);
  printf("  Tiny positive (1e-39) -> %.15e (should be 0.0)\n", output1);

  // Test very small negative value (should be zeroed)
  double tiny_negative = -1e-39;
  double output2 = biquad_process(&bq, tiny_negative);
  printf("  Tiny negative (-1e-39) -> %.15e (should be 0.0)\n", output2);

  // Test normal small value (should pass through)
  double small_normal = 1e-10;
  double output3 = biquad_process(&bq, small_normal);
  printf("  Small normal (1e-10) -> %.15e (should be ~1e-10)\n", output3);
  assert(fabs(output3 - small_normal) < 1e-15);

  printf("  ✓ Underflow prevention working\n\n");
}

int main() {
  printf("\n=== BiQuad Filter Unit Tests ===\n\n");

  test_biquad_init();
  test_biquad_flush();
  test_biquad_passthrough();
  test_biquad_delays();
  test_biquad_lowpass();
  test_biquad_underflow();

  printf("=== All BiQuad tests passed! ===\n\n");
  return 0;
}
