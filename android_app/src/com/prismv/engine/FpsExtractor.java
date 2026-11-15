package com.prismv.engine;

import android.content.Context;
import android.content.res.AssetFileDescriptor;
import android.media.MediaExtractor;
import android.media.MediaFormat;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.os.Build;
import android.util.Log;

import java.io.File;

/**
 * High-Precision Multi-Tier Video Frame Rate Detector for Prism-V.
 *
 * Tier 1: MediaExtractor Track Format KEY_FRAME_RATE (handles both float and int types safely)
 * Tier 2: MediaMetadataRetriever METADATA_KEY_CAPTURE_FRAMERATE (Samsung, Pixel, Xiaomi slow-mo 120/240fps)
 * Tier 3: MediaExtractor Ground-Truth Inter-Frame Interval Sampling (first 40 sample PTS deltas)
 * Tier 4: MediaMetadataRetriever METADATA_KEY_VIDEO_FRAME_COUNT / durationMs
 */
public class FpsExtractor {
    private static final String TAG = "FpsExtractor";

    public static class FpsResult {
        public final int fps;
        public final float exactFps;
        public final String source;
        public final boolean isHighFrameRate;

        public FpsResult(int fps, float exactFps, String source) {
            this.fps = fps;
            this.exactFps = exactFps;
            this.source = source;
            this.isHighFrameRate = fps >= 50;
        }

        @Override
        public String toString() {
            return fps + " fps (exact: " + exactFps + ", tier: " + source + ")";
        }
    }

    public static FpsResult detectFps(Context context, Uri uri, String filePath, MediaMetadataRetriever preloadedMmr, long knownDurationMs) {
        float captureFps = 0f;
        float trackFps = 0f;
        float countFps = 0f;
        float sampleFps = 0f;

        // -------------------------------------------------------------
        // TIER 1 & TIER 2: MediaMetadataRetriever Metadata Analysis
        // -------------------------------------------------------------
        MediaMetadataRetriever mmr = preloadedMmr;
        boolean ownsMmr = false;
        try {
            if (mmr == null) {
                mmr = new MediaMetadataRetriever();
                ownsMmr = true;
                setDataSourceMmr(mmr, context, uri, filePath);
            }

            // Extract Slow-Motion / High-Speed Capture Frame Rate (API 23+)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
                String capFpsStr = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_CAPTURE_FRAMERATE);
                if (capFpsStr != null && !capFpsStr.trim().isEmpty()) {
                    try {
                        float parsed = Float.parseFloat(capFpsStr.trim());
                        if (parsed >= 10.0f && parsed <= 960.0f) {
                            captureFps = parsed;
                            Log.d(TAG, "Tier 2 Capture Framerate detected: " + captureFps);
                        }
                    } catch (Exception ignored) {}
                }
            }

            // Extract Frame Count & Duration Frame Rate (API 28+)
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                String fcStr = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_FRAME_COUNT);
                String durStr = (knownDurationMs > 0) ? String.valueOf(knownDurationMs) : mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION);
                if (fcStr != null && durStr != null) {
                    try {
                        int fc = Integer.parseInt(fcStr.trim());
                        long dMs = Long.parseLong(durStr.trim());
                        if (fc > 0 && dMs > 0) {
                            countFps = (fc * 1000.0f) / dMs;
                            Log.d(TAG, "Tier 4 Frame Count Framerate: " + countFps + " (" + fc + " frames / " + dMs + " ms)");
                        }
                    } catch (Exception ignored) {}
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "MMR inspection warning: " + e.getMessage());
        } finally {
            if (ownsMmr && mmr != null) {
                try { mmr.release(); } catch (Exception ignored) {}
            }
        }

        // -------------------------------------------------------------
        // TIER 1 & TIER 3: MediaExtractor Track Format & PTS Sampling
        // -------------------------------------------------------------
        MediaExtractor extractor = null;
        try {
            extractor = new MediaExtractor();
            setDataSourceExtractor(extractor, context, uri, filePath);

            int videoTrackIndex = -1;
            int numTracks = extractor.getTrackCount();
            for (int i = 0; i < numTracks; i++) {
                MediaFormat format = extractor.getTrackFormat(i);
                String mime = format.getString(MediaFormat.KEY_MIME);
                if (mime != null && mime.startsWith("video/")) {
                    videoTrackIndex = i;

                    // Safely extract KEY_FRAME_RATE (handles float or integer)
                    if (format.containsKey(MediaFormat.KEY_FRAME_RATE)) {
                        try {
                            trackFps = format.getFloat(MediaFormat.KEY_FRAME_RATE);
                        } catch (Exception e1) {
                            try {
                                trackFps = format.getInteger(MediaFormat.KEY_FRAME_RATE);
                            } catch (Exception ignored) {}
                        }
                        if (trackFps > 0) {
                            Log.d(TAG, "Tier 1 MediaFormat KEY_FRAME_RATE: " + trackFps);
                        }
                    }

                    // Check vendor capture-rate key if present
                    if (format.containsKey("capture-rate")) {
                        try {
                            float cr = format.getFloat("capture-rate");
                            if (cr > captureFps) captureFps = cr;
                        } catch (Exception e) {
                            try {
                                float cr = format.getInteger("capture-rate");
                                if (cr > captureFps) captureFps = cr;
                            } catch (Exception ignored) {}
                        }
                    }
                    break;
                }
            }

            // TIER 3: Ground-Truth PTS Inter-Frame Interval Sampling
            if (videoTrackIndex >= 0) {
                extractor.selectTrack(videoTrackIndex);
                long prevPts = -1;
                int sampleCount = 0;
                long totalDeltaUs = 0;
                int deltaCount = 0;

                while (sampleCount < 40) {
                    long pts = extractor.getSampleTime();
                    if (pts < 0) break;
                    if (prevPts >= 0 && pts > prevPts) {
                        long delta = pts - prevPts;
                        // Frame intervals between 1,000 us (1000 fps) and 250,000 us (4 fps)
                        if (delta >= 1000 && delta <= 250000) {
                            totalDeltaUs += delta;
                            deltaCount++;
                        }
                    }
                    prevPts = pts;
                    sampleCount++;
                    if (!extractor.advance()) break;
                }

                if (deltaCount >= 3 && totalDeltaUs > 0) {
                    float avgIntervalUs = (float) totalDeltaUs / (float) deltaCount;
                    sampleFps = 1000000.0f / avgIntervalUs;
                    Log.d(TAG, "Tier 3 Sample PTS Framerate: " + sampleFps + " (avg interval: " + avgIntervalUs + " us, deltas: " + deltaCount + ")");
                }
            }
        } catch (Exception e) {
            Log.w(TAG, "MediaExtractor inspection warning: " + e.getMessage());
        } finally {
            if (extractor != null) {
                try { extractor.release(); } catch (Exception ignored) {}
            }
        }

        // -------------------------------------------------------------
        // DECISION MATRIX & RATE SNAPPING
        // -------------------------------------------------------------
        // Priority 1: High-Speed Slow-Motion Capture (Samsung / Pixel 120fps, 240fps)
        if (captureFps >= 50.0f) {
            int snapped = snapStandardFps(captureFps);
            return new FpsResult(snapped, captureFps, "CAPTURE_RATE");
        }

        // Priority 2: Real Encoded Sample PTS Interval (120fps, 60fps, etc.)
        if (sampleFps >= 45.0f) {
            int snapped = snapStandardFps(sampleFps);
            return new FpsResult(snapped, sampleFps, "SAMPLE_PTS");
        }

        // Priority 3: Track Format Frame Rate (>= 45 fps)
        if (trackFps >= 45.0f) {
            int snapped = snapStandardFps(trackFps);
            return new FpsResult(snapped, trackFps, "TRACK_FORMAT");
        }

        // Priority 4: Frame Count / Duration (>= 45 fps)
        if (countFps >= 45.0f) {
            int snapped = snapStandardFps(countFps);
            return new FpsResult(snapped, countFps, "FRAME_COUNT");
        }

        // Priority 5: Standard Frame Rates (24, 25, 30, etc.)
        if (sampleFps > 10.0f) {
            return new FpsResult(snapStandardFps(sampleFps), sampleFps, "SAMPLE_PTS");
        }
        if (trackFps > 10.0f) {
            return new FpsResult(snapStandardFps(trackFps), trackFps, "TRACK_FORMAT");
        }
        if (captureFps > 10.0f) {
            return new FpsResult(snapStandardFps(captureFps), captureFps, "CAPTURE_RATE");
        }
        if (countFps > 10.0f) {
            return new FpsResult(snapStandardFps(countFps), countFps, "FRAME_COUNT");
        }

        return new FpsResult(30, 30.0f, "DEFAULT");
    }

    public static int snapStandardFps(float raw) {
        if (raw >= 115.0f && raw <= 125.0f) return 120;
        if (raw >= 235.0f && raw <= 245.0f) return 240;
        if (raw >= 85.0f && raw <= 95.0f) return 90;
        if (raw >= 58.0f && raw <= 62.0f) return 60;
        if (raw >= 48.0f && raw <= 52.0f) return 50;
        if (raw >= 28.5f && raw <= 31.5f) return 30;
        if (raw >= 23.2f && raw <= 25.2f) return 24;
        if (raw >= 14.0f && raw <= 16.0f) return 15;
        int rounded = Math.round(raw);
        return rounded > 0 ? rounded : 30;
    }

    private static void setDataSourceMmr(MediaMetadataRetriever mmr, Context context, Uri uri, String filePath) throws Exception {
        if (filePath != null && new File(filePath).exists()) {
            mmr.setDataSource(filePath);
            return;
        }
        if (uri != null) {
            if ("file".equalsIgnoreCase(uri.getScheme()) && uri.getPath() != null) {
                mmr.setDataSource(uri.getPath());
                return;
            }
            if (context != null) {
                try {
                    mmr.setDataSource(context, uri);
                    return;
                } catch (Exception e) {
                    try (AssetFileDescriptor afd = context.getContentResolver().openAssetFileDescriptor(uri, "r")) {
                        if (afd != null) {
                            mmr.setDataSource(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
                            return;
                        }
                    }
                }
            }
        }
        throw new IllegalArgumentException("No valid data source for MediaMetadataRetriever");
    }

    private static void setDataSourceExtractor(MediaExtractor extractor, Context context, Uri uri, String filePath) throws Exception {
        if (filePath != null && new File(filePath).exists()) {
            extractor.setDataSource(filePath);
            return;
        }
        if (uri != null) {
            if ("file".equalsIgnoreCase(uri.getScheme()) && uri.getPath() != null) {
                extractor.setDataSource(uri.getPath());
                return;
            }
            if (context != null) {
                try {
                    extractor.setDataSource(context, uri, null);
                    return;
                } catch (Exception e) {
                    try (AssetFileDescriptor afd = context.getContentResolver().openAssetFileDescriptor(uri, "r")) {
                        if (afd != null) {
                            extractor.setDataSource(afd.getFileDescriptor(), afd.getStartOffset(), afd.getLength());
                            return;
                        }
                    }
                }
            }
        }
        throw new IllegalArgumentException("No valid data source for MediaExtractor");
    }
}
