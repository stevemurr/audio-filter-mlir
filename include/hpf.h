#ifndef HPF_H
#define HPF_H

#include "biquad.h"
#include "audio_io.h"

#ifdef USE_MLIR
#include "mlir_biquad.h"
#endif

// High-Pass Filter structure
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
} HPFFilter;

// Initialize HPF filter with given sample rate and cutoff frequency
// Sets up Butterworth high-pass filter coefficients
// Parameters:
//   hpf: Pointer to HPFFilter structure
//   sample_rate: Audio sample rate in Hz (e.g., 44100, 48000)
//   freq: Cutoff frequency in Hz (e.g., 80, 100)
void hpf_init(HPFFilter *hpf, double sample_rate, double freq);

// Update filter coefficients for new cutoff frequency
// Call this if you need to change the cutoff frequency dynamically
// Parameters:
//   hpf: Pointer to HPFFilter structure
//   sample_rate: Audio sample rate in Hz
//   freq: New cutoff frequency in Hz
void hpf_update_coefficients(HPFFilter *hpf, double sample_rate, double freq);

// Process an audio buffer through the high-pass filter
// Supports both mono and stereo processing
// For mono: processes all samples with left filter
// For stereo: alternates between left and right filters
// Parameters:
//   hpf: Pointer to HPFFilter structure
//   buffer: Pointer to AudioBuffer containing audio data
void hpf_process_buffer(HPFFilter *hpf, AudioBuffer *buffer);

// Process a single channel of audio data
// Useful for manual channel-by-channel processing
// Parameters:
//   hpf: Pointer to HPFFilter structure
//   data: Array of audio samples (float64, normalized [-1.0, 1.0])
//   length: Number of samples to process
//   channel: 0 for left, 1 for right
void hpf_process_channel(HPFFilter *hpf, double *data, size_t length, int channel);

#endif // HPF_H
