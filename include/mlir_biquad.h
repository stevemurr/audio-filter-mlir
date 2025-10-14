#ifndef MLIR_BIQUAD_H
#define MLIR_BIQUAD_H

#include <stddef.h>
#include "biquad.h"

#ifdef USE_MLIR

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief MLIR-optimized BiQuad filter implementation
 * 
 * This module provides an MLIR-based implementation of the BiQuad difference equation
 * that can be used as a drop-in replacement for the standard biquad_process() function.
 * 
 * The MLIR implementation generates optimized IR for the BiQuad processing loop and
 * uses LLVM's JIT compiler for execution.
 */

/**
 * @brief Opaque handle to MLIR BiQuad JIT-compiled function
 */
typedef struct MLIRBiQuadJIT MLIRBiQuadJIT;

/**
 * @brief Initialize MLIR BiQuad JIT compiler
 * 
 * Creates a JIT-compiled function for BiQuad processing based on the provided
 * filter coefficients. This function should be called once per filter instance.
 * 
 * @param bq Pointer to BiQuad filter structure with initialized coefficients
 * @return Pointer to JIT context, or NULL on failure
 */
MLIRBiQuadJIT* mlir_biquad_jit_create(const BiQuad *bq);

/**
 * @brief Process a single sample through MLIR-optimized BiQuad filter
 * 
 * Drop-in replacement for biquad_process() that uses JIT-compiled MLIR code.
 * 
 * @param jit Pointer to JIT context created by mlir_biquad_jit_create()
 * @param bq Pointer to BiQuad filter structure (for state variables)
 * @param input Input sample
 * @return Filtered output sample
 */
double mlir_biquad_process(MLIRBiQuadJIT *jit, BiQuad *bq, double input);

/**
 * @brief Process a buffer of samples through MLIR-optimized BiQuad filter
 * 
 * Optimized batch processing of multiple samples. This is more efficient than
 * calling mlir_biquad_process() in a loop because it allows MLIR to optimize
 * across multiple samples.
 * 
 * @param jit Pointer to JIT context created by mlir_biquad_jit_create()
 * @param bq Pointer to BiQuad filter structure (for state variables)
 * @param input Input buffer
 * @param output Output buffer (can be same as input for in-place processing)
 * @param length Number of samples to process
 */
void mlir_biquad_process_buffer(MLIRBiQuadJIT *jit, BiQuad *bq, 
                                const double *input, double *output, 
                                size_t length);

/**
 * @brief Destroy MLIR BiQuad JIT context and free resources
 * 
 * @param jit Pointer to JIT context to destroy
 */
void mlir_biquad_jit_destroy(MLIRBiQuadJIT *jit);

/**
 * @brief Check if MLIR BiQuad is available
 * 
 * @return 1 if MLIR BiQuad support is available, 0 otherwise
 */
int mlir_biquad_available(void);

#ifdef __cplusplus
}
#endif

#endif /* USE_MLIR */

#endif /* MLIR_BIQUAD_H */
