package com.prismv.bbr;

import com.prismv.detint.DetInt;
import com.prismv.rans.Rans8Codec;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;

/**
 * Mode L-BBR (Bit-Exact Block Reconstruction) Compressor.
 * Strictly adheres to Invariant 1 (Lossless expansion prohibition) and
 * Invariant 2 (Deterministic integer arithmetic).
 */
public final class BbrCodec {

    public static class CompressionResult {
        public final boolean success;
        public final byte[] compressedData;
        public final String originalSha256;
        public final String reconstructedSha256;
        public final boolean bitExactMatch;
        public final double ratio;
        public final long durationMs;

        public CompressionResult(boolean success, byte[] compressedData, String origSha,
                                 String recSha, boolean bitExact, double ratio, long durationMs) {
            this.success = success;
            this.compressedData = compressedData;
            this.originalSha256 = origSha;
            this.reconstructedSha256 = recSha;
            this.bitExactMatch = bitExact;
            this.ratio = ratio;
            this.durationMs = durationMs;
        }
    }

    private BbrCodec() {}

    /**
     * Compresses input buffer using spatial differential prediction and rANS entropy coding.
     */
    public static byte[] compressGop(byte[] rawData) {
        if (rawData == null || rawData.length == 0) {
            return new byte[0];
        }

        // 1. Spatial Intra Prediction / Differential pass
        byte[] residuals = new byte[rawData.length];
        residuals[0] = rawData[0];
        for (int i = 1; i < rawData.length; i++) {
            int diff = ((rawData[i] & 0xFF) - (rawData[i - 1] & 0xFF)) & 0xFF;
            residuals[i] = (byte) diff;
        }

        // 2. Entropy compress via rANS
        byte[] entropyData = Rans8Codec.compress(residuals);

        // 3. Invariant 1: Check expansion prohibition
        ByteArrayOutputStream out = new ByteArrayOutputStream();
        // Magic header: "BBR1" (4 bytes)
        out.write('B'); out.write('B'); out.write('R'); out.write('1');
        
        // Write uncompressed length (4 bytes)
        int len = rawData.length;
        out.write((len >> 24) & 0xFF);
        out.write((len >> 16) & 0xFF);
        out.write((len >> 8) & 0xFF);
        out.write(len & 0xFF);

        // Flag: 0 = compressed, 1 = uncompressed bypass
        if (entropyData.length + 9 < rawData.length) {
            out.write(0); // Compressed
            out.write(entropyData, 0, entropyData.length);
        } else {
            // Invariant 1: Bypass mode if compressed size >= raw size
            out.write(1); // Bypass
            out.write(rawData, 0, rawData.length);
        }

        return out.toByteArray();
    }

    /**
     * Decompresses BBR container back to bit-exact raw GOP data.
     */
    public static byte[] decompressGop(byte[] bbrData) {
        if (bbrData == null || bbrData.length < 9) {
            return new byte[0];
        }

        int offset = 0;
        if (bbrData[0] != 'B' || bbrData[1] != 'B' || bbrData[2] != 'R' || bbrData[3] != '1') {
            throw new IllegalArgumentException("Invalid BBR1 magic header");
        }
        offset += 4;

        int origLen = ((bbrData[offset++] & 0xFF) << 24) |
                      ((bbrData[offset++] & 0xFF) << 16) |
                      ((bbrData[offset++] & 0xFF) << 8) |
                      (bbrData[offset++] & 0xFF);

        int flag = bbrData[offset++] & 0xFF;

        if (flag == 1) {
            // Bypass mode
            return Arrays.copyOfRange(bbrData, offset, offset + origLen);
        }

        // Decompress entropy stream
        byte[] entropySlice = Arrays.copyOfRange(bbrData, offset, bbrData.length);
        byte[] residuals = Rans8Codec.decompress(entropySlice);

        // Invert differential prediction
        byte[] restored = new byte[origLen];
        if (residuals.length > 0) {
            restored[0] = residuals[0];
            for (int i = 1; i < origLen && i < residuals.length; i++) {
                int val = ((restored[i - 1] & 0xFF) + (residuals[i] & 0xFF)) & 0xFF;
                restored[i] = (byte) val;
            }
        }
        return restored;
    }

    /**
     * Executes complete bit-exact round-trip test.
     */
    public static CompressionResult testBitExactRoundTrip(byte[] testPayload) {
        long t0 = System.currentTimeMillis();

        byte[] origDigest = DetInt.computeSha256(testPayload);
        String origSha = DetInt.toHexString(origDigest);

        byte[] compressed = compressGop(testPayload);
        byte[] restored = decompressGop(compressed);

        byte[] recDigest = DetInt.computeSha256(restored);
        String recSha = DetInt.toHexString(recDigest);

        boolean bitExact = origSha.equals(recSha) && Arrays.equals(testPayload, restored);
        double ratio = (double) compressed.length / (double) testPayload.length;
        long durationMs = System.currentTimeMillis() - t0;

        return new CompressionResult(true, compressed, origSha, recSha, bitExact, ratio, durationMs);
    }
}
