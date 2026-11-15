package com.prismv.ui;

import android.app.ActivityManager;
import android.content.Context;
import android.content.SharedPreferences;

import java.util.ArrayList;
import java.util.List;

/**
 * PrismPreferences:
 * Central configuration and persistent state manager for Prism-V.
 * Implements all 9 categories and 52 user-configurable settings from
 * the 2027+ Android UI/UX Architecture Specification (ANDROID_UI_UX_IMPLEMENTATION_PLAN.md).
 */
public final class PrismPreferences {

    private static final String PREF_NAME = "prism_v_preferences";
    private static PrismPreferences instance;
    private final SharedPreferences prefs;

    public interface OnSettingChangedListener {
        void onSettingChanged(String key);
    }

    private final List<OnSettingChangedListener> listeners = new ArrayList<>();

    private PrismPreferences(Context context) {
        prefs = context.getApplicationContext().getSharedPreferences(PREF_NAME, Context.MODE_PRIVATE);
    }

    public static synchronized PrismPreferences get(Context context) {
        if (instance == null) {
            instance = new PrismPreferences(context);
        }
        return instance;
    }

    public void registerListener(OnSettingChangedListener listener) {
        if (!listeners.contains(listener)) listeners.add(listener);
    }

    public void unregisterListener(OnSettingChangedListener listener) {
        listeners.remove(listener);
    }

    private void notifyChanged(String key) {
        for (OnSettingChangedListener l : listeners) {
            l.onSettingChanged(key);
        }
    }

    // ==========================================
    // 1. APPEARANCE & PERSONA
    // ==========================================
    public static final String KEY_FIRST_RUN_COMPLETED = "pref_first_run_completed";
    public static final String KEY_PERSONA_TIER = "pref_persona_tier";
    public static final String KEY_DESIGN_SKIN = "pref_design_skin";
    public static final String KEY_HAPTIC_LEVEL = "pref_haptic_level";
    public static final String KEY_FILE_SIZE_UNIT = "pref_file_size_unit";
    public static final String KEY_APP_LANGUAGE = "pref_app_language";

    public boolean isFirstRun() {
        return !prefs.getBoolean(KEY_FIRST_RUN_COMPLETED, false);
    }

    public void setFirstRunCompleted(boolean completed) {
        prefs.edit().putBoolean(KEY_FIRST_RUN_COMPLETED, completed).apply();
        notifyChanged(KEY_FIRST_RUN_COMPLETED);
    }

    public String getPersonaTier() {
        return prefs.getString(KEY_PERSONA_TIER, "Creator");
    }

    public void setPersonaTier(String tier) {
        prefs.edit().putString(KEY_PERSONA_TIER, tier).apply();
        notifyChanged(KEY_PERSONA_TIER);
    }

    public String getDesignSkin() {
        return prefs.getString(KEY_DESIGN_SKIN, "Liquid Frost Glass");
    }

    public void setDesignSkin(String skin) {
        prefs.edit().putString(KEY_DESIGN_SKIN, skin).apply();
        notifyChanged(KEY_DESIGN_SKIN);
    }

    public String getHapticLevel() {
        return prefs.getString(KEY_HAPTIC_LEVEL, "Medium");
    }

    public void setHapticLevel(String level) {
        prefs.edit().putString(KEY_HAPTIC_LEVEL, level).apply();
        notifyChanged(KEY_HAPTIC_LEVEL);
    }

    public String getFileSizeUnit() {
        return prefs.getString(KEY_FILE_SIZE_UNIT, "Decimal (MB/GB)");
    }

    public void setFileSizeUnit(String unit) {
        prefs.edit().putString(KEY_FILE_SIZE_UNIT, unit).apply();
        notifyChanged(KEY_FILE_SIZE_UNIT);
    }

    // ==========================================
    // 2. DEFAULT VIDEO ENGINE
    // ==========================================
    public static final String KEY_DEFAULT_COMP_MODE = "pref_default_comp_mode";
    public static final String KEY_CODEC_PRIORITY = "pref_codec_priority";
    public static final String KEY_NEURAL_RESIDUAL = "pref_neural_residual";
    public static final String KEY_FILM_GRAIN = "pref_film_grain";
    public static final String KEY_UPSCALE_GUARD = "pref_upscale_guard";
    public static final String KEY_VFR_SANTIZER = "pref_vfr_sanitizer";
    public static final String KEY_HDR_TONE_MAP = "pref_hdr_tone_map";

    public String getDefaultCompressionMode() {
        return prefs.getString(KEY_DEFAULT_COMP_MODE, "H-ARCH");
    }

    public void setDefaultCompressionMode(String mode) {
        prefs.edit().putString(KEY_DEFAULT_COMP_MODE, mode).apply();
        notifyChanged(KEY_DEFAULT_COMP_MODE);
    }

    public String getCodecPriority() {
        return prefs.getString(KEY_CODEC_PRIORITY, "Auto HW (AV1 -> HEVC)");
    }

    public void setCodecPriority(String priority) {
        prefs.edit().putString(KEY_CODEC_PRIORITY, priority).apply();
        notifyChanged(KEY_CODEC_PRIORITY);
    }

    public boolean isNeuralResidualEnabled() {
        return prefs.getBoolean(KEY_NEURAL_RESIDUAL, true);
    }

    public void setNeuralResidualEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_NEURAL_RESIDUAL, enabled).apply();
        notifyChanged(KEY_NEURAL_RESIDUAL);
    }

    public boolean isFilmGrainEnabled() {
        return prefs.getBoolean(KEY_FILM_GRAIN, true);
    }

    public void setFilmGrainEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_FILM_GRAIN, enabled).apply();
        notifyChanged(KEY_FILM_GRAIN);
    }

    public boolean isUpscaleGuardEnabled() {
        return prefs.getBoolean(KEY_UPSCALE_GUARD, true);
    }

    public void setUpscaleGuardEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_UPSCALE_GUARD, enabled).apply();
        notifyChanged(KEY_UPSCALE_GUARD);
    }

    public String getHdrToneMappingCurve() {
        return prefs.getString(KEY_HDR_TONE_MAP, "ACES Filmic (Vibrant SDR)");
    }

    public void setHdrToneMappingCurve(String curve) {
        prefs.edit().putString(KEY_HDR_TONE_MAP, curve).apply();
        notifyChanged(KEY_HDR_TONE_MAP);
    }

    // ==========================================
    // 3. DEFAULT AUDIO PREFERENCES
    // ==========================================
    public static final String KEY_AUDIO_ACTION = "pref_audio_action";
    public static final String KEY_DEFAULT_AUDIO_BITRATE = "pref_default_audio_bitrate";
    public static final String KEY_LOUDNESS_NORMALIZATION = "pref_loudness_normalization";
    public static final String KEY_WIND_SUPPRESSION = "pref_wind_suppression";

    public String getAudioAction() {
        return prefs.getString(KEY_AUDIO_ACTION, "Compress to Opus");
    }

    public void setAudioAction(String action) {
        prefs.edit().putString(KEY_AUDIO_ACTION, action).apply();
        notifyChanged(KEY_AUDIO_ACTION);
    }

    public int getDefaultAudioBitrate() {
        return prefs.getInt(KEY_DEFAULT_AUDIO_BITRATE, 192);
    }

    public void setDefaultAudioBitrate(int kbps) {
        prefs.edit().putInt(KEY_DEFAULT_AUDIO_BITRATE, kbps).apply();
        notifyChanged(KEY_DEFAULT_AUDIO_BITRATE);
    }

    public String getBroadcastLoudnessTarget() {
        return prefs.getString(KEY_LOUDNESS_NORMALIZATION, "-14 LUFS (YouTube/TikTok)");
    }

    public void setBroadcastLoudnessTarget(String target) {
        prefs.edit().putString(KEY_LOUDNESS_NORMALIZATION, target).apply();
        notifyChanged(KEY_LOUDNESS_NORMALIZATION);
    }

    public boolean isWindSuppressionEnabled() {
        return prefs.getBoolean(KEY_WIND_SUPPRESSION, true);
    }

    public void setWindSuppressionEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_WIND_SUPPRESSION, enabled).apply();
        notifyChanged(KEY_WIND_SUPPRESSION);
    }

    // ==========================================
    // 4. STORAGE, DIRECTORY & VAULT POLICIES
    // ==========================================
    public static final String KEY_OUTPUT_DIRECTORY = "pref_output_directory";
    public static final String KEY_REPLACEMENT_BEHAVIOR = "pref_replacement_behavior";
    public static final String KEY_TRASH_PURGE_DAYS = "pref_trash_purge_days";
    public static final String KEY_FILE_NAMING_RULE = "pref_file_naming_rule";
    public static final String KEY_PRESERVE_TIMESTAMPS = "pref_preserve_timestamps";
    public static final String KEY_THUMBNAIL_CACHE_MB = "pref_thumbnail_cache_mb";

    public String getOutputDirectory() {
        return prefs.getString(KEY_OUTPUT_DIRECTORY, "/DCIM/PrismCompressed/");
    }

    public void setOutputDirectory(String dir) {
        prefs.edit().putString(KEY_OUTPUT_DIRECTORY, dir).apply();
        notifyChanged(KEY_OUTPUT_DIRECTORY);
    }

    public String getReplacementBehavior() {
        return prefs.getString(KEY_REPLACEMENT_BEHAVIOR, "Move to 30-Day Trash");
    }

    public void setReplacementBehavior(String behavior) {
        prefs.edit().putString(KEY_REPLACEMENT_BEHAVIOR, behavior).apply();
        notifyChanged(KEY_REPLACEMENT_BEHAVIOR);
    }

    public int getTrashPurgeDays() {
        return prefs.getInt(KEY_TRASH_PURGE_DAYS, 30);
    }

    public void setTrashPurgeDays(int days) {
        prefs.edit().putInt(KEY_TRASH_PURGE_DAYS, days).apply();
        notifyChanged(KEY_TRASH_PURGE_DAYS);
    }

    public String getFileNamingRule() {
        return prefs.getString(KEY_FILE_NAMING_RULE, "{name}_prism");
    }

    public void setFileNamingRule(String rule) {
        prefs.edit().putString(KEY_FILE_NAMING_RULE, rule).apply();
        notifyChanged(KEY_FILE_NAMING_RULE);
    }

    public boolean isPreserveTimestamps() {
        return prefs.getBoolean(KEY_PRESERVE_TIMESTAMPS, true);
    }

    public void setPreserveTimestamps(boolean preserve) {
        prefs.edit().putBoolean(KEY_PRESERVE_TIMESTAMPS, preserve).apply();
        notifyChanged(KEY_PRESERVE_TIMESTAMPS);
    }

    public int getThumbnailCacheQuotaMb() {
        return prefs.getInt(KEY_THUMBNAIL_CACHE_MB, 500);
    }

    public void setThumbnailCacheQuotaMb(int mb) {
        prefs.edit().putInt(KEY_THUMBNAIL_CACHE_MB, mb).apply();
        notifyChanged(KEY_THUMBNAIL_CACHE_MB);
    }

    // ==========================================
    // 5. HARDWARE, BATTERY & THERMAL GUARDS
    // ==========================================
    public static final String KEY_POWER_PROFILE = "pref_power_profile";
    public static final String KEY_RAM_BUDGET_MB = "pref_ram_budget_mb";
    public static final String KEY_RAM_PERCENT = "pref_ram_percent";
    public static final String KEY_CPU_PERCENT = "pref_cpu_percent";
    public static final String KEY_GPU_TURBO = "pref_gpu_turbo";
    public static final String KEY_LOW_BATTERY_PAUSE = "pref_low_battery_pause";
    public static final String KEY_THERMAL_THROTTLE_C = "pref_thermal_throttle_c";
    public static final String KEY_BACKGROUND_FGS = "pref_background_fgs";
    public static final String KEY_CHARGING_BUTLER = "pref_charging_butler";

    public String getPowerProfile() {
        return prefs.getString(KEY_POWER_PROFILE, "Turbo");
    }

    public void setPowerProfile(String profile) {
        prefs.edit().putString(KEY_POWER_PROFILE, profile).apply();
        notifyChanged(KEY_POWER_PROFILE);
    }

    public int getRamPercent() {
        return prefs.getInt(KEY_RAM_PERCENT, 50); // 50% default allocation
    }

    public void setRamPercent(int pct) {
        prefs.edit().putInt(KEY_RAM_PERCENT, pct).apply();
        notifyChanged(KEY_RAM_PERCENT);
    }

    public int getCpuPercent() {
        return prefs.getInt(KEY_CPU_PERCENT, 75); // 75% cores active by default
    }

    public void setCpuPercent(int pct) {
        prefs.edit().putInt(KEY_CPU_PERCENT, pct).apply();
        notifyChanged(KEY_CPU_PERCENT);
    }

    public boolean isGpuTurboEnabled() {
        return prefs.getBoolean(KEY_GPU_TURBO, true);
    }

    public void setGpuTurboEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_GPU_TURBO, enabled).apply();
        notifyChanged(KEY_GPU_TURBO);
    }

    public static long getTotalDeviceRamBytes(Context context) {
        if (context != null) {
            ActivityManager am = (ActivityManager) context.getSystemService(Context.ACTIVITY_SERVICE);
            if (am != null) {
                ActivityManager.MemoryInfo mi = new ActivityManager.MemoryInfo();
                am.getMemoryInfo(mi);
                return mi.totalMem;
            }
        }
        return 6L * 1024L * 1024L * 1024L; // 6GB fallback
    }

    public static int getAllocatedRamBudgetMb(Context context) {
        long totalBytes = getTotalDeviceRamBytes(context);
        PrismPreferences p = (context != null) ? get(context) : null;
        int pct = (p != null) ? p.getRamPercent() : 50;
        long allocatedBytes = (totalBytes * (long) pct) / 100L;
        int mb = (int) (allocatedBytes / (1024L * 1024L));
        return Math.max(128, mb);
    }

    public int getRamBudgetMb() {
        return prefs.getInt(KEY_RAM_BUDGET_MB, 2048);
    }

    public void setRamBudgetMb(int mb) {
        prefs.edit().putInt(KEY_RAM_BUDGET_MB, mb).apply();
        notifyChanged(KEY_RAM_BUDGET_MB);
    }

    public boolean isLowBatteryPauseEnabled() {
        return prefs.getBoolean(KEY_LOW_BATTERY_PAUSE, true);
    }

    public void setLowBatteryPauseEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_LOW_BATTERY_PAUSE, enabled).apply();
        notifyChanged(KEY_LOW_BATTERY_PAUSE);
    }

    public int getThermalThrottleThresholdC() {
        return prefs.getInt(KEY_THERMAL_THROTTLE_C, 39);
    }

    public void setThermalThrottleThresholdC(int tempC) {
        prefs.edit().putInt(KEY_THERMAL_THROTTLE_C, tempC).apply();
        notifyChanged(KEY_THERMAL_THROTTLE_C);
    }

    public boolean isBackgroundFgsEnabled() {
        return prefs.getBoolean(KEY_BACKGROUND_FGS, true);
    }

    public void setBackgroundFgsEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_BACKGROUND_FGS, enabled).apply();
        notifyChanged(KEY_BACKGROUND_FGS);
    }

    public boolean isChargingButlerMode() {
        return prefs.getBoolean(KEY_CHARGING_BUTLER, false);
    }

    public void setChargingButlerMode(boolean enabled) {
        prefs.edit().putBoolean(KEY_CHARGING_BUTLER, enabled).apply();
        notifyChanged(KEY_CHARGING_BUTLER);
    }

    public boolean isOvernightButler() {
        return isChargingButlerMode();
    }

    public void setOvernightButler(boolean enabled) {
        setChargingButlerMode(enabled);
    }

    // ==========================================
    // 6. PRIVACY & SOCIAL SHARING PRESETS
    // ==========================================
    public static final String KEY_SOCIAL_PRIVACY_SHIELD = "pref_social_privacy_shield";
    public static final String KEY_STRIP_DEVICE_SERIAL = "pref_strip_device_serial";
    public static final String KEY_BIOMETRIC_VAULT = "pref_biometric_vault";
    public static final String KEY_SCREEN_SECURITY = "pref_screen_security";

    public boolean isSocialPrivacyShield() {
        return prefs.getBoolean(KEY_SOCIAL_PRIVACY_SHIELD, true);
    }

    public void setSocialPrivacyShield(boolean enabled) {
        prefs.edit().putBoolean(KEY_SOCIAL_PRIVACY_SHIELD, enabled).apply();
        notifyChanged(KEY_SOCIAL_PRIVACY_SHIELD);
    }

    public boolean isStripDeviceSerial() {
        return prefs.getBoolean(KEY_STRIP_DEVICE_SERIAL, true);
    }

    public void setStripDeviceSerial(boolean enabled) {
        prefs.edit().putBoolean(KEY_STRIP_DEVICE_SERIAL, enabled).apply();
        notifyChanged(KEY_STRIP_DEVICE_SERIAL);
    }

    public boolean isBiometricVaultEnabled() {
        return prefs.getBoolean(KEY_BIOMETRIC_VAULT, true);
    }

    public void setBiometricVaultEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_BIOMETRIC_VAULT, enabled).apply();
        notifyChanged(KEY_BIOMETRIC_VAULT);
    }

    public boolean isScreenSecurityFlag() {
        return prefs.getBoolean(KEY_SCREEN_SECURITY, false);
    }

    public void setScreenSecurityFlag(boolean enabled) {
        prefs.edit().putBoolean(KEY_SCREEN_SECURITY, enabled).apply();
        notifyChanged(KEY_SCREEN_SECURITY);
    }

    // ==========================================
    // 7. AUTOMATION & BUTLER
    // ==========================================
    public static final String KEY_HOT_FOLDER_WATCHER = "pref_hot_folder_watcher";
    public static final String KEY_SHARE_INTERCEPTOR = "pref_share_interceptor";

    public boolean isHotFolderWatcherEnabled() {
        return prefs.getBoolean(KEY_HOT_FOLDER_WATCHER, false);
    }

    public void setHotFolderWatcherEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_HOT_FOLDER_WATCHER, enabled).apply();
        notifyChanged(KEY_HOT_FOLDER_WATCHER);
    }

    public boolean isShareInterceptorEnabled() {
        return prefs.getBoolean(KEY_SHARE_INTERCEPTOR, true);
    }

    public void setShareInterceptorEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_SHARE_INTERCEPTOR, enabled).apply();
        notifyChanged(KEY_SHARE_INTERCEPTOR);
    }

    // ==========================================
    // 8. NOTIFICATIONS & HAPTICS
    // ==========================================
    public static final String KEY_STATUS_BAR_CHIP = "pref_status_bar_chip";
    public static final String KEY_COMPLETION_CHIME = "pref_completion_chime";

    public boolean isStatusBarChipEnabled() {
        return prefs.getBoolean(KEY_STATUS_BAR_CHIP, true);
    }

    public void setStatusBarChipEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_STATUS_BAR_CHIP, enabled).apply();
        notifyChanged(KEY_STATUS_BAR_CHIP);
    }

    public boolean isCompletionChimeEnabled() {
        return prefs.getBoolean(KEY_COMPLETION_CHIME, true);
    }

    public void setCompletionChimeEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_COMPLETION_CHIME, enabled).apply();
        notifyChanged(KEY_COMPLETION_CHIME);
    }

    // ==========================================
    // 9. DEVELOPER & EXPERIMENTAL LABS
    // ==========================================
    public static final String KEY_FORCE_SIMD_PATH = "pref_force_simd_path";
    public static final String KEY_LOGGING_LEVEL = "pref_logging_level";

    public String getForceSimdPath() {
        return prefs.getString(KEY_FORCE_SIMD_PATH, "Auto Detect");
    }

    public void setForceSimdPath(String path) {
        prefs.edit().putString(KEY_FORCE_SIMD_PATH, path).apply();
        notifyChanged(KEY_FORCE_SIMD_PATH);
    }

    public String getLoggingLevel() {
        return prefs.getString(KEY_LOGGING_LEVEL, "Standard");
    }

    public void setLoggingLevel(String level) {
        prefs.edit().putString(KEY_LOGGING_LEVEL, level).apply();
        notifyChanged(KEY_LOGGING_LEVEL);
    }

    // ==========================================
    // 10. GEMINI COPILOT & AI ROUTING
    // ==========================================
    public static final String KEY_GEMINI_API_KEY = "pref_gemini_api_key";
    public static final String KEY_BACKGROUND_SERVICE_ENABLED = "pref_background_service_enabled";

    public String getGeminiApiKey() {
        return prefs.getString(KEY_GEMINI_API_KEY, "");
    }

    public void setGeminiApiKey(String key) {
        prefs.edit().putString(KEY_GEMINI_API_KEY, key != null ? key.trim() : "").apply();
        notifyChanged(KEY_GEMINI_API_KEY);
    }

    public boolean isBackgroundServiceEnabled() {
        return prefs.getBoolean(KEY_BACKGROUND_SERVICE_ENABLED, true);
    }

    public void setBackgroundServiceEnabled(boolean enabled) {
        prefs.edit().putBoolean(KEY_BACKGROUND_SERVICE_ENABLED, enabled).apply();
        notifyChanged(KEY_BACKGROUND_SERVICE_ENABLED);
    }

    public void resetToDefaults() {
        prefs.edit().clear().apply();
        notifyChanged("ALL");
    }
}
