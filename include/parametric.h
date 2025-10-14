#ifndef PARAMETRIC_H
#define PARAMETRIC_H

#include "biquad.h"
#include "audio_io.h"

#ifdef USE_MLIR
#include "mlir_biquad.h"
#endif

// Parametric EQ Filter structure
// Uses constant-Q parametric equalization with biquad implementation
// Supports both boost and cut at specified frequency
typedef struct {
    BiQuad left;        // Left channel biquad filter
    BiQuad right;       // Right channel biquad filter
    double frequency;   // Center frequency in Hz
    double gain;        // Gain in dB (positive = boost, negative = cut)
    double q;           // Q factor (bandwidth control, typically 0.5-10.0)
#ifdef USE_MLIR
    MLIRBiQuadJIT *left_jit;   // MLIR JIT context for left channel
    MLIRBiQuadJIT *right_jit;  // MLIR JIT context for right channel
#endif
} ParametricFilter;

// Initialize parametric EQ filter with given parameters
// Sets up constant-Q parametric equalization coefficients
// Parameters:
//   peq: Pointer to ParametricFilter structure
//   sample_rate: Audio sample rate in Hz (e.g., 44100, 48000)
//   freq: Center frequency in Hz (e.g., 1000, 5000)
//   gain: Gain in dB, positive for boost, negative for cut (e.g., +6.0, -3.0)
//   q: Q factor for bandwidth control (e.g., 0.707, 1.0, 2.0)
//      Lower Q = wider bandwidth, Higher Q = narrower bandwidth
void parametric_init(ParametricFilter *peq, double sample_rate, double freq, 
                    double gain, double q);

// Update filter coefficients for new parameters
// Call this if you need to change frequency, gain, or Q dynamically
// Parameters:
//   peq: Pointer to ParametricFilter structure
//   sample_rate: Audio sample rate in Hz
//   freq: New center frequency in Hz
//   gain: New gain in dB
//   q: New Q factor
void parametric_update_coefficients(ParametricFilter *peq, double sample_rate, 
                                   double freq, double gain, double q);

// Process an audio buffer through the parametric EQ
// Supports both mono and stereo processing
// For mono: processes all samples with left filter
// For stereo: alternates between left and right filters
// Parameters:
//   peq: Pointer to ParametricFilter structure
//   buffer: Pointer to AudioBuffer containing audio data
void parametric_process_buffer(ParametricFilter *peq, AudioBuffer *buffer);

// Process a single channel of audio data
// Useful for manual channel-by-channel processing
// Parameters:
//   peq: Pointer to ParametricFilter structure
//   data: Array of audio samples (float64, normalized [-1.0, 1.0])
//   length: Number of samples to process
//   channel: 0 for left, 1 for right
void parametric_process_channel(ParametricFilter *peq, double *data, size_t length, int channel);

#endif // PARAMETRIC_H
