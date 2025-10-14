#include "biquad.h"
#include <string.h>

// Initialize a BiQuad filter to default state
void biquad_init(BiQuad *bq) {
  if (!bq)
    return;

  // Zero all coefficients
  bq->a0 = 0.0;
  bq->a1 = 0.0;
  bq->a2 = 0.0;
  bq->b1 = 0.0;
  bq->b2 = 0.0;

  // Default wet/dry mix (full wet, no dry)
  bq->c0 = 1.0;
  bq->d0 = 0.0;

  // Flush delays
  biquad_flush_delays(bq);
}

// Flush the delay elements
void biquad_flush_delays(BiQuad *bq) {
  if (!bq)
    return;

  bq->xz1 = 0.0;
  bq->xz2 = 0.0;
  bq->yz1 = 0.0;
  bq->yz2 = 0.0;
}

// Process a single sample through the biquad filter
double biquad_process(BiQuad *bq, double input) {
  if (!bq)
    return input;

  // Difference equation: y(n) = a0*x(n) + a1*x(n-1) + a2*x(n-2) - b1*y(n-1) -
  // b2*y(n-2)
  double yn = bq->a0 * input + bq->a1 * bq->xz1 + bq->a2 * bq->xz2 -
              bq->b1 * bq->yz1 - bq->b2 * bq->yz2;

  // Underflow check - prevent denormal numbers
  if (yn > 0.0 && yn < FLT_MIN_PLUS) {
    yn = 0.0;
  }
  if (yn < 0.0 && yn > FLT_MIN_MINUS) {
    yn = 0.0;
  }

  // Shuffle delays
  // Y delays (output history)
  bq->yz2 = bq->yz1;
  bq->yz1 = yn;

  // X delays (input history)
  bq->xz2 = bq->xz1;
  bq->xz1 = input;

  return yn;
}
