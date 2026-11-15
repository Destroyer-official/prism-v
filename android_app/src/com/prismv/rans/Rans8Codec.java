package com.prismv.rans;

import java.io.*;
import java.util.*;

/**
 * 8-Way Interleaved rANS Entropy Codec.
 * Conforms strictly to Invariant 2 (Deterministic integer arithmetic) and
 * delivers 100.000% bit-exact lossless round-trip compression.
 */
public final class Rans8Codec {
    public static final int NUM_LANES = 8;
    public static final int RANS_L = 1 << 16; // 65536
    public static final int RANS_PROB_BITS = 12;
    public static final int RANS_M = 1 << RANS_PROB_BITS; // 4096

    public static class RansSymbol {
        public int freq;
        public int start;

        public RansSymbol(int freq, int start) {
            this.freq = freq;
            this.start = start;
        }
    }

    private Rans8Codec() {}

    public static List<RansSymbol> buildStats(byte[] input) {
        int[] counts = new int[256];
        for (byte b : input) {
            counts[b & 0xFF]++;
        }
        int total = input.length > 0 ? input.length : 1;
        List<RansSymbol> stats = new ArrayList<>(256);
        int sum = 0;
        for (int i = 0; i < 256; i++) {
            int f = (int) (((long) counts[i] * (RANS_M - 256)) / total) + 1;
            stats.add(new RansSymbol(f, 0));
            sum += f;
        }
        stats.get(0).freq += (RANS_M - sum);
        int cum = 0;
        for (int i = 0; i < 256; i++) {
            stats.get(i).start = cum;
            cum += stats.get(i).freq;
        }
        return stats;
    }

    public static int[] buildCdfLut(List<RansSymbol> stats) {
        int[] lut = new int[RANS_M];
        for (int s = 0; s < stats.size(); s++) {
            int start = stats.get(s).start;
            int end = start + stats.get(s).freq;
            for (int m = start; m < end && m < RANS_M; m++) {
                lut[m] = s;
            }
        }
        return lut;
    }

    public static class EncodedChunk {
        public int symbolCount;
        public int[] finalStates = new int[NUM_LANES];
        public byte[] byteStream;
    }

    public static EncodedChunk encode(byte[] symbols, List<RansSymbol> stats) {
        EncodedChunk chunk = new EncodedChunk();
        chunk.symbolCount = symbols.length;
        Arrays.fill(chunk.finalStates, RANS_L);

        List<List<Byte>> laneBytes = new ArrayList<>(NUM_LANES);
        for (int i = 0; i < NUM_LANES; i++) {
            laneBytes.add(new ArrayList<>());
        }

        for (int idx = symbols.length; idx > 0; idx--) {
            int i = idx - 1;
            int lane = i % NUM_LANES;
            int sym = symbols[i] & 0xFF;
            RansSymbol sStats = stats.get(sym);
            int f = sStats.freq;
            int c = sStats.start;

            int x = chunk.finalStates[lane];
            // Renormalization: emit 8-bit byte while x >= (f << 12)
            int xMax = (f << 12);
            while (x >= xMax) {
                laneBytes.get(lane).add((byte) (x & 0xFF));
                x >>>= 8;
            }

            chunk.finalStates[lane] = (x / f) * RANS_M + (x % f) + c;
        }

        // Reverse each lane so decoder reads sequentially
        for (int l = 0; l < NUM_LANES; l++) {
            Collections.reverse(laneBytes.get(l));
        }

        // Pack: [8 counts (4 bytes each)] + [lane bytes]
        int totalBytes = 0;
        for (int l = 0; l < NUM_LANES; l++) {
            totalBytes += laneBytes.get(l).size();
        }

        byte[] stream = new byte[NUM_LANES * 4 + totalBytes];
        int ptr = 0;
        for (int l = 0; l < NUM_LANES; l++) {
            int count = laneBytes.get(l).size();
            stream[ptr++] = (byte) (count & 0xFF);
            stream[ptr++] = (byte) ((count >>> 8) & 0xFF);
            stream[ptr++] = (byte) ((count >>> 16) & 0xFF);
            stream[ptr++] = (byte) ((count >>> 24) & 0xFF);
        }
        for (int l = 0; l < NUM_LANES; l++) {
            for (byte b : laneBytes.get(l)) {
                stream[ptr++] = b;
            }
        }
        chunk.byteStream = stream;
        return chunk;
    }

    public static byte[] decode(EncodedChunk chunk, int[] cdfLut, List<RansSymbol> stats) {
        byte[] symbols = new byte[chunk.symbolCount];
        int ptr = 0;
        int[] byteCounts = new int[NUM_LANES];
        for (int l = 0; l < NUM_LANES; l++) {
            byteCounts[l] = (chunk.byteStream[ptr++] & 0xFF) |
                           ((chunk.byteStream[ptr++] & 0xFF) << 8) |
                           ((chunk.byteStream[ptr++] & 0xFF) << 16) |
                           ((chunk.byteStream[ptr++] & 0xFF) << 24);
        }

        byte[][] laneBytes = new byte[NUM_LANES][];
        int[] byteIdx = new int[NUM_LANES];
        for (int l = 0; l < NUM_LANES; l++) {
            laneBytes[l] = new byte[byteCounts[l]];
            for (int b = 0; b < byteCounts[l]; b++) {
                laneBytes[l][b] = chunk.byteStream[ptr++];
            }
        }

        int[] states = Arrays.copyOf(chunk.finalStates, NUM_LANES);

        for (int i = 0; i < chunk.symbolCount; i++) {
            int lane = i % NUM_LANES;
            int x = states[lane];

            int m = x & (RANS_M - 1);
            int sym = cdfLut[m];
            symbols[i] = (byte) sym;

            RansSymbol sStats = stats.get(sym);
            int f = sStats.freq;
            int c = sStats.start;

            x = f * (x >>> RANS_PROB_BITS) + (m - c);

            while (x < RANS_L) {
                if (byteIdx[lane] < byteCounts[lane]) {
                    int b = laneBytes[lane][byteIdx[lane]++] & 0xFF;
                    x = (x << 8) | b;
                } else {
                    break;
                }
            }
            states[lane] = x;
        }

        return symbols;
    }

    public static byte[] compress(byte[] input) {
        if (input == null || input.length == 0) return new byte[0];
        List<RansSymbol> stats = buildStats(input);
        EncodedChunk chunk = encode(input, stats);

        ByteArrayOutputStream out = new ByteArrayOutputStream();
        DataOutputStream dos = new DataOutputStream(out);
        try {
            dos.writeInt(chunk.symbolCount);
            for (int i = 0; i < 256; i++) {
                dos.writeShort(stats.get(i).freq);
            }
            for (int l = 0; l < NUM_LANES; l++) {
                dos.writeInt(chunk.finalStates[l]);
            }
            dos.writeInt(chunk.byteStream.length);
            dos.write(chunk.byteStream);
            dos.flush();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
        return out.toByteArray();
    }

    public static byte[] decompress(byte[] compressed) {
        if (compressed == null || compressed.length == 0) return new byte[0];
        try {
            DataInputStream dis = new DataInputStream(new ByteArrayInputStream(compressed));
            int symbolCount = dis.readInt();
            List<RansSymbol> stats = new ArrayList<>(256);
            int sum = 0;
            for (int i = 0; i < 256; i++) {
                int f = dis.readShort() & 0xFFFF;
                stats.add(new RansSymbol(f, sum));
                sum += f;
            }
            int[] cdfLut = buildCdfLut(stats);

            EncodedChunk chunk = new EncodedChunk();
            chunk.symbolCount = symbolCount;
            for (int l = 0; l < NUM_LANES; l++) {
                chunk.finalStates[l] = dis.readInt();
            }
            int streamLen = dis.readInt();
            byte[] stream = new byte[streamLen];
            dis.readFully(stream);
            chunk.byteStream = stream;

            return decode(chunk, cdfLut, stats);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
