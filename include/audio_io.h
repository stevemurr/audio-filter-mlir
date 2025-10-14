#ifndef AUDIO_IO_H
#define AUDIO_IO_H

#include <stdint.h>
#include <stddef.h>

// WAV file format structures
#pragma pack(push, 1)

// RIFF Chunk Descriptor
typedef struct {
    char chunk_id[4];      // "RIFF"
    uint32_t chunk_size;   // File size - 8
    char format[4];        // "WAVE"
} RIFFHeader;

// fmt sub-chunk
typedef struct {
    char subchunk_id[4];   // "fmt "
    uint32_t subchunk_size; // 16 for PCM
    uint16_t audio_format;  // 1 = PCM, 3 = IEEE float
    uint16_t num_channels;  // 1 = Mono, 2 = Stereo
    uint32_t sample_rate;   // 8000, 44100, 48000, etc.
    uint32_t byte_rate;     // sample_rate * num_channels * bits_per_sample/8
    uint16_t block_align;   // num_channels * bits_per_sample/8
    uint16_t bits_per_sample; // 8, 16, 24, 32
} FmtChunk;

// data sub-chunk header
typedef struct {
    char subchunk_id[4];   // "data"
    uint32_t subchunk_size; // num_samples * num_channels * bits_per_sample/8
} DataChunkHeader;

#pragma pack(pop)

// Audio format constants
#define AUDIO_FORMAT_PCM    1
#define AUDIO_FORMAT_FLOAT  3

// Audio buffer structure for float64 samples
typedef struct {
    double *data;          // Normalized audio samples [-1.0, 1.0]
    size_t length;         // Number of samples (total across all channels)
    int sample_rate;       // Sample rate in Hz
    int channels;          // Number of channels
    int bit_depth;         // Original bit depth (for writing back)
} AudioBuffer;

// PCM buffer structure for raw PCM data
typedef struct {
    uint8_t *data;         // Raw PCM bytes
    size_t length;         // Length in bytes
    int bit_depth;         // Bit depth (8, 16, 24, 32)
} PCMBuffer;

// Error codes
typedef enum {
    AUDIO_SUCCESS = 0,
    AUDIO_ERROR_FILE_NOT_FOUND = -1,
    AUDIO_ERROR_INVALID_FORMAT = -2,
    AUDIO_ERROR_UNSUPPORTED_FORMAT = -3,
    AUDIO_ERROR_READ_ERROR = -4,
    AUDIO_ERROR_WRITE_ERROR = -5,
    AUDIO_ERROR_MEMORY_ERROR = -6,
    AUDIO_ERROR_INVALID_PARAMETER = -7
} AudioError;

// Function prototypes

// Read WAV file and convert to float64 normalized samples
AudioBuffer* read_wave(const char *filepath, AudioError *error);

// Convert normalized float64 to PCM and write WAV file
AudioError write_wave(const char *filepath, AudioBuffer *buffer);

// Conversion utilities
void pcm_to_float64(PCMBuffer *pcm, double *output, size_t sample_count);
void float64_to_pcm(double *input, PCMBuffer *pcm, size_t sample_count);

// Memory management
AudioBuffer* audio_buffer_create(size_t length, int sample_rate, int channels, int bit_depth);
void audio_buffer_free(AudioBuffer *buffer);

PCMBuffer* pcm_buffer_create(size_t length, int bit_depth);
void pcm_buffer_free(PCMBuffer *buffer);

// Utility functions
const char* audio_error_string(AudioError error);
int validate_wav_header(RIFFHeader *riff, FmtChunk *fmt);

#endif // AUDIO_IO_H
