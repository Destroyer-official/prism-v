package com.prismv.bbr;

import com.prismv.detint.DetInt;
import com.prismv.rans.Rans8Codec;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;

/**
 * BBR block compressor used by the standalone Java app.
 * Header: "BBR1", 4-byte original length, 1-byte mode, payload.
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
        if ((long) entropyData.length + 9 < (long) rawData.length) {
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
        if (bbrData == null || bbrData.length == 0) {
            return new byte[0];
        }
        if (bbrData.length < 9) {
            throw new IllegalArgumentException("BBR1 header is truncated");
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
        if (origLen < 0) {
            throw new IllegalArgumentException("Invalid original length");
        }

        int flag = bbrData[offset++] & 0xFF;

        if (flag == 1) {
            if (offset + origLen > bbrData.length) {
                throw new IllegalArgumentException("Bypass payload is truncated");
            }
            return Arrays.copyOfRange(bbrData, offset, offset + origLen);
        }
        if (flag != 0) {
            throw new IllegalArgumentException("Unknown BBR1 mode: " + flag);
        }

        byte[] entropySlice = Arrays.copyOfRange(bbrData, offset, bbrData.length);
        byte[] residuals = Rans8Codec.decompress(entropySlice);
        if (residuals.length != origLen) {
            throw new IllegalArgumentException("Entropy payload length does not match the header");
        }

        byte[] restored = new byte[origLen];
        if (origLen > 0) {
            restored[0] = residuals[0];
            for (int i = 1; i < origLen; i++) {
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

    /**
     * Mode L-BBR 2.0 (Frontier 1) GOP compression using C-MAVQ + SSG-CR4 + 16-way SIMD rANS.
     * Header: "BBR2" (4 bytes), origLen (4 bytes), codecId (1 byte), payload.
     */
    public static byte[] compressGopBbr2(byte[] rawData, int codecId) {
        if (rawData == null || rawData.length == 0) {
            return new byte[0];
        }

        try {
            byte[] nativeCompressed = com.prismv.engine.VideoProcessingPipeline.nativeCompressBbr2(rawData, codecId);
            if (nativeCompressed != null && nativeCompressed.length > 0) {
                ByteArrayOutputStream out = new ByteArrayOutputStream(nativeCompressed.length + 9);
                out.write('B'); out.write('B'); out.write('R'); out.write('2');
                int len = rawData.length;
                out.write((len >> 24) & 0xFF);
                out.write((len >> 16) & 0xFF);
                out.write((len >> 8) & 0xFF);
                out.write(len & 0xFF);
                out.write(codecId & 0xFF);
                out.write(nativeCompressed, 0, nativeCompressed.length);
                return out.toByteArray();
            }
        } catch (UnsatisfiedLinkError | NoClassDefFoundError ignored) {
            // Fall back to Java implementation if native shared library is not loaded
        }

        // Pure Java fallback: wrap standard BBR1 compressed payload with BBR2 header
        byte[] bbr1 = compressGop(rawData);
        if (bbr1.length >= 4) {
            bbr1[3] = '2'; // Convert magic to BBR2
        }
        return bbr1;
    }

    /**
     * Decompresses Mode L-BBR 2.0 container back to bit-exact raw GOP data.
     */
    public static byte[] decompressGopBbr2(byte[] bbr2Data) {
        if (bbr2Data == null || bbr2Data.length == 0) {
            return new byte[0];
        }
        if (bbr2Data.length < 9) {
            throw new IllegalArgumentException("BBR2 header is truncated");
        }

        if (bbr2Data[0] != 'B' || bbr2Data[1] != 'B' || bbr2Data[2] != 'R' || bbr2Data[3] != '2') {
            // Fall back to BBR1 if magic is BBR1
            if (bbr2Data[0] == 'B' && bbr2Data[1] == 'B' && bbr2Data[2] == 'R' && bbr2Data[3] == '1') {
                return decompressGop(bbr2Data);
            }
            throw new IllegalArgumentException("Invalid BBR2 magic header");
        }

        int offset = 4;
        int origLen = ((bbr2Data[offset++] & 0xFF) << 24) |
                      ((bbr2Data[offset++] & 0xFF) << 16) |
                      ((bbr2Data[offset++] & 0xFF) << 8) |
                      (bbr2Data[offset++] & 0xFF);
        if (origLen < 0) {
            throw new IllegalArgumentException("Invalid original length in BBR2 header");
        }

        int codecId = bbr2Data[offset++] & 0xFF;
        // codecId is preserved in header for stream routing

        byte[] payload = Arrays.copyOfRange(bbr2Data, offset, bbr2Data.length);
        try {
            byte[] nativeDecomp = com.prismv.engine.VideoProcessingPipeline.nativeDecompressBbr2(payload, origLen);
            if (nativeDecomp != null && nativeDecomp.length == origLen) {
                return nativeDecomp;
            }
        } catch (UnsatisfiedLinkError | NoClassDefFoundError ignored) {
            // Fall back to Java path if native is unavailable
        }

        // Java fallback
        byte[] mockBbr1 = new byte[bbr2Data.length];
        System.arraycopy(bbr2Data, 0, mockBbr1, 0, bbr2Data.length);
        mockBbr1[3] = '1';
        return decompressGop(mockBbr1);
    }

    /**
     * Executes complete bit-exact round-trip test using Mode L-BBR 2.0.
     */
    public static CompressionResult testBitExactRoundTripBbr2(byte[] testPayload, int codecId) {
        long t0 = System.currentTimeMillis();

        byte[] origDigest = DetInt.computeSha256(testPayload);
        String origSha = DetInt.toHexString(origDigest);

        byte[] compressed = compressGopBbr2(testPayload, codecId);
        byte[] restored = decompressGopBbr2(compressed);

        byte[] recDigest = DetInt.computeSha256(restored);
        String recSha = DetInt.toHexString(recDigest);

        boolean bitExact = origSha.equals(recSha) && Arrays.equals(testPayload, restored);
        double ratio = (double) compressed.length / (double) testPayload.length;
        long durationMs = System.currentTimeMillis() - t0;

        return new CompressionResult(true, compressed, origSha, recSha, bitExact, ratio, durationMs);
    }
}
