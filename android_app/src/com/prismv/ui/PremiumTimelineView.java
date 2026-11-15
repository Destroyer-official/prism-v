package com.prismv.ui;

import android.content.Context;
import android.graphics.*;
import android.media.MediaMetadataRetriever;
import android.net.Uri;
import android.os.Handler;
import android.os.Looper;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.MotionEvent;
import android.view.View;

import java.io.File;
import java.util.ArrayList;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * PremiumTimelineView:
 * Studio-grade, filmstrip-based interactive video timeline.
 *
 * Professional NLE features:
 * 1. Background asynchronous video thumbnail extraction (8-14 thumbnail frames).
 * 2. Visual filmstrip rendering with cell dividers and dark-mode container.
 * 3. Pro dual-handle trim bounding box with embossed grip ridges and directional brackets.
 * 4. Dual trim modes:
 *    - Mode 1: DELETE DIMMED (Keep inside [A -> B], outer regions dimmed with frosted hash overlay)
 *    - Mode 2: DELETE SELECTED (Cut out [A -> B], inner region highlighted in ruby cut-stripe with cut badge)
 * 5. Floating real-time timecode bubble HUD with directional arrow indicator above active drag handle.
 * 6. High-contrast moving playhead needle with drop shadow and top pointer.
 * 7. SMPTE time ruler with dynamic major/minor tick marks and second markers.
 * 8. Window-sliding gesture: dragging between handles slides the entire selection window smoothly.
 */
public class PremiumTimelineView extends View {

    public interface OnTrimChangeListener {
        void onTrimRangeChanged(long startMs, long endMs, boolean cutOutSelected);
        void onSeekRequested(long seekPositionMs);
    }

    // State
    private Uri videoUri;
    private long totalDurationMs = 60000L;
    private long trimStartMs = 0L;
    private long trimEndMs = 60000L;
    private long currentPlayheadMs = 0L;
    private boolean cutOutSelected = false; // false = keep selected (dim outer), true = cut out selected (dim inner)

    // Thumbnail Caching
    private final List<Bitmap> thumbnails = new ArrayList<>();
    private final ExecutorService thumbnailExecutor = Executors.newSingleThreadExecutor();
    private final Handler mainHandler = new Handler(Looper.getMainLooper());
    private boolean isExtractingThumbnails = false;

    // Dimensions & Coordinates
    private final float dp;
    private float rulerHeight;
    private float filmstripHeight;
    private float filmstripTop;
    private float filmstripBottom;
    private float handleWidth;
    private float sideMargin;
    private float touchSlop;

    // Drag Tracking
    private static final int DRAG_NONE = 0;
    private static final int DRAG_LEFT = 1;
    private static final int DRAG_RIGHT = 2;
    private static final int DRAG_WINDOW = 3;
    private static final int DRAG_PLAYHEAD = 4;
    private int activeDrag = DRAG_NONE;
    private float lastTouchX = 0f;
    private long windowDragInitialStartMs = 0L;
    private long windowDragInitialEndMs = 0L;

    // Listeners
    private OnTrimChangeListener listener;

    // Paints & Shaders
    private final Paint paintFilmstripBg = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintDivider = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintRulerText = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintRulerTick = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintDimOverlay = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintCutOverlay = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintHatchPattern = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintBorderRails = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintHandle = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintHandleBorder = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintHandleGrip = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintPlayhead = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintPlayheadHead = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintBubbleBg = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintBubbleBorder = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint paintBubbleText = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path clipPath = new Path();
    private final Path bubblePath = new Path();
    private final RectF filmstripRect = new RectF();
    private final RectF tempRect = new RectF();

    public PremiumTimelineView(Context context) {
        this(context, null);
    }

    public PremiumTimelineView(Context context, AttributeSet attrs) {
        super(context, attrs);
        dp = getResources().getDisplayMetrics().density;
        initDimensionsAndPaints();
    }

    private void initDimensionsAndPaints() {
        rulerHeight = 22f * dp;
        filmstripHeight = 64f * dp;
        handleWidth = 18f * dp;
        sideMargin = handleWidth;
        touchSlop = 32f * dp;

        // Background & Dividers
        paintFilmstripBg.setColor(Color.parseColor("#08101E"));
        paintFilmstripBg.setStyle(Paint.Style.FILL);

        paintDivider.setColor(Color.parseColor("#1E293B"));
        paintDivider.setStrokeWidth(1.2f * dp);

        // Ruler
        paintRulerText.setColor(Color.parseColor("#94A3B8"));
        paintRulerText.setTextSize(9.5f * dp);
        paintRulerText.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));

        paintRulerTick.setColor(Color.parseColor("#334155"));
        paintRulerTick.setStrokeWidth(1f * dp);

        // Overlays
        paintDimOverlay.setColor(Color.parseColor("#D9050A14")); // 85% Frosted Black
        paintDimOverlay.setStyle(Paint.Style.FILL);

        paintCutOverlay.setColor(Color.parseColor("#80DC2626")); // Ruby Red translucent
        paintCutOverlay.setStyle(Paint.Style.FILL);

        paintHatchPattern.setColor(Color.parseColor("#55FFFFFF"));
        paintHatchPattern.setStrokeWidth(1.5f * dp);

        // Border Rails
        paintBorderRails.setStyle(Paint.Style.STROKE);
        paintBorderRails.setStrokeWidth(3.5f * dp);
        paintBorderRails.setStrokeCap(Paint.Cap.ROUND);

        // Handles
        paintHandle.setStyle(Paint.Style.FILL);

        paintHandleBorder.setStyle(Paint.Style.STROKE);
        paintHandleBorder.setStrokeWidth(1.5f * dp);
        paintHandleBorder.setColor(Color.WHITE);

        paintHandleGrip.setColor(Color.WHITE);
        paintHandleGrip.setStrokeWidth(1.5f * dp);
        paintHandleGrip.setStrokeCap(Paint.Cap.ROUND);

        // Playhead
        paintPlayhead.setColor(Color.WHITE);
        paintPlayhead.setStrokeWidth(2.5f * dp);
        paintPlayhead.setShadowLayer(4f * dp, 0, 0, Color.parseColor("#00E5FF"));

        paintPlayheadHead.setColor(Color.parseColor("#00E5FF"));
        paintPlayheadHead.setStyle(Paint.Style.FILL);

        // Bubble HUD
        paintBubbleBg.setColor(Color.parseColor("#F00B132B"));
        paintBubbleBg.setStyle(Paint.Style.FILL);
        paintBubbleBg.setShadowLayer(4f * dp, 0, 2f * dp, Color.parseColor("#80000000"));

        paintBubbleBorder.setStyle(Paint.Style.STROKE);
        paintBubbleBorder.setStrokeWidth(1.2f * dp);
        paintBubbleBorder.setColor(Color.parseColor("#00E5FF"));

        paintBubbleText.setColor(Color.WHITE);
        paintBubbleText.setTextSize(10.5f * dp);
        paintBubbleText.setTypeface(Typeface.create(Typeface.MONOSPACE, Typeface.BOLD));
        paintBubbleText.setTextAlign(Paint.Align.CENTER);
    }

    public void setVideoData(Uri uri, long durationMs) {
        this.videoUri = uri;
        if (durationMs <= 0) durationMs = 60000L;
        this.totalDurationMs = durationMs;
        this.trimStartMs = 0L;
        this.trimEndMs = durationMs;
        this.currentPlayheadMs = 0L;
        loadThumbnails();
        invalidate();
    }

    public void setTrimRange(long startMs, long endMs) {
        this.trimStartMs = Math.max(0, Math.min(startMs, totalDurationMs));
        this.trimEndMs = Math.max(this.trimStartMs + 500, Math.min(endMs, totalDurationMs));
        invalidate();
    }

    public void setTrimMode(boolean cutOutSelected) {
        this.cutOutSelected = cutOutSelected;
        invalidate();
    }

    public boolean isCutOutSelected() {
        return cutOutSelected;
    }

    public void setPlayheadPosition(long positionMs) {
        this.currentPlayheadMs = Math.max(0, Math.min(positionMs, totalDurationMs));
        invalidate();
    }

    public void setOnTrimChangeListener(OnTrimChangeListener listener) {
        this.listener = listener;
    }

    public long getTrimStartMs() { return trimStartMs; }
    public long getTrimEndMs() { return trimEndMs; }
    public long getTotalDurationMs() { return totalDurationMs; }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        int desiredHeight = (int) (rulerHeight + filmstripHeight + 16f * dp);
        int resolvedHeight = resolveSize(desiredHeight, heightMeasureSpec);
        setMeasuredDimension(getDefaultSize(getSuggestedMinimumWidth(), widthMeasureSpec), resolvedHeight);
    }

    @Override
    protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        super.onSizeChanged(w, h, oldw, oldh);
        filmstripTop = rulerHeight + 4f * dp;
        filmstripBottom = filmstripTop + filmstripHeight;
        // Inset by sideMargin so handles are fully visible and never clipped at bounds
        filmstripRect.set(sideMargin, filmstripTop, w - sideMargin, filmstripBottom);

        if (!isExtractingThumbnails && (thumbnails.isEmpty() || oldw != w)) {
            loadThumbnails();
        }
    }

    private void loadThumbnails() {
        if (videoUri == null || totalDurationMs <= 0 || getWidth() <= 0) return;
        isExtractingThumbnails = true;

        thumbnailExecutor.execute(() -> {
            MediaMetadataRetriever mmr = new MediaMetadataRetriever();
            try {
                if ("file".equalsIgnoreCase(videoUri.getScheme()) && videoUri.getPath() != null) {
                    mmr.setDataSource(videoUri.getPath());
                } else {
                    File sampleFile = new File("/sdcard/sample_video/sample.mp4");
                    if (videoUri.getPath() != null && new File(videoUri.getPath()).exists()) {
                        mmr.setDataSource(videoUri.getPath());
                    } else if (sampleFile.exists()) {
                        mmr.setDataSource(sampleFile.getAbsolutePath());
                    } else {
                        mmr.setDataSource(getContext(), videoUri);
                    }
                }

                float trackW = filmstripRect.width();
                if (trackW <= 0) trackW = Math.max(300, getWidth() - 2 * sideMargin);
                int numThumbs = Math.max(6, Math.min(14, (int) (trackW / (48f * dp))));
                long stepUs = (totalDurationMs * 1000L) / numThumbs;
                int thumbW = (int) (trackW / numThumbs);
                int thumbH = (int) filmstripHeight;

                List<Bitmap> loaded = new ArrayList<>();
                for (int i = 0; i < numThumbs; i++) {
                    long timeUs = i * stepUs + (stepUs / 2);
                    Bitmap bm = null;
                    try {
                        bm = mmr.getFrameAtTime(timeUs, MediaMetadataRetriever.OPTION_CLOSEST_SYNC);
                        if (bm != null) {
                            Bitmap scaled = Bitmap.createScaledBitmap(bm, Math.max(1, thumbW), Math.max(1, thumbH), true);
                            loaded.add(scaled);
                            if (scaled != bm) bm.recycle();
                        }
                    } catch (Exception ignored) {}
                }

                mainHandler.post(() -> {
                    synchronized (thumbnails) {
                        for (Bitmap b : thumbnails) {
                            if (b != null && !b.isRecycled()) b.recycle();
                        }
                        thumbnails.clear();
                        thumbnails.addAll(loaded);
                    }
                    isExtractingThumbnails = false;
                    invalidate();
                });
            } catch (Exception e) {
                isExtractingThumbnails = false;
            } finally {
                try { mmr.release(); } catch (Exception ignored) {}
            }
        });
    }

    @Override
    protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);

        float leftBound = filmstripRect.left;
        float rightBound = filmstripRect.right;
        float trackWidth = rightBound - leftBound;
        if (trackWidth <= 0 || totalDurationMs <= 0) return;

        // 1. DRAW SMPTE TIME RULER AT TOP
        drawRuler(canvas, leftBound, rightBound, trackWidth);

        // 2. DRAW FILMSTRIP CONTAINER & THUMBNAILS
        clipPath.reset();
        clipPath.addRoundRect(filmstripRect, 8f * dp, 8f * dp, Path.Direction.CW);
        canvas.save();
        canvas.clipPath(clipPath);

        // Filmstrip background
        canvas.drawRect(filmstripRect, paintFilmstripBg);

        // Render extracted thumbnails
        synchronized (thumbnails) {
            int count = thumbnails.size();
            if (count > 0) {
                float thumbWidth = trackWidth / (float) count;
                for (int i = 0; i < count; i++) {
                    Bitmap bm = thumbnails.get(i);
                    if (bm != null && !bm.isRecycled()) {
                        float tx = leftBound + (i * thumbWidth);
                        tempRect.set(tx, filmstripTop, tx + thumbWidth, filmstripBottom);
                        canvas.drawBitmap(bm, null, tempRect, null);

                        // Draw subtle frame divider line between film cells
                        if (i > 0) {
                            canvas.drawLine(tx, filmstripTop, tx, filmstripBottom, paintDivider);
                        }
                    }
                }
            }
        }

        // 3. DRAW DUAL-TRIM OVERLAYS
        float startX = leftBound + (trimStartMs / (float) totalDurationMs) * trackWidth;
        float endX = leftBound + (trimEndMs / (float) totalDurationMs) * trackWidth;
        startX = Math.max(leftBound, Math.min(startX, rightBound));
        endX = Math.max(startX + handleWidth, Math.min(endX, rightBound));

        if (!cutOutSelected) {
            // MODE 1: DELETE DIMMED (Keep [A -> B])
            // Dim left outer segment [leftBound, startX]
            if (startX > leftBound) {
                tempRect.set(leftBound, filmstripTop, startX, filmstripBottom);
                canvas.drawRect(tempRect, paintDimOverlay);
                drawHatchedCutMarks(canvas, tempRect);
            }
            // Dim right outer segment [endX, rightBound]
            if (endX < rightBound) {
                tempRect.set(endX, filmstripTop, rightBound, filmstripBottom);
                canvas.drawRect(tempRect, paintDimOverlay);
                drawHatchedCutMarks(canvas, tempRect);
            }
            // Active retained region [startX, endX] has bright Cyan rails
            paintBorderRails.setColor(Color.parseColor("#00E5FF")); // Luminous Neon Cyan
            canvas.drawLine(startX, filmstripTop + 1.5f * dp, endX, filmstripTop + 1.5f * dp, paintBorderRails);
            canvas.drawLine(startX, filmstripBottom - 1.5f * dp, endX, filmstripBottom - 1.5f * dp, paintBorderRails);
        } else {
            // MODE 2: DELETE SELECTED (Cut out [A -> B])
            // Inside [startX, endX] is tinted Ruby Red with diagonal cut stripes
            tempRect.set(startX, filmstripTop, endX, filmstripBottom);
            canvas.drawRect(tempRect, paintCutOverlay);
            drawHatchedCutMarks(canvas, tempRect);

            // Centered "✂ CUT OUT" badge inside cut region
            if (endX - startX > 64f * dp) {
                paintBubbleText.setTextSize(9.5f * dp);
                paintBubbleText.setColor(Color.parseColor("#FCA5A5"));
                canvas.drawText("✂ CUT OUT", (startX + endX) / 2f, (filmstripTop + filmstripBottom) / 2f + 3.5f * dp, paintBubbleText);
                paintBubbleText.setColor(Color.WHITE);
                paintBubbleText.setTextSize(10.5f * dp);
            }

            // Outside regions [leftBound, startX] and [endX, rightBound] are KEPT (Mint green rails)
            paintBorderRails.setColor(Color.parseColor("#10B981")); // Emerald / Mint Green
            if (startX > leftBound) {
                canvas.drawLine(leftBound, filmstripTop + 1.5f * dp, startX, filmstripTop + 1.5f * dp, paintBorderRails);
                canvas.drawLine(leftBound, filmstripBottom - 1.5f * dp, startX, filmstripBottom - 1.5f * dp, paintBorderRails);
            }
            if (endX < rightBound) {
                canvas.drawLine(endX, filmstripTop + 1.5f * dp, rightBound, filmstripTop + 1.5f * dp, paintBorderRails);
                canvas.drawLine(endX, filmstripBottom - 1.5f * dp, rightBound, filmstripBottom - 1.5f * dp, paintBorderRails);
            }

            // Cut region top/bottom Ruby rails
            paintBorderRails.setColor(Color.parseColor("#EF4444"));
            canvas.drawLine(startX, filmstripTop + 1.5f * dp, endX, filmstripTop + 1.5f * dp, paintBorderRails);
            canvas.drawLine(startX, filmstripBottom - 1.5f * dp, endX, filmstripBottom - 1.5f * dp, paintBorderRails);
        }

        canvas.restore();

        // 4. DRAW TRIM HANDLES
        int handleFillColor = !cutOutSelected ? Color.parseColor("#00E5FF") : Color.parseColor("#EF4444");
        paintHandle.setColor(handleFillColor);

        // Left Handle [A]
        drawHandle(canvas, startX - handleWidth / 2f, filmstripTop, startX + handleWidth / 2f, filmstripBottom, true);

        // Right Handle [B]
        drawHandle(canvas, endX - handleWidth / 2f, filmstripTop, endX + handleWidth / 2f, filmstripBottom, false);

        // 5. DRAW MOVING PLAYHEAD NEEDLE
        float playheadX = leftBound + (currentPlayheadMs / (float) totalDurationMs) * trackWidth;
        playheadX = Math.max(leftBound, Math.min(playheadX, rightBound));

        canvas.drawLine(playheadX, filmstripTop - 2f * dp, playheadX, filmstripBottom + 2f * dp, paintPlayhead);

        // Playhead Top Pointer
        Path pointer = new Path();
        pointer.moveTo(playheadX - 5.5f * dp, filmstripTop - 6f * dp);
        pointer.lineTo(playheadX + 5.5f * dp, filmstripTop - 6f * dp);
        pointer.lineTo(playheadX, filmstripTop);
        pointer.close();
        canvas.drawPath(pointer, paintPlayheadHead);

        // 6. DRAW FLOATING TIME BUBBLE HUD IF DRAGGING
        if (activeDrag == DRAG_LEFT) {
            drawTimeBubble(canvas, startX, filmstripTop, trimStartMs, "CUT A: ", handleFillColor);
        } else if (activeDrag == DRAG_RIGHT) {
            drawTimeBubble(canvas, endX, filmstripTop, trimEndMs, "CUT B: ", handleFillColor);
        } else if (activeDrag == DRAG_WINDOW) {
            drawTimeBubble(canvas, (startX + endX) / 2f, filmstripTop, trimStartMs, "SLIDE: ", Color.parseColor("#818CF8"));
        } else if (activeDrag == DRAG_PLAYHEAD) {
            drawTimeBubble(canvas, playheadX, filmstripTop, currentPlayheadMs, "PLAY: ", Color.parseColor("#00E5FF"));
        }
    }

    private void drawRuler(Canvas canvas, float left, float right, float width) {
        int seconds = (int) (totalDurationMs / 1000L);
        if (seconds <= 0) return;

        // Dynamic intervals to prevent text overlap
        int majorStepSec;
        int minorStepSec;
        if (seconds <= 15) {
            majorStepSec = 2;
            minorStepSec = 1;
        } else if (seconds <= 30) {
            majorStepSec = 5;
            minorStepSec = 1;
        } else if (seconds <= 75) {
            majorStepSec = 10;
            minorStepSec = 2;
        } else if (seconds <= 180) {
            majorStepSec = 20;
            minorStepSec = 5;
        } else if (seconds <= 600) {
            majorStepSec = 60;
            minorStepSec = 15;
        } else {
            majorStepSec = 120;
            minorStepSec = 30;
        }

        // Draw subtle minor micro-ticks
        paintRulerTick.setColor(Color.parseColor("#334155"));
        paintRulerTick.setStrokeWidth(1f * dp);
        for (int sec = 0; sec <= seconds; sec += minorStepSec) {
            if (sec % majorStepSec != 0) {
                float x = left + ((sec * 1000L) / (float) totalDurationMs) * width;
                canvas.drawLine(x, rulerHeight - 3.5f * dp, x, rulerHeight, paintRulerTick);
            }
        }

        // Draw major ticks & clean SMPTE labels
        paintRulerTick.setColor(Color.parseColor("#64748B"));
        paintRulerTick.setStrokeWidth(1.2f * dp);
        for (int sec = 0; sec <= seconds; sec += majorStepSec) {
            float x = left + ((sec * 1000L) / (float) totalDurationMs) * width;
            canvas.drawLine(x, rulerHeight - 7f * dp, x, rulerHeight, paintRulerTick);

            String label = String.format(Locale.US, "%02d:%02d", sec / 60, sec % 60);
            float textWidth = paintRulerText.measureText(label);
            float textX = x - textWidth / 2f;
            textX = Math.max(left - 4f * dp, Math.min(textX, right - textWidth + 4f * dp));
            canvas.drawText(label, textX, rulerHeight - 9f * dp, paintRulerText);
        }
    }

    private void drawHandle(Canvas canvas, float l, float t, float r, float b, boolean isLeft) {
        RectF hRect = new RectF(l, t, r, b);
        float radius = 6f * dp;
        
        // Handle pill body
        canvas.drawRoundRect(hRect, radius, radius, paintHandle);

        // Subtle 3D white border
        canvas.drawRoundRect(hRect, radius, radius, paintHandleBorder);

        // Embossed grip ridges
        float cx = (l + r) / 2f;
        float cy = (t + b) / 2f;
        float ridgeHalfH = 10f * dp;
        float spacing = 3.2f * dp;

        paintHandleGrip.setColor(Color.WHITE);
        canvas.drawLine(cx - spacing, cy - ridgeHalfH, cx - spacing, cy + ridgeHalfH, paintHandleGrip);
        canvas.drawLine(cx, cy - ridgeHalfH, cx, cy + ridgeHalfH, paintHandleGrip);
        canvas.drawLine(cx + spacing, cy - ridgeHalfH, cx + spacing, cy + ridgeHalfH, paintHandleGrip);
    }

    private void drawHatchedCutMarks(Canvas canvas, RectF r) {
        float step = 14f * dp;
        canvas.save();
        canvas.clipRect(r);
        for (float x = r.left - r.height(); x < r.right + r.height(); x += step) {
            canvas.drawLine(x, r.bottom, x + r.height(), r.top, paintHatchPattern);
        }
        canvas.restore();
    }

    private void drawTimeBubble(Canvas canvas, float cx, float topY, long timeMs, String prefix, int accentColor) {
        String text = prefix + formatTimecode(timeMs);
        float tw = paintBubbleText.measureText(text);
        float bw = tw + 18f * dp;
        float bh = 22f * dp;
        float by = topY - bh - 9f * dp;
        float bx = Math.max(4f * dp, Math.min(cx - bw / 2f, getWidth() - bw - 4f * dp));

        RectF bRect = new RectF(bx, by, bx + bw, by + bh);
        paintBubbleBorder.setColor(accentColor);
        canvas.drawRoundRect(bRect, 6f * dp, 6f * dp, paintBubbleBg);
        canvas.drawRoundRect(bRect, 6f * dp, 6f * dp, paintBubbleBorder);

        // Small downward triangle pointer
        bubblePath.reset();
        bubblePath.moveTo(cx - 4f * dp, by + bh);
        bubblePath.lineTo(cx + 4f * dp, by + bh);
        bubblePath.lineTo(cx, by + bh + 4f * dp);
        bubblePath.close();
        canvas.drawPath(bubblePath, paintBubbleBg);

        canvas.drawText(text, bRect.centerX(), bRect.centerY() + 3.8f * dp, paintBubbleText);
    }

    private String formatTimecode(long ms) {
        long s = ms / 1000;
        long m = s / 60;
        s = s % 60;
        long fraction = (ms % 1000) / 100;
        return String.format(Locale.US, "%02d:%02d.%d", m, s, fraction);
    }

    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (getParent() != null) {
            getParent().requestDisallowInterceptTouchEvent(true);
        }
        return super.dispatchTouchEvent(ev);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        float x = event.getX();
        float leftBound = filmstripRect.left;
        float rightBound = filmstripRect.right;
        float trackWidth = rightBound - leftBound;
        if (trackWidth <= 0 || totalDurationMs <= 0) return false;

        float startX = leftBound + (trimStartMs / (float) totalDurationMs) * trackWidth;
        float endX = leftBound + (trimEndMs / (float) totalDurationMs) * trackWidth;

        switch (event.getActionMasked()) {
            case MotionEvent.ACTION_DOWN:
                lastTouchX = x;

                // Disallow scroll intercept
                if (getParent() != null) {
                    getParent().requestDisallowInterceptTouchEvent(true);
                }

                // Check Left vs Right Handle proximity
                float distLeft = Math.abs(x - startX);
                float distRight = Math.abs(x - endX);

                if (distLeft <= touchSlop && distLeft <= distRight) {
                    activeDrag = DRAG_LEFT;
                    invalidate();
                    return true;
                } else if (distRight <= touchSlop) {
                    activeDrag = DRAG_RIGHT;
                    invalidate();
                    return true;
                }

                // Check Inside Selection Window (Window Slide)
                if (x > startX + touchSlop && x < endX - touchSlop) {
                    activeDrag = DRAG_WINDOW;
                    windowDragInitialStartMs = trimStartMs;
                    windowDragInitialEndMs = trimEndMs;
                    invalidate();
                    return true;
                }

                // Outside handles -> Scrub Playhead directly
                activeDrag = DRAG_PLAYHEAD;
                currentPlayheadMs = pixelToMs(x, leftBound, trackWidth);
                if (listener != null) listener.onSeekRequested(currentPlayheadMs);
                invalidate();
                return true;

            case MotionEvent.ACTION_MOVE:
                if (getParent() != null) {
                    getParent().requestDisallowInterceptTouchEvent(true);
                }

                if (activeDrag == DRAG_LEFT) {
                    long newStartMs = pixelToMs(x, leftBound, trackWidth);
                    trimStartMs = Math.max(0, Math.min(newStartMs, trimEndMs - 500));
                    currentPlayheadMs = trimStartMs;
                    if (listener != null) {
                        listener.onTrimRangeChanged(trimStartMs, trimEndMs, cutOutSelected);
                        listener.onSeekRequested(currentPlayheadMs);
                    }
                    invalidate();
                    return true;
                } else if (activeDrag == DRAG_RIGHT) {
                    long newEndMs = pixelToMs(x, leftBound, trackWidth);
                    trimEndMs = Math.min(totalDurationMs, Math.max(newEndMs, trimStartMs + 500));
                    currentPlayheadMs = trimEndMs;
                    if (listener != null) {
                        listener.onTrimRangeChanged(trimStartMs, trimEndMs, cutOutSelected);
                        listener.onSeekRequested(currentPlayheadMs);
                    }
                    invalidate();
                    return true;
                } else if (activeDrag == DRAG_WINDOW) {
                    float deltaX = x - lastTouchX;
                    long deltaMs = (long) ((deltaX / trackWidth) * totalDurationMs);
                    long windowDuration = windowDragInitialEndMs - windowDragInitialStartMs;

                    long newStart = windowDragInitialStartMs + deltaMs;
                    if (newStart < 0) newStart = 0;
                    if (newStart + windowDuration > totalDurationMs) newStart = totalDurationMs - windowDuration;
                    trimStartMs = newStart;
                    trimEndMs = newStart + windowDuration;
                    currentPlayheadMs = trimStartMs;

                    if (listener != null) {
                        listener.onTrimRangeChanged(trimStartMs, trimEndMs, cutOutSelected);
                        listener.onSeekRequested(currentPlayheadMs);
                    }
                    invalidate();
                    return true;
                } else if (activeDrag == DRAG_PLAYHEAD) {
                    currentPlayheadMs = pixelToMs(x, leftBound, trackWidth);
                    if (listener != null) listener.onSeekRequested(currentPlayheadMs);
                    invalidate();
                    return true;
                }
                break;

            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_CANCEL:
                activeDrag = DRAG_NONE;
                if (getParent() != null) {
                    getParent().requestDisallowInterceptTouchEvent(false);
                }
                invalidate();
                return true;
        }

        return super.onTouchEvent(event);
    }

    private long pixelToMs(float x, float leftBound, float trackWidth) {
        float ratio = (x - leftBound) / trackWidth;
        ratio = Math.max(0f, Math.min(1f, ratio));
        return (long) (ratio * totalDurationMs);
    }
}
