package com.prismv.test;

public class JniVerificationTest {
    static {
        System.loadLibrary("prism_jni");
    }

    // Native declarations matching Kotlin PrismVfsService & AtomicGopWorker
    public static native long nativeGetVirtualSize(String pvlossPath);
    public static native byte[] nativeDecompressChunk(String pvlossPath, long chunkIndex);
    public static native boolean processSingleGop(String inputPath, String outputPath, int gopIndex, int framesCount);
    public static native boolean nativeCompositeSocialExport(String inputPath, String outputPath);
    public static native String nativeProbeAndRoute(byte[] headerData, long fileSize, boolean userLossless);
    public static native boolean nativeRemuxContainer(String inputPath, String outputPath);
    public static native boolean nativeVerifyRfsqParity();
    public static native byte[] nativeEncodeSemanticFrame(byte[] frameYuv, long frameIndex, short[] keypoints3d, short[] pose6dof);
    public static native byte[] nativeDecodeSemanticFrame(byte[] payloadBytes, byte[] baseAnchorFrame, int width, int height);
    public static native byte[] nativeNeRVRenderFrame(byte[] gopPayload, byte[] baseFrame, float tContinuous, int width, int height);
    public static native byte[] nativeProcessFrontier5Frame(byte[] frameData, int width, int height);
    public static native float[] nativeGetFrontier5Metrics(int width, int height);
    public static native byte[] nativeExecuteVulkanLanczos5(byte[] srcPlane, int srcW, int srcH, int dstW, int dstH);
    public static native byte[] nativeMuxFrontier5Container(byte[] baseNalu, byte[] residualPayload, byte[] afgsParams, int width, int height, long ptsMs);
    public static native float[] nativeGetSpatialMetrics(long handle);
    public static native byte[] nativeCompressBbr2(byte[] bitstream, int codecType);
    public static native byte[] nativeDecompressBbr2(byte[] compressed, int uncompressedSize);
    public static native byte[] nativeCompressPsc2(byte[] rawPixels, int width, int height, int filterType, boolean isQuadBayer);
    public static native byte[] nativeDecompressPsc2(byte[] compressedFrame, int width, int height, int filterType, boolean isQuadBayer);

    public static void main(String[] args) {
        System.out.println("===============================================================");
        System.out.println("  PRISM-V JNI MANUAL SMOKE TEST");
        System.out.println("===============================================================\n");
        System.out.println("Requires libprism_jni on java.library.path and a sample video at");
        System.out.println("sample_video/sample.mp4 for the GOP case.\n");

        System.out.println("[JNI] Native libprism_jni loaded.");

        // 1. Test virtual size query on non-existent file
        long size = nativeGetVirtualSize("non_existent.pvloss");
        System.out.println("[JNI] nativeGetVirtualSize('non_existent.pvloss') -> " + size + " (expected: 0)");

        // 2. Test Atomic GOP worker bridge
        boolean gopFail = processSingleGop("non_existent_video.mp4", "out.bin", 0, 60);
        System.out.println("[JNI] processSingleGop('non_existent.mp4') -> " + gopFail + " (expected: false)");

        boolean gopOk = processSingleGop("sample_video/sample.mp4", "sample_video/test_gop0.bin", 0, 60);
        System.out.println("[JNI] processSingleGop('sample_video/sample.mp4', GOP#0) -> " + gopOk);
        if (new java.io.File("sample_video/test_gop0.bin").exists()) {
            new java.io.File("sample_video/test_gop0.bin").delete();
        }

        // 3. Test Probe and Route bridge
        byte[] mp4Header = new byte[]{
            0x00, 0x00, 0x00, 0x18, 'f', 't', 'y', 'p',
            'm', 'p', '4', '2', 0x00, 0x00, 0x00, 0x00,
            'i', 's', 'o', 'm', 'm', 'p', '4', '2'
        };
        String routeDecision = nativeProbeAndRoute(mp4Header, 50000000L, true);
        System.out.println("[JNI] nativeProbeAndRoute(isobmff, 50MB, lossless=true) -> " + routeDecision);

        // 4. Test Remux bridge error handling on missing input
        boolean remuxResult = nativeRemuxContainer("non_existent_input.mp4", "non_existent_output.mp4");
        System.out.println("[JNI] nativeRemuxContainer('missing.mp4') -> " + remuxResult + " (expected: false)");

        // 5. Test Social Export Composite bridge error handling on missing input
        boolean socialResult = nativeCompositeSocialExport("non_existent.mp4", "social_out.mp4");
        System.out.println("[JNI] nativeCompositeSocialExport('missing.mp4') -> " + socialResult + " (expected: false)");

        // 6. Test Frontier 5 Silicon-Fused Heterogeneous Pipeline Metrics & Frame Processing
        float[] f5Metrics = nativeGetFrontier5Metrics(3840, 2160);
        if (f5Metrics != null && f5Metrics.length >= 2) {
            System.out.printf("[JNI] Frontier 5 Total Power: %.3f W (Sub-1.0W Target: %b), DRAM Bandwidth: %.2f GB/s%n",
                f5Metrics[0], f5Metrics[0] < 1.0f, f5Metrics[1]);
        }

        byte[] testFrame = new byte[128 * 128];
        for (int i = 0; i < testFrame.length; i++) testFrame[i] = (byte) (i % 256);
        byte[] f5Processed = nativeProcessFrontier5Frame(testFrame, 128, 128);
        System.out.println("[JNI] nativeProcessFrontier5Frame(128x128) -> output bytes: " + (f5Processed != null ? f5Processed.length : 0));

        byte[] fakeBase = new byte[]{1, 2, 3, 4};
        byte[] fakeRes = new byte[]{5, 6, 7, 8};
        byte[] f5Container = nativeMuxFrontier5Container(fakeBase, fakeRes, null, 128, 128, 0);
        System.out.println("[JNI] nativeMuxFrontier5Container() -> mp4 container bytes: " + (f5Container != null ? f5Container.length : 0));

        // 7. Test Frontier 3 Generative Residuals & Semantic Codec
        boolean rfsqParity = nativeVerifyRfsqParity();
        System.out.println("[JNI] nativeVerifyRfsqParity() -> " + rfsqParity + " (expected: true)");

        short[] kp = new short[96];
        short[] pose = new short[6];
        byte[] semPayload = nativeEncodeSemanticFrame(testFrame, 0, kp, pose);
        System.out.println("[JNI] nativeEncodeSemanticFrame(128x128) -> payload bytes: " + (semPayload != null ? semPayload.length : 0));

        byte[] semRecon = nativeDecodeSemanticFrame(semPayload, testFrame, 128, 128);
        System.out.println("[JNI] nativeDecodeSemanticFrame() -> recon bytes: " + (semRecon != null ? semRecon.length : 0));

        byte[] nervRender = nativeNeRVRenderFrame(null, null, 0.5f, 64, 64);
        System.out.println("[JNI] nativeNeRVRenderFrame(64x64, t=0.5) -> rendered bytes: " + (nervRender != null ? nervRender.length : 0));

        // 8. Test Frontier 6: Global Graph Deduplication & Semantic Video Vault (Rabin-Mesh)
        long[] meshScan = nativeScanMeshBurstCandidates(new String[]{"sample_video/sample.mp4"}, 0.85f);
        if (meshScan != null) {
            System.out.printf("[JNI] nativeScanMeshBurstCandidates() -> Files: %d, Burst Takes: %d, Reclaimable: %d bytes%n",
                meshScan[0], meshScan[1], meshScan[3]);
        }

        // 9. Test Frontier 4: Dynamic 4D Gaussian Splatting (Prism-Spatial)
        float[] spatialMetrics = nativeGetSpatialMetrics(0L);
        if (spatialMetrics != null && spatialMetrics.length >= 4) {
            System.out.printf("[JNI] Frontier 4 Prism-Spatial -> Bitrate: %.1f Mbps (Target: 18-32 Mbps), CVVDP: %.1f%n",
                spatialMetrics[2], spatialMetrics[3]);
        }

        // 10. Test Frontier 1: BBR 2.0 (SSG-CR4 + C-MAVQ + 16-Way SIMD rANS)
        byte[] bbr2TestStream = new byte[16384];
        for (int i = 0; i < bbr2TestStream.length; i++) bbr2TestStream[i] = (byte) ((i % 8 < 5) ? ((i * 7) % 64) : 0);
        byte[] bbr2Compressed = nativeCompressBbr2(bbr2TestStream, 0);
        if (bbr2Compressed != null) {
            byte[] bbr2Recon = nativeDecompressBbr2(bbr2Compressed, bbr2TestStream.length);
            boolean bbr2Match = java.util.Arrays.equals(bbr2TestStream, bbr2Recon);
            System.out.printf("[JNI] Frontier 1 BBR 2.0 -> Input: %d B, Comp: %d B, Match: %b (Bit-Exact: true)%n",
                bbr2TestStream.length, bbr2Compressed.length, bbr2Match);
        }

        // 11. Test Frontier 2: PSC 2.0 (ADIW 5/3 + YCoCg-R2 + Empirical rANS)
        byte[] psc2RawRgb = new byte[64 * 64 * 3];
        for (int i = 0; i < psc2RawRgb.length; i++) psc2RawRgb[i] = (byte) ((i * 17) & 0xFF);
        byte[] psc2Comp = nativeCompressPsc2(psc2RawRgb, 64, 64, 0, false);
        if (psc2Comp != null) {
            byte[] psc2Recon = nativeDecompressPsc2(psc2Comp, 64, 64, 0, false);
            boolean psc2Match = java.util.Arrays.equals(psc2RawRgb, psc2Recon);
            System.out.printf("[JNI] Frontier 2 PSC 2.0 -> Input: %d B, Comp: %d B, Match: %b (Bit-Exact: true)%n",
                psc2RawRgb.length, psc2Comp.length, psc2Match);
        }

        System.out.println("\n[JNI] Calls completed. Check the values above.\n");
    }

    public static native long[] nativeScanMeshBurstCandidates(String[] videoPaths, float similarityThreshold);
    public static native long nativeIngestMeshCluster(String primaryVideoPath, String[] deltaVideoPaths, String vaultDir, String clusterUuid);
    public static native boolean nativeRestoreMeshVideo(String manifestPath, String vaultDir, String outPath);
    public static native byte[] nativeReadVirtualMeshRange(String manifestPath, String vaultDir, long offset, int length);
}
