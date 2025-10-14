#include "mlir_context.h"
#include <assert.h>
#include <stdio.h>

// Test MLIR availability
void test_mlir_availability() {
  printf("Test 1: MLIR Availability\n");

  int available = mlir_is_available();
  printf("  MLIR available: %s\n", available ? "YES" : "NO");
  assert(available == 1);

  const char *version = mlir_get_version();
  printf("  MLIR version: %s\n", version ? version : "unknown");
  assert(version != NULL);

  printf("  ✓ MLIR is available\n\n");
}

// Test MLIR context initialization
void test_mlir_context_init() {
  printf("Test 2: MLIR Context Initialization\n");

  MLIRContextHandle handle = {NULL, 0};

  int result = mlir_context_init(&handle);
  printf("  Context init result: %d\n", result);
  assert(result == 0);
  assert(handle.initialized == 1);
  assert(handle.impl != NULL);

  printf("  ✓ MLIR context initialized successfully\n\n");

  // Cleanup
  mlir_context_cleanup(&handle);
}

// Test MLIR functionality verification
void test_mlir_functionality() {
  printf("Test 3: MLIR Functionality Verification\n");

  MLIRContextHandle handle = {NULL, 0};

  // Initialize
  int result = mlir_context_init(&handle);
  assert(result == 0);

  // Verify functionality
  result = mlir_verify_functionality(&handle);
  printf("  Functionality verification: %d\n", result);
  assert(result == 0);

  printf("  ✓ MLIR functionality verified\n\n");

  // Cleanup
  mlir_context_cleanup(&handle);
}

// Test multiple init/cleanup cycles
void test_mlir_multiple_cycles() {
  printf("Test 4: Multiple Init/Cleanup Cycles\n");

  for (int i = 0; i < 3; i++) {
    MLIRContextHandle handle = {NULL, 0};

    int result = mlir_context_init(&handle);
    assert(result == 0);

    result = mlir_verify_functionality(&handle);
    assert(result == 0);

    mlir_context_cleanup(&handle);

    assert(handle.initialized == 0);
    assert(handle.impl == NULL);

    printf("  Cycle %d: OK\n", i + 1);
  }

  printf("  ✓ Multiple cycles working correctly\n\n");
}

// Test cleanup of uninitialized context
void test_mlir_cleanup_uninitialized() {
  printf("Test 5: Cleanup Uninitialized Context\n");

  MLIRContextHandle handle = {NULL, 0};

  // Should not crash
  mlir_context_cleanup(&handle);

  printf("  ✓ Cleanup of uninitialized context is safe\n\n");
}

int main() {
  printf("\n=== MLIR Basic Infrastructure Tests ===\n\n");

#ifdef USE_MLIR
  printf("MLIR support: ENABLED\n\n");

  test_mlir_availability();
  test_mlir_context_init();
  test_mlir_functionality();
  test_mlir_multiple_cycles();
  test_mlir_cleanup_uninitialized();

  printf("=== All MLIR basic tests passed! ===\n\n");
  return 0;
#else
  printf("MLIR support: DISABLED\n");
  printf("Skipping MLIR tests.\n\n");
  printf("To enable MLIR, configure with: cmake -DENABLE_MLIR=ON\n\n");
  return 0;
#endif
}
