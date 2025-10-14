#include "audio_io.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Helper function to read exact number of bytes
static int read_exact(FILE *file, void *buffer, size_t size) {
  size_t bytes_read = fread(buffer, 1, size, file);
  return bytes_read == size ? 1 : 0;
}

// Helper function to write exact number of bytes
static int write_exact(FILE *file, const void *buffer, size_t size) {
  size_t bytes_written = fwrite(buffer, 1, size, file);
  return bytes_written == size ? 1 : 0;
}

// Validate WAV header
int validate_wav_header(RIFFHeader *riff, FmtChunk *fmt) {
  // Check RIFF header
  if (memcmp(riff->chunk_id, "RIFF", 4) != 0) {
    return 0;
  }
  if (memcmp(riff->format, "WAVE", 4) != 0) {
    return 0;
  }

  // Check fmt chunk
  if (memcmp(fmt->subchunk_id, "fmt ", 4) != 0) {
    return 0;
  }

  // Validate audio format (PCM or float)
  if (fmt->audio_format != AUDIO_FORMAT_PCM &&
      fmt->audio_format != AUDIO_FORMAT_FLOAT) {
    return 0;
  }

  // Validate channels
  if (fmt->num_channels < 1 || fmt->num_channels > 16) {
    return 0;
  }

  // Validate bit depth
  if (fmt->bits_per_sample != 8 && fmt->bits_per_sample != 16 &&
      fmt->bits_per_sample != 24 && fmt->bits_per_sample != 32) {
    return 0;
  }

  // Validate calculations
  if (fmt->byte_rate !=
      fmt->sample_rate * fmt->num_channels * fmt->bits_per_sample / 8) {
    return 0;
  }
  if (fmt->block_align != fmt->num_channels * fmt->bits_per_sample / 8) {
    return 0;
  }

  return 1;
}

// Convert PCM to float64
void pcm_to_float64(PCMBuffer *pcm, double *output, size_t sample_count) {
  if (!pcm || !output || sample_count == 0) {
    return;
  }

  switch (pcm->bit_depth) {
  case 8: {
    // 8-bit PCM is unsigned (0-255)
    uint8_t *samples = (uint8_t *)pcm->data;
    for (size_t i = 0; i < sample_count; i++) {
      output[i] = (samples[i] - 128) / 128.0;
    }
    break;
  }
  case 16: {
    // 16-bit PCM is signed
    int16_t *samples = (int16_t *)pcm->data;
    for (size_t i = 0; i < sample_count; i++) {
      output[i] = samples[i] / 32768.0;
    }
    break;
  }
  case 24: {
    // 24-bit PCM is signed (stored in 3 bytes)
    uint8_t *bytes = pcm->data;
    for (size_t i = 0; i < sample_count; i++) {
      int32_t sample = 0;
      // Little-endian: LSB first
      sample =
          bytes[i * 3] | (bytes[i * 3 + 1] << 8) | (bytes[i * 3 + 2] << 16);
      // Sign extend from 24-bit to 32-bit
      if (sample & 0x800000) {
        sample |= 0xFF000000;
      }
      output[i] = sample / 8388608.0;
    }
    break;
  }
  case 32: {
    // 32-bit can be PCM or float
    // Assume PCM for now (will handle float separately)
    int32_t *samples = (int32_t *)pcm->data;
    for (size_t i = 0; i < sample_count; i++) {
      output[i] = samples[i] / 2147483648.0;
    }
    break;
  }
  }
}

// Convert float64 to PCM
void float64_to_pcm(double *input, PCMBuffer *pcm, size_t sample_count) {
  if (!input || !pcm || sample_count == 0) {
    return;
  }

  switch (pcm->bit_depth) {
  case 8: {
    uint8_t *samples = (uint8_t *)pcm->data;
    for (size_t i = 0; i < sample_count; i++) {
      // Clamp to [-1.0, 1.0]
      double clamped = fmax(-1.0, fmin(1.0, input[i]));
      samples[i] = (uint8_t)((clamped * 128.0) + 128);
    }
    break;
  }
  case 16: {
    int16_t *samples = (int16_t *)pcm->data;
    for (size_t i = 0; i < sample_count; i++) {
      double clamped = fmax(-1.0, fmin(1.0, input[i]));
      samples[i] = (int16_t)(clamped * 32767.0);
    }
    break;
  }
  case 24: {
    uint8_t *bytes = pcm->data;
    for (size_t i = 0; i < sample_count; i++) {
      double clamped = fmax(-1.0, fmin(1.0, input[i]));
      int32_t sample = (int32_t)(clamped * 8388607.0);
      // Store as little-endian 24-bit
      bytes[i * 3] = sample & 0xFF;
      bytes[i * 3 + 1] = (sample >> 8) & 0xFF;
      bytes[i * 3 + 2] = (sample >> 16) & 0xFF;
    }
    break;
  }
  case 32: {
    int32_t *samples = (int32_t *)pcm->data;
    for (size_t i = 0; i < sample_count; i++) {
      double clamped = fmax(-1.0, fmin(1.0, input[i]));
      samples[i] = (int32_t)(clamped * 2147483647.0);
    }
    break;
  }
  }
}

// Read WAV file
AudioBuffer *read_wave(const char *filepath, AudioError *error) {
  if (!filepath) {
    if (error)
      *error = AUDIO_ERROR_INVALID_PARAMETER;
    return NULL;
  }

  FILE *file = fopen(filepath, "rb");
  if (!file) {
    if (error)
      *error = AUDIO_ERROR_FILE_NOT_FOUND;
    return NULL;
  }

  // Read RIFF header
  RIFFHeader riff;
  if (!read_exact(file, &riff, sizeof(RIFFHeader))) {
    fclose(file);
    if (error)
      *error = AUDIO_ERROR_READ_ERROR;
    return NULL;
  }

  // Validate RIFF header
  if (memcmp(riff.chunk_id, "RIFF", 4) != 0) {
    fclose(file);
    if (error)
      *error = AUDIO_ERROR_INVALID_FORMAT;
    return NULL;
  }
  if (memcmp(riff.format, "WAVE", 4) != 0) {
    fclose(file);
    if (error)
      *error = AUDIO_ERROR_INVALID_FORMAT;
    return NULL;
  }

  // Search for fmt chunk (there might be JUNK or other chunks before it)
  FmtChunk fmt;
  int found_fmt = 0;

  while (!found_fmt) {
    // Read chunk ID and size
    char chunk_id[4];
    uint32_t chunk_size;

    if (!read_exact(file, chunk_id, 4) || !read_exact(file, &chunk_size, 4)) {
      fclose(file);
      if (error)
        *error = AUDIO_ERROR_INVALID_FORMAT;
      return NULL;
    }

    if (memcmp(chunk_id, "fmt ", 4) == 0) {
      // Found fmt chunk - read the rest of it
      // We already read the chunk_id and subchunk_size, so read remaining
      // fields
      if (!read_exact(file, &fmt.audio_format, sizeof(FmtChunk) - 8)) {
        fclose(file);
        if (error)
          *error = AUDIO_ERROR_READ_ERROR;
        return NULL;
      }

      // Copy the chunk_id and size we already read
      memcpy(fmt.subchunk_id, chunk_id, 4);
      fmt.subchunk_size = chunk_size;

      found_fmt = 1;

      // Skip extra fmt bytes if present (fmt size > 16)
      if (fmt.subchunk_size > 16) {
        fseek(file, fmt.subchunk_size - 16, SEEK_CUR);
      }
    } else {
      // Skip unknown chunk
      fseek(file, chunk_size, SEEK_CUR);
    }
  }

  // Validate fmt chunk
  if (!validate_wav_header(&riff, &fmt)) {
    fclose(file);
    if (error)
      *error = AUDIO_ERROR_INVALID_FORMAT;
    return NULL;
  }

  // Find data chunk (there might be other chunks in between)
  DataChunkHeader data_header;
  int found_data = 0;
  while (!found_data) {
    if (!read_exact(file, &data_header, sizeof(DataChunkHeader))) {
      fclose(file);
      if (error)
        *error = AUDIO_ERROR_INVALID_FORMAT;
      return NULL;
    }

    if (memcmp(data_header.subchunk_id, "data", 4) == 0) {
      found_data = 1;
    } else {
      // Skip unknown chunk
      fseek(file, data_header.subchunk_size, SEEK_CUR);
    }
  }

  // Calculate number of samples
  size_t bytes_per_sample = fmt.bits_per_sample / 8;
  size_t total_samples = data_header.subchunk_size / bytes_per_sample;

  // Create PCM buffer
  PCMBuffer *pcm =
      pcm_buffer_create(data_header.subchunk_size, fmt.bits_per_sample);
  if (!pcm) {
    fclose(file);
    if (error)
      *error = AUDIO_ERROR_MEMORY_ERROR;
    return NULL;
  }

  // Read PCM data
  if (!read_exact(file, pcm->data, data_header.subchunk_size)) {
    pcm_buffer_free(pcm);
    fclose(file);
    if (error)
      *error = AUDIO_ERROR_READ_ERROR;
    return NULL;
  }

  fclose(file);

  // Create audio buffer
  AudioBuffer *buffer = audio_buffer_create(
      total_samples, fmt.sample_rate, fmt.num_channels, fmt.bits_per_sample);
  if (!buffer) {
    pcm_buffer_free(pcm);
    if (error)
      *error = AUDIO_ERROR_MEMORY_ERROR;
    return NULL;
  }

  // Convert PCM to float64
  pcm_to_float64(pcm, buffer->data, total_samples);

  pcm_buffer_free(pcm);

  if (error)
    *error = AUDIO_SUCCESS;
  return buffer;
}

// Write WAV file
AudioError write_wave(const char *filepath, AudioBuffer *buffer) {
  if (!filepath || !buffer || !buffer->data) {
    return AUDIO_ERROR_INVALID_PARAMETER;
  }

  FILE *file = fopen(filepath, "wb");
  if (!file) {
    return AUDIO_ERROR_WRITE_ERROR;
  }

  // Calculate sizes
  size_t bytes_per_sample = buffer->bit_depth / 8;
  size_t data_size = buffer->length * bytes_per_sample;

  // Prepare RIFF header
  RIFFHeader riff;
  memcpy(riff.chunk_id, "RIFF", 4);
  riff.chunk_size = 36 + data_size;
  memcpy(riff.format, "WAVE", 4);

  // Prepare fmt chunk
  FmtChunk fmt;
  memcpy(fmt.subchunk_id, "fmt ", 4);
  fmt.subchunk_size = 16;
  fmt.audio_format = AUDIO_FORMAT_PCM;
  fmt.num_channels = buffer->channels;
  fmt.sample_rate = buffer->sample_rate;
  fmt.byte_rate = buffer->sample_rate * buffer->channels * bytes_per_sample;
  fmt.block_align = buffer->channels * bytes_per_sample;
  fmt.bits_per_sample = buffer->bit_depth;

  // Prepare data chunk header
  DataChunkHeader data_header;
  memcpy(data_header.subchunk_id, "data", 4);
  data_header.subchunk_size = data_size;

  // Write headers
  if (!write_exact(file, &riff, sizeof(RIFFHeader)) ||
      !write_exact(file, &fmt, sizeof(FmtChunk)) ||
      !write_exact(file, &data_header, sizeof(DataChunkHeader))) {
    fclose(file);
    return AUDIO_ERROR_WRITE_ERROR;
  }

  // Create PCM buffer
  PCMBuffer *pcm = pcm_buffer_create(data_size, buffer->bit_depth);
  if (!pcm) {
    fclose(file);
    return AUDIO_ERROR_MEMORY_ERROR;
  }

  // Convert float64 to PCM
  float64_to_pcm(buffer->data, pcm, buffer->length);

  // Write PCM data
  if (!write_exact(file, pcm->data, data_size)) {
    pcm_buffer_free(pcm);
    fclose(file);
    return AUDIO_ERROR_WRITE_ERROR;
  }

  pcm_buffer_free(pcm);
  fclose(file);

  return AUDIO_SUCCESS;
}

// Create audio buffer
AudioBuffer *audio_buffer_create(size_t length, int sample_rate, int channels,
                                 int bit_depth) {
  AudioBuffer *buffer = (AudioBuffer *)malloc(sizeof(AudioBuffer));
  if (!buffer) {
    return NULL;
  }

  buffer->data = (double *)malloc(length * sizeof(double));
  if (!buffer->data) {
    free(buffer);
    return NULL;
  }

  buffer->length = length;
  buffer->sample_rate = sample_rate;
  buffer->channels = channels;
  buffer->bit_depth = bit_depth;

  return buffer;
}

// Free audio buffer
void audio_buffer_free(AudioBuffer *buffer) {
  if (buffer) {
    if (buffer->data) {
      free(buffer->data);
    }
    free(buffer);
  }
}

// Create PCM buffer
PCMBuffer *pcm_buffer_create(size_t length, int bit_depth) {
  PCMBuffer *buffer = (PCMBuffer *)malloc(sizeof(PCMBuffer));
  if (!buffer) {
    return NULL;
  }

  buffer->data = (uint8_t *)malloc(length);
  if (!buffer->data) {
    free(buffer);
    return NULL;
  }

  buffer->length = length;
  buffer->bit_depth = bit_depth;

  return buffer;
}

// Free PCM buffer
void pcm_buffer_free(PCMBuffer *buffer) {
  if (buffer) {
    if (buffer->data) {
      free(buffer->data);
    }
    free(buffer);
  }
}

// Get error string
const char *audio_error_string(AudioError error) {
  switch (error) {
  case AUDIO_SUCCESS:
    return "Success";
  case AUDIO_ERROR_FILE_NOT_FOUND:
    return "File not found";
  case AUDIO_ERROR_INVALID_FORMAT:
    return "Invalid WAV format";
  case AUDIO_ERROR_UNSUPPORTED_FORMAT:
    return "Unsupported audio format";
  case AUDIO_ERROR_READ_ERROR:
    return "File read error";
  case AUDIO_ERROR_WRITE_ERROR:
    return "File write error";
  case AUDIO_ERROR_MEMORY_ERROR:
    return "Memory allocation error";
  case AUDIO_ERROR_INVALID_PARAMETER:
    return "Invalid parameter";
  default:
    return "Unknown error";
  }
}
