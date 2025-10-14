#ifndef LPF_H
#define LPF_H

#include "biquad.h"
#include "audio_io.h"

#ifdef USE_MLIR
#include "mlir_biquad.h"
#endif

// Low-Pass Filter structure
// Uses Butterworth design with biquad implementation
// Supports stereo processing with separate left/right filters
typedef struct {
    BiQuad left;        // Left channel biquad filter
    BiQuad right;       // Right channel biquad filter
    double frequency;   // Cutoff frequency in Hz
#ifdef USE_MLIR
    MLIRBiQuadJIT *left_jit;   // MLIR JIT context for left channel
    MLIRBiQuadJIT *right_jit;  // MLIR JIT context for right channel
#endif
} LPFFilter;

// Initialize LPF filter with given sample rate and cutoff frequency
// Sets up Butterworth low-pass filter coefficients
// Parameters:
//   lpf: Pointer to LPFFilter structure
//   sample_rate: Audio sample rate in Hz (e.g., 44100, 48000)
//   freq: Cutoff frequency in Hz (e.g., 5000, 8000)
void lpf_init(LPFFilter *lpf, double sample_rate, double freq);

// Update filter coefficients for new cutoff frequency
// Call this if you need to change the cutoff frequency dynamically
// Parameters:
//   lpf: Pointer to LPFFilter structure
//   sample_rate: Audio sample rate in Hz
//   freq: New cutoff frequency in Hz
void lpf_update_coefficients(LPFFilter *lpf, double sample_rate, double freq);

// Process an audio buffer through the low-pass filter
// Supports both mono and stereo processing
// For mono: processes all samples with left filter
// For stereo: alternates between left and right filters
// Parameters:
//   lpf: Pointer to LPFFilter structure
//   buffer: Pointer to AudioBuffer containing audio data
void lpf_process_buffer(LPFFilter *lpf, AudioBuffer *buffer);

// Process a single channel of audio data
// Useful for manual channel-by-channel processing
// Parameters:
//   lpf: Pointer to LPFFilter structure
//   data: Array of audio samples (float64, normalized [-1.0, 1.0])
//   length: Number of samples to process
//   channel: 0 for left, 1 for right
void lpf_process_channel(LPFFilter *lpf, double *data, size_t length, int channel);

#endif // LPF_H
