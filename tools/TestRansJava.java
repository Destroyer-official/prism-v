import java.util.*;

public class TestRansJava {
    public static final int NUM_LANES = 8;
    public static final int RANS_L = 1 << 16; // 65536
    public static final int RANS_PROB_BITS = 12;
    public static final int RANS_M = 1 << RANS_PROB_BITS; // 4096

    public static class RansSymbol {
        public int freq;
        public int start;
        public RansSymbol(int f, int s) { this.freq = f; this.start = s; }
    }

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
        for (int l = 0; l < NUM_LANES; l++) totalBytes += laneBytes.get(l).size();

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

    public static void main(String[] args) {
        // Test 1: Structured data with differential prediction
        byte[] structured = new byte[16384];
        for (int i = 0; i < structured.length; i++) {
            structured[i] = (byte) ((i / 64) & 0xFF);
        }
        byte[] residuals = new byte[structured.length];
        residuals[0] = structured[0];
        for (int i = 1; i < structured.length; i++) {
            residuals[i] = (byte) (((structured[i] & 0xFF) - (structured[i - 1] & 0xFF)) & 0xFF);
        }

        List<RansSymbol> stats = buildStats(residuals);
        int[] cdf = buildCdfLut(stats);
        EncodedChunk chunk = encode(residuals, stats);
        byte[] restoredRes = decode(chunk, cdf, stats);

        boolean match = Arrays.equals(residuals, restoredRes);
        System.out.println("Structured residuals 100% bit-exact: " + match);

        // Test differential restoration
        byte[] rec = new byte[structured.length];
        rec[0] = restoredRes[0];
        for (int i = 1; i < structured.length; i++) {
            rec[i] = (byte) (((rec[i - 1] & 0xFF) + (restoredRes[i] & 0xFF)) & 0xFF);
        }
        System.out.println("Structured reconstructed 100% bit-exact: " + Arrays.equals(structured, rec));

        // Test 2: Random data
        byte[] randomData = new byte[8192];
        new Random(42).nextBytes(randomData);
        List<RansSymbol> rStats = buildStats(randomData);
        int[] rCdf = buildCdfLut(rStats);
        EncodedChunk rChunk = encode(randomData, rStats);
        byte[] rRestored = decode(rChunk, rCdf, rStats);
        System.out.println("Random data 100% bit-exact: " + Arrays.equals(randomData, rRestored));
    }
}
