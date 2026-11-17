package com.prismv.rans;

import java.io.*;
import java.util.*;

/**
 * 8-way interleaved rANS entropy codec used by the Java BBR path.
 * Integer-only. Decode validates frequencies, lane lengths, and final states.
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
        if (symbols == null || stats == null || stats.isEmpty()) {
            throw new IllegalArgumentException("Symbols and statistics are required");
        }
        int mass = 0;
        for (int s = 0; s < stats.size(); s++) {
            RansSymbol symbol = stats.get(s);
            if (symbol.freq <= 0 || symbol.start != mass) {
                throw new IllegalArgumentException("Statistics must be contiguous and positive");
            }
            mass += symbol.freq;
        }
        if (mass != RANS_M) {
            throw new IllegalArgumentException("Statistics must sum to " + RANS_M);
        }

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
            if (sym >= stats.size()) {
                throw new IllegalArgumentException("Symbol value has no statistics");
            }
            RansSymbol sStats = stats.get(sym);
            int f = sStats.freq;
            int c = sStats.start;

            int x = chunk.finalStates[lane];
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
        if (chunk == null || cdfLut == null || stats == null) {
            throw new IllegalArgumentException("Decode inputs are required");
        }
        if (chunk.symbolCount < 0 || chunk.finalStates == null || chunk.byteStream == null) {
            throw new IllegalArgumentException("Invalid encoded chunk");
        }
        if (cdfLut.length != RANS_M) {
            throw new IllegalArgumentException("CDF lookup must contain " + RANS_M + " entries");
        }

        int[] byteCounts = new int[NUM_LANES];
        int ptr = 0;
        if (chunk.byteStream.length < NUM_LANES * 4) {
            throw new IllegalArgumentException("Lane header is truncated");
        }
        long totalLaneBytes = 0;
        for (int l = 0; l < NUM_LANES; l++) {
            int count = (chunk.byteStream[ptr++] & 0xFF) |
                        ((chunk.byteStream[ptr++] & 0xFF) << 8) |
                        ((chunk.byteStream[ptr++] & 0xFF) << 16) |
                        ((chunk.byteStream[ptr++] & 0xFF) << 24);
            if (count < 0) {
                throw new IllegalArgumentException("Negative lane byte count");
            }
            byteCounts[l] = count;
            totalLaneBytes += count;
        }
        if (totalLaneBytes != chunk.byteStream.length - ptr) {
            throw new IllegalArgumentException("Lane byte counts do not match the stream length");
        }

        byte[][] laneBytes = new byte[NUM_LANES][];
        int[] byteIdx = new int[NUM_LANES];
        for (int l = 0; l < NUM_LANES; l++) {
            laneBytes[l] = new byte[byteCounts[l]];
            System.arraycopy(chunk.byteStream, ptr, laneBytes[l], 0, byteCounts[l]);
            ptr += byteCounts[l];
        }

        int[] states = Arrays.copyOf(chunk.finalStates, NUM_LANES);
        for (int l = 0; l < NUM_LANES; l++) {
            if (states[l] <= 0) {
                throw new IllegalArgumentException("Invalid rANS state");
            }
        }

        byte[] symbols = new byte[chunk.symbolCount];
        for (int i = 0; i < chunk.symbolCount; i++) {
            int lane = i % NUM_LANES;
            int x = states[lane];

            int m = x & (RANS_M - 1);
            int sym = cdfLut[m];
            if (sym < 0 || sym >= stats.size()) {
                throw new IllegalArgumentException("CDF lookup returned an invalid symbol");
            }
            symbols[i] = (byte) sym;

            RansSymbol sStats = stats.get(sym);
            int f = sStats.freq;
            int c = sStats.start;

            x = f * (x >>> RANS_PROB_BITS) + (m - c);

            while (x < RANS_L) {
                if (byteIdx[lane] >= byteCounts[lane]) {
                    throw new IllegalArgumentException("Lane stream is truncated");
                }
                int b = laneBytes[lane][byteIdx[lane]++] & 0xFF;
                x = (x << 8) | b;
            }
            states[lane] = x;
        }

        for (int l = 0; l < NUM_LANES; l++) {
            if (states[l] != RANS_L || byteIdx[l] != byteCounts[l]) {
                throw new IllegalArgumentException("Lane stream did not terminate cleanly");
            }
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
            if (symbolCount < 0) {
                throw new IllegalArgumentException("Negative symbol count");
            }
            List<RansSymbol> stats = new ArrayList<>(256);
            int sum = 0;
            for (int i = 0; i < 256; i++) {
                int f = dis.readShort() & 0xFFFF;
                if (f <= 0) {
                    throw new IllegalArgumentException("Zero-frequency symbol in header");
                }
                stats.add(new RansSymbol(f, sum));
                sum += f;
                if (sum > RANS_M) {
                    throw new IllegalArgumentException("Symbol frequencies exceed " + RANS_M);
                }
            }
            if (sum != RANS_M) {
                throw new IllegalArgumentException("Symbol frequencies do not sum to " + RANS_M);
            }
            int[] cdfLut = buildCdfLut(stats);

            EncodedChunk chunk = new EncodedChunk();
            chunk.symbolCount = symbolCount;
            for (int l = 0; l < NUM_LANES; l++) {
                int state = dis.readInt();
                if (state <= 0) {
                    throw new IllegalArgumentException("Invalid rANS state in header");
                }
                chunk.finalStates[l] = state;
            }
            int streamLen = dis.readInt();
            if (streamLen < 0 || streamLen > dis.available()) {
                throw new IllegalArgumentException("Invalid lane stream length");
            }
            byte[] stream = new byte[streamLen];
            dis.readFully(stream);
            chunk.byteStream = stream;

            return decode(chunk, cdfLut, stats);
        } catch (java.io.EOFException e) {
            throw new IllegalArgumentException("Truncated rANS stream", e);
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }

    public static void main(String[] args) {
        byte[] input = new byte[8192];
        for (int i = 0; i < input.length; i++) {
            input[i] = (byte) ((i * 73 + 19) & 0x1F);
        }
        byte[] compressed = compress(input);
        byte[] restored = decompress(compressed);
        if (restored.length != input.length) {
            throw new AssertionError("Length mismatch: " + restored.length + " vs " + input.length);
        }
        for (int i = 0; i < input.length; i++) {
            if (input[i] != restored[i]) {
                throw new AssertionError("Byte mismatch at " + i);
            }
        }
        System.out.println("[RANS8 JAVA] 100.000% Bit-exact roundtrip verified! (" + input.length + " -> " + compressed.length + " bytes)");
    }
}
