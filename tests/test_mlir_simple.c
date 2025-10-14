#include "biquad.h"
#include "mlir_biquad.h"
#include <stdio.h>

int main(void) {
  printf("Test 1: MLIR available\n");
  if (mlir_biquad_available()) {
    printf("  MLIR is available\n");
  } else {
    printf("  MLIR NOT available\n");
    return 1;
  }

  printf("\nTest 2: Creating JIT context...\n");
  fflush(stdout);

  BiQuad bq;
  biquad_init(&bq);
  bq.a0 = 1.0;
  bq.a1 = 0.5;
  bq.a2 = 0.25;
  bq.b1 = 0.1;
  bq.b2 = 0.05;

  MLIRBiQuadJIT *jit = mlir_biquad_jit_create(&bq);

  if (jit) {
    printf("  JIT context created successfully\n");
    mlir_biquad_jit_destroy(jit);
    printf("  JIT context destroyed\n");
    return 0;
  } else {
    printf("  Failed to create JIT context\n");
    return 1;
  }
}
