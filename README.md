# audio-filter-mlir

Audio processing utility with MLIR optimization support.

## Quick Start

```bash
# Setup and build
make setup
make build

# Run tests
make test

# Install utility
make install

# Use the utility
audio-util --input audio.wav --filter hpf --freq 100 --output audio-clean.wav
```

## Project Status

- ✅ **Phase 1:** Audio I/O Foundation (Complete)
- ✅ **Phase 2:** High-Pass Filter Implementation (Complete)
- ⏳ **Phase 3:** MLIR Integration (Pending)
- ⏳ **Phase 4:** MLIR Optimization (Pending)

## Features

- **WAV File I/O:** Read/write 8/16/24/32-bit PCM audio files
- **High-Pass Filter:** Butterworth 2nd-order filter for removing low frequencies
- **Command-Line Tool:** `audio-util` for batch processing
- **Comprehensive Tests:** 100% test pass rate with 12 test cases

## Documentation

- [Project Plan](docs/PLAN.md) - Full development roadmap
- [Audio Utility](docs/AUDIO_UTIL.md) - Command-line tool documentation
- [Phase 1 Complete](docs/PHASE1_1_COMPLETE.md) - Audio I/O implementation
- [Phase 2 Complete](docs/PHASE2_COMPLETE.md) - Filter implementation

## Make Targets

```
make help       # Show available targets
make setup      # Initial project setup
make build      # Build all libraries and executables
make test       # Run all tests
make install    # Install audio-util to /usr/local/bin
make uninstall  # Remove audio-util
make clean      # Remove build artifacts
```

## Requirements

- CMake 3.10+
- C99-compatible compiler
- Math library (libm)

## Project Structure

```
audio-filter-mlir/
├── include/          # Public headers
│   ├── audio_io.h
│   ├── biquad.h
│   └── hpf.h
├── src/              # Implementation
│   ├── audio_io.c
│   ├── biquad.c
│   ├── hpf.c
│   └── audio_util.c
├── tests/            # Test suites
│   ├── test_audio_io.c
│   ├── test_biquad.c
│   └── test_hpf.c
└── docs/             # Documentation
```

## Example Usage

### Remove Low-Frequency Rumble

```bash
audio-util --input recording.wav --filter hpf --freq 80 --output clean.wav
```

### Remove DC Offset

```bash
audio-util --input audio.wav --filter hpf --freq 5 --output audio-clean.wav
```

### Batch Processing

```bash
for file in *.wav; do
    audio-util --input "$file" --filter hpf --freq 100 \
               --output "processed/${file%.wav}_clean.wav"
done
```

## Development

### Build System

- **CMake:** Primary build system
- **Makefile:** Convenience wrapper
- **clangd:** LSP support via compile_commands.json

### Code Statistics

- **Total Lines:** 2,443 (implementation + tests)
- **Libraries:** 3 static libraries
- **Executables:** 1 utility + 3 test suites
- **Test Coverage:** 12 test cases, 100% pass rate

### Running Tests

```bash
# All tests via CTest
make test

# Individual test suites
./build/test_audio_io
./build/test_biquad
./build/test_hpf
```

## Technology Stack

- **Language:** C99
- **Build:** CMake, Make
- **Testing:** CTest, custom test harness
- **Audio Format:** WAV (RIFF/WAVE) with PCM
- **DSP:** Butterworth filters via biquad implementation
- **Future:** MLIR optimization framework

## License

See LICENSE file.

## Contributing

This is a personal learning project focused on MLIR optimization. The current implementation serves as a baseline for MLIR performance comparisons.

## Roadmap

### Phase 3: MLIR Integration (Next)
- Set up MLIR build infrastructure
- Link MLIR libraries
- Create basic MLIR context
- Verify compilation

### Phase 4: MLIR Optimization (Future)
- MLIR IR generation for biquad operations
- Vectorization of sample processing
- Optimization pass pipeline
- Performance benchmarks (target: 2-5× speedup)

## See Also

- MLIR Documentation: https://mlir.llvm.org/
- WAV Format Spec: http://soundfile.sapp.org/doc/WaveFormat/
- Butterworth Filter Theory: Digital Signal Processing texts
