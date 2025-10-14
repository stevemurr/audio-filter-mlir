#include "audio_io.h"
#include "lpf.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 44100.0
#define LPF_FREQ 5000.0
#define TEST_DURATION 1.0 // seconds

// Generate a test signal with mixed low and high frequencies
void generate_mixed_signal(AudioBuffer *buffer, double low_freq,
                           double high_freq) {
  double dt = 1.0 / buffer->sample_rate;

  for (size_t i = 0; i < buffer->length; i++) {
    double t = i * dt;

    if (buffer->channels == 1) {
      // Mix low and high frequency signals
      buffer->data[i] = 0.7 * sin(2.0 * M_PI * low_freq * t) +
                        0.3 * sin(2.0 * M_PI * high_freq * t);
    } else if (buffer->channels == 2) {
      size_t frame = i / 2;
      double t = frame * dt;
      double mixed = 0.7 * sin(2.0 * M_PI * low_freq * t) +
                     0.3 * sin(2.0 * M_PI * high_freq * t);
      buffer->data[i] = mixed;
    }
  }
}

// Test LPF initialization
void test_lpf_init() {
  printf("Test 1: LPF Initialization\n");

  LPFFilter lpf;
  lpf_init(&lpf, SAMPLE_RATE, LPF_FREQ);

  assert(lpf.frequency == LPF_FREQ);
  assert(lpf.left.c0 == 1.0);
  assert(lpf.left.d0 == 0.0);
  assert(lpf.right.c0 == 1.0);
  assert(lpf.right.d0 == 0.0);

  // Check that coefficients are non-zero
  assert(lpf.left.a0 != 0.0);
  assert(lpf.right.a0 != 0.0);

  printf("  ✓ LPF initialized successfully\n");
  printf("  Frequency: %.1f Hz\n", lpf.frequency);
  printf("  Left a0: %.6f, a1: %.6f, a2: %.6f\n", lpf.left.a0, lpf.left.a1,
         lpf.left.a2);
  printf("  Left b1: %.6f, b2: %.6f\n\n", lpf.left.b1, lpf.left.b2);
}

// Test coefficient update
void test_lpf_update_coefficients() {
  printf("Test 2: Coefficient Update\n");

  LPFFilter lpf;
  lpf_init(&lpf, SAMPLE_RATE, LPF_FREQ);

  double old_a0 = lpf.left.a0;
  double old_b1 = lpf.left.b1;

  // Update to a different frequency
  double new_freq = 10000.0;
  lpf_update_coefficients(&lpf, SAMPLE_RATE, new_freq);

  assert(lpf.frequency == new_freq);
  assert(lpf.left.a0 != old_a0);
  assert(lpf.left.b1 != old_b1);

  printf("  ✓ Coefficients updated successfully\n");
  printf("  Old frequency: %.1f Hz, New frequency: %.1f Hz\n", LPF_FREQ,
         new_freq);
  printf("  Old a0: %.6f -> New a0: %.6f\n\n", old_a0, lpf.left.a0);
}

// Test mono signal processing
void test_lpf_process_mono() {
  printf("Test 3: Mono Signal Processing\n");

  // Create mono buffer
  int num_samples = (int)(SAMPLE_RATE * TEST_DURATION);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 1, 16);
  assert(buffer != NULL);

  // Generate mixed signal: 1000 Hz (should pass) + 10000 Hz (should be
  // attenuated)
  generate_mixed_signal(buffer, 1000.0, 10000.0);

  // Calculate RMS before filtering
  double rms_before = 0.0;
  for (size_t i = 0; i < buffer->length; i++) {
    rms_before += buffer->data[i] * buffer->data[i];
  }
  rms_before = sqrt(rms_before / buffer->length);

  // Apply LPF
  LPFFilter lpf;
  lpf_init(&lpf, SAMPLE_RATE, LPF_FREQ);
  lpf_process_buffer(&lpf, buffer);

  // Calculate RMS after filtering
  double rms_after = 0.0;
  for (size_t i = 0; i < buffer->length; i++) {
    rms_after += buffer->data[i] * buffer->data[i];
  }
  rms_after = sqrt(rms_after / buffer->length);

  printf("  RMS before: %.6f\n", rms_before);
  printf("  RMS after:  %.6f\n", rms_after);
  printf("  Attenuation: %.2f%%\n", (1.0 - rms_after / rms_before) * 100.0);

  // Low frequencies should pass, so RMS shouldn't drop to zero
  assert(rms_after > 0.1);

  // But some attenuation should occur (high freq component removed)
  assert(rms_after < rms_before);

  audio_buffer_free(buffer);
  printf("  ✓ Mono processing working correctly\n\n");
}

// Test stereo signal processing
void test_lpf_process_stereo() {
  printf("Test 4: Stereo Signal Processing\n");

  // Create stereo buffer (interleaved)
  int num_frames = (int)(SAMPLE_RATE * TEST_DURATION);
  int num_samples = num_frames * 2;
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 2, 16);
  assert(buffer != NULL);

  // Generate mixed signal
  generate_mixed_signal(buffer, 1000.0, 10000.0);

  // Apply LPF
  LPFFilter lpf;
  lpf_init(&lpf, SAMPLE_RATE, LPF_FREQ);
  lpf_process_buffer(&lpf, buffer);

  // Check that both channels were processed
  int left_nonzero = 0;
  int right_nonzero = 0;

  for (size_t i = 0; i < buffer->length; i += 2) {
    if (fabs(buffer->data[i]) > 0.001)
      left_nonzero++;
    if (fabs(buffer->data[i + 1]) > 0.001)
      right_nonzero++;
  }

  printf("  Left channel non-zero samples: %d/%d\n", left_nonzero, num_frames);
  printf("  Right channel non-zero samples: %d/%d\n", right_nonzero,
         num_frames);

  assert(left_nonzero > num_frames / 2);
  assert(right_nonzero > num_frames / 2);

  audio_buffer_free(buffer);
  printf("  ✓ Stereo processing working correctly\n\n");
}

// Test high-frequency attenuation
void test_lpf_high_freq_attenuation() {
  printf("Test 5: High-Frequency Attenuation\n");

  // Create buffer with high frequency only
  int num_samples = (int)(SAMPLE_RATE * 0.5);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 1, 16);
  assert(buffer != NULL);

  // Generate 15000 Hz signal (well above 5000 Hz cutoff)
  for (size_t i = 0; i < buffer->length; i++) {
    double t = i / SAMPLE_RATE;
    buffer->data[i] = sin(2.0 * M_PI * 15000.0 * t);
  }

  // Calculate RMS before filtering
  double rms_before = 0.0;
  for (size_t i = 0; i < buffer->length; i++) {
    rms_before += buffer->data[i] * buffer->data[i];
  }
  rms_before = sqrt(rms_before / buffer->length);

  // Apply LPF (5000 Hz cutoff)
  LPFFilter lpf;
  lpf_init(&lpf, SAMPLE_RATE, LPF_FREQ);
  lpf_process_buffer(&lpf, buffer);

  // Calculate RMS after filtering (skip first 100 samples for filter settling)
  double rms_after = 0.0;
  for (size_t i = 100; i < buffer->length; i++) {
    rms_after += buffer->data[i] * buffer->data[i];
  }
  rms_after /= (buffer->length - 100);
  rms_after = sqrt(rms_after);

  printf("  RMS before: %.6f\n", rms_before);
  printf("  RMS after:  %.6f\n", rms_after);
  printf("  Attenuation: %.2f%%\n", (1.0 - rms_after / rms_before) * 100.0);

  // High frequency should be significantly attenuated
  assert(rms_after < rms_before * 0.5); // At least 50% attenuation

  audio_buffer_free(buffer);
  printf("  ✓ High-frequency attenuation working\n\n");
}

// Test LPF with WAV file I/O
void test_lpf_wav_roundtrip() {
  printf("Test 6: WAV File Roundtrip with LPF\n");

  const char *input_file = "tests/test_data/lpf_input.wav";
  const char *output_file = "tests/test_data/lpf_output.wav";

  // Create test signal
  int num_samples = (int)(SAMPLE_RATE * 0.5);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 2, 16);
  assert(buffer != NULL);

  // Mix of frequencies: 1000Hz (below cutoff) + 12000Hz (above cutoff)
  generate_mixed_signal(buffer, 1000.0, 12000.0);

  // Write input file
  AudioError error = write_wave(input_file, buffer);
  assert(error == AUDIO_SUCCESS);
  printf("  ✓ Wrote input file: %s\n", input_file);

  // Apply LPF
  LPFFilter lpf;
  lpf_init(&lpf, SAMPLE_RATE, LPF_FREQ);
  lpf_process_buffer(&lpf, buffer);

  // Write output file
  error = write_wave(output_file, buffer);
  assert(error == AUDIO_SUCCESS);
  printf("  ✓ Wrote output file: %s\n", output_file);

  // Read back and verify
  AudioBuffer *readback = read_wave(output_file, &error);
  assert(readback != NULL);
  assert(readback->sample_rate == buffer->sample_rate);
  assert(readback->channels == buffer->channels);
  printf("  ✓ Read back output file successfully\n");

  audio_buffer_free(buffer);
  audio_buffer_free(readback);
  printf("  ✓ WAV roundtrip with LPF working\n\n");
}

int main() {
  printf("\n=== Low-Pass Filter Tests ===\n\n");

  test_lpf_init();
  test_lpf_update_coefficients();
  test_lpf_process_mono();
  test_lpf_process_stereo();
  test_lpf_high_freq_attenuation();
  test_lpf_wav_roundtrip();

  printf("=== All LPF tests passed! ===\n\n");
  return 0;
}
