package com.prismv;

import android.app.Activity;
import android.app.Application;
import android.content.ComponentCallbacks2;
import android.content.res.Configuration;
import android.os.Bundle;

import com.prismv.logging.PrismLogger;
import com.prismv.ui.PrismPreferences;

/**
 * PrismApplication:
 * Global Application entry point for Prism-V.
 *
 * Responsibilities:
 * 1. Initializes PrismLogger before any Activity, Service, JobService, Receiver, or Provider runs.
 * 2. Installs a global Thread.UncaughtExceptionHandler to capture fatal crashes (with full stack
 *    traces, active thread dump, memory state, and recent log tail) synchronously to disk.
 * 3. Registers ActivityLifecycleCallbacks to log all Activity lifecycle transitions.
 * 4. Logs low-memory / trim-memory signals and configuration changes.
 */
public class PrismApplication extends Application {
    private static final String TAG = "PrismApplication";

    @Override
    public void onCreate() {
        super.onCreate();

        PrismLogger.init(this);
        PrismLogger.i(TAG, "Application.onCreate() initialized. Process started.");

        // Centralized native library initialization with safe exception isolation
        com.prismv.engine.NativeBridge.init();

        // Warm up and cache device hardware capabilities profile
        try {
            com.prismv.engine.DeviceProfile.get(this);
        } catch (Throwable t) {
            PrismLogger.w(TAG, "Failed to initialize DeviceProfile: " + t.getMessage());
        }

        // Keep verbosity synced with user preference changes
        try {
            PrismPreferences prefs = PrismPreferences.get(this);
            PrismLogger.setVerbosity(prefs.getLoggingLevel());
            prefs.registerListener(key -> {
                if (PrismPreferences.KEY_LOGGING_LEVEL.equals(key)) {
                    PrismLogger.setVerbosity(prefs.getLoggingLevel());
                }
            });
        } catch (Throwable t) {
            PrismLogger.w(TAG, "Failed to bind logging verbosity listener: " + t.getMessage(), t);
        }

        // Global Uncaught Exception Handler (captures crashes across ALL threads)
        final Thread.UncaughtExceptionHandler defaultHandler = Thread.getDefaultUncaughtExceptionHandler();
        Thread.setDefaultUncaughtExceptionHandler((thread, throwable) -> {
            try {
                PrismLogger.recordCrash(thread, throwable);
            } catch (Throwable ignored) {}
            if (defaultHandler != null) {
                defaultHandler.uncaughtException(thread, throwable);
            }
        });

        // Automatic Activity Lifecycle Logging
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
                PrismLogger.i("Lifecycle", activity.getClass().getSimpleName() + ".onCreate (restored=" + (savedInstanceState != null) + ")");
            }

            @Override
            public void onActivityStarted(Activity activity) {
                PrismLogger.d("Lifecycle", activity.getClass().getSimpleName() + ".onStart");
            }

            @Override
            public void onActivityResumed(Activity activity) {
                PrismLogger.i("Lifecycle", activity.getClass().getSimpleName() + ".onResume");
            }

            @Override
            public void onActivityPaused(Activity activity) {
                PrismLogger.d("Lifecycle", activity.getClass().getSimpleName() + ".onPause");
            }

            @Override
            public void onActivityStopped(Activity activity) {
                PrismLogger.d("Lifecycle", activity.getClass().getSimpleName() + ".onStop");
                PrismLogger.flushSync();
            }

            @Override
            public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
                PrismLogger.v("Lifecycle", activity.getClass().getSimpleName() + ".onSaveInstanceState");
            }

            @Override
            public void onActivityDestroyed(Activity activity) {
                PrismLogger.i("Lifecycle", activity.getClass().getSimpleName() + ".onDestroy (isFinishing=" + activity.isFinishing() + ")");
                PrismLogger.flushSync();
            }
        });
    }

    @Override
    public void onLowMemory() {
        super.onLowMemory();
        PrismLogger.w(TAG, "System onLowMemory() callback received! Memory pressure critical.");
        PrismLogger.flushSync();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        String name;
        switch (level) {
            case ComponentCallbacks2.TRIM_MEMORY_RUNNING_CRITICAL: name = "RUNNING_CRITICAL"; break;
            case ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW: name = "RUNNING_LOW"; break;
            case ComponentCallbacks2.TRIM_MEMORY_RUNNING_MODERATE: name = "RUNNING_MODERATE"; break;
            case ComponentCallbacks2.TRIM_MEMORY_UI_HIDDEN: name = "UI_HIDDEN"; break;
            case ComponentCallbacks2.TRIM_MEMORY_BACKGROUND: name = "BACKGROUND"; break;
            case ComponentCallbacks2.TRIM_MEMORY_MODERATE: name = "MODERATE"; break;
            case ComponentCallbacks2.TRIM_MEMORY_COMPLETE: name = "COMPLETE"; break;
            default: name = "LEVEL_" + level; break;
        }
        if (level >= ComponentCallbacks2.TRIM_MEMORY_RUNNING_LOW) {
            PrismLogger.w(TAG, "onTrimMemory(" + name + ", level=" + level + ")");
        } else {
            PrismLogger.d(TAG, "onTrimMemory(" + name + ", level=" + level + ")");
        }
        PrismLogger.flushSync();
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);
        PrismLogger.i(TAG, "onConfigurationChanged: orientation=" + newConfig.orientation
                + " screenWidthDp=" + newConfig.screenWidthDp
                + " screenHeightDp=" + newConfig.screenHeightDp
                + " uiMode=" + newConfig.uiMode);
    }
}
