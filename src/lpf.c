#include "lpf.h"
#include <math.h>
#include <stdlib.h>

// Internal helper to calculate Butterworth LPF coefficients
static void calculate_butterworth_coefficients(BiQuad *bq, double sample_rate,
                                               double freq) {
  if (!bq)
    return;

  // Butterworth low-pass filter design
  // C = 1 / tan(π * freq / sample_rate)
  double C = 1.0 / tan(M_PI * freq / sample_rate);
  double C_squared = C * C;
  double sqrt2 = sqrt(2.0);

  // Calculate coefficients
  // a0 = 1 / (1 + √2*C + C²)
  bq->a0 = 1.0 / (1.0 + sqrt2 * C + C_squared);
  // a1 = 2 * a0
  bq->a1 = 2.0 * bq->a0;
  // a2 = a0
  bq->a2 = bq->a0;
  // b1 = 2 * a0 * (1 - C²)
  bq->b1 = 2.0 * bq->a0 * (1.0 - C_squared);
  // b2 = a0 * (1 - √2*C + C²)
  bq->b2 = bq->a0 * (1.0 - sqrt2 * C + C_squared);

  // Wet/dry mix (full wet for LPF)
  bq->c0 = 1.0;
  bq->d0 = 0.0;

  // Flush delays for clean start
  biquad_flush_delays(bq);
}

// Initialize LPF filter
void lpf_init(LPFFilter *lpf, double sample_rate, double freq) {
  if (!lpf)
    return;

  // Initialize biquad structures
  biquad_init(&lpf->left);
  biquad_init(&lpf->right);

  // Store frequency
  lpf->frequency = freq;

  // Calculate and set coefficients for both channels
  calculate_butterworth_coefficients(&lpf->left, sample_rate, freq);
  calculate_butterworth_coefficients(&lpf->right, sample_rate, freq);

#ifdef USE_MLIR
  // Create MLIR JIT contexts for optimized processing
  if (mlir_biquad_available()) {
    lpf->left_jit = mlir_biquad_jit_create(&lpf->left);
    lpf->right_jit = mlir_biquad_jit_create(&lpf->right);
  } else {
    lpf->left_jit = NULL;
    lpf->right_jit = NULL;
  }
#endif
}

// Update filter coefficients
void lpf_update_coefficients(LPFFilter *lpf, double sample_rate, double freq) {
  if (!lpf)
    return;

  lpf->frequency = freq;
  calculate_butterworth_coefficients(&lpf->left, sample_rate, freq);
  calculate_butterworth_coefficients(&lpf->right, sample_rate, freq);

#ifdef USE_MLIR
  // Recreate JIT contexts with new coefficients
  if (lpf->left_jit) {
    mlir_biquad_jit_destroy(lpf->left_jit);
    lpf->left_jit = mlir_biquad_jit_create(&lpf->left);
  }
  if (lpf->right_jit) {
    mlir_biquad_jit_destroy(lpf->right_jit);
    lpf->right_jit = mlir_biquad_jit_create(&lpf->right);
  }
#endif
}

// Process a single channel of audio
void lpf_process_channel(LPFFilter *lpf, double *data, size_t length,
                         int channel) {
  if (!lpf || !data)
    return;

  // Select the appropriate filter
  BiQuad *filter = (channel == 0) ? &lpf->left : &lpf->right;

  // Process each sample
  for (size_t i = 0; i < length; i++) {
    double input = data[i];
    double filtered = biquad_process(filter, input);
    // Apply wet/dry mix: output = filtered*c0 + input*d0
    data[i] = filtered * filter->c0 + input * filter->d0;
  }
}

// Process an entire audio buffer
void lpf_process_buffer(LPFFilter *lpf, AudioBuffer *buffer) {
  if (!lpf || !buffer || !buffer->data)
    return;

#ifdef USE_MLIR
  // Use MLIR-optimized processing if available
  if (lpf->left_jit && lpf->right_jit) {
    if (buffer->channels == 1) {
      // Mono: process all samples with left filter using MLIR
      mlir_biquad_process_buffer(lpf->left_jit, &lpf->left, buffer->data,
                                 buffer->data, buffer->length);
      return;
    } else if (buffer->channels == 2) {
      // Stereo: deinterleave, process, reinterleave
      size_t samples_per_channel = buffer->length / 2;
      double *left_channel = malloc(samples_per_channel * sizeof(double));
      double *right_channel = malloc(samples_per_channel * sizeof(double));

      if (left_channel && right_channel) {
        // Deinterleave
        for (size_t i = 0; i < samples_per_channel; i++) {
          left_channel[i] = buffer->data[i * 2];
          right_channel[i] = buffer->data[i * 2 + 1];
        }

        // Process both channels with MLIR
        mlir_biquad_process_buffer(lpf->left_jit, &lpf->left, left_channel,
                                   left_channel, samples_per_channel);
        mlir_biquad_process_buffer(lpf->right_jit, &lpf->right, right_channel,
                                   right_channel, samples_per_channel);

        // Reinterleave
        for (size_t i = 0; i < samples_per_channel; i++) {
          buffer->data[i * 2] = left_channel[i];
          buffer->data[i * 2 + 1] = right_channel[i];
        }

        free(left_channel);
        free(right_channel);
        return;
      }

      // Fallback if allocation failed
      free(left_channel);
      free(right_channel);
    }
  }
#endif

  // Fallback to standard C implementation
  if (buffer->channels == 1) {
    // Mono: process all samples with left filter
    lpf_process_channel(lpf, buffer->data, buffer->length, 0);
  } else if (buffer->channels == 2) {
    // Stereo: process interleaved samples
    for (size_t i = 0; i < buffer->length; i += 2) {
      // Left channel (even indices)
      double left_in = buffer->data[i];
      double left_out = biquad_process(&lpf->left, left_in);
      buffer->data[i] = left_out * lpf->left.c0 + left_in * lpf->left.d0;

      // Right channel (odd indices)
      if (i + 1 < buffer->length) {
        double right_in = buffer->data[i + 1];
        double right_out = biquad_process(&lpf->right, right_in);
        buffer->data[i + 1] =
            right_out * lpf->right.c0 + right_in * lpf->right.d0;
      }
    }
  } else {
    // Multi-channel: alternate between left and right filters
    for (size_t i = 0; i < buffer->length; i++) {
      int channel = i % buffer->channels;
      BiQuad *filter = (channel % 2 == 0) ? &lpf->left : &lpf->right;
      double input = buffer->data[i];
      double filtered = biquad_process(filter, input);
      buffer->data[i] = filtered * filter->c0 + input * filter->d0;
    }
  }
}
