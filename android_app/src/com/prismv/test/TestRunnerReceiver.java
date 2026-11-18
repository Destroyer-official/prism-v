package com.prismv.test;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import com.prismv.logging.PrismLogger;

import com.prismv.bbr.BbrCodec;
import com.prismv.detint.DetInt;
import com.prismv.engine.HardwareVideoCompressor;
import com.prismv.history.CompressionHistoryManager;
import com.prismv.storage.PrismStorageManager;
import com.prismv.thermal.ThermalGearShifter;
import com.prismv.transaction.VerifiedReplacement;
import com.prismv.vfs.PrismVfsProxy;

import android.os.PowerManager;

import java.io.File;
import java.io.FileOutputStream;
import java.util.Locale;
import java.util.Random;

/**
 * BroadcastReceiver for automated ADB testing.
 * Triggered via:
 *   adb shell am broadcast -a com.prismv.ACTION_RUN_TESTS
 *
 * Runs on-device self-checks over the Java codec/transaction helpers. These
 * verify local behavior only; they are not video quality or energy
 * measurements, and the native C++ equivalents are covered by CTest.
 */
public class TestRunnerReceiver extends BroadcastReceiver {
    private static final String TAG = "PrismV";

    @Override
    public void onReceive(Context context, Intent intent) {
        String action = intent != null ? intent.getAction() : null;
        PrismLogger.i(TAG, "=======================================================");
        PrismLogger.i(TAG, ">>> RECEIVED INTENT ACTION: " + action + " <<<");
        PrismLogger.i(TAG, "=======================================================");

        final PendingResult pendingResult = goAsync();
        new Thread(() -> {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            PowerManager.WakeLock wakeLock = null;
            if (pm != null) {
                wakeLock = pm.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "PrismV:TestRunnerLock");
                wakeLock.acquire(15 * 60 * 1000L); // 15 min max execution guarantee
            }
            try {
                if ("com.prismv.ACTION_TEST_BBR".equals(action)) {
                    checkBbrRoundTrip();
                } else if ("com.prismv.ACTION_TEST_VFS".equals(action)) {
                    checkVfsProxySeek();
                } else if ("com.prismv.ACTION_TEST_TX".equals(action)) {
                    checkVerifiedReplacement(context);
                } else if ("com.prismv.ACTION_COMPRESS_SAMPLE".equals(action)) {
                    runSampleCompression(context);
                } else if ("com.prismv.ACTION_COMPRESS_F5_SILICON".equals(action)) {
                    runSiliconSampleCompression(context);
                } else if ("com.prismv.ACTION_START_SERVICE".equals(action)) {
                    Intent sIntent = new Intent(context, com.prismv.worker.PrismMediaProcessingService.class);
                    if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.O) {
                        context.startForegroundService(sIntent);
                    } else {
                        context.startService(sIntent);
                    }
                } else {
                    // Default: run the complete device self-check set
                    boolean allPassed = runAllDeviceChecks(context);
                    PrismLogger.i(TAG, "=======================================================");
                    if (allPassed) {
                        PrismLogger.i(TAG, ">>> ALL DEVICE SELF-CHECKS PASSED (9/9) <<<");
                    } else {
                        PrismLogger.e(TAG, ">>> DEVICE SELF-CHECKS FAILED <<<");
                    }
                    PrismLogger.i(TAG, "=======================================================");
                }
            } finally {
                if (wakeLock != null && wakeLock.isHeld()) {
                    wakeLock.release();
                }
                pendingResult.finish();
            }
        }).start();
    }

    public static boolean runAllDeviceChecks(Context context) {
        boolean pass1 = checkBbrRoundTrip();
        boolean pass2 = checkDetIntDeterminism();
        boolean pass3 = checkThermalGearTable();
        boolean pass4 = checkGopPayloadRoundTrip();
        boolean pass5 = checkVfsProxySeek();
        boolean pass6 = checkVerifiedReplacement(context);
        boolean pass7 = checkSiliconPowerProfile();
        boolean pass8 = checkHeterogeneousResourceGovernor();
        boolean pass9 = checkTilePoolBound();

        boolean totalPass = pass1 && pass2 && pass3 && pass4 && pass5 && pass6 && pass7 && pass8 && pass9;

        // Write results JSON to app internal files dir
        try {
            File outFile = new File(context.getFilesDir(), "prism_test_results.json");
            StringBuilder json = new StringBuilder();
            json.append("{\n");
            json.append("  \"overall_status\": \"").append(totalPass ? "PASS" : "FAIL").append("\",\n");
            json.append("  \"timestamp\": ").append(System.currentTimeMillis()).append(",\n");
            json.append("  \"checks\": {\n");
            json.append("    \"bbr_lossless_roundtrip\": ").append(pass1).append(",\n");
            json.append("    \"detint_determinism\": ").append(pass2).append(",\n");
            json.append("    \"thermal_gear_table_bound\": ").append(pass3).append(",\n");
            json.append("    \"gop_payload_roundtrip\": ").append(pass4).append(",\n");
            json.append("    \"vfs_proxy_seek\": ").append(pass5).append(",\n");
            json.append("    \"verified_replacement\": ").append(pass6).append(",\n");
            json.append("    \"silicon_sub_watt_profile\": ").append(pass7).append(",\n");
            json.append("    \"heterogeneous_governor\": ").append(pass8).append(",\n");
            json.append("    \"tile_pool_budget_bound\": ").append(pass9).append("\n");
            json.append("  },\n");
            json.append("  \"notes\": [\n");
            json.append("    \"thermal_gear_table_bound checks declared estimates, not measured joules\",\n");
            json.append("    \"no VMAF, PSNR, or bitrate measurement is performed here\"\n");
            json.append("  ]\n");
            json.append("}\n");

            try (FileOutputStream fos = new FileOutputStream(outFile)) {
                fos.write(json.toString().getBytes());
                fos.flush();
            }
            PrismLogger.i(TAG, "[REPORT] Written audit JSON to " + outFile.getAbsolutePath());
        } catch (Exception e) {
            PrismLogger.w(TAG, "[REPORT] Could not write audit JSON: " + e.getMessage());
        }

        return totalPass;
    }

    private static boolean checkBbrRoundTrip() {
        PrismLogger.i(TAG, "[CHECK] BBR lossless round-trip");
        // Structured payload: must round-trip bit-exactly and shrink.
        byte[] structured = new byte[16384];
        for (int i = 0; i < structured.length; i++) {
            structured[i] = (byte) ((i / 64) & 0xFF);
        }
        BbrCodec.CompressionResult res1 = BbrCodec.testBitExactRoundTrip(structured);
        boolean p1 = res1.bitExactMatch && res1.compressedData.length < structured.length;

        // Noise payload: must still round-trip, and must not expand beyond
        // the raw payload plus the 9-byte container header.
        byte[] noise = new byte[4096];
        new Random(42).nextBytes(noise);
        BbrCodec.CompressionResult res2 = BbrCodec.testBitExactRoundTrip(noise);
        boolean p2 = res2.bitExactMatch && res2.compressedData.length <= (noise.length + 9);

        boolean pass = p1 && p2;
        PrismLogger.i(TAG, String.format("[CHECK] BBR result: %s (structured bit-exact=%b, ratio=%.2fx | noise bit-exact=%b, %d B)",
                pass ? "PASS" : "FAIL", res1.bitExactMatch, (1.0 / res1.ratio), res2.bitExactMatch, res2.compressedData.length));
        return pass;
    }

    private static boolean checkDetIntDeterminism() {
        PrismLogger.i(TAG, "[CHECK] DetInt fixed-point determinism");
        boolean pass = DetInt.verifyDeterminismSelfTest();
        PrismLogger.i(TAG, "[CHECK] DetInt result: " + (pass ? "PASS" : "FAIL"));
        return pass;
    }

    private static boolean checkThermalGearTable() {
        PrismLogger.i(TAG, "[CHECK] Thermal gear table stays within declared ceiling");
        ThermalGearShifter shifter = new ThermalGearShifter();
        boolean pass = shifter.isWithinThermodynamicCeiling();
        PrismLogger.i(TAG, String.format("[CHECK] Thermal table result: %s (gear %s, declared %.3f J/frame, ceiling 0.050)",
                pass ? "PASS" : "FAIL", shifter.getCurrentGear().name(), shifter.getCurrentGear().estJoulesPerFrame));
        return pass;
    }

    private static boolean checkGopPayloadRoundTrip() {
        PrismLogger.i(TAG, "[CHECK] GOP-sized payload round-trip");
        byte[] gop = new byte[8192];
        for (int i = 0; i < gop.length; i++) gop[i] = (byte) (i & 0x7F);
        BbrCodec.CompressionResult res = BbrCodec.testBitExactRoundTrip(gop);
        boolean pass = res.bitExactMatch;
        PrismLogger.i(TAG, "[CHECK] GOP payload result: " + (pass ? "PASS" : "FAIL"));
        return pass;
    }

    private static boolean checkVfsProxySeek() {
        PrismLogger.i(TAG, "[CHECK] VFS proxy random-access seek");
        boolean pass = PrismVfsProxy.verifyVfsSelfTest();
        PrismLogger.i(TAG, "[CHECK] VFS proxy result: " + (pass ? "PASS" : "FAIL"));
        return pass;
    }

    private static boolean checkVerifiedReplacement(Context context) {
        PrismLogger.i(TAG, "[CHECK] Verified replacement transaction");
        try {
            File cacheDir = context.getCacheDir();
            File testSrc = new File(cacheDir, "inv6_test.raw");
            byte[] sample = new byte[4096];
            for (int i = 0; i < sample.length; i++) sample[i] = (byte) ((i * 13) & 0xFF);
            try (FileOutputStream fos = new FileOutputStream(testSrc)) {
                fos.write(sample);
            }

            VerifiedReplacement.TransactionResult tx = VerifiedReplacement.executeTransaction(testSrc, cacheDir);
            boolean pass = tx.success && tx.finalStage == VerifiedReplacement.Stage.STAGE_4_CLEANUP;

            // Cleanup test files
            testSrc.delete();
            File pvl = new File(cacheDir, "inv6_test.raw.pvloss");
            if (pvl.exists()) pvl.delete();
            File jrn = new File(cacheDir, "inv6_test.raw.journal");
            if (jrn.exists()) jrn.delete();

            PrismLogger.i(TAG, "[CHECK] Verified replacement result: " + (pass ? "PASS" : "FAIL") + " (" + tx.message + ")");
            return pass;
        } catch (Exception e) {
            PrismLogger.e(TAG, "[CHECK] Verified replacement exception: " + e.getMessage());
            return false;
        }
    }

    private static boolean checkSiliconPowerProfile() {
        try {
            com.prismv.engine.SiliconFusedPipelineEngine.SiliconPowerProfile profile =
                    com.prismv.engine.SiliconFusedPipelineEngine.getSiliconPowerProfile(3840, 2160);
            boolean ok = (profile != null && profile.totalSocPowerWatts < 1.000f && profile.dramBandwidthGbps <= 2.0f);
            PrismLogger.i(TAG, "[CHECK] Silicon Power Profile: " + (profile != null ? profile.totalSocPowerWatts : -1) +
                    " W, DRAM: " + (profile != null ? profile.dramBandwidthGbps : -1) + " GB/s -> " + (ok ? "PASS" : "FAIL"));
            return ok;
        } catch (Throwable t) {
            PrismLogger.e(TAG, "[CHECK] Silicon Power Profile error: " + t.getMessage());
            return false;
        }
    }

    private static boolean checkHeterogeneousResourceGovernor() {
        PrismLogger.i(TAG, "[CHECK] Heterogeneous Resource Governor & Thread Affinity");
        try {
            boolean passGov = com.prismv.engine.SiliconFusedPipelineEngine.setResourceConstraints(256, 1, true, 1);
            boolean passAff = com.prismv.engine.SiliconFusedPipelineEngine.applyThreadAffinity(Runtime.getRuntime().availableProcessors());
            boolean pass = passGov && passAff;
            PrismLogger.i(TAG, "[CHECK] Governor result: " + (pass ? "PASS" : "FAIL"));
            return pass;
        } catch (Throwable t) {
            PrismLogger.e(TAG, "[CHECK] Governor test error: " + t.getMessage());
            return false;
        }
    }

    private static boolean checkTilePoolBound() {
        PrismLogger.i(TAG, "[CHECK] TilePool <512MB Budget Enforced");
        try {
            long[] stats = com.prismv.engine.SiliconFusedPipelineEngine.getTilePoolStats();
            boolean pass = (stats != null && stats.length >= 2 && stats[1] <= 512L * 1024L * 1024L);
            PrismLogger.i(TAG, "[CHECK] TilePool result: " + (pass ? "PASS" : "FAIL"));
            return pass;
        } catch (Throwable t) {
            PrismLogger.e(TAG, "[CHECK] TilePool test error: " + t.getMessage());
            return false;
        }
    }

    private void runSampleCompression(Context context) {
        try {
            File inFile = new File("/sdcard/sample_video/sample_action_reel.mp4");
            if (!inFile.exists() || inFile.length() == 0) {
                inFile = new File("/sdcard/sample_video/sample.mp4");
            }
            if (!inFile.exists() || inFile.length() == 0) {
                File inDir = new File("/storage/emulated/0/DCIM/Camera");
                if (inDir.exists() && inDir.isDirectory()) {
                    File[] files = inDir.listFiles((dir, name) -> name.toLowerCase(Locale.US).endsWith(".mp4"));
                    if (files != null && files.length > 0) inFile = files[0];
                }
            }
            if (!inFile.exists() || inFile.length() == 0) {
                PrismLogger.e(TAG, "[SAMPLE-COMPRESS] Source sample video not found on device.");
                return;
            }

            File outDir = new File("/storage/emulated/0/Movies/Prism-V");
            if (!outDir.exists()) outDir.mkdirs();
            File outFile = new File(outDir, "sample_action_reel_compressed.mp4");
            if (outFile.exists()) outFile.delete();

            PrismLogger.i(TAG, "[SAMPLE-COMPRESS] Ingesting " + inFile.getAbsolutePath() + " (" + (inFile.length() / (1024 * 1024)) + " MB)");

            HardwareVideoCompressor.CompressionOptions options = new HardwareVideoCompressor.CompressionOptions("H-ARCH");
            options.targetWidth = 1080;
            options.targetHeight = 1920;
            options.targetFps = 30;
            options.targetBitrateKbps = 3200;
            options.aspectRatio = "No Change";
            options.outputFormat = "MP4";
            options.audioChannel = "Stereo";
            options.audioBitrateKbps = 320;

            boolean ok = HardwareVideoCompressor.compress(
                    inFile.getAbsolutePath(),
                    outFile.getAbsolutePath(),
                    options,
                    new HardwareVideoCompressor.TranscodeListener() {
                        private int lastLoggedPct = -1;

                        @Override
                        public void onProgress(int percent, long currentBytes, long totalBytes) {
                            if (percent % 10 == 0 && percent != lastLoggedPct) {
                                lastLoggedPct = percent;
                                PrismLogger.i(TAG, "[SAMPLE-COMPRESS] Progress: " + percent + "%");
                            }
                        }

                        @Override
                        public void onLog(String message) {
                            PrismLogger.d(TAG, "[SAMPLE-COMPRESS] " + message);
                        }
                    }
            );

            if (ok && outFile.exists() && outFile.length() > 0) {
                PrismLogger.i(TAG, ">>> SAMPLE COMPRESSION SUCCESSFUL! Out: " + outFile.getAbsolutePath() + " (" + (outFile.length() / (1024 * 1024)) + " MB) <<<");
                PrismStorageManager.indexVideoInMediaStore(context, outFile);

                CompressionHistoryManager.get(context).recordCompression(
                        inFile.getName(),
                        Uri.fromFile(inFile),
                        inFile.getAbsolutePath(),
                        inFile.length(),
                        outFile.getAbsolutePath(),
                        outFile.length(),
                        "H-ARCH",
                        1080,
                        1920,
                        3200,
                        30,
                        60000L
                );
                PrismLogger.i(TAG, ">>> RECORDED IN COMPRESSION HISTORY DATASTORE! <<<");
            } else {
                PrismLogger.e(TAG, ">>> SAMPLE COMPRESSION FAILED! <<<");
            }
        } catch (Exception e) {
            PrismLogger.e(TAG, "[SAMPLE-COMPRESS] Error: " + e.getMessage(), e);
        }
    }

    private void runSiliconSampleCompression(Context context) {
        try {
            PrismLogger.i(TAG, "[SILICON-COMPRESS] Starting Frontier 5 Silicon-Fused Heterogeneous Transcode...");
            File inDir = new File("/storage/emulated/0/DCIM/Camera");
            File inFile = null;
            if (inDir.exists() && inDir.isDirectory()) {
                File[] files = inDir.listFiles((dir, name) -> name.toLowerCase(java.util.Locale.US).endsWith(".mp4"));
                if (files != null && files.length > 0) {
                    inFile = files[0];
                }
            }
            if (inFile == null || !inFile.exists()) {
                File movDir = new File("/storage/emulated/0/Movies/Prism-V");
                if (movDir.exists() && movDir.isDirectory()) {
                    File[] files = movDir.listFiles((dir, name) -> name.toLowerCase(java.util.Locale.US).endsWith(".mp4"));
                    if (files != null && files.length > 0) inFile = files[0];
                }
            }
            if (inFile == null || !inFile.exists()) {
                PrismLogger.w(TAG, "[SILICON-COMPRESS] No sample MP4 available on device.");
                return;
            }

            File outDir = new File("/storage/emulated/0/Movies/Prism-V");
            if (!outDir.exists()) outDir.mkdirs();
            File outFile = new File(outDir, "sample_silicon_compressed.mp4");
            if (outFile.exists()) outFile.delete();

            PrismLogger.i(TAG, "[SILICON-COMPRESS] Ingesting " + inFile.getAbsolutePath() + " (" + (inFile.length() / (1024 * 1024)) + " MB)");

            HardwareVideoCompressor.CompressionOptions options = new HardwareVideoCompressor.CompressionOptions("F5-SILICON");
            options.targetWidth = 1080;
            options.targetHeight = 1920;
            options.targetFps = 30;
            options.targetBitrateKbps = 1800; // Frontier 5 target
            options.aspectRatio = "No Change";
            options.outputFormat = "MP4";
            options.audioChannel = "Stereo";
            options.audioBitrateKbps = 192;

            boolean ok = HardwareVideoCompressor.compress(
                    inFile.getAbsolutePath(),
                    outFile.getAbsolutePath(),
                    options,
                    new HardwareVideoCompressor.TranscodeListener() {
                        @Override
                        public void onProgress(int percent, long currentBytes, long totalBytes) {
                            if (percent % 20 == 0) {
                                PrismLogger.i(TAG, "[SILICON-COMPRESS] Progress: " + percent + "%");
                            }
                        }

                        @Override
                        public void onLog(String message) {
                            PrismLogger.d(TAG, "[SILICON-COMPRESS] " + message);
                        }
                    }
            );

            if (ok && outFile.exists() && outFile.length() > 0) {
                PrismLogger.i(TAG, ">>> SILICON COMPRESSION SUCCESSFUL! Out: " + outFile.getAbsolutePath() + " (" + (outFile.length() / (1024 * 1024)) + " MB) <<<");
                PrismStorageManager.indexVideoInMediaStore(context, outFile);

                CompressionHistoryManager.get(context).recordCompression(
                        inFile.getName(),
                        Uri.fromFile(inFile),
                        inFile.getAbsolutePath(),
                        inFile.length(),
                        outFile.getAbsolutePath(),
                        outFile.length(),
                        "F5-SILICON",
                        1080,
                        1920,
                        1800,
                        30,
                        60000L
                );
            }
        } catch (Exception e) {
            PrismLogger.e(TAG, "[SILICON-COMPRESS] Error: " + e.getMessage(), e);
        }
    }
}
