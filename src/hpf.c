#include "hpf.h"
#include <math.h>
#include <stdlib.h>

// Internal helper to calculate Butterworth HPF coefficients
static void calculate_butterworth_coefficients(BiQuad *bq, double sample_rate,
                                               double freq) {
  if (!bq)
    return;

  // Butterworth high-pass filter design
  // C = tan(π * freq / sample_rate)
  double C = tan(M_PI * freq / sample_rate);
  double C_squared = C * C;
  double sqrt2 = sqrt(2.0);

  // Calculate coefficients
  // a0 = 1 / (1 + √2*C + C²)
  bq->a0 = 1.0 / (1.0 + sqrt2 * C + C_squared);
  // a1 = -2 * a0
  bq->a1 = -2.0 * bq->a0;
  // a2 = a0
  bq->a2 = bq->a0;
  // b1 = 2 * a0 * (C² - 1)
  bq->b1 = 2.0 * bq->a0 * (C_squared - 1.0);
  // b2 = a0 * (1 - √2*C + C²)
  bq->b2 = bq->a0 * (1.0 - sqrt2 * C + C_squared);

  // Wet/dry mix (full wet for HPF)
  bq->c0 = 1.0;
  bq->d0 = 0.0;

  // Flush delays for clean start
  biquad_flush_delays(bq);
}

// Initialize HPF filter
void hpf_init(HPFFilter *hpf, double sample_rate, double freq) {
  if (!hpf)
    return;

  // Initialize biquad structures
  biquad_init(&hpf->left);
  biquad_init(&hpf->right);

  // Store frequency
  hpf->frequency = freq;

  // Calculate and set coefficients for both channels
  calculate_butterworth_coefficients(&hpf->left, sample_rate, freq);
  calculate_butterworth_coefficients(&hpf->right, sample_rate, freq);

#ifdef USE_MLIR
  // Create MLIR JIT contexts for optimized processing
  if (mlir_biquad_available()) {
    hpf->left_jit = mlir_biquad_jit_create(&hpf->left);
    hpf->right_jit = mlir_biquad_jit_create(&hpf->right);
  } else {
    hpf->left_jit = NULL;
    hpf->right_jit = NULL;
  }
#endif
}

// Update filter coefficients
void hpf_update_coefficients(HPFFilter *hpf, double sample_rate, double freq) {
  if (!hpf)
    return;

  hpf->frequency = freq;
  calculate_butterworth_coefficients(&hpf->left, sample_rate, freq);
  calculate_butterworth_coefficients(&hpf->right, sample_rate, freq);

#ifdef USE_MLIR
  // Recreate JIT contexts with new coefficients
  if (hpf->left_jit) {
    mlir_biquad_jit_destroy(hpf->left_jit);
    hpf->left_jit = mlir_biquad_jit_create(&hpf->left);
  }
  if (hpf->right_jit) {
    mlir_biquad_jit_destroy(hpf->right_jit);
    hpf->right_jit = mlir_biquad_jit_create(&hpf->right);
  }
#endif
}

// Process a single channel of audio
void hpf_process_channel(HPFFilter *hpf, double *data, size_t length,
                         int channel) {
  if (!hpf || !data)
    return;

  // Select the appropriate filter
  BiQuad *filter = (channel == 0) ? &hpf->left : &hpf->right;

  // Process each sample
  for (size_t i = 0; i < length; i++) {
    double input = data[i];
    double filtered = biquad_process(filter, input);
    // Apply wet/dry mix: output = filtered*c0 + input*d0
    data[i] = filtered * filter->c0 + input * filter->d0;
  }
}

// Process an entire audio buffer
void hpf_process_buffer(HPFFilter *hpf, AudioBuffer *buffer) {
  if (!hpf || !buffer || !buffer->data)
    return;

#ifdef USE_MLIR
  // Use MLIR-optimized processing if available
  if (hpf->left_jit && hpf->right_jit) {
    if (buffer->channels == 1) {
      // Mono: process all samples with left filter using MLIR
      mlir_biquad_process_buffer(hpf->left_jit, &hpf->left, buffer->data,
                                 buffer->data, buffer->length);

      // Apply wet/dry mix if needed (c0 should be 1.0, d0 should be 0.0 for
      // HPF)
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
        mlir_biquad_process_buffer(hpf->left_jit, &hpf->left, left_channel,
                                   left_channel, samples_per_channel);
        mlir_biquad_process_buffer(hpf->right_jit, &hpf->right, right_channel,
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
    hpf_process_channel(hpf, buffer->data, buffer->length, 0);
  } else if (buffer->channels == 2) {
    // Stereo: process interleaved samples
    for (size_t i = 0; i < buffer->length; i += 2) {
      // Left channel (even indices)
      double left_in = buffer->data[i];
      double left_out = biquad_process(&hpf->left, left_in);
      buffer->data[i] = left_out * hpf->left.c0 + left_in * hpf->left.d0;

      // Right channel (odd indices)
      if (i + 1 < buffer->length) {
        double right_in = buffer->data[i + 1];
        double right_out = biquad_process(&hpf->right, right_in);
        buffer->data[i + 1] =
            right_out * hpf->right.c0 + right_in * hpf->right.d0;
      }
    }
  } else {
    // Multi-channel: alternate between left and right filters
    for (size_t i = 0; i < buffer->length; i++) {
      int channel = i % buffer->channels;
      BiQuad *filter = (channel % 2 == 0) ? &hpf->left : &hpf->right;
      double input = buffer->data[i];
      double filtered = biquad_process(filter, input);
      buffer->data[i] = filtered * filter->c0 + input * filter->d0;
    }
  }
}
