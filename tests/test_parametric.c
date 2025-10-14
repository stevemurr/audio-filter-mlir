#include "audio_io.h"
#include "parametric.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>

#define SAMPLE_RATE 44100.0
#define TEST_DURATION 1.0 // seconds

// Generate a test signal at specific frequency
void generate_tone(AudioBuffer *buffer, double freq) {
  double dt = 1.0 / buffer->sample_rate;

  for (size_t i = 0; i < buffer->length; i++) {
    double t = (i / buffer->channels) * dt;
    double sample = sin(2.0 * M_PI * freq * t);
    buffer->data[i] = sample;
  }
}

// Calculate RMS of buffer
double calculate_rms(AudioBuffer *buffer) {
  double sum = 0.0;
  for (size_t i = 0; i < buffer->length; i++) {
    sum += buffer->data[i] * buffer->data[i];
  }
  return sqrt(sum / buffer->length);
}

// Test parametric EQ initialization
void test_parametric_init() {
  printf("Test 1: Parametric EQ Initialization\n");

  ParametricFilter peq;
  parametric_init(&peq, SAMPLE_RATE, 1000.0, 6.0, 1.0);

  assert(peq.frequency == 1000.0);
  assert(peq.gain == 6.0);
  assert(peq.q == 1.0);
  assert(peq.left.c0 == 1.0);
  assert(peq.left.d0 == 0.0);

  // Check that coefficients are non-zero
  assert(peq.left.a0 != 0.0);
  assert(peq.right.a0 != 0.0);

  printf("  ✓ Parametric EQ initialized successfully\n");
  printf("  Frequency: %.1f Hz, Gain: %.1f dB, Q: %.2f\n", peq.frequency,
         peq.gain, peq.q);
  printf("  Left a0: %.6f, a1: %.6f, a2: %.6f\n", peq.left.a0, peq.left.a1,
         peq.left.a2);
  printf("  Left b1: %.6f, b2: %.6f\n\n", peq.left.b1, peq.left.b2);
}

// Test coefficient update
void test_parametric_update_coefficients() {
  printf("Test 2: Coefficient Update\n");

  ParametricFilter peq;
  parametric_init(&peq, SAMPLE_RATE, 1000.0, 6.0, 1.0);

  double old_a0 = peq.left.a0;

  // Update to different parameters
  parametric_update_coefficients(&peq, SAMPLE_RATE, 2000.0, -3.0, 2.0);

  assert(peq.frequency == 2000.0);
  assert(peq.gain == -3.0);
  assert(peq.q == 2.0);
  assert(peq.left.a0 != old_a0);

  printf("  ✓ Coefficients updated successfully\n");
  printf("  Old: 1000 Hz, +6.0 dB, Q=1.0, a0=%.6f\n", old_a0);
  printf("  New: 2000 Hz, -3.0 dB, Q=2.0, a0=%.6f\n\n", peq.left.a0);
}

// Test boost at center frequency
void test_parametric_boost() {
  printf("Test 3: Boost at Center Frequency\n");

  // Create buffer with 1000 Hz tone
  int num_samples = (int)(SAMPLE_RATE * TEST_DURATION);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 1, 16);
  assert(buffer != NULL);

  generate_tone(buffer, 1000.0);
  double rms_before = calculate_rms(buffer);

  // Apply +6 dB boost at 1000 Hz
  ParametricFilter peq;
  parametric_init(&peq, SAMPLE_RATE, 1000.0, 6.0, 1.0);
  parametric_process_buffer(&peq, buffer);

  double rms_after = calculate_rms(buffer);
  double gain_db = 20.0 * log10(rms_after / rms_before);

  printf("  RMS before: %.6f\n", rms_before);
  printf("  RMS after:  %.6f\n", rms_after);
  printf("  Measured gain: %.2f dB (expected: ~6.0 dB)\n", gain_db);

  // Should be boosted (within tolerance for filter settling)
  assert(rms_after > rms_before);
  assert(gain_db > 4.0 && gain_db < 8.0); // Reasonable range

  audio_buffer_free(buffer);
  printf("  ✓ Boost working correctly\n\n");
}

// Test cut at center frequency
void test_parametric_cut() {
  printf("Test 4: Cut at Center Frequency\n");

  // Create buffer with 1000 Hz tone
  int num_samples = (int)(SAMPLE_RATE * TEST_DURATION);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 1, 16);
  assert(buffer != NULL);

  generate_tone(buffer, 1000.0);
  double rms_before = calculate_rms(buffer);

  // Apply -6 dB cut at 1000 Hz
  ParametricFilter peq;
  parametric_init(&peq, SAMPLE_RATE, 1000.0, -6.0, 1.0);
  parametric_process_buffer(&peq, buffer);

  double rms_after = calculate_rms(buffer);
  double gain_db = 20.0 * log10(rms_after / rms_before);

  printf("  RMS before: %.6f\n", rms_before);
  printf("  RMS after:  %.6f\n", rms_after);
  printf("  Measured gain: %.2f dB (expected: ~-6.0 dB)\n", gain_db);

  // Should be attenuated
  assert(rms_after < rms_before);
  assert(gain_db < -4.0 && gain_db > -8.0); // Reasonable range

  audio_buffer_free(buffer);
  printf("  ✓ Cut working correctly\n\n");
}

// Test Q factor (bandwidth) effect
void test_parametric_q_factor() {
  printf("Test 5: Q Factor Effect\n");

  // Test narrow band (high Q)
  int num_samples = (int)(SAMPLE_RATE * TEST_DURATION);
  AudioBuffer *buffer1 = audio_buffer_create(num_samples, SAMPLE_RATE, 1, 16);
  AudioBuffer *buffer2 = audio_buffer_create(num_samples, SAMPLE_RATE, 1, 16);
  assert(buffer1 != NULL && buffer2 != NULL);

  // Generate 1100 Hz tone (slightly off center)
  generate_tone(buffer1, 1100.0);
  generate_tone(buffer2, 1100.0);

  double rms_orig = calculate_rms(buffer1);

  // Apply boost with narrow Q (Q=5.0)
  ParametricFilter peq_narrow;
  parametric_init(&peq_narrow, SAMPLE_RATE, 1000.0, 6.0, 5.0);
  parametric_process_buffer(&peq_narrow, buffer1);
  double rms_narrow = calculate_rms(buffer1);

  // Apply boost with wide Q (Q=0.5)
  ParametricFilter peq_wide;
  parametric_init(&peq_wide, SAMPLE_RATE, 1000.0, 6.0, 0.5);
  parametric_process_buffer(&peq_wide, buffer2);
  double rms_wide = calculate_rms(buffer2);

  double gain_narrow = 20.0 * log10(rms_narrow / rms_orig);
  double gain_wide = 20.0 * log10(rms_wide / rms_orig);

  printf("  Testing 1100 Hz tone with 1000 Hz center:\n");
  printf("  Narrow Q (5.0): %.2f dB boost\n", gain_narrow);
  printf("  Wide Q (0.5):   %.2f dB boost\n", gain_wide);

  // Wide Q should affect off-center frequency more
  assert(gain_wide > gain_narrow);

  audio_buffer_free(buffer1);
  audio_buffer_free(buffer2);
  printf("  ✓ Q factor working correctly\n\n");
}

// Test stereo processing
void test_parametric_stereo() {
  printf("Test 6: Stereo Signal Processing\n");

  // Create stereo buffer
  int num_frames = (int)(SAMPLE_RATE * TEST_DURATION);
  int num_samples = num_frames * 2;
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 2, 16);
  assert(buffer != NULL);

  // Generate 1000 Hz tone
  generate_tone(buffer, 1000.0);

  // Apply parametric EQ
  ParametricFilter peq;
  parametric_init(&peq, SAMPLE_RATE, 1000.0, 6.0, 1.0);
  parametric_process_buffer(&peq, buffer);

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

// Test WAV file roundtrip
void test_parametric_wav_roundtrip() {
  printf("Test 7: WAV File Roundtrip with Parametric EQ\n");

  const char *input_file = "tests/test_data/peq_input.wav";
  const char *output_file = "tests/test_data/peq_output.wav";

  // Create test signal with 1000 Hz tone
  int num_samples = (int)(SAMPLE_RATE * 0.5);
  AudioBuffer *buffer = audio_buffer_create(num_samples, SAMPLE_RATE, 2, 16);
  assert(buffer != NULL);

  generate_tone(buffer, 1000.0);

  // Write input file
  AudioError error = write_wave(input_file, buffer);
  assert(error == AUDIO_SUCCESS);
  printf("  ✓ Wrote input file: %s\n", input_file);

  // Apply parametric EQ (+6 dB at 1000 Hz, Q=1.0)
  ParametricFilter peq;
  parametric_init(&peq, SAMPLE_RATE, 1000.0, 6.0, 1.0);
  parametric_process_buffer(&peq, buffer);

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
  printf("  ✓ WAV roundtrip with parametric EQ working\n\n");
}

int main() {
  printf("\n=== Parametric EQ Filter Tests ===\n\n");

  test_parametric_init();
  test_parametric_update_coefficients();
  test_parametric_boost();
  test_parametric_cut();
  test_parametric_q_factor();
  test_parametric_stereo();
  test_parametric_wav_roundtrip();

  printf("=== All Parametric EQ tests passed! ===\n\n");
  return 0;
}
