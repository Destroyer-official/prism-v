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
    public static final long ABSOLUTE_MAX_ACCUM = (long) MAX_K_CHANNELS * WEIGHT_MAX * ACT_MAX;

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

        // Test LeakyReLU
        if (leakyReluFixed(1000) != 1000) return false;
        int negLeaky = leakyReluFixed(-1000); // (-26000 + 128) >> 8 = -25872 >> 8 = -102
        if (negLeaky != -102) return false;

        return true;
    }
}
