package com.prismv.detint;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;

/**
 * Deterministic Integer Arithmetic (DetInt) for Prism-V.
 * Strictly adheres to Invariant 2:
 *  - K <= 512 channels
 *  - Class A INT8 weights, INT16 activations, INT32 accumulators
 *  - Zero floating-point operations in normative entropy priors / Cool-chic MLP
 *  - Guaranteed no-overflow bound: 512 * 127 * 32767 = 2,130,641,408 < 2^31 - 1
 */
public final class DetInt {
    public static final int MAX_K_CHANNELS = 512;
    public static final int WEIGHT_MAX = 127;
    public static final int WEIGHT_MIN = -127;
    public static final int ACT_MAX = 32767;
    public static final int ACT_MIN = -32767;
    public static final int CDF_NORMALIZATION_MASS = 32768;
    public static final long ABSOLUTE_MAX_ACCUM = (long) MAX_K_CHANNELS * WEIGHT_MAX * ACT_MAX;

    // Static 512-entry Sigmoid/Laplace lookup table initialized identically to C++ detint.cpp
    public static final int[] CDF_SIGMOID_LUT = new int[512];

    static {
        for (int i = 0; i < 512; i++) {
            double x = (i - 256.0) / 32.0;
            double s = 1.0 / (1.0 + Math.exp(-x));
            int val = (int) (s * 32768.0);
            if (val > 32768) val = 32768;
            CDF_SIGMOID_LUT[i] = val;
        }
        CDF_SIGMOID_LUT[0] = 0;
        CDF_SIGMOID_LUT[511] = 32768;
    }

    private DetInt() {}

    public static int leakyReluFixed(int x) {
        if (x >= 0) {
            return x;
        }
        // LeakyReLU slope ~0.10: (x * 26 + 128) >> 8
        return (x * 26 + 128) >> 8;
    }

    public static int scaleFixed(int x, int wScale) {
        // Rounding fixed point scale: (x * wScale + 8192) >> 14
        long prod = (long) x * wScale;
        return (int) ((prod + 8192) >> 14);
    }

    public static short dyadicRescale(int acc, int mScale, int sShift) {
        long scaled = (long) acc * mScale;
        long roundBias = 1L << (sShift - 1);
        long shifted = (scaled + roundBias) >> sShift;
        if (shifted > ACT_MAX) return (short) ACT_MAX;
        if (shifted < ACT_MIN) return (short) ACT_MIN;
        return (short) shifted;
    }

    public static int dotProduct(byte[] weights, short[] activations, int length) {
        if (length > MAX_K_CHANNELS) {
            throw new IllegalArgumentException("Length exceeds MAX_K_CHANNELS (512)");
        }
        int sum = 0;
        for (int i = 0; i < length; i++) {
            sum += ((int) weights[i]) * ((int) activations[i]);
        }
        return sum;
    }

    public static void linearLayer(short[] x, byte[] weights, int[] bias, short[] y,
                                  int kIn, int mOut, int mScale, int sShift) {
        if (kIn > MAX_K_CHANNELS) {
            throw new IllegalArgumentException("kIn exceeds MAX_K_CHANNELS");
        }
        byte[] rowW = new byte[kIn];
        for (int m = 0; m < mOut; m++) {
            System.arraycopy(weights, m * kIn, rowW, 0, kIn);
            int acc = dotProduct(rowW, x, kIn);
            if (bias != null) {
                acc += bias[m];
            }
            y[m] = dyadicRescale(acc, mScale, sShift);
        }
    }

    public static void relu(short[] data, int count) {
        for (int i = 0; i < count; i++) {
            if (data[i] < 0) {
                data[i] = 0;
            }
        }
    }

    public static int lookupCdf(int diff) {
        return lookupCdf(diff, 0);
    }

    public static int lookupCdf(int diff, int index) {
        int lutIdx = diff + 256;
        if (lutIdx < 0) lutIdx = 0;
        if (lutIdx > 511) lutIdx = 511;
        int baseProb = CDF_SIGMOID_LUT[lutIdx];
        int combined = baseProb + (index << 1);
        if (combined > CDF_NORMALIZATION_MASS) {
            combined = CDF_NORMALIZATION_MASS;
        }
        return combined;
    }

    public static byte[] computeSha256(byte[] data) {
        try {
            MessageDigest md = MessageDigest.getInstance("SHA-256");
            return md.digest(data);
        } catch (NoSuchAlgorithmException e) {
            throw new RuntimeException("SHA-256 not available", e);
        }
    }

    public static String toHexString(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) {
            sb.append(String.format("%02x", b));
        }
        return sb.toString();
    }

    public static boolean verifyDeterminismSelfTest() {
        // Verify invariant bound
        if (ABSOLUTE_MAX_ACCUM > Integer.MAX_VALUE) {
            return false;
        }
        if (ABSOLUTE_MAX_ACCUM != 2130641408L) {
            return false;
        }

        // Test dot product with synthetic boundary vectors
        int k = 512;
        byte[] weights = new byte[k];
        short[] activations = new short[k];
        for (int i = 0; i < k; i++) {
            weights[i] = (byte) ((i % 2 == 0) ? 127 : -127);
            activations[i] = (short) ((i % 2 == 0) ? 32767 : -32767);
        }

        int dot = dotProduct(weights, activations, k);
        // All terms are positive product: 512 * 127 * 32767 = 2,130,641,408
        if (dot != 2130641408) {
            return false;
        }

        // Test negative dot product bound
        for (int i = 0; i < k; i++) {
            weights[i] = (byte) -127;
            activations[i] = (short) 32767;
        }
        int negDot = dotProduct(weights, activations, k);
        if (negDot != -2130641408) {
            return false;
        }

        // Test LeakyReLU
        if (leakyReluFixed(1000) != 1000) return false;
        int negLeaky = leakyReluFixed(-1000); // (-26000 + 128) >> 8 = -25872 >> 8 = -102
        if (negLeaky != -102) return false;

        // Test dyadic rescaling (matching C++ DetInt::dyadic_rescale tests)
        if (dyadicRescale(65536, 16384, 15) != 32767) return false;
        if (dyadicRescale(1000, 16384, 15) != 500) return false;
        if (dyadicRescale(10000000, 32767, 10) != ACT_MAX) return false;
        if (dyadicRescale(-10000000, 32767, 10) != ACT_MIN) return false;

        // Test ReLU
        short[] testReluArr = new short[] { 500, -100, 0, -32767, 32767 };
        relu(testReluArr, 5);
        if (testReluArr[0] != 500 || testReluArr[1] != 0 || testReluArr[2] != 0 ||
            testReluArr[3] != 0 || testReluArr[4] != 32767) {
            return false;
        }

        // Test Linear Layer
        // x = [10, -20], weights = [[2, 1], [-3, 4]], bias = [5, -5]
        // row 0: 10*2 + (-20)*1 = 0 + 5 = 5. dyadicRescale(5, 16384, 14) -> (5*16384 + 8192) >> 14 = (81920 + 8192) >> 14 = 90112 >> 14 = 5
        // row 1: 10*(-3) + (-20)*4 = -30 - 80 = -110 + (-5) = -115. dyadicRescale(-115, 16384, 14) -> (-115*16384 + 8192) >> 14 = (-1884160 + 8192) >> 14 = -1875968 >> 14 = -115
        short[] linX = new short[] { 10, -20 };
        byte[] linW = new byte[] { 2, 1, -3, 4 };
        int[] linBias = new int[] { 5, -5 };
        short[] linY = new short[2];
        linearLayer(linX, linW, linBias, linY, 2, 2, 16384, 14);
        if (linY[0] != 5 || linY[1] != -115) {
            return false;
        }

        // Test static LUT and lookupCdf
        if (CDF_SIGMOID_LUT[0] != 0) return false;
        if (CDF_SIGMOID_LUT[511] != 32768) return false;
        int center = CDF_SIGMOID_LUT[256];
        if (center < 16000 || center > 16800) return false;

        int cdf1 = lookupCdf(-255, 0);
        int cdf2 = lookupCdf(255, 10);
        if (cdf1 > cdf2) return false;
        if (cdf2 > CDF_NORMALIZATION_MASS) return false;

        return true;
    }

    public static void main(String[] args) {
        boolean pass = verifyDeterminismSelfTest();
        System.out.println("DetInt Determinism Self-Test: " + (pass ? "PASSED" : "FAILED"));
        if (!pass) {
            System.exit(1);
        }
    }
}
