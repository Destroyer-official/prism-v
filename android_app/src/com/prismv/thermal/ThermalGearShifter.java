package com.prismv.thermal;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;
import com.prismv.logging.PrismLogger;
import com.prismv.engine.SiliconFusedPipelineEngine;

/**
 * Thermal Gear Shifter for Android (Android 15/16/17+ ADPF Integration).
 *
 * Enforces Invariant 3: Thermodynamic ceiling (<0.05 J/frame) and
 * dynamically adapts encoder complexity to Android PowerManager thermal status
 * and predictive ADPF thermal headroom forecasting (getThermalHeadroom).
 *
 * Implements 0 ms immediate downshift for hardware safety and 5000 ms hysteresis
 * debounce window for upshifting to prevent rapid gear oscillation.
 */
public final class ThermalGearShifter {
    private static final String TAG = "ThermalGearShifter";
    private static final long DEBOUNCE_UPSHIFT_MS = 5000L;

    public enum Gear {
        GEAR_4_MAX_FIDELITY(4, 0.048, 3.20f, "Full Cool-chic + AFGS1 + BBR"),
        GEAR_3_STANDARD(3, 0.034, 2.10f, "Cool-chic micro-residual + BBR"),
        GEAR_2_ECONOMY(2, 0.022, 1.35f, "Fast micro-residual + HW Base"),
        GEAR_1_CONSERVATIVE(1, 0.012, 0.75f, "Hardware MediaCodec only"),
        GEAR_0_SUSPEND(0, 0.000, 0.05f, "Thermal Pause (Atomic GOP Yield)");

        public final int level;
        public final double estJoulesPerFrame;
        public final float targetSocPowerWatts;
        public final String description;

        Gear(int level, double estJoulesPerFrame, float targetSocPowerWatts, String description) {
            this.level = level;
            this.estJoulesPerFrame = estJoulesPerFrame;
            this.targetSocPowerWatts = targetSocPowerWatts;
            this.description = description;
        }
    }

    private Gear currentGear = Gear.GEAR_3_STANDARD;
    private long lastGearChangeMs = 0;
    private float lastHeadroom = 1.0f;
    private PowerManager.OnThermalStatusChangedListener thermalListener = null;

    /**
     * Registers listener with Android PowerManager on API 29+.
     */
    public synchronized void registerThermalListener(Context context) {
        if (context == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return;
        try {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            if (pm != null && thermalListener == null) {
                thermalListener = status -> mapStatusToGear(status);
                pm.addThermalStatusListener(thermalListener);
                mapStatusToGear(pm.getCurrentThermalStatus());
            }
        } catch (Throwable t) {
            PrismLogger.w(TAG, "Unable to register thermal status listener: " + t.getMessage());
        }
    }

    /**
     * Unregisters thermal status listener.
     */
    public synchronized void unregisterThermalListener(Context context) {
        if (context == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) return;
        try {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            if (pm != null && thermalListener != null) {
                pm.removeThermalStatusListener(thermalListener);
                thermalListener = null;
            }
        } catch (Throwable t) {
            PrismLogger.w(TAG, "Unable to unregister thermal status listener: " + t.getMessage());
        }
    }

    /**
     * Evaluates ADPF thermal headroom forecasting (Android 11+, API 30+).
     * @param context Application context
     * @param forecastSeconds Lookahead window (e.g. 10 seconds)
     * @return Float headroom value (0.0 to 1.5+)
     */
    public float queryThermalHeadroom(Context context, int forecastSeconds) {
        if (context == null || Build.VERSION.SDK_INT < Build.VERSION_CODES.R) {
            return 1.0f;
        }
        try {
            PowerManager pm = (PowerManager) context.getSystemService(Context.POWER_SERVICE);
            if (pm != null) {
                float headroom = pm.getThermalHeadroom(forecastSeconds);
                if (!Float.isNaN(headroom) && headroom >= 0.0f) {
                    lastHeadroom = headroom;
                    updateWithHeadroom(headroom, System.currentTimeMillis());
                    return headroom;
                }
            }
        } catch (Throwable ignored) {}

        // Graceful fallback for custom OEM ROMs (Failure Mode 6):
        // Estimate thermal headroom from battery temperature sensor
        try {
            Intent batteryIntent = context.registerReceiver(null, new IntentFilter(Intent.ACTION_BATTERY_CHANGED));
            if (batteryIntent != null) {
                int tempTenths = batteryIntent.getIntExtra(BatteryManager.EXTRA_TEMPERATURE, 300);
                float tempC = tempTenths / 10.0f;
                // IEC 62368-1 guard ceiling 42.0 C, ambient baseline 25.0 C (delta 17.0 C)
                float estHeadroom = Math.max(0.0f, Math.min(1.5f, 1.0f - ((tempC - 25.0f) / 17.0f)));
                lastHeadroom = estHeadroom;
                updateWithHeadroom(estHeadroom, System.currentTimeMillis());
                return estHeadroom;
            }
        } catch (Throwable ignored) {}

        return lastHeadroom;
    }

    /**
     * Maps Android thermal status integer to operating gear.
     */
    public synchronized Gear mapStatusToGear(int thermalStatus) {
        Gear proposed;
        switch (thermalStatus) {
            case PowerManager.THERMAL_STATUS_NONE:
                proposed = Gear.GEAR_4_MAX_FIDELITY;
                break;
            case PowerManager.THERMAL_STATUS_LIGHT:
                proposed = Gear.GEAR_3_STANDARD;
                break;
            case PowerManager.THERMAL_STATUS_MODERATE:
                proposed = Gear.GEAR_2_ECONOMY;
                break;
            case PowerManager.THERMAL_STATUS_SEVERE:
                proposed = Gear.GEAR_1_CONSERVATIVE;
                break;
            case PowerManager.THERMAL_STATUS_CRITICAL:
            case PowerManager.THERMAL_STATUS_EMERGENCY:
            case PowerManager.THERMAL_STATUS_SHUTDOWN:
                proposed = Gear.GEAR_0_SUSPEND;
                break;
            default:
                proposed = Gear.GEAR_3_STANDARD;
                break;
        }
        applyGearWithHysteresis(proposed, System.currentTimeMillis());
        return currentGear;
    }

    /**
     * Updates operating gear from continuous ADPF thermal headroom metric.
     * @param headroom Forecast headroom metric (1.0 = room temp, 0.0 = throttling)
     * @param nowMs Current timestamp in milliseconds
     */
    public synchronized Gear updateWithHeadroom(float headroom, long nowMs) {
        lastHeadroom = headroom;
        SiliconFusedPipelineEngine.evaluateThermalHeadroom(headroom, nowMs);

        Gear proposed;
        if (headroom > 0.85f) {
            proposed = Gear.GEAR_4_MAX_FIDELITY;
        } else if (headroom > 0.65f) {
            proposed = Gear.GEAR_3_STANDARD;
        } else if (headroom > 0.40f) {
            proposed = Gear.GEAR_2_ECONOMY;
        } else if (headroom > 0.15f) {
            proposed = Gear.GEAR_1_CONSERVATIVE;
        } else {
            proposed = Gear.GEAR_0_SUSPEND;
        }

        applyGearWithHysteresis(proposed, nowMs);
        return currentGear;
    }

    private void applyGearWithHysteresis(Gear proposed, long nowMs) {
        if (lastGearChangeMs == 0) {
            currentGear = proposed;
            lastGearChangeMs = nowMs;
            return;
        }

        boolean isDownshift = proposed.level < currentGear.level;
        boolean isUpshift = proposed.level > currentGear.level;

        if (isDownshift) {
            // Immediate downshift to protect silicon and prevent throttling
            currentGear = proposed;
            lastGearChangeMs = nowMs;
            PrismLogger.i(TAG, "Thermal downshift to: " + currentGear.name() + " (" + currentGear.description + ")");
        } else if (isUpshift) {
            // Require 5000 ms of sustained cooling before stepping up
            if (nowMs - lastGearChangeMs >= DEBOUNCE_UPSHIFT_MS) {
                currentGear = proposed;
                lastGearChangeMs = nowMs;
                PrismLogger.i(TAG, "Thermal upshift to: " + currentGear.name() + " (" + currentGear.description + ")");
            }
        }
    }

    public synchronized Gear getCurrentGear() {
        return currentGear;
    }

    public synchronized boolean isAtomicPaused() {
        return currentGear == Gear.GEAR_0_SUSPEND;
    }

    public synchronized float getLastHeadroom() {
        return lastHeadroom;
    }

    public synchronized boolean isWithinThermodynamicCeiling() {
        return currentGear.estJoulesPerFrame <= 0.050;
    }
}
