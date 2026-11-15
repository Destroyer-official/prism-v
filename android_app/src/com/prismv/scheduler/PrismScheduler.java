package com.prismv.scheduler;

import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.os.BatteryManager;
import android.os.Build;
import android.os.PowerManager;

import java.util.Calendar;

/**
 * User-configurable background scheduler and hardware gating policy.
 * Gives the user complete control over when and how background compression runs.
 */
public final class PrismScheduler {
    private static final String PREF_NAME = "prism_scheduler_prefs";

    public static final String KEY_AUTO_ENABLED = "auto_compression_enabled";
    public static final String KEY_REQ_CHARGING = "require_charging";
    public static final String KEY_PAUSE_LOW_BATT = "pause_low_battery";
    public static final String KEY_SCHEDULE_MODE = "schedule_mode"; // "IDLE", "NIGHT", "CONTINUOUS"

    public static class ScheduleStatus {
        public final boolean canRunNow;
        public final int batteryLevel;
        public final boolean isCharging;
        public final boolean isAutoEnabled;
        public final String scheduleMode;
        public final String statusMessage;

        public ScheduleStatus(boolean canRunNow, int batteryLevel, boolean isCharging,
                              boolean isAutoEnabled, String scheduleMode, String statusMessage) {
            this.canRunNow = canRunNow;
            this.batteryLevel = batteryLevel;
            this.isCharging = isCharging;
            this.isAutoEnabled = isAutoEnabled;
            this.scheduleMode = scheduleMode;
            this.statusMessage = statusMessage;
        }
    }

    private PrismScheduler() {}

    private static SharedPreferences getPrefs(Context context) {
        return context.getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
    }

    public static boolean isAutoEnabled(Context context) {
        return getPrefs(context).getBoolean(KEY_AUTO_ENABLED, true);
    }

    public static void setAutoEnabled(Context context, boolean enabled) {
        getPrefs(context).edit().putBoolean(KEY_AUTO_ENABLED, enabled).apply();
    }

    public static boolean isRequireCharging(Context context) {
        return getPrefs(context).getBoolean(KEY_REQ_CHARGING, true);
    }

    public static void setRequireCharging(Context context, boolean req) {
        getPrefs(context).edit().putBoolean(KEY_REQ_CHARGING, req).apply();
    }

    public static boolean isPauseLowBattery(Context context) {
        return getPrefs(context).getBoolean(KEY_PAUSE_LOW_BATT, true);
    }

    public static void setPauseLowBattery(Context context, boolean pause) {
        getPrefs(context).edit().putBoolean(KEY_PAUSE_LOW_BATT, pause).apply();
    }

    public static String getScheduleMode(Context context) {
        return getPrefs(context).getString(KEY_SCHEDULE_MODE, "IDLE");
    }

    public static void setScheduleMode(Context context, String mode) {
        getPrefs(context).edit().putString(KEY_SCHEDULE_MODE, mode).apply();
    }

    public static int getBatteryLevel(Context context) {
        try {
            IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
            Intent batteryStatus = context.registerReceiver(null, ifilter);
            if (batteryStatus == null) return 100;
            int level = batteryStatus.getIntExtra(BatteryManager.EXTRA_LEVEL, -1);
            int scale = batteryStatus.getIntExtra(BatteryManager.EXTRA_SCALE, -1);
            if (level >= 0 && scale > 0) {
                return (int) ((level / (float) scale) * 100);
            }
        } catch (Exception ignored) {}
        return 100;
    }

    public static boolean isCharging(Context context) {
        try {
            IntentFilter ifilter = new IntentFilter(Intent.ACTION_BATTERY_CHANGED);
            Intent batteryStatus = context.registerReceiver(null, ifilter);
            if (batteryStatus == null) return false;
            int status = batteryStatus.getIntExtra(BatteryManager.EXTRA_STATUS, -1);
            return status == BatteryManager.BATTERY_STATUS_CHARGING ||
                    status == BatteryManager.BATTERY_STATUS_FULL;
        } catch (Exception ignored) {}
        return false;
    }

    public static ScheduleStatus evaluateStatus(Context context) {
        boolean autoEnabled = isAutoEnabled(context);
        boolean reqCharging = isRequireCharging(context);
        boolean pauseLowBatt = isPauseLowBattery(context);
        String mode = getScheduleMode(context);

        int battery = getBatteryLevel(context);
        boolean charging = isCharging(context);

        if (!autoEnabled) {
            return new ScheduleStatus(false, battery, charging, false, mode, "Background compression paused by user (Toggle is OFF)");
        }

        if (reqCharging && !charging) {
            return new ScheduleStatus(false, battery, charging, true, mode, "Waiting for charger connection (Policy: Require Charging)");
        }

        if (pauseLowBatt && battery < 30 && !charging) {
            return new ScheduleStatus(false, battery, charging, true, mode, "Battery low (" + battery + "% < 30%). Paused to conserve power.");
        }

        if ("NIGHT".equals(mode)) {
            int hour = Calendar.getInstance().get(Calendar.HOUR_OF_DAY);
            if (hour < 2 || hour >= 6) {
                return new ScheduleStatus(false, battery, charging, true, mode, "Outside Night Window (2:00 AM - 6:00 AM). Ready.");
            }
        }

        return new ScheduleStatus(true, battery, charging, true, mode, "Ready to process atomic GOPs (All conditions satisfied)");
    }
}
