package com.prismv.engine;

import android.content.Context;
import android.media.MediaCodecInfo;
import android.media.MediaCodecList;
import android.media.MediaFormat;
import android.os.Build;
import com.prismv.logging.PrismLogger;

import java.util.Arrays;
import java.util.Locale;

/**
 * DeviceProfile:
 * Single cached source of truth detailing the exact hardware capabilities of the host device.
 * 
 * Answers:
 * - What hardware encoders and decoders exist?
 * - Are 10-bit / HDR profiles supported?
 * - What is the SoC and ABI architecture?
 */
public final class DeviceProfile {
    private static final String TAG = "DeviceProfile";
    private static volatile DeviceProfile sInstance = null;

    public final int apiLevel;
    public final String[] abis;
    public final String socModel;
    public final String hardware;
    public final String manufacturer;
    public final String model;

    public final boolean hasAvcHardwareEncoder;
    public final boolean hasHevcHardwareEncoder;
    public final boolean hasAv1HardwareEncoder;
    public final boolean hasAv1HardwareDecoder;
    public final boolean hasHevcHardwareDecoder;
    public final boolean hdr10Encode;
    public final boolean hlgEncode;

    public final String preferredEncoderName;

    private DeviceProfile(Context context) {
        this.apiLevel = Build.VERSION.SDK_INT;
        this.abis = Build.SUPPORTED_ABIS;
        this.manufacturer = Build.MANUFACTURER;
        this.model = Build.MODEL;
        this.hardware = Build.HARDWARE;

        String soc = "unknown";
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            try {
                soc = Build.SOC_MODEL;
            } catch (Throwable ignored) {}
        }
        if ("unknown".equals(soc) || soc == null || soc.isEmpty()) {
            soc = Build.HARDWARE;
        }
        this.socModel = soc;

        boolean avcEnc = false;
        boolean hevcEnc = false;
        boolean av1Enc = false;
        boolean av1Dec = false;
        boolean hevcDec = false;
        boolean hdr10 = false;
        boolean hlg = false;
        String bestEnc = "unknown";

        try {
            MediaCodecList codecList = new MediaCodecList(MediaCodecList.REGULAR_CODECS);
            for (MediaCodecInfo info : codecList.getCodecInfos()) {
                boolean isHw = EncoderSelector.isHardware(info);
                String[] types = info.getSupportedTypes();

                if (info.isEncoder()) {
                    for (String t : types) {
                        if (MediaFormat.MIMETYPE_VIDEO_AVC.equalsIgnoreCase(t) && isHw) avcEnc = true;
                        if (MediaFormat.MIMETYPE_VIDEO_HEVC.equalsIgnoreCase(t) && isHw) {
                            hevcEnc = true;
                            if ("unknown".equals(bestEnc)) bestEnc = info.getName();
                            try {
                                MediaCodecInfo.CodecCapabilities caps = info.getCapabilitiesForType(t);
                                if (caps != null && caps.profileLevels != null) {
                                    for (MediaCodecInfo.CodecProfileLevel pl : caps.profileLevels) {
                                        if (pl.profile == MediaCodecInfo.CodecProfileLevel.HEVCProfileMain10HDR10
                                                || pl.profile == MediaCodecInfo.CodecProfileLevel.HEVCProfileMain10) {
                                            hdr10 = true;
                                            hlg = true;
                                        }
                                    }
                                }
                            } catch (Throwable ignored) {}
                        }
                        if (MediaFormat.MIMETYPE_VIDEO_AV1.equalsIgnoreCase(t) && isHw) {
                            av1Enc = true;
                            bestEnc = info.getName();
                            try {
                                MediaCodecInfo.CodecCapabilities caps = info.getCapabilitiesForType(t);
                                if (caps != null && caps.profileLevels != null) {
                                    for (MediaCodecInfo.CodecProfileLevel pl : caps.profileLevels) {
                                        if (pl.profile >= 0x1000) { // AV1ProfileMain10HDR10
                                            hdr10 = true;
                                            hlg = true;
                                        }
                                    }
                                }
                            } catch (Throwable ignored) {}
                        }
                    }
                } else {
                    for (String t : types) {
                        if (MediaFormat.MIMETYPE_VIDEO_HEVC.equalsIgnoreCase(t) && isHw) hevcDec = true;
                        if (MediaFormat.MIMETYPE_VIDEO_AV1.equalsIgnoreCase(t) && isHw) av1Dec = true;
                    }
                }
            }
        } catch (Throwable t) {
            PrismLogger.w(TAG, "Device capability probe warning: " + t.getMessage());
        }

        this.hasAvcHardwareEncoder = avcEnc;
        this.hasHevcHardwareEncoder = hevcEnc;
        this.hasAv1HardwareEncoder = av1Enc;
        this.hasAv1HardwareDecoder = av1Dec;
        this.hasHevcHardwareDecoder = hevcDec;
        this.hdr10Encode = hdr10;
        this.hlgEncode = hlg;
        this.preferredEncoderName = bestEnc;

        PrismLogger.i(TAG, String.format(Locale.US,
                "DeviceProfile cached: SoC=%s API=%d ABIs=%s HW_ENC[AV1=%b, HEVC=%b, AVC=%b, HDR10=%b, HLG=%b] Primary=%s",
                this.socModel, this.apiLevel, Arrays.toString(this.abis),
                this.hasAv1HardwareEncoder, this.hasHevcHardwareEncoder, this.hasAvcHardwareEncoder,
                this.hdr10Encode, this.hlgEncode,
                this.preferredEncoderName));
    }

    public static DeviceProfile get(Context context) {
        if (sInstance == null) {
            synchronized (DeviceProfile.class) {
                if (sInstance == null) {
                    sInstance = new DeviceProfile(context != null ? context.getApplicationContext() : null);
                }
            }
        }
        return sInstance;
    }

    public static DeviceProfile get() {
        return get(null);
    }
}
