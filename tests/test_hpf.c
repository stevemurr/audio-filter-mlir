#include "audio_io.h"
#include "hpf.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 44100.0
#define HPF_FREQ 100.0
#define TEST_DURATION 1.0 // seconds

// Generate a test signal with mixed low and high frequencies
void generate_mixed_signal(AudioBuffer *buffer, double low_freq,
                           double high_freq) {
  double dt = 1.0 / buffer->sample_rate;

  for (size_t i = 0; i < buffer->length; i++) {
    double t = i * dt;

    if (buffer->channels == 1) {
      // Mix low and high frequency signals
      buffer->data[i] = 0.3 * sin(2.0 * M_PI * low_freq * t) +
                        0.7 * sin(2.0 * M_PI * high_freq * t);
    } else if (buffer->channels == 2) {
      size_t frame = i / 2;
      double t = frame * dt;
      double mixed = 0.3 * sin(2.0 * M_PI * low_freq * t) +
                     0.7 * sin(2.0 * M_PI * high_freq * t);
      buffer->data[i] = mixed;
    }
  }
}

// Test HPF initialization
void test_hpf_init() {
  printf("Test 1: HPF Initialization\n");

  HPFFilter hpf;
  hpf_init(&hpf, SAMPLE_RATE, HPF_FREQ);

  assert(hpf.frequency == HPF_FREQ);
  assert(hpf.left.c0 == 1.0);
  assert(hpf.left.d0 == 0.0);
  assert(hpf.right.c0 == 1.0);
  assert(hpf.right.d0 == 0.0);

  // Check that coefficients are non-zero
  assert(hpf.left.a0 != 0.0);
  assert(hpf.right.a0 != 0.0);

  printf("  ✓ HPF initialized successfully\n");
  printf("  Frequency: %.1f Hz\n", hpf.frequency);
  printf("  Left a0: %.6f, a1: %.6f, a2: %.6f\n", hpf.left.a0, hpf.left.a1,
         hpf.left.a2);
  printf("  Left b1: %.6f, b2: %.6f\n\n", hpf.left.b1, hpf.left.b2);
}

// Test coefficient update
void test_hpf_update_coefficients() {
  printf("Test 2: Coefficient Update\n");

  HPFFilter hpf;
  hpf_init(&hpf, SAMPLE_RATE, HPF_FREQ);

  double old_a0 = hpf.left.a0;
  double old_b1 = hpf.left.b1;

  // Update to a different frequency
  double new_freq = 200.0;
  hpf_update_coefficients(&hpf, SAMPLE_RATE, new_freq);

  assert(hpf.frequency == new_freq);
  assert(hpf.left.a0 != old_a0);
  assert(hpf.left.b1 != old_b1);

  printf("  ✓ Coefficients updated successfully\n");
  printf("  Old frequency: %.1f Hz, New frequency: %.1f Hz\n", HPF_FREQ,
         new_freq);
  printf("  Old a0: %.6f -> New a0: %.6f\n\n", old_a0, hpf.left.a0);
}

// Test mono signal processing
void test_hpf_process_mono() {
  printf("Test 3: Mono Signal Processing\n");

  // Create mono buffer
  int num_samples = (int)(SAMPLE_RATE * TEST_DURATION);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 1, 16);
  assert(buffer != NULL);

  // Generate mixed signal: 20 Hz (should be attenuated) + 1000 Hz (should pass)
  generate_mixed_signal(buffer, 20.0, 1000.0);

  // Calculate RMS before filtering
  double rms_before = 0.0;
  for (size_t i = 0; i < buffer->length; i++) {
    rms_before += buffer->data[i] * buffer->data[i];
  }
  rms_before = sqrt(rms_before / buffer->length);

  // Apply HPF
  HPFFilter hpf;
  hpf_init(&hpf, SAMPLE_RATE, HPF_FREQ);
  hpf_process_buffer(&hpf, buffer);

  // Calculate RMS after filtering
  double rms_after = 0.0;
  for (size_t i = 0; i < buffer->length; i++) {
    rms_after += buffer->data[i] * buffer->data[i];
  }
  rms_after = sqrt(rms_after / buffer->length);

  printf("  RMS before: %.6f\n", rms_before);
  printf("  RMS after:  %.6f\n", rms_after);
  printf("  Attenuation: %.2f%%\n", (1.0 - rms_after / rms_before) * 100.0);

  // High frequencies should pass, so RMS shouldn't drop to zero
  assert(rms_after > 0.1);

  // But some attenuation should occur (low freq component removed)
  assert(rms_after < rms_before);

  audio_buffer_free(buffer);
  printf("  ✓ Mono processing working correctly\n\n");
}

// Test stereo signal processing
void test_hpf_process_stereo() {
  printf("Test 4: Stereo Signal Processing\n");

  // Create stereo buffer (interleaved)
  int num_frames = (int)(SAMPLE_RATE * TEST_DURATION);
  int num_samples = num_frames * 2;
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 2, 16);
  assert(buffer != NULL);

  // Generate mixed signal
  generate_mixed_signal(buffer, 20.0, 1000.0);

  // Apply HPF
  HPFFilter hpf;
  hpf_init(&hpf, SAMPLE_RATE, HPF_FREQ);
  hpf_process_buffer(&hpf, buffer);

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

// Test DC offset removal
void test_hpf_dc_removal() {
  printf("Test 5: DC Offset Removal\n");

  // Create buffer with DC offset
  int num_samples = (int)(SAMPLE_RATE * 0.5);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 1, 16);
  assert(buffer != NULL);

  // Add DC offset + small AC component
  double dc_offset = 0.5;
  for (size_t i = 0; i < buffer->length; i++) {
    double t = i / SAMPLE_RATE;
    buffer->data[i] = dc_offset + 0.1 * sin(2.0 * M_PI * 440.0 * t);
  }

  // Calculate mean before filtering
  double mean_before = 0.0;
  for (size_t i = 0; i < buffer->length; i++) {
    mean_before += buffer->data[i];
  }
  mean_before /= buffer->length;

  // Apply HPF (should remove DC)
  HPFFilter hpf;
  hpf_init(&hpf, SAMPLE_RATE, HPF_FREQ);
  hpf_process_buffer(&hpf, buffer);

  // Calculate mean after filtering (skip first 100 samples for filter settling)
  double mean_after = 0.0;
  for (size_t i = 100; i < buffer->length; i++) {
    mean_after += buffer->data[i];
  }
  mean_after /= (buffer->length - 100);

  printf("  DC offset before: %.6f\n", mean_before);
  printf("  DC offset after:  %.6f\n", mean_after);
  printf("  Removal: %.2f%%\n", (1.0 - fabs(mean_after / mean_before)) * 100.0);

  // DC should be significantly reduced
  assert(fabs(mean_after) < fabs(mean_before) * 0.1);

  audio_buffer_free(buffer);
  printf("  ✓ DC offset removal working\n\n");
}

// Test HPF with WAV file I/O
void test_hpf_wav_roundtrip() {
  printf("Test 6: WAV File Roundtrip with HPF\n");

  const char *input_file = "tests/test_data/hpf_input.wav";
  const char *output_file = "tests/test_data/hpf_output.wav";

  // Create test signal
  int num_samples = (int)(SAMPLE_RATE * 0.5);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 2, 16);
  assert(buffer != NULL);

  // Mix of frequencies: 30Hz (below cutoff) + 440Hz (above cutoff)
  generate_mixed_signal(buffer, 30.0, 440.0);

  // Write input file
  AudioError error = write_wave(input_file, buffer);
  assert(error == AUDIO_SUCCESS);
  printf("  ✓ Wrote input file: %s\n", input_file);

  // Apply HPF
  HPFFilter hpf;
  hpf_init(&hpf, SAMPLE_RATE, HPF_FREQ);
  hpf_process_buffer(&hpf, buffer);

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
  printf("  ✓ WAV roundtrip with HPF working\n\n");
}

int main() {
  printf("\n=== High-Pass Filter Tests ===\n\n");

  test_hpf_init();
  test_hpf_update_coefficients();
  test_hpf_process_mono();
  test_hpf_process_stereo();
  test_hpf_dc_removal();
  test_hpf_wav_roundtrip();

  printf("=== All HPF tests passed! ===\n\n");
  return 0;
}
