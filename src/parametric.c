#include "parametric.h"
#include <math.h>
#include <stdlib.h>

// Internal helper to calculate constant-Q parametric EQ coefficients
static void calculate_parametric_coefficients(BiQuad *bq, double sample_rate,
                                              double freq, double gain,
                                              double q) {
  if (!bq)
    return;

  // Constant-Q parametric EQ design
  double K = tan((M_PI * freq) / sample_rate);
  double V0 = pow(10.0, (gain / 20.0)); // Convert dB to linear gain
  double K_squared = K * K;

  // Calculate intermediate values
  double D0 = 1.0 + ((1.0 / q) * K) + K_squared;
  double E0 = 1.0 + ((1.0 / (V0 * q)) * K) + K_squared;
  double A = 1.0 + ((V0 / q) * K) + K_squared;
  double B = 2.0 * (K_squared - 1.0);
  double G = 1.0 - ((V0 / q) * K) + K_squared;
  double D = 1.0 - ((1.0 / q) * K) + K_squared;
  double E = 1.0 - ((1.0 / (V0 * q)) * K) + K_squared;

  if (gain >= 0.0) {
    // Boost
    bq->a0 = A / D0;
    bq->a1 = B / D0;
    bq->a2 = G / D0;
    bq->b1 = B / D0;
    bq->b2 = D / D0;
  } else {
    // Cut
    bq->a0 = D0 / E0;
    bq->a1 = B / E0;
    bq->a2 = D / E0;
    bq->b1 = B / E0;
    bq->b2 = E / E0;
  }

  // Full wet, no dry (EQ processes entire signal)
  bq->c0 = 1.0;
  bq->d0 = 0.0;

  // Flush delays for clean start
  biquad_flush_delays(bq);
}

// Initialize parametric EQ filter
void parametric_init(ParametricFilter *peq, double sample_rate, double freq,
                     double gain, double q) {
  if (!peq)
    return;

  // Initialize biquad structures
  biquad_init(&peq->left);
  biquad_init(&peq->right);

  // Store parameters
  peq->frequency = freq;
  peq->gain = gain;
  peq->q = q;

  // Calculate and set coefficients for both channels
  calculate_parametric_coefficients(&peq->left, sample_rate, freq, gain, q);
  calculate_parametric_coefficients(&peq->right, sample_rate, freq, gain, q);

#ifdef USE_MLIR
  // Create MLIR JIT contexts for optimized processing
  if (mlir_biquad_available()) {
    peq->left_jit = mlir_biquad_jit_create(&peq->left);
    peq->right_jit = mlir_biquad_jit_create(&peq->right);
  } else {
    peq->left_jit = NULL;
    peq->right_jit = NULL;
  }
#endif
}

// Update filter coefficients
void parametric_update_coefficients(ParametricFilter *peq, double sample_rate,
                                    double freq, double gain, double q) {
  if (!peq)
    return;

  peq->frequency = freq;
  peq->gain = gain;
  peq->q = q;
  calculate_parametric_coefficients(&peq->left, sample_rate, freq, gain, q);
  calculate_parametric_coefficients(&peq->right, sample_rate, freq, gain, q);

#ifdef USE_MLIR
  // Recreate JIT contexts with new coefficients
  if (peq->left_jit) {
    mlir_biquad_jit_destroy(peq->left_jit);
    peq->left_jit = mlir_biquad_jit_create(&peq->left);
  }
  if (peq->right_jit) {
    mlir_biquad_jit_destroy(peq->right_jit);
    peq->right_jit = mlir_biquad_jit_create(&peq->right);
  }
#endif
}

// Process a single channel of audio
void parametric_process_channel(ParametricFilter *peq, double *data,
                                size_t length, int channel) {
  if (!peq || !data)
    return;

  // Select the appropriate filter
  BiQuad *filter = (channel == 0) ? &peq->left : &peq->right;

  // Process each sample
  for (size_t i = 0; i < length; i++) {
    double input = data[i];
    double filtered = biquad_process(filter, input);
    // Apply wet/dry mix: output = filtered*c0 + input*d0
    data[i] = filtered * filter->c0 + input * filter->d0;
  }
}

// Process an entire audio buffer
void parametric_process_buffer(ParametricFilter *peq, AudioBuffer *buffer) {
  if (!peq || !buffer || !buffer->data)
    return;

#ifdef USE_MLIR
  // Use MLIR-optimized processing if available
  if (peq->left_jit && peq->right_jit) {
    if (buffer->channels == 1) {
      // Mono: process all samples with left filter using MLIR
      mlir_biquad_process_buffer(peq->left_jit, &peq->left, buffer->data,
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
        mlir_biquad_process_buffer(peq->left_jit, &peq->left, left_channel,
                                   left_channel, samples_per_channel);
        mlir_biquad_process_buffer(peq->right_jit, &peq->right, right_channel,
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
    parametric_process_channel(peq, buffer->data, buffer->length, 0);
  } else if (buffer->channels == 2) {
    // Stereo: process interleaved samples
    for (size_t i = 0; i < buffer->length; i += 2) {
      // Left channel (even indices)
      double left_in = buffer->data[i];
      double left_out = biquad_process(&peq->left, left_in);
      buffer->data[i] = left_out * peq->left.c0 + left_in * peq->left.d0;

      // Right channel (odd indices)
      if (i + 1 < buffer->length) {
        double right_in = buffer->data[i + 1];
        double right_out = biquad_process(&peq->right, right_in);
        buffer->data[i + 1] =
            right_out * peq->right.c0 + right_in * peq->right.d0;
      }
    }
  } else {
    // Multi-channel: alternate between left and right filters
    for (size_t i = 0; i < buffer->length; i++) {
      int channel = i % buffer->channels;
      BiQuad *filter = (channel % 2 == 0) ? &peq->left : &peq->right;
      double input = buffer->data[i];
      double filtered = biquad_process(filter, input);
      buffer->data[i] = filtered * filter->c0 + input * filter->d0;
    }
  }
}
