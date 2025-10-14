#include "audio_io.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Test 1: Create a simple sine wave and write it
int test_write_sine_wave() {
    printf("Test 1: Writing sine wave...\n");
    
    int sample_rate = 44100;
    int channels = 2;
    int bit_depth = 16;
    double duration = 1.0; // 1 second
    double frequency = 440.0; // A4 note
    
    size_t num_samples = (size_t)(sample_rate * duration * channels);
    
    AudioBuffer *buffer = audio_buffer_create(num_samples, sample_rate, channels, bit_depth);
    if (!buffer) {
        printf("  FAILED: Could not create audio buffer\n");
        return 0;
    }
    
    // Generate stereo sine wave
    for (size_t i = 0; i < num_samples / channels; i++) {
        double t = (double)i / sample_rate;
        double value = 0.5 * sin(2.0 * M_PI * frequency * t);
        buffer->data[i * channels] = value;      // Left channel
        buffer->data[i * channels + 1] = value;  // Right channel
    }
    
    AudioError error = write_wave("tests/test_data/sine_wave.wav", buffer);
    audio_buffer_free(buffer);
    
    if (error != AUDIO_SUCCESS) {
        printf("  FAILED: %s\n", audio_error_string(error));
        return 0;
    }
    
    printf("  PASSED: Wrote sine_wave.wav\n");
    return 1;
}

// Test 2: Read the file we just wrote
int test_read_sine_wave() {
    printf("Test 2: Reading sine wave...\n");
    
    AudioError error;
    AudioBuffer *buffer = read_wave("tests/test_data/sine_wave.wav", &error);
    
    if (!buffer) {
        printf("  FAILED: %s\n", audio_error_string(error));
        return 0;
    }
    
    printf("  Sample rate: %d Hz\n", buffer->sample_rate);
    printf("  Channels: %d\n", buffer->channels);
    printf("  Bit depth: %d\n", buffer->bit_depth);
    printf("  Total samples: %zu\n", buffer->length);
    
    // Verify some properties
    if (buffer->sample_rate != 44100) {
        printf("  FAILED: Expected sample rate 44100, got %d\n", buffer->sample_rate);
        audio_buffer_free(buffer);
        return 0;
    }
    
    if (buffer->channels != 2) {
        printf("  FAILED: Expected 2 channels, got %d\n", buffer->channels);
        audio_buffer_free(buffer);
        return 0;
    }
    
    // Check if data looks reasonable (should be in [-1, 1])
    int in_range = 1;
    for (size_t i = 0; i < buffer->length && i < 100; i++) {
        if (buffer->data[i] < -1.0 || buffer->data[i] > 1.0) {
            in_range = 0;
            break;
        }
    }
    
    if (!in_range) {
        printf("  FAILED: Audio data out of range\n");
        audio_buffer_free(buffer);
        return 0;
    }
    
    audio_buffer_free(buffer);
    printf("  PASSED: Read sine_wave.wav successfully\n");
    return 1;
}

// Test 3: Roundtrip test
int test_roundtrip() {
    printf("Test 3: Roundtrip test...\n");
    
    // Write original
    AudioError error;
    AudioBuffer *original = read_wave("tests/test_data/sine_wave.wav", &error);
    if (!original) {
        printf("  FAILED: Could not read original file\n");
        return 0;
    }
    
    // Write to new file
    error = write_wave("tests/test_data/sine_wave_copy.wav", original);
    if (error != AUDIO_SUCCESS) {
        printf("  FAILED: Could not write copy: %s\n", audio_error_string(error));
        audio_buffer_free(original);
        return 0;
    }
    
    // Read back the copy
    AudioBuffer *copy = read_wave("tests/test_data/sine_wave_copy.wav", &error);
    if (!copy) {
        printf("  FAILED: Could not read copy: %s\n", audio_error_string(error));
        audio_buffer_free(original);
        return 0;
    }
    
    // Compare metadata
    if (original->sample_rate != copy->sample_rate ||
        original->channels != copy->channels ||
        original->bit_depth != copy->bit_depth ||
        original->length != copy->length) {
        printf("  FAILED: Metadata mismatch\n");
        audio_buffer_free(original);
        audio_buffer_free(copy);
        return 0;
    }
    
    // Compare data (allow small error due to quantization)
    double max_error = 0.0;
    for (size_t i = 0; i < original->length; i++) {
        double diff = fabs(original->data[i] - copy->data[i]);
        if (diff > max_error) {
            max_error = diff;
        }
    }
    
    // For 16-bit, quantization error should be < 1/32768
    double acceptable_error = 1.0 / 32768.0 * 2.0; // Allow 2x quantization step
    if (max_error > acceptable_error) {
        printf("  FAILED: Data mismatch, max error: %f\n", max_error);
        audio_buffer_free(original);
        audio_buffer_free(copy);
        return 0;
    }
    
    printf("  Max error: %f (acceptable: %f)\n", max_error, acceptable_error);
    audio_buffer_free(original);
    audio_buffer_free(copy);
    
    printf("  PASSED: Roundtrip successful\n");
    return 1;
}

// Test 4: Different bit depths
int test_bit_depths() {
    printf("Test 4: Testing different bit depths...\n");
    
    int sample_rate = 44100;
    int channels = 1;
    double duration = 0.1; // 0.1 seconds
    int bit_depths[] = {8, 16, 24, 32};
    int num_depths = 4;
    
    for (int d = 0; d < num_depths; d++) {
        int bit_depth = bit_depths[d];
        size_t num_samples = (size_t)(sample_rate * duration * channels);
        
        AudioBuffer *buffer = audio_buffer_create(num_samples, sample_rate, channels, bit_depth);
        if (!buffer) {
            printf("  FAILED: Could not create buffer for %d-bit\n", bit_depth);
            return 0;
        }
        
        // Generate simple ramp
        for (size_t i = 0; i < num_samples; i++) {
            buffer->data[i] = (double)i / num_samples * 2.0 - 1.0;
        }
        
        char filename[256];
        snprintf(filename, sizeof(filename), "tests/test_data/test_%dbit.wav", bit_depth);
        
        AudioError error = write_wave(filename, buffer);
        audio_buffer_free(buffer);
        
        if (error != AUDIO_SUCCESS) {
            printf("  FAILED: Could not write %d-bit file\n", bit_depth);
            return 0;
        }
        
        // Read it back
        AudioBuffer *read_buffer = read_wave(filename, &error);
        if (!read_buffer) {
            printf("  FAILED: Could not read %d-bit file\n", bit_depth);
            return 0;
        }
        
        audio_buffer_free(read_buffer);
        printf("  %d-bit: OK\n", bit_depth);
    }
    
    printf("  PASSED: All bit depths work\n");
    return 1;
}

// Test 5: Error handling
int test_error_handling() {
    printf("Test 5: Testing error handling...\n");
    
    AudioError error;
    
    // Test reading non-existent file
    AudioBuffer *buffer = read_wave("tests/test_data/nonexistent.wav", &error);
    if (buffer != NULL || error != AUDIO_ERROR_FILE_NOT_FOUND) {
        printf("  FAILED: Should return FILE_NOT_FOUND error\n");
        if (buffer) audio_buffer_free(buffer);
        return 0;
    }
    printf("  Non-existent file: OK\n");
    
    // Test NULL filepath
    buffer = read_wave(NULL, &error);
    if (buffer != NULL || error != AUDIO_ERROR_INVALID_PARAMETER) {
        printf("  FAILED: Should return INVALID_PARAMETER for NULL filepath\n");
        if (buffer) audio_buffer_free(buffer);
        return 0;
    }
    printf("  NULL filepath: OK\n");
    
    // Test writing NULL buffer
    error = write_wave("tests/test_data/test.wav", NULL);
    if (error != AUDIO_ERROR_INVALID_PARAMETER) {
        printf("  FAILED: Should return INVALID_PARAMETER for NULL buffer\n");
        return 0;
    }
    printf("  NULL buffer: OK\n");
    
    printf("  PASSED: Error handling works correctly\n");
    return 1;
}

int main() {
    printf("=== Audio I/O Test Suite ===\n\n");
    
    int passed = 0;
    int total = 5;
    
    passed += test_write_sine_wave();
    printf("\n");
    
    passed += test_read_sine_wave();
    printf("\n");
    
    passed += test_roundtrip();
    printf("\n");
    
    passed += test_bit_depths();
    printf("\n");
    
    passed += test_error_handling();
    printf("\n");
    
    printf("=== Results: %d/%d tests passed ===\n", passed, total);
    
    return (passed == total) ? 0 : 1;
}
