package com.prismv.test;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.util.Log;

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
        Log.i(TAG, "=======================================================");
        Log.i(TAG, ">>> RECEIVED INTENT ACTION: " + action + " <<<");
        Log.i(TAG, "=======================================================");

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
                    Log.i(TAG, "=======================================================");
                    if (allPassed) {
                        Log.i(TAG, ">>> ALL DEVICE SELF-CHECKS PASSED (7/7) <<<");
                    } else {
                        Log.e(TAG, ">>> DEVICE SELF-CHECKS FAILED <<<");
                    }
                    Log.i(TAG, "=======================================================");
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

        boolean totalPass = pass1 && pass2 && pass3 && pass4 && pass5 && pass6 && pass7;

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
            json.append("    \"silicon_sub_watt_profile\": ").append(pass7).append("\n");
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
            Log.i(TAG, "[REPORT] Written audit JSON to " + outFile.getAbsolutePath());
        } catch (Exception e) {
            Log.w(TAG, "[REPORT] Could not write audit JSON: " + e.getMessage());
        }

        return totalPass;
    }

    private static boolean checkBbrRoundTrip() {
        Log.i(TAG, "[CHECK] BBR lossless round-trip");
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
        Log.i(TAG, String.format("[CHECK] BBR result: %s (structured bit-exact=%b, ratio=%.2fx | noise bit-exact=%b, %d B)",
                pass ? "PASS" : "FAIL", res1.bitExactMatch, (1.0 / res1.ratio), res2.bitExactMatch, res2.compressedData.length));
        return pass;
    }

    private static boolean checkDetIntDeterminism() {
        Log.i(TAG, "[CHECK] DetInt fixed-point determinism");
        boolean pass = DetInt.verifyDeterminismSelfTest();
        Log.i(TAG, "[CHECK] DetInt result: " + (pass ? "PASS" : "FAIL"));
        return pass;
    }

    private static boolean checkThermalGearTable() {
        Log.i(TAG, "[CHECK] Thermal gear table stays within declared ceiling");
        ThermalGearShifter shifter = new ThermalGearShifter();
        boolean pass = shifter.isWithinThermodynamicCeiling();
        Log.i(TAG, String.format("[CHECK] Thermal table result: %s (gear %s, declared %.3f J/frame, ceiling 0.050)",
                pass ? "PASS" : "FAIL", shifter.getCurrentGear().name(), shifter.getCurrentGear().estJoulesPerFrame));
        return pass;
    }

    private static boolean checkGopPayloadRoundTrip() {
        Log.i(TAG, "[CHECK] GOP-sized payload round-trip");
        byte[] gop = new byte[8192];
        for (int i = 0; i < gop.length; i++) gop[i] = (byte) (i & 0x7F);
        BbrCodec.CompressionResult res = BbrCodec.testBitExactRoundTrip(gop);
        boolean pass = res.bitExactMatch;
        Log.i(TAG, "[CHECK] GOP payload result: " + (pass ? "PASS" : "FAIL"));
        return pass;
    }

    private static boolean checkVfsProxySeek() {
        Log.i(TAG, "[CHECK] VFS proxy random-access seek");
        boolean pass = PrismVfsProxy.verifyVfsSelfTest();
        Log.i(TAG, "[CHECK] VFS proxy result: " + (pass ? "PASS" : "FAIL"));
        return pass;
    }

    private static boolean checkVerifiedReplacement(Context context) {
        Log.i(TAG, "[CHECK] Verified replacement transaction");
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

            Log.i(TAG, "[CHECK] Verified replacement result: " + (pass ? "PASS" : "FAIL") + " (" + tx.message + ")");
            return pass;
        } catch (Exception e) {
            Log.e(TAG, "[CHECK] Verified replacement exception: " + e.getMessage());
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
                Log.e(TAG, "[SAMPLE-COMPRESS] Source sample video not found on /sdcard/sample_video/");
                return;
            }

            File outDir = new File("/storage/emulated/0/Movies/Prism-V");
            if (!outDir.exists()) outDir.mkdirs();
            File outFile = new File(outDir, "sample_action_reel_compressed.mp4");
            if (outFile.exists()) outFile.delete();

            Log.i(TAG, "[SAMPLE-COMPRESS] Ingesting " + inFile.getAbsolutePath() + " (" + (inFile.length() / (1024 * 1024)) + " MB)");

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
                                Log.i(TAG, "[SAMPLE-COMPRESS] Progress: " + percent + "%");
                            }
                        }

                        @Override
                        public void onLog(String message) {
                            Log.d(TAG, "[SAMPLE-COMPRESS] " + message);
                        }
                    }
            );

            if (ok && outFile.exists() && outFile.length() > 0) {
                Log.i(TAG, ">>> SAMPLE COMPRESSION SUCCESSFUL! Out: " + outFile.getAbsolutePath() + " (" + (outFile.length() / (1024 * 1024)) + " MB) <<<");
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
                Log.i(TAG, ">>> RECORDED IN COMPRESSION HISTORY DATASTORE! <<<");
            } else {
                Log.e(TAG, ">>> SAMPLE COMPRESSION FAILED! <<<");
            }
        } catch (Exception e) {
            Log.e(TAG, "[SAMPLE-COMPRESS] Error: " + e.getMessage(), e);
        }
    }
}
