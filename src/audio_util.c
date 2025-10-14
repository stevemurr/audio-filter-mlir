#include "audio_io.h"
#include "hpf.h"
#include "lpf.h"
#include "parametric.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VERSION "1.0.0"
#define PROGRAM_NAME "audio-util"

// Filter types
typedef enum { FILTER_NONE, FILTER_HPF, FILTER_LPF, FILTER_PEQ } FilterType;

// Configuration structure
typedef struct {
  char *input_path;
  char *output_path;
  FilterType filter;
  double frequency;
  double gain; // For parametric EQ (dB)
  double q;    // For parametric EQ (Q factor)
} Config;

// Print usage information
void print_usage(const char *program_name) {
  printf("Usage: %s [OPTIONS]\n\n", program_name);
  printf("Audio processing utility with support for various filters.\n\n");
  printf("Required Options:\n");
  printf("  --input PATH      Input WAV file path\n");
  printf("  --output PATH     Output WAV file path\n");
  printf("  --filter TYPE     Filter type (hpf, lpf, peq)\n");
  printf("  --freq HZ         Filter frequency parameter (Hz)\n\n");
  printf("Optional:\n");
  printf("  --gain DB         Gain in dB for parametric EQ (default: 0.0)\n");
  printf("  --q FACTOR        Q factor for parametric EQ (default: 1.0)\n");
  printf("  -h, --help        Show this help message\n");
  printf("  -v, --version     Show version information\n\n");
  printf("Supported Filters:\n");
  printf("  hpf               High-pass filter (Butterworth, 2nd order)\n");
  printf("  lpf               Low-pass filter (Butterworth, 2nd order)\n");
  printf("  peq               Parametric EQ (constant-Q, boost/cut)\n\n");
  printf("Examples:\n");
  printf("  # Apply 100 Hz high-pass filter\n");
  printf("  %s --input audio.wav --filter hpf --freq 100 --output "
         "audio-out.wav\n\n",
         program_name);
  printf("  # Remove low-frequency rumble (80 Hz cutoff)\n");
  printf("  %s --input recording.wav --filter hpf --freq 80 --output "
         "clean.wav\n\n",
         program_name);
  printf("  # Apply 5000 Hz low-pass filter\n");
  printf("  %s --input audio.wav --filter lpf --freq 5000 --output "
         "filtered.wav\n\n",
         program_name);
  printf("  # Apply parametric EQ: +6 dB boost at 1000 Hz, Q=1.0\n");
  printf("  %s --input audio.wav --filter peq --freq 1000 --gain 6.0 --q 1.0 "
         "--output boosted.wav\n\n",
         program_name);
}

// Print version information
void print_version() {
  printf("%s version %s\n", PROGRAM_NAME, VERSION);
  printf("Audio processing utility with MLIR optimization support\n");
}

// Parse filter type from string
FilterType parse_filter_type(const char *filter_str) {
  if (filter_str == NULL)
    return FILTER_NONE;

  if (strcmp(filter_str, "hpf") == 0) {
    return FILTER_HPF;
  }

  if (strcmp(filter_str, "lpf") == 0) {
    return FILTER_LPF;
  }

  if (strcmp(filter_str, "peq") == 0) {
    return FILTER_PEQ;
  }

  return FILTER_NONE;
}

// Validate configuration
int validate_config(const Config *config) {
  if (config->input_path == NULL) {
    fprintf(stderr, "Error: --input is required\n");
    return 0;
  }

  if (config->output_path == NULL) {
    fprintf(stderr, "Error: --output is required\n");
    return 0;
  }

  if (config->filter == FILTER_NONE) {
    fprintf(stderr, "Error: --filter is required\n");
    return 0;
  }

  if (config->frequency <= 0.0) {
    fprintf(stderr, "Error: --freq must be positive\n");
    return 0;
  }

  // Validate parametric EQ specific parameters
  if (config->filter == FILTER_PEQ) {
    if (config->q <= 0.0) {
      fprintf(stderr, "Error: --q must be positive\n");
      return 0;
    }
  }

  // Validate input file exists
  FILE *test = fopen(config->input_path, "rb");
  if (test == NULL) {
    fprintf(stderr, "Error: Cannot open input file: %s\n", config->input_path);
    return 0;
  }
  fclose(test);

  return 1;
}

// Apply high-pass filter
int apply_hpf(AudioBuffer *buffer, double frequency) {
  printf("Applying high-pass filter:\n");
  printf("  Cutoff frequency: %.1f Hz\n", frequency);
  printf("  Sample rate: %d Hz\n", buffer->sample_rate);
  printf("  Channels: %d\n", buffer->channels);
  printf("  Samples: %zu\n", buffer->length);

  // Validate frequency against Nyquist limit
  double nyquist = buffer->sample_rate / 2.0;
  if (frequency >= nyquist) {
    fprintf(stderr,
            "Error: Frequency %.1f Hz exceeds Nyquist limit (%.1f Hz)\n",
            frequency, nyquist);
    return 0;
  }

  // Initialize and apply HPF
  HPFFilter hpf;
  hpf_init(&hpf, buffer->sample_rate, frequency);
  hpf_process_buffer(&hpf, buffer);

  printf("  ✓ Filter applied successfully\n");
  return 1;
}

// Apply low-pass filter
int apply_lpf(AudioBuffer *buffer, double frequency) {
  printf("Applying low-pass filter:\n");
  printf("  Cutoff frequency: %.1f Hz\n", frequency);
  printf("  Sample rate: %d Hz\n", buffer->sample_rate);
  printf("  Channels: %d\n", buffer->channels);
  printf("  Samples: %zu\n", buffer->length);

  // Validate frequency against Nyquist limit
  double nyquist = buffer->sample_rate / 2.0;
  if (frequency >= nyquist) {
    fprintf(stderr,
            "Error: Frequency %.1f Hz exceeds Nyquist limit (%.1f Hz)\n",
            frequency, nyquist);
    return 0;
  }

  // Initialize and apply LPF
  LPFFilter lpf;
  lpf_init(&lpf, buffer->sample_rate, frequency);
  lpf_process_buffer(&lpf, buffer);

  printf("  ✓ Filter applied successfully\n");
  return 1;
}

// Apply parametric EQ
int apply_peq(AudioBuffer *buffer, double frequency, double gain, double q) {
  printf("Applying parametric EQ:\n");
  printf("  Center frequency: %.1f Hz\n", frequency);
  printf("  Gain: %.1f dB\n", gain);
  printf("  Q factor: %.2f\n", q);
  printf("  Sample rate: %d Hz\n", buffer->sample_rate);
  printf("  Channels: %d\n", buffer->channels);
  printf("  Samples: %zu\n", buffer->length);

  // Validate frequency against Nyquist limit
  double nyquist = buffer->sample_rate / 2.0;
  if (frequency >= nyquist) {
    fprintf(stderr,
            "Error: Frequency %.1f Hz exceeds Nyquist limit (%.1f Hz)\n",
            frequency, nyquist);
    return 0;
  }

  // Initialize and apply parametric EQ
  ParametricFilter peq;
  parametric_init(&peq, buffer->sample_rate, frequency, gain, q);
  parametric_process_buffer(&peq, buffer);

  printf("  ✓ Filter applied successfully\n");
  return 1;
}

// Main processing function
int process_audio(const Config *config) {
  AudioError error;

  // Read input file
  printf("Reading input file: %s\n", config->input_path);
  AudioBuffer *buffer = read_wave(config->input_path, &error);

  if (buffer == NULL) {
    fprintf(stderr, "Error reading input file: %s\n",
            audio_error_string(error));
    return 1;
  }

  printf("  Sample rate: %d Hz\n", buffer->sample_rate);
  printf("  Channels: %d\n", buffer->channels);
  printf("  Bit depth: %d bits\n", buffer->bit_depth);
  printf("  Duration: %.2f seconds\n",
         (double)buffer->length / buffer->channels / buffer->sample_rate);
  printf("  Samples: %zu\n", buffer->length);

  // Apply filter
  int success = 1;
  switch (config->filter) {
  case FILTER_HPF:
    success = apply_hpf(buffer, config->frequency);
    break;
  case FILTER_LPF:
    success = apply_lpf(buffer, config->frequency);
    break;
  case FILTER_PEQ:
    success = apply_peq(buffer, config->frequency, config->gain, config->q);
    break;
  default:
    fprintf(stderr, "Error: Unknown filter type\n");
    success = 0;
    break;
  }

  if (!success) {
    audio_buffer_free(buffer);
    return 1;
  }

  // Write output file
  printf("Writing output file: %s\n", config->output_path);
  error = write_wave(config->output_path, buffer);

  if (error != AUDIO_SUCCESS) {
    fprintf(stderr, "Error writing output file: %s\n",
            audio_error_string(error));
    audio_buffer_free(buffer);
    return 1;
  }

  printf("  ✓ Output file written successfully\n");

  // Cleanup
  audio_buffer_free(buffer);

  printf("\n✓ Processing complete!\n");
  return 0;
}

int main(int argc, char *argv[]) {
  Config config = {.input_path = NULL,
                   .output_path = NULL,
                   .filter = FILTER_NONE,
                   .frequency = 0.0,
                   .gain = 0.0,
                   .q = 1.0};

  // Define long options
  static struct option long_options[] = {{"input", required_argument, 0, 'i'},
                                         {"output", required_argument, 0, 'o'},
                                         {"filter", required_argument, 0, 'f'},
                                         {"freq", required_argument, 0, 'r'},
                                         {"gain", required_argument, 0, 'g'},
                                         {"q", required_argument, 0, 'q'},
                                         {"help", no_argument, 0, 'h'},
                                         {"version", no_argument, 0, 'v'},
                                         {0, 0, 0, 0}};

  // Parse command-line arguments
  int opt;
  int option_index = 0;

  while ((opt = getopt_long(argc, argv, "i:o:f:r:g:q:hv", long_options,
                            &option_index)) != -1) {
    switch (opt) {
    case 'i':
      config.input_path = optarg;
      break;
    case 'o':
      config.output_path = optarg;
      break;
    case 'f':
      config.filter = parse_filter_type(optarg);
      if (config.filter == FILTER_NONE) {
        fprintf(stderr, "Error: Unknown filter type '%s'\n", optarg);
        fprintf(stderr, "Supported filters: hpf, lpf, peq\n");
        return 1;
      }
      break;
    case 'r':
      config.frequency = atof(optarg);
      break;
    case 'g':
      config.gain = atof(optarg);
      break;
    case 'q':
      config.q = atof(optarg);
      break;
    case 'h':
      print_usage(argv[0]);
      return 0;
    case 'v':
      print_version();
      return 0;
    case '?':
      // getopt_long already printed error message
      fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
      return 1;
    default:
      print_usage(argv[0]);
      return 1;
    }
  }

  // Check for unexpected positional arguments
  if (optind < argc) {
    fprintf(stderr, "Error: Unexpected argument '%s'\n", argv[optind]);
    fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
    return 1;
  }

  // Validate configuration
  if (!validate_config(&config)) {
    fprintf(stderr, "Try '%s --help' for more information.\n", argv[0]);
    return 1;
  }

  // Process audio
  printf("\n=== %s v%s ===\n\n", PROGRAM_NAME, VERSION);
  return process_audio(&config);
}
