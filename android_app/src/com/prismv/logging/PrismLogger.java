package com.prismv.logging;

import android.app.ActivityManager;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.PackageInfo;
import android.database.Cursor;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.net.Uri;
import android.os.BatteryManager;
import android.os.Build;
import android.os.Debug;
import android.os.Environment;
import android.os.PowerManager;
import android.os.Process;
import android.os.StatFs;
import android.provider.MediaStore;
import android.system.Os;
import android.system.OsConstants;
import android.util.DisplayMetrics;
import android.util.Log;

import com.prismv.engine.HardwareRamBufferPool;
import com.prismv.engine.VideoProcessingPipeline;
import com.prismv.history.CompressionHistoryManager;
import com.prismv.provider.PrismFileProvider;
import com.prismv.scheduler.PrismScheduler;
import com.prismv.storage.PrismWorkJournal;
import com.prismv.ui.PrismPreferences;

import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.nio.charset.StandardCharsets;
import java.text.SimpleDateFormat;
import java.util.ArrayDeque;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.List;
import java.util.Locale;
import java.util.Map;
import java.util.TimeZone;
import java.util.UUID;
import java.util.concurrent.LinkedBlockingQueue;
import java.util.concurrent.TimeUnit;
import java.util.regex.Pattern;

/**
 * PrismLogger:
 * Full-system persistent diagnostic logger & crash recorder for Prism-V.
 *
 * Captures logs from ALL components (UI, MediaCodec, Audio, FGS, JobScheduler,
 * Storage, WorkJournal, Thermal, AI Copilot, VFS, BBR/rANS, JNI, Lifecycle)
 * into:
 *   1. Android Logcat (for real-time adb debugging)
 *   2. In-memory ring buffer (for instant in-app Live Log Viewer)
 *   3. Rotating persistent log files (<filesDir>/prism_logs/prism_current.log + 3 archives)
 *   4. Dedicated crash dumps (<filesDir>/prism_logs/crashes/crash_*.log)
 *
 * Exports a single comprehensive diagnostic report to public Downloads/Prism-V/
 * (and shareable via Android Share Sheet) containing device specs, MediaCodec
 * inventory, settings snapshot, WorkJournal, history, crash dumps, process logcat,
 * and full multi-session component logs.
 */
public final class PrismLogger {
    private static final String LOG_TAG = "PrismLogger";
    public static final String LOGS_SUBDIR = "prism_logs";
    public static final String CRASHES_SUBDIR = "crashes";
    private static final String CURRENT_LOG_NAME = "prism_current.log";
    private static final long MAX_LOG_FILE_BYTES = 4L * 1024L * 1024L; // 4 MB per file
    private static final int MAX_ROTATED_FILES = 3; // current + 3 rotated = up to 16 MB
    private static final int MAX_CRASH_FILES = 10;
    private static final int RING_BUFFER_CAPACITY = 2500;

    private static final Pattern API_KEY_PATTERN = Pattern.compile("(key=)([A-Za-z0-9_\\-]{10,})");
    private static final Pattern AI_KEY_PREFIX_PATTERN = Pattern.compile("AIza[A-Za-z0-9_\\-]{20,}");

    private static volatile Context appContext;
    private static volatile boolean initialized = false;
    private static volatile String sessionId = UUID.randomUUID().toString().substring(0, 8);
    private static volatile long sessionStartMs = System.currentTimeMillis();
    private static volatile String verbosityLevel = "Standard"; // Standard, Verbose, Debug

    private static File logDir;
    private static File crashDir;
    private static File currentLogFile;

    private static final Object RING_LOCK = new Object();
    private static final ArrayDeque<String> ringBuffer = new ArrayDeque<>(RING_BUFFER_CAPACITY + 64);

    private static final Object FILE_LOCK = new Object();
    private static final LinkedBlockingQueue<String> writeQueue = new LinkedBlockingQueue<>(8192);
    private static volatile Thread writerThread;
    private static volatile boolean writerRunning = false;

    public static class LogStats {
        public long totalBytes;
        public int logFileCount;
        public int crashCount;
        public int sessionLines;
        public String verbosity;
        public String logDirPath;
    }

    public static class ExportResult {
        public boolean success;
        public File shareableFile;
        public String publicDisplayPath;
        public Uri publicDownloadUri;
        public long sizeBytes;
        public int totalLines;
        public int crashCount;
        public String errorMessage;
    }

    private PrismLogger() {}

    public static Context getAppContext() {
        return appContext;
    }

    public static synchronized void init(Context context) {
        if (context == null) return;
        appContext = context.getApplicationContext();

        if (!initialized) {
            sessionStartMs = System.currentTimeMillis();
            sessionId = UUID.randomUUID().toString().substring(0, 8);

            logDir = new File(appContext.getFilesDir(), LOGS_SUBDIR);
            if (!logDir.exists()) {
                logDir.mkdirs();
            }
            crashDir = new File(logDir, CRASHES_SUBDIR);
            if (!crashDir.exists()) {
                crashDir.mkdirs();
            }
            currentLogFile = new File(logDir, CURRENT_LOG_NAME);

            try {
                verbosityLevel = PrismPreferences.get(appContext).getLoggingLevel();
            } catch (Throwable ignored) {
                verbosityLevel = "Standard";
            }

            startWriterThread();
            initialized = true;

            String banner = String.format(Locale.US,
                    "================ PRISM-V SESSION START [sid=%s, pid=%d, api=%d, device=%s %s, nativeJni=%s, verbosity=%s] ================",
                    sessionId, Process.myPid(), Build.VERSION.SDK_INT,
                    safe(Build.MANUFACTURER), safe(Build.MODEL),
                    VideoProcessingPipeline.isNativeLoaded, verbosityLevel);
            logInternal("I", "PrismLogger", banner, null);
        }
    }

    public static void setVerbosity(String level) {
        if (level == null || level.trim().isEmpty()) return;
        String prev = verbosityLevel;
        verbosityLevel = level.trim();
        i("PrismLogger", "Logging verbosity changed: " + prev + " -> " + verbosityLevel);
    }

    public static String getVerbosity() {
        return verbosityLevel;
    }

    public static boolean isVerboseEnabled() {
        return "Verbose".equalsIgnoreCase(verbosityLevel) || "Debug".equalsIgnoreCase(verbosityLevel);
    }

    public static void v(String tag, String msg) {
        if (isVerboseEnabled()) {
            logInternal("V", tag, msg, null);
        }
    }

    public static void v(String tag, String msg, Throwable tr) {
        if (isVerboseEnabled()) {
            logInternal("V", tag, msg, tr);
        }
    }

    public static void d(String tag, String msg) {
        logInternal("D", tag, msg, null);
    }

    public static void d(String tag, String msg, Throwable tr) {
        logInternal("D", tag, msg, tr);
    }

    public static void i(String tag, String msg) {
        logInternal("I", tag, msg, null);
    }

    public static void i(String tag, String msg, Throwable tr) {
        logInternal("I", tag, msg, tr);
    }

    public static void w(String tag, String msg) {
        logInternal("W", tag, msg, null);
    }

    public static void w(String tag, String msg, Throwable tr) {
        logInternal("W", tag, msg, tr);
    }

    public static void e(String tag, String msg) {
        logInternal("E", tag, msg, null);
    }

    public static void e(String tag, String msg, Throwable tr) {
        logInternal("E", tag, msg, tr);
    }

    public static void wtf(String tag, String msg, Throwable tr) {
        logInternal("F", tag, msg, tr);
    }

    public static void event(String component, String action, String details) {
        String msg = details != null && !details.isEmpty()
                ? ("[" + action + "] " + details)
                : ("[" + action + "]");
        logInternal("I", component, msg, null);
    }

    private static void logInternal(String level, String tag, String msg, Throwable tr) {
        String cleanTag = (tag == null || tag.isEmpty()) ? "PrismV" : tag;
        String cleanMsg = redactSecrets(msg != null ? msg : "");

        // 1. Forward to Android Logcat safely (also safe on desktop JVM unit tests)
        try {
            switch (level) {
                case "V":
                    if (tr != null) Log.v(cleanTag, cleanMsg, tr); else Log.v(cleanTag, cleanMsg);
                    break;
                case "D":
                    if (tr != null) Log.d(cleanTag, cleanMsg, tr); else Log.d(cleanTag, cleanMsg);
                    break;
                case "I":
                    if (tr != null) Log.i(cleanTag, cleanMsg, tr); else Log.i(cleanTag, cleanMsg);
                    break;
                case "W":
                    if (tr != null) Log.w(cleanTag, cleanMsg, tr); else Log.w(cleanTag, cleanMsg);
                    break;
                case "E":
                case "F":
                    if (tr != null) Log.e(cleanTag, cleanMsg, tr); else Log.e(cleanTag, cleanMsg);
                    break;
                default:
                    Log.i(cleanTag, cleanMsg);
                    break;
            }
        } catch (Throwable ignored) {
            // Non-Android JVM fallback
        }

        // 2. Format structured line
        String ts = formatTimestamp(System.currentTimeMillis());
        Thread t = Thread.currentThread();
        String threadLabel = "TID:" + t.getId() + "/" + t.getName();
        StringBuilder sb = new StringBuilder(cleanMsg.length() + 96);
        sb.append(ts).append(" [").append(level).append("] [").append(threadLabel)
                .append("] [").append(cleanTag).append("] ").append(cleanMsg);

        if (tr != null) {
            sb.append('\n').append(redactSecrets(stackTraceToString(tr)));
        }

        String line = sb.toString();

        // 3. Append to in-memory ring buffer
        synchronized (RING_LOCK) {
            if (ringBuffer.size() >= RING_BUFFER_CAPACITY) {
                ringBuffer.pollFirst();
            }
            ringBuffer.addLast(line);
        }

        // 4. Enqueue for persistent disk write
        if (initialized) {
            if ("E".equals(level) || "F".equals(level)) {
                // Immediate synchronous write on errors/fatals so nothing is lost on crash
                writeLineDirect(line);
            } else {
                if (!writeQueue.offer(line)) {
                    // Queue full: drop oldest queued item and enqueue current
                    writeQueue.poll();
                    writeQueue.offer(line);
                }
            }
        }
    }

    private static void startWriterThread() {
        if (writerRunning) return;
        writerRunning = true;
        writerThread = new Thread(() -> {
            List<String> batch = new ArrayList<>(64);
            while (writerRunning) {
                try {
                    String first = writeQueue.poll(1000, TimeUnit.MILLISECONDS);
                    if (first == null) continue;
                    batch.clear();
                    batch.add(first);
                    writeQueue.drainTo(batch, 63);
                    writeBatchDirect(batch);
                } catch (InterruptedException ignored) {
                    if (!writerRunning) break;
                } catch (Throwable t) {
                    try {
                        Log.e(LOG_TAG, "Background log writer error: " + t.getMessage());
                    } catch (Throwable ignored2) {}
                }
            }
        }, "PrismLogger-Writer");
        writerThread.setDaemon(true);
        writerThread.setPriority(Thread.MIN_PRIORITY + 1);
        writerThread.start();
    }

    private static void writeLineDirect(String line) {
        if (currentLogFile == null) return;
        synchronized (FILE_LOCK) {
            try {
                rotateIfNeededLocked();
                try (BufferedWriter bw = new BufferedWriter(
                        new OutputStreamWriter(new FileOutputStream(currentLogFile, true), StandardCharsets.UTF_8))) {
                    bw.write(line);
                    bw.newLine();
                    bw.flush();
                }
            } catch (Throwable ignored) {}
        }
    }

    private static void writeBatchDirect(List<String> lines) {
        if (currentLogFile == null || lines == null || lines.isEmpty()) return;
        synchronized (FILE_LOCK) {
            try {
                rotateIfNeededLocked();
                try (BufferedWriter bw = new BufferedWriter(
                        new OutputStreamWriter(new FileOutputStream(currentLogFile, true), StandardCharsets.UTF_8))) {
                    for (String l : lines) {
                        bw.write(l);
                        bw.newLine();
                    }
                    bw.flush();
                }
            } catch (Throwable ignored) {}
        }
    }

    private static void rotateIfNeededLocked() {
        if (currentLogFile == null || !currentLogFile.exists()) return;
        if (currentLogFile.length() < MAX_LOG_FILE_BYTES) return;
        try {
            File oldest = new File(logDir, "prism_" + MAX_ROTATED_FILES + ".log");
            if (oldest.exists()) oldest.delete();
            for (int i = MAX_ROTATED_FILES - 1; i >= 1; i--) {
                File src = new File(logDir, "prism_" + i + ".log");
                File dst = new File(logDir, "prism_" + (i + 1) + ".log");
                if (src.exists()) src.renameTo(dst);
            }
            File firstArchive = new File(logDir, "prism_1.log");
            currentLogFile.renameTo(firstArchive);
        } catch (Throwable ignored) {}
    }

    /**
     * Synchronously flushes all queued log lines to disk.
     */
    public static void flushSync() {
        if (!initialized) return;
        List<String> pending = new ArrayList<>();
        writeQueue.drainTo(pending);
        if (!pending.isEmpty()) {
            writeBatchDirect(pending);
        }
    }

    /**
     * Records a fatal uncaught exception synchronously with full stack trace,
     * thread dump, memory stats, and recent log tail before process termination.
     */
    public static void recordCrash(Thread crashedThread, Throwable throwable) {
        try {
            String tsFile = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
            String tsHuman = formatTimestamp(System.currentTimeMillis());
            String threadName = crashedThread != null ? crashedThread.getName() : "unknown";
            long threadId = crashedThread != null ? crashedThread.getId() : -1;

            StringBuilder sb = new StringBuilder(8192);
            sb.append("==================== PRISM-V FATAL CRASH REPORT ====================\n");
            sb.append("Crash Time:   ").append(tsHuman).append('\n');
            sb.append("Session ID:   ").append(sessionId).append('\n');
            sb.append("PID:          ").append(Process.myPid()).append('\n');
            sb.append("Thread:       ").append(threadName).append(" (id=").append(threadId).append(")\n");
            sb.append("Device:       ").append(safe(Build.MANUFACTURER)).append(' ').append(safe(Build.MODEL))
                    .append(" (Android ").append(safe(Build.VERSION.RELEASE))
                    .append(", API ").append(Build.VERSION.SDK_INT).append(")\n");

            Runtime rt = Runtime.getRuntime();
            long usedHeapMb = (rt.totalMemory() - rt.freeMemory()) / (1024L * 1024L);
            long maxHeapMb = rt.maxMemory() / (1024L * 1024L);
            long nativeHeapMb = Debug.getNativeHeapAllocatedSize() / (1024L * 1024L);
            sb.append("Memory:       JVM ").append(usedHeapMb).append('/').append(maxHeapMb)
                    .append(" MB | Native ").append(nativeHeapMb).append(" MB | RamPool ")
                    .append(HardwareRamBufferPool.getGlobalAllocatedBytes() / (1024L * 1024L)).append(" MB\n\n");

            sb.append("--- EXCEPTION STACK TRACE ---\n");
            sb.append(redactSecrets(stackTraceToString(throwable))).append('\n');

            sb.append("\n--- LAST 120 LOG LINES BEFORE CRASH ---\n");
            List<String> tail = getRecentLines(120, null);
            for (String line : tail) {
                sb.append(line).append('\n');
            }

            sb.append("\n--- ALL ACTIVE THREADS DUMP ---\n");
            try {
                Map<Thread, StackTraceElement[]> allStacks = Thread.getAllStackTraces();
                for (Map.Entry<Thread, StackTraceElement[]> entry : allStacks.entrySet()) {
                    Thread t = entry.getKey();
                    sb.append("\"").append(t.getName()).append("\" id=").append(t.getId())
                            .append(" state=").append(t.getState()).append('\n');
                    for (StackTraceElement ste : entry.getValue()) {
                        sb.append("    at ").append(ste.toString()).append('\n');
                    }
                }
            } catch (Throwable ignored) {}
            sb.append("====================================================================\n");

            String crashBody = sb.toString();

            // Flush existing queue first
            flushSync();

            // Append summary to main log
            logInternal("F", "UncaughtCrash", "FATAL CRASH on thread " + threadName + ": " +
                    (throwable != null ? throwable.toString() : "null"), throwable);

            // Save dedicated crash file
            if (crashDir != null) {
                if (!crashDir.exists()) crashDir.mkdirs();
                File crashFile = new File(crashDir, "crash_" + tsFile + ".log");
                try (FileOutputStream fos = new FileOutputStream(crashFile)) {
                    fos.write(crashBody.getBytes(StandardCharsets.UTF_8));
                    fos.flush();
                    fos.getFD().sync();
                }
                pruneOldCrashFiles();
            }
        } catch (Throwable ignored) {}
    }

    private static void pruneOldCrashFiles() {
        if (crashDir == null || !crashDir.exists()) return;
        File[] crashes = crashDir.listFiles();
        if (crashes == null || crashes.length <= MAX_CRASH_FILES) return;
        Arrays.sort(crashes, (a, b) -> Long.compare(b.lastModified(), a.lastModified()));
        for (int i = MAX_CRASH_FILES; i < crashes.length; i++) {
            crashes[i].delete();
        }
    }

    /**
     * Returns recent lines from the in-memory ring buffer, optionally filtered by level ("E", "W", "I", "D", "V").
     */
    public static List<String> getRecentLines(int maxLines, String levelFilter) {
        List<String> result = new ArrayList<>();
        synchronized (RING_LOCK) {
            for (String line : ringBuffer) {
                if (levelFilter == null || levelFilter.isEmpty() || "ALL".equalsIgnoreCase(levelFilter)) {
                    result.add(line);
                } else {
                    String token = " [" + levelFilter.toUpperCase(Locale.US) + "] ";
                    if ("E".equalsIgnoreCase(levelFilter)) {
                        if (line.contains(" [E] ") || line.contains(" [F] ")) result.add(line);
                    } else if ("W".equalsIgnoreCase(levelFilter)) {
                        if (line.contains(" [W] ") || line.contains(" [E] ") || line.contains(" [F] ")) result.add(line);
                    } else if (line.contains(token)) {
                        result.add(line);
                    }
                }
            }
        }
        if (maxLines > 0 && result.size() > maxLines) {
            return new ArrayList<>(result.subList(result.size() - maxLines, result.size()));
        }
        return result;
    }

    /**
     * Returns summary stats for Settings / Diagnostics UI.
     */
    public static LogStats getStats(Context context) {
        if (!initialized && context != null) init(context);
        flushSync();
        LogStats stats = new LogStats();
        stats.verbosity = verbosityLevel;
        stats.logDirPath = logDir != null ? logDir.getAbsolutePath() : "";
        synchronized (RING_LOCK) {
            stats.sessionLines = ringBuffer.size();
        }
        if (logDir != null && logDir.exists()) {
            File[] files = logDir.listFiles();
            if (files != null) {
                for (File f : files) {
                    if (f.isFile()) {
                        stats.logFileCount++;
                        stats.totalBytes += f.length();
                    }
                }
            }
        }
        if (crashDir != null && crashDir.exists()) {
            File[] crashes = crashDir.listFiles();
            if (crashes != null) {
                for (File f : crashes) {
                    if (f.isFile()) {
                        stats.crashCount++;
                        stats.totalBytes += f.length();
                    }
                }
            }
        }
        return stats;
    }

    /**
     * Clears all stored log files, crash dumps, and ring buffer.
     */
    public static void clearAllLogs(Context context) {
        if (!initialized && context != null) init(context);
        flushSync();
        synchronized (RING_LOCK) {
            ringBuffer.clear();
        }
        synchronized (FILE_LOCK) {
            if (logDir != null && logDir.exists()) {
                File[] files = logDir.listFiles();
                if (files != null) {
                    for (File f : files) {
                        if (f.isFile()) f.delete();
                    }
                }
            }
            if (crashDir != null && crashDir.exists()) {
                File[] crashes = crashDir.listFiles();
                if (crashes != null) {
                    for (File f : crashes) {
                        if (f.isFile()) f.delete();
                    }
                }
            }
        }
        // Also clear old exported reports in external files dir
        try {
            if (context != null) {
                File extLogDir = new File(context.getExternalFilesDir(null), LOGS_SUBDIR);
                if (extLogDir.exists() && extLogDir.isDirectory()) {
                    File[] exports = extLogDir.listFiles();
                    if (exports != null) {
                        for (File f : exports) {
                            if (f.isFile()) f.delete();
                        }
                    }
                }
            }
        } catch (Throwable ignored) {}

        i("PrismLogger", "All previous diagnostic logs and crash dumps cleared by user.");
    }

    /**
     * Generates a complete diagnostic log report covering ALL components, device/codec info,
     * preferences, SQLite work journal, compression history, crash reports, process logcat,
     * and full rotating log files, then saves it to public Downloads/Prism-V/ AND a shareable copy.
     */
    public static ExportResult exportFullDiagnosticLog(Context context) {
        ExportResult result = new ExportResult();
        if (context == null) {
            result.success = false;
            result.errorMessage = "Context is null";
            return result;
        }
        if (!initialized) init(context);

        i("PrismLogger", "Starting full diagnostic log export...");
        flushSync();

        String tsFile = new SimpleDateFormat("yyyyMMdd_HHmmss", Locale.US).format(new Date());
        String fileName = "Prism-V_Full_Logs_" + tsFile + ".txt";

        try {
            // Prepare shareable file in external app files dir (always accessible to PrismFileProvider)
            File extBase = context.getExternalFilesDir(null);
            File shareDir = (extBase != null) ? new File(extBase, LOGS_SUBDIR) : new File(context.getFilesDir(), LOGS_SUBDIR);
            if (!shareDir.exists()) shareDir.mkdirs();
            File shareFile = new File(shareDir, fileName);

            int totalLines = 0;
            int crashCount = 0;

            try (BufferedWriter writer = new BufferedWriter(
                    new OutputStreamWriter(new FileOutputStream(shareFile), StandardCharsets.UTF_8), 65536)) {

                // 0. Header
                totalLines += writeSectionHeader(writer, "PRISM-V FULL DIAGNOSTIC LOG REPORT");
                totalLines += writeLine(writer, "Export Time:      " + formatTimestamp(System.currentTimeMillis())
                        + " (" + TimeZone.getDefault().getID() + ")");
                totalLines += writeLine(writer, "Session ID:       " + sessionId);
                totalLines += writeLine(writer, "Session Started:  " + formatTimestamp(sessionStartMs));
                totalLines += writeLine(writer, "Process PID:      " + Process.myPid());
                totalLines += writeLine(writer, "Package:          " + context.getPackageName());
                try {
                    PackageInfo pi = context.getPackageManager().getPackageInfo(context.getPackageName(), 0);
                    totalLines += writeLine(writer, "App Version:      " + safe(pi.versionName)
                            + " (versionCode=" + (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ? pi.getLongVersionCode() : pi.versionCode) + ")");
                } catch (Throwable ignored) {}
                totalLines += writeLine(writer, "Native JNI Lib:   " + (VideoProcessingPipeline.isNativeLoaded ? "LOADED (libprism_jni.so)" : "NOT LOADED (Java/MediaCodec path)"));
                totalLines += writeLine(writer, "Logging Level:    " + verbosityLevel);
                totalLines += writeLine(writer, "");

                // 1. Device & Hardware Environment
                totalLines += writeSectionHeader(writer, "1. DEVICE & HARDWARE ENVIRONMENT");
                totalLines += writeLine(writer, "Manufacturer:     " + safe(Build.MANUFACTURER));
                totalLines += writeLine(writer, "Brand:            " + safe(Build.BRAND));
                totalLines += writeLine(writer, "Model:            " + safe(Build.MODEL));
                totalLines += writeLine(writer, "Device / Product: " + safe(Build.DEVICE) + " / " + safe(Build.PRODUCT));
                totalLines += writeLine(writer, "Hardware / Board: " + safe(Build.HARDWARE) + " / " + safe(Build.BOARD));
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
                    totalLines += writeLine(writer, "SoC:              " + safe(Build.SOC_MANUFACTURER) + " " + safe(Build.SOC_MODEL));
                }
                totalLines += writeLine(writer, "Android OS:       " + safe(Build.VERSION.RELEASE) + " (API " + Build.VERSION.SDK_INT + ")");
                totalLines += writeLine(writer, "Security Patch:   " + safe(Build.VERSION.SECURITY_PATCH));
                totalLines += writeLine(writer, "Build Fingerprint:" + safe(Build.FINGERPRINT));
                totalLines += writeLine(writer, "Supported ABIs:   " + String.join(", ", Build.SUPPORTED_ABIS));
                totalLines += writeLine(writer, "CPU Cores:        " + Runtime.getRuntime().availableProcessors());
                try {
                    long pageSize = Os.sysconf(OsConstants._SC_PAGESIZE);
                    totalLines += writeLine(writer, "Page Size:        " + pageSize + " bytes");
                } catch (Throwable t) {
                    totalLines += writeLine(writer, "Page Size:        unavailable (" + t.getMessage() + ")");
                }
                try {
                    DisplayMetrics dm = context.getResources().getDisplayMetrics();
                    totalLines += writeLine(writer, "Display:          " + dm.widthPixels + "x" + dm.heightPixels
                            + " @ " + dm.densityDpi + " dpi (density=" + dm.density + ")");
                } catch (Throwable ignored) {}
                totalLines += writeLine(writer, "Locale:           " + Locale.getDefault().toString());
                totalLines += writeLine(writer, "");

                // 2. Memory, Storage, Battery & Thermal
                totalLines += writeSectionHeader(writer, "2. MEMORY, STORAGE, BATTERY & THERMAL SNAPSHOT");
                try {
                    ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
                    if (am != null) {
                        ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
                        am.getMemoryInfo(mi);
                        totalLines += writeLine(writer, String.format(Locale.US,
                                "Device RAM:       %.2f GB total | %.2f GB available | threshold %.0f MB | lowMemory=%s",
                                mi.totalMem / (1024.0 * 1024.0 * 1024.0),
                                mi.availMem / (1024.0 * 1024.0 * 1024.0),
                                mi.threshold / (1024.0 * 1024.0),
                                mi.lowMemory));
                        totalLines += writeLine(writer, "Memory Class:     " + am.getMemoryClass()
                                + " MB (large=" + am.getLargeMemoryClass() + " MB)");
                    }
                } catch (Throwable t) {
                    totalLines += writeLine(writer, "Device RAM Error: " + t.getMessage());
                }
                Runtime rt = Runtime.getRuntime();
                totalLines += writeLine(writer, String.format(Locale.US,
                        "JVM Heap:         %.1f MB used / %.1f MB allocated / %.1f MB max",
                        (rt.totalMemory() - rt.freeMemory()) / (1024.0 * 1024.0),
                        rt.totalMemory() / (1024.0 * 1024.0),
                        rt.maxMemory() / (1024.0 * 1024.0)));
                totalLines += writeLine(writer, String.format(Locale.US,
                        "Native Heap:      %.1f MB allocated / %.1f MB size | RamBufferPool=%.1f MB",
                        Debug.getNativeHeapAllocatedSize() / (1024.0 * 1024.0),
                        Debug.getNativeHeapSize() / (1024.0 * 1024.0),
                        HardwareRamBufferPool.getGlobalAllocatedBytes() / (1024.0 * 1024.0)));

                totalLines += writeStorageLine(writer, "Internal Files:  ", context.getFilesDir());
                totalLines += writeStorageLine(writer, "External Files:  ", context.getExternalFilesDir(null));
                totalLines += writeStorageLine(writer, "Public Movies:   ", Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MOVIES));

                try {
                    Intent bi = context.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
                    if (bi != null) {
                        int level = bi.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
                        int scale = bi.getIntExtra(BatteryManager.EXTRA_SCALE, 100);
                        int pct = scale > 0 ? (level * 100 / scale) : -1;
                        float tempC = bi.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 0) / 10.0f;
                        int status = bi.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
                        boolean charging = status == BatteryManager.BATTERY_STATUS_CHARGING || status == BatteryManager.BATTERY_STATUS_FULL;
                        totalLines += writeLine(writer, String.format(Locale.US,
                                "Battery:          %d%% | charging=%s | temp=%.1f C", pct, charging, tempC));
                    }
                } catch (Throwable ignored) {}

                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                    try {
                        PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
                        if (pm != null) {
                            int tStatus = pm.getCurrentThermalStatus();
                            String headroomStr = "N/A";
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                                try {
                                    headroomStr = String.format(Locale.US, "%.3f", pm.getThermalHeadroom(10));
                                } catch (Throwable ignored2) {}
                            }
                            totalLines += writeLine(writer, "Thermal:          status=" + tStatus
                                    + " | ADPF headroom(10s)=" + headroomStr
                                    + " | powerSaveMode=" + pm.isPowerSaveMode());
                        }
                    } catch (Throwable ignored) {}
                }
                totalLines += writeLine(writer, "");

                // 3. Hardware & Software MediaCodec Inventory
                totalLines += writeSectionHeader(writer, "3. MEDIACODEC INVENTORY (VIDEO & AUDIO)");
                try {
                    MediaCodecList codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
                    MediaCodecInfo[] infos = codecList.getCodecInfos();
                    totalLines += writeLine(writer, "Total codecs reported: " + infos.length);
                    for (MediaCodecInfo info : infos) {
                        String[] types = info.getSupportedTypes();
                        boolean relevant = false;
                        for (String t : types) {
                            if (t.startsWith("video/") || t.startsWith("audio/")) {
                                relevant = true;
                                break;
                            }
                        }
                        if (!relevant) continue;
                        String role = info.isEncoder() ? "ENC" : "DEC";
                        String hw = "";
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                            hw = info.isHardwareAccelerated() ? "HW" : (info.isSoftwareOnly() ? "SW" : "HYB");
                        }
                        totalLines += writeLine(writer, String.format(Locale.US,
                                "  [%s][%s] %-40s -> %s", role, hw, info.getName(), Arrays.toString(types)));
                    }
                } catch (Throwable t) {
                    totalLines += writeLine(writer, "MediaCodecList error: " + t.getMessage());
                }
                totalLines += writeLine(writer, "");

                // 4. Active App Settings Snapshot
                totalLines += writeSectionHeader(writer, "4. USER PREFERENCES & SCHEDULER SNAPSHOT");
                try {
                    PrismPreferences p = PrismPreferences.get(context);
                    totalLines += writeLine(writer, "  designSkin:            " + p.getDesignSkin());
                    totalLines += writeLine(writer, "  personaTier:           " + p.getPersonaTier());
                    totalLines += writeLine(writer, "  hapticLevel:           " + p.getHapticLevel());
                    totalLines += writeLine(writer, "  fileSizeUnit:          " + p.getFileSizeUnit());
                    totalLines += writeLine(writer, "  defaultCompMode:       " + p.getDefaultCompressionMode());
                    totalLines += writeLine(writer, "  codecPriority:         " + p.getCodecPriority());
                    totalLines += writeLine(writer, "  neuralResidual:        " + p.isNeuralResidualEnabled());
                    totalLines += writeLine(writer, "  filmGrain:             " + p.isFilmGrainEnabled());
                    totalLines += writeLine(writer, "  upscaleGuard:          " + p.isUpscaleGuardEnabled());
                    totalLines += writeLine(writer, "  hdrToneMapping:        " + p.getHdrToneMappingCurve());
                    totalLines += writeLine(writer, "  audioAction:           " + p.getAudioAction());
                    totalLines += writeLine(writer, "  defaultAudioBitrate:   " + p.getDefaultAudioBitrate() + " kbps");
                    totalLines += writeLine(writer, "  loudnessTarget:        " + p.getBroadcastLoudnessTarget());
                    totalLines += writeLine(writer, "  windSuppression:       " + p.isWindSuppressionEnabled());
                    totalLines += writeLine(writer, "  outputDirectory:       " + p.getOutputDirectory());
                    totalLines += writeLine(writer, "  replacementBehavior:   " + p.getReplacementBehavior());
                    totalLines += writeLine(writer, "  fileNamingRule:        " + p.getFileNamingRule());
                    totalLines += writeLine(writer, "  preserveTimestamps:    " + p.isPreserveTimestamps());
                    totalLines += writeLine(writer, "  ramPercent:            " + p.getRamPercent() + "% (" + p.getRamBudgetMb() + " MB)");
                    totalLines += writeLine(writer, "  cpuPercent:            " + p.getCpuPercent() + "%");
                    totalLines += writeLine(writer, "  gpuTurbo:              " + p.isGpuTurboEnabled());
                    totalLines += writeLine(writer, "  powerProfile:          " + p.getPowerProfile());
                    totalLines += writeLine(writer, "  lowBatteryPause:       " + p.isLowBatteryPauseEnabled());
                    totalLines += writeLine(writer, "  thermalThrottleC:      " + p.getThermalThrottleThresholdC() + " C");
                    totalLines += writeLine(writer, "  backgroundFgs:         " + p.isBackgroundFgsEnabled());
                    totalLines += writeLine(writer, "  chargingButler:        " + p.isChargingButlerMode());
                    totalLines += writeLine(writer, "  socialPrivacyShield:   " + p.isSocialPrivacyShield());
                    totalLines += writeLine(writer, "  stripDeviceSerial:     " + p.isStripDeviceSerial());
                    totalLines += writeLine(writer, "  biometricVault:        " + p.isBiometricVaultEnabled());
                    totalLines += writeLine(writer, "  screenSecurityFlag:    " + p.isScreenSecurityFlag());
                    totalLines += writeLine(writer, "  hotFolderWatcher:      " + p.isHotFolderWatcherEnabled());
                    totalLines += writeLine(writer, "  shareInterceptor:      " + p.isShareInterceptorEnabled());
                    totalLines += writeLine(writer, "  statusBarChip:         " + p.isStatusBarChipEnabled());
                    totalLines += writeLine(writer, "  completionChime:       " + p.isCompletionChimeEnabled());
                    totalLines += writeLine(writer, "  forceSimdPath:         " + p.getForceSimdPath());
                    totalLines += writeLine(writer, "  loggingLevel:          " + p.getLoggingLevel());
                    String apiKey = p.getGeminiApiKey();
                    totalLines += writeLine(writer, "  geminiApiKey:          " + (apiKey != null && !apiKey.isEmpty() ? "<SET len=" + apiKey.length() + ">" : "<EMPTY>"));
                    PrismScheduler.ScheduleStatus sched = PrismScheduler.evaluateStatus(context);
                    totalLines += writeLine(writer, "  schedulerStatus:       canRun=" + sched.canRunNow
                            + " mode=" + sched.scheduleMode + " batt=" + sched.batteryLevel + "% charging=" + sched.isCharging
                            + " msg=" + sched.statusMessage);
                } catch (Throwable t) {
                    totalLines += writeLine(writer, "Preferences snapshot error: " + t.getMessage());
                }
                totalLines += writeLine(writer, "");

                // 5. SQLite Work Journal
                totalLines += writeSectionHeader(writer, "5. SQLITE WORK JOURNAL (RECENT JOBS)");
                try {
                    PrismWorkJournal journal = PrismWorkJournal.get(context);
                    List<PrismWorkJournal.JournalEntry> entries = journal.getRecentJobs(50);
                    totalLines += writeLine(writer, "Total recent journal entries: " + entries.size());
                    for (PrismWorkJournal.JournalEntry je : entries) {
                        totalLines += writeLine(writer, String.format(Locale.US,
                                "  [%s] id=%s pct=%d%% gop=%d ptsUs=%d updated=%s in=%s out=%s err=%s",
                                safe(je.status), safe(je.jobId), je.percent, je.gopIndex, je.ptsCheckpointUs,
                                formatTimestamp(je.lastUpdatedMs), safe(je.inputPath), safe(je.outputPath),
                                je.errorMessage != null ? je.errorMessage : "none"));
                    }
                } catch (Throwable t) {
                    totalLines += writeLine(writer, "WorkJournal query error: " + t.getMessage());
                }
                totalLines += writeLine(writer, "");

                // 6. Compression History
                totalLines += writeSectionHeader(writer, "6. COMPRESSION HISTORY SUMMARY");
                try {
                    List<CompressionHistoryManager.Record> records = CompressionHistoryManager.get(context).getRecords();
                    totalLines += writeLine(writer, "Total history records: " + records.size());
                    int limit = Math.min(records.size(), 50);
                    for (int idx = 0; idx < limit; idx++) {
                        CompressionHistoryManager.Record r = records.get(idx);
                        totalLines += writeLine(writer, String.format(Locale.US,
                                "  #%d [%s] %s | mode=%s %dx%d %dfps %dkbps | %.2f MB -> %.2f MB (-%.1f%%) | outExists=%s | out=%s",
                                idx + 1, safe(r.formattedDate), safe(r.title), safe(r.mode),
                                r.width, r.height, r.fps, r.bitrateKbps,
                                r.originalSizeBytes / (1024.0 * 1024.0),
                                r.compressedSizeBytes / (1024.0 * 1024.0),
                                r.percentReduction, r.isCompressedFileExisting(), safe(r.compressedPath)));
                    }
                } catch (Throwable t) {
                    totalLines += writeLine(writer, "History snapshot error: " + t.getMessage());
                }
                totalLines += writeLine(writer, "");

                // 7. Saved Crash Reports
                totalLines += writeSectionHeader(writer, "7. SAVED FATAL CRASH REPORTS");
                if (crashDir != null && crashDir.exists()) {
                    File[] crashes = crashDir.listFiles();
                    if (crashes != null && crashes.length > 0) {
                        Arrays.sort(crashes, (a, b) -> Long.compare(a.lastModified(), b.lastModified()));
                        for (File cf : crashes) {
                            if (!cf.isFile()) continue;
                            crashCount++;
                            totalLines += writeLine(writer, "--- CRASH FILE: " + cf.getName() + " (" + cf.length() + " bytes) ---");
                            totalLines += appendFileContents(writer, cf);
                            totalLines += writeLine(writer, "");
                        }
                    } else {
                        totalLines += writeLine(writer, "(No fatal crashes recorded)");
                    }
                } else {
                    totalLines += writeLine(writer, "(No crash directory)");
                }
                totalLines += writeLine(writer, "");

                // 8. Live Process Logcat Capture (captures Android MediaCodec / C2 / Stagefright / JNI / ART)
                totalLines += writeSectionHeader(writer, "8. ANDROID SYSTEM LOGCAT FOR THIS PROCESS (PID=" + Process.myPid() + ")");
                totalLines += appendProcessLogcat(writer);
                totalLines += writeLine(writer, "");

                // 9. Full Persistent Component Logs (oldest rotated -> newest -> current)
                totalLines += writeSectionHeader(writer, "9. FULL PERSISTENT COMPONENT LOGS (ALL SESSIONS)");
                synchronized (FILE_LOCK) {
                    for (int idx = MAX_ROTATED_FILES; idx >= 1; idx--) {
                        File rotated = new File(logDir, "prism_" + idx + ".log");
                        if (rotated.exists() && rotated.length() > 0) {
                            totalLines += writeLine(writer, "--- ARCHIVE: " + rotated.getName() + " (" + rotated.length() + " bytes) ---");
                            totalLines += appendFileContents(writer, rotated);
                        }
                    }
                    if (currentLogFile != null && currentLogFile.exists() && currentLogFile.length() > 0) {
                        totalLines += writeLine(writer, "--- CURRENT SESSION LOG: " + currentLogFile.getName()
                                + " (" + currentLogFile.length() + " bytes) ---");
                        totalLines += appendFileContents(writer, currentLogFile);
                    }
                }

                totalLines += writeLine(writer, "");
                totalLines += writeLine(writer, "================ END OF PRISM-V DIAGNOSTIC REPORT ================");
                writer.flush();
            }

            // Copy to public Downloads/Prism-V/ so users can easily find & send it from their Files app
            String publicPath = saveCopyToPublicDownloads(context, shareFile, fileName, result);

            result.success = true;
            result.shareableFile = shareFile;
            result.publicDisplayPath = publicPath;
            result.sizeBytes = shareFile.length();
            result.totalLines = totalLines;
            result.crashCount = crashCount;

            i("PrismLogger", String.format(Locale.US,
                    "Full diagnostic log exported successfully: file=%s, publicPath=%s, size=%.1f KB, lines=%d, crashes=%d",
                    shareFile.getAbsolutePath(), publicPath, shareFile.length() / 1024.0, totalLines, crashCount));

        } catch (Throwable t) {
            e("PrismLogger", "Failed to export full diagnostic log: " + t.getMessage(), t);
            result.success = false;
            result.errorMessage = t.getMessage() != null ? t.getMessage() : t.getClass().getSimpleName();
        }
        return result;
    }

    private static String saveCopyToPublicDownloads(Context context, File sourceFile, String fileName, ExportResult result) {
        // Path A: Android 10+ (API 29+) MediaStore.Downloads (works with zero storage permissions!)
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            try {
                ContentResolver resolver = context.getContentResolver();
                ContentValues values = new ContentValues();
                values.put(MediaStore.Downloads.DISPLAY_NAME, fileName);
                values.put(MediaStore.Downloads.MIME_TYPE, "text/plain");
                values.put(MediaStore.Downloads.RELATIVE_PATH, Environment.DIRECTORY_DOWNLOADS + "/Prism-V");
                values.put(MediaStore.Downloads.IS_PENDING, 1);

                Uri uri = resolver.insert(MediaStore.Downloads.EXTERNAL_CONTENT_URI, values);
                if (uri != null) {
                    try (FileInputStream fis = new FileInputStream(sourceFile);
                         OutputStream os = resolver.openOutputStream(uri)) {
                        if (os != null) {
                            byte[] buf = new byte[32768];
                            int len;
                            while ((len = fis.read(buf)) > 0) {
                                os.write(buf, 0, len);
                            }
                            os.flush();
                        }
                    }
                    ContentValues done = new ContentValues();
                    done.put(MediaStore.Downloads.IS_PENDING, 0);
                    resolver.update(uri, done, null, null);
                    result.publicDownloadUri = uri;
                    return "Downloads/Prism-V/" + fileName;
                }
            } catch (Throwable t) {
                w("PrismLogger", "MediaStore.Downloads save note (trying fallback): " + t.getMessage());
            }
        }

        // Path B: Direct public Downloads/Prism-V/ folder (API 26-28 or MANAGE_EXTERNAL_STORAGE)
        try {
            File dlDir = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS), "Prism-V");
            if (!dlDir.exists()) dlDir.mkdirs();
            File dst = new File(dlDir, fileName);
            try (FileInputStream fis = new FileInputStream(sourceFile);
                 FileOutputStream fos = new FileOutputStream(dst)) {
                byte[] buf = new byte[32768];
                int len;
                while ((len = fis.read(buf)) > 0) {
                    fos.write(buf, 0, len);
                }
                fos.flush();
            }
            return "Downloads/Prism-V/" + fileName;
        } catch (Throwable t) {
            w("PrismLogger", "Direct Downloads folder save fallback: " + t.getMessage());
        }

        // Path C: App external files path (always succeeds)
        return sourceFile.getAbsolutePath();
    }

    /**
     * Opens the Android Share Sheet to send the exported full log file via WhatsApp, Telegram, Email, Drive, etc.
     */
    public static boolean shareLogFile(Context context, ExportResult export) {
        if (context == null || export == null || export.shareableFile == null || !export.shareableFile.exists()) {
            return false;
        }
        try {
            Uri uri = PrismFileProvider.getUriForFile(context, PrismFileProvider.AUTHORITY, export.shareableFile);
            Intent share = new Intent(Intent.ACTION_SEND);
            share.setType("text/plain");
            share.putExtra(Intent.EXTRA_STREAM, uri);
            share.putExtra(Intent.EXTRA_SUBJECT,
                    "Prism-V Diagnostic Logs (" + safe(Build.MANUFACTURER) + " " + safe(Build.MODEL) + ")");
            share.putExtra(Intent.EXTRA_TEXT,
                    "Attached are my Prism-V full diagnostic logs (" + export.shareableFile.getName()
                            + ", " + String.format(Locale.US, "%.1f KB", export.sizeBytes / 1024.0)
                            + ").\n\nPlease describe what happened right before exporting these logs:");
            share.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION | Intent.FLAG_ACTIVITY_NEW_TASK);
            Intent chooser = Intent.createChooser(share, "Send Prism-V Full Logs");
            chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            context.startActivity(chooser);
            i("PrismLogger", "Opened Android Share Sheet for log file: " + export.shareableFile.getName());
            return true;
        } catch (Throwable t) {
            e("PrismLogger", "Failed to share log file: " + t.getMessage(), t);
            return false;
        }
    }

    private static int appendProcessLogcat(BufferedWriter writer) {
        int lines = 0;
        java.lang.Process proc = null;
        try {
            int pid = Process.myPid();
            String[] cmd = new String[]{"logcat", "-d", "-v", "threadtime", "--pid=" + pid, "-t", "1500"};
            proc = Runtime.getRuntime().exec(cmd);
            try (BufferedReader br = new BufferedReader(new InputStreamReader(proc.getInputStream(), StandardCharsets.UTF_8))) {
                String line;
                while ((line = br.readLine()) != null) {
                    writer.write(redactSecrets(line));
                    writer.newLine();
                    lines++;
                }
            }
            proc.waitFor(3, TimeUnit.SECONDS);
            if (lines == 0) {
                // Fallback for older devices that do not support --pid=
                proc.destroy();
                proc = Runtime.getRuntime().exec(new String[]{"logcat", "-d", "-v", "threadtime", "-t", "1000"});
                try (BufferedReader br = new BufferedReader(new InputStreamReader(proc.getInputStream(), StandardCharsets.UTF_8))) {
                    String line;
                    while ((line = br.readLine()) != null) {
                        writer.write(redactSecrets(line));
                        writer.newLine();
                        lines++;
                    }
                }
                proc.waitFor(3, TimeUnit.SECONDS);
            }
        } catch (Throwable t) {
            lines += writeLine(writer, "(Logcat capture note: " + t.getMessage() + ")");
        } finally {
            if (proc != null) {
                try { proc.destroy(); } catch (Throwable ignored) {}
            }
        }
        return lines;
    }

    private static int appendFileContents(BufferedWriter writer, File file) {
        int lines = 0;
        try (BufferedReader br = new BufferedReader(new InputStreamReader(new FileInputStream(file), StandardCharsets.UTF_8))) {
            String line;
            while ((line = br.readLine()) != null) {
                writer.write(redactSecrets(line));
                writer.newLine();
                lines++;
            }
        } catch (Throwable t) {
            lines += writeLine(writer, "(Error reading " + file.getName() + ": " + t.getMessage() + ")");
        }
        return lines;
    }

    private static int writeStorageLine(BufferedWriter writer, String label, File dir) {
        try {
            if (dir == null) return writeLine(writer, label + " <null>");
            if (!dir.exists()) return writeLine(writer, label + " " + dir.getAbsolutePath() + " (does not exist)");
            StatFs sf = new StatFs(dir.getAbsolutePath());
            double availGb = sf.getAvailableBytes() / (1024.0 * 1024.0 * 1024.0);
            double totalGb = sf.getTotalBytes() / (1024.0 * 1024.0 * 1024.0);
            return writeLine(writer, String.format(Locale.US, "%s %.2f GB free / %.2f GB total (%s)",
                    label, availGb, totalGb, dir.getAbsolutePath()));
        } catch (Throwable t) {
            return writeLine(writer, label + " error: " + t.getMessage());
        }
    }

    private static int writeSectionHeader(BufferedWriter writer, String title) {
        int count = 0;
        count += writeLine(writer, "========================================================================");
        count += writeLine(writer, " " + title);
        count += writeLine(writer, "========================================================================");
        return count;
    }

    private static int writeLine(BufferedWriter writer, String text) {
        try {
            writer.write(text != null ? text : "");
            writer.newLine();
            return 1;
        } catch (Throwable ignored) {
            return 0;
        }
    }

    public static String redactSecrets(String input) {
        if (input == null || input.isEmpty()) return "";
        String out = API_KEY_PATTERN.matcher(input).replaceAll("$1[REDACTED]");
        out = AI_KEY_PREFIX_PATTERN.matcher(out).replaceAll("[REDACTED_KEY]");
        return out;
    }

    private static String stackTraceToString(Throwable tr) {
        if (tr == null) return "";
        StringWriter sw = new StringWriter();
        PrintWriter pw = new PrintWriter(sw);
        tr.printStackTrace(pw);
        pw.flush();
        return sw.toString();
    }

    private static String formatTimestamp(long timeMs) {
        return new SimpleDateFormat("yyyy-MM-dd HH:mm:ss.SSS", Locale.US).format(new Date(timeMs));
    }

    private static String safe(String s) {
        return (s == null || s.isEmpty()) ? "unknown" : s;
    }
}
