#ifndef MLIR_CONTEXT_H
#define MLIR_CONTEXT_H

#ifdef __cplusplus
extern "C" {
#endif

// Forward declarations for MLIR types
typedef struct MLIRContextImpl MLIRContextImpl;

// MLIR context handle
typedef struct {
    MLIRContextImpl *impl;
    int initialized;
} MLIRContextHandle;

// Initialize MLIR context
// Returns: 0 on success, -1 on failure
int mlir_context_init(MLIRContextHandle *handle);

// Cleanup MLIR context
void mlir_context_cleanup(MLIRContextHandle *handle);

// Check if MLIR is available
// Returns: 1 if available, 0 if not
int mlir_is_available(void);

// Get MLIR version string
// Returns: Version string (e.g., "20.1.8") or NULL if not available
const char* mlir_get_version(void);

// Verify MLIR functionality with a simple test
// Returns: 0 on success, -1 on failure
int mlir_verify_functionality(MLIRContextHandle *handle);

#ifdef __cplusplus
}
#endif

#endif // MLIR_CONTEXT_H
