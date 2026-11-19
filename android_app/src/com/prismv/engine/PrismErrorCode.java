package com.prismv.engine;

/**
 * PrismErrorCode:
 * Standardized error taxonomy for Prism-V transcoding and media processing operations.
 * 
 * Provides distinct machine-readable error codes and user-friendly explanations
 * to replace opaque "Compression failed" messages.
 */
public enum PrismErrorCode {
    NONE(0, "Operation completed successfully."),
    E_NO_ENCODER(101, "No compatible hardware video encoder found on this device."),
    E_UNSUPPORTED_INPUT(102, "This video uses a codec or container that cannot be decoded on this device."),
    E_HDR_UNSUPPORTED(103, "This HDR video requires an encoder profile not supported by this phone."),
    E_STORAGE(104, "Insufficient free storage space to write the compressed video file."),
    E_THERMAL(105, "Phone is too hot; operation paused to prevent hardware thermal damage."),
    E_NATIVE_MISSING(106, "Native acceleration library unavailable; operating in pure MediaCodec fallback."),
    E_CANCELLED(107, "Compression was stopped by the user."),
    E_CODEC_EXCEPTION(108, "Hardware video codec encountered a transient failure during processing."),
    E_AUDIO_UNSUPPORTED(109, "Source audio stream could not be decoded or transcoded to AAC-LC."),
    E_UNKNOWN(199, "An unexpected hardware processing error occurred.");

    public final int code;
    public final String message;

    PrismErrorCode(int code, String message) {
        this.code = code;
        this.message = message;
    }

    @Override
    public String toString() {
        return "[" + name() + " (" + code + "): " + message + "]";
    }
}
