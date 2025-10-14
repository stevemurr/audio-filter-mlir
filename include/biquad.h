#ifndef BIQUAD_H
#define BIQUAD_H

#ifdef __cplusplus
extern "C" {
#endif

// Float epsilon constants for underflow checking
#define FLT_EPSILON_PLUS  1.192092896e-07  // smallest such that 1.0+FLT_EPSILON != 1.0
#define FLT_EPSILON_MINUS -1.192092896e-07 // smallest such that 1.0-FLT_EPSILON != 1.0
#define FLT_MIN_PLUS      1.175494351e-38  // min positive value
#define FLT_MIN_MINUS     -1.175494351e-38 // min negative value

// BiQuad filter structure
// Implements a modified biquad filter with wet (C0) and dry (D0) coefficients
typedef struct {
    // Filter coefficients
    double a0, a1, a2;  // Feedforward coefficients
    double b1, b2;      // Feedback coefficients

    // Wet/dry mix coefficients
    double c0;          // Wet (filtered) signal gain
    double d0;          // Dry (unfiltered) signal gain

    // Delay elements (state variables)
    double xz1, xz2;    // Input delays x(n-1), x(n-2)
    double yz1, yz2;    // Output delays y(n-1), y(n-2)
} BiQuad;

// Initialize a BiQuad filter to default state
// Sets all coefficients and delays to zero
void biquad_init(BiQuad *bq);

// Flush (zero) the delay elements
// Call this when starting to process a new audio stream
void biquad_flush_delays(BiQuad *bq);

// Process a single sample through the biquad filter
// Implements the difference equation:
// y(n) = a0*x(n) + a1*x(n-1) + a2*x(n-2) - b1*y(n-1) - b2*y(n-2)
// Returns the filtered output sample
double biquad_process(BiQuad *bq, double input);

#ifdef __cplusplus
}
#endif

#endif // BIQUAD_H
