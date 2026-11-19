package com.prismv.engine;

import com.prismv.logging.PrismLogger;

/**
 * NativeBridge:
 * Single unified facade for loading and safely executing native Prism-V C++ routines.
 * 
 * Guarantees:
 * 1. Safe lifecycle initialization (called from PrismApplication.onCreate).
 * 2. Hardened protection against UnsatisfiedLinkError / NoClassDefFoundError.
 * 3. Graceful degradation: If libprism_jni.so is missing from APK packaging,
 *    isAvailable() returns false and all callers safely fall back to pure-Java paths.
 */
public final class NativeBridge {
    private static final String TAG = "NativeBridge";
    private static volatile boolean sInitialized = false;
    private static volatile boolean sNativeAvailable = false;
    private static String sLoadError = null;

    private NativeBridge() {}

    /**
     * Initializes the native library bridge once at application startup.
     */
    public static synchronized void init() {
        if (sInitialized) return;
        sInitialized = true;

        try {
            System.loadLibrary("prism_jni");
            sNativeAvailable = true;
            sLoadError = null;
            PrismLogger.i(TAG, "✓ Native library 'prism_jni' successfully loaded into process.");
        } catch (Throwable t) {
            sNativeAvailable = false;
            sLoadError = t.getClass().getSimpleName() + ": " + t.getMessage();
            PrismLogger.w(TAG, "⚠ Native library 'prism_jni' unavailable (" + sLoadError + "). Running in Pure-Java / MediaCodec fallback mode.");
        }
    }

    /**
     * Returns true if libprism_jni.so is loaded and ready for JNI calls.
     */
    public static boolean isAvailable() {
        if (!sInitialized) {
            init();
        }
        return sNativeAvailable;
    }

    /**
     * Returns the exact reason why native library loading failed, or null if loaded.
     */
    public static String getLoadError() {
        return sLoadError;
    }
}
