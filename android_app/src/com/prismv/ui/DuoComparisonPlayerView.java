package com.prismv.ui;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.SurfaceTexture;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.media.MediaMetadataRetriever;
import android.media.MediaPlayer;
import android.media.PlaybackParams;
import android.net.Uri;
import android.os.Build;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;
import android.util.AttributeSet;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.MotionEvent;
import android.view.ScaleGestureDetector;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.*;

import com.prismv.engine.FpsExtractor;
import com.prismv.history.CompressionHistoryManager;

import java.io.File;
import java.util.Locale;

/**
 * DuoComparisonPlayerView:
 * Studio-Grade Synchronized Dual Comparison Video Player for Prism-V.
 *
 * Implements:
 * 1. Frame-by-Frame Lockstep Synchronization:
 *    - Strict frame alignment via MediaPlayer.SEEK_CLOSEST (API 26+)
 *    - Active 40ms drift watchdog to eliminate stream desynchronization
 * 2. 4 Interactive Comparison Modes:
 *    - Split Curtain (Wipe Slider): Neon divider slicing Original vs Compressed in real-time.
 *    - Side-by-Side (Dual View): Synchronized dual playback with independent badges.
 *    - Rapid Hold-to-Peek: Instant frame toggle utilizing human vision motion flicker detection.
 *    - Perceptual Difference Scorecard: PSNR, VMAF, SSIM & error distribution.
 * 3. Pixel-Peep Zoom Engine:
 *    - 1.0x (Fit), 2.0x, 4.0x, 8.0x synchronized matrix scaling across both TextureViews
 *    - Pan & Drag gestures for inspecting micro-compression artifacts
 * 4. Fullscreen Cinema Mode:
 *    - Immersive edge-to-edge view with floating glass HUD controls
 *    - Zero-interruption playback switching without SurfaceTexture recreation
 */
public class DuoComparisonPlayerView extends LinearLayout {

    public enum ComparisonMode {
        SPLIT_CURTAIN,
        SIDE_BY_SIDE,
        HOLD_TO_PEEK,
        ERROR_SCORECARD,
        SOLO_OUTPUT,
        SOLO_ORIGINAL
    }

    public interface FullscreenListener {
        void onFullscreenToggled(boolean isFullscreen);
    }

    private ComparisonMode currentMode = ComparisonMode.SIDE_BY_SIDE;
    private FullscreenListener fullscreenListener;
    private boolean isFullscreen = false;

    // MediaPlayers & Textures
    private MediaPlayer playerOriginal;
    private MediaPlayer playerCompressed;
    private Surface surfaceOriginal;
    private Surface surfaceCompressed;
    private TextureView textureOriginal;
    private TextureView textureCompressed;

    // ── Readiness Latch (Bug 1 & 7 fix) ───────────────────────────────────────
    // All 4 must be true before any MediaPlayer is created.
    private boolean isSurfaceOrigReady  = false;
    private boolean isSurfaceCompReady  = false;
    private int     origRetryCount      = 0;
    private int     compRetryCount      = 0;
    private static final int MAX_RECOVER_ATTEMPTS = 2;
    private Uri     pendingOrigUri       = null;   // set by loadVideoPair / loadSourceVideoOnly
    private Uri     pendingCompUri       = null;   // set by loadVideoPair / loadOutputVideoSolo
    private boolean initPending          = false;  // a loadVideoPair call is waiting for surfaces
    // ──────────────────────────────────────────────────────────────────────────

    private boolean isOriginalPrepared  = false;
    private boolean isCompressedPrepared = false;

    // "Armed" = both prepared; seek-to-zero done; ready for frame-perfect start
    private boolean isArmed              = false;

    private boolean isPlaying           = false;
    private boolean isUserScrubbing     = false;
    private boolean isAudioOriginal     = false;
    private float   currentPlaybackSpeed = 1.0f;

    private Uri originalUri;
    private Uri compressedUri;

    // Zoom & Pan Engine
    private float currentZoom = 1.0f;
    private float panX = 0f;
    private float panY = 0f;
    private float lastTouchX = 0f;
    private float lastTouchY = 0f;
    private boolean isPanning = false;
    private ScaleGestureDetector scaleGestureDetector;
    private long lastHardSeekTime = 0L;
    private long lastDriftCheckTime = 0L;
    private float currentOriginalSteerSpeed = 1.0f;
    private int lastHapticPct = 50;
    private TextView[] zoomButtons;
    private TextView[] cinemaZoomButtons;
    private TextView tvZoomBadge;

    // View Containers
    private FrameLayout playerContainer;
    private FrameLayout containerOriginal;
    private FrameLayout containerCompressed;

    // Split Curtain Elements
    private View dividerLine;
    private LinearLayout dividerHandle;
    private TextView tvLabelOriginal;
    private TextView tvLabelCompressed;
    private TextView tvSplitRatio;
    private int curtainPositionX = -1;

    // Dual Stream Telemetry Cards & Sync Delta Badge
    private LinearLayout cardOriginal;
    private LinearLayout cardCompressed;
    private TextView tvOrigTime;
    private TextView tvOrigSpecs;
    private TextView tvOrigBitrate;

    private TextView tvCompTime;
    private TextView tvCompSpecs;
    private TextView tvCompBitrate;

    private TextView badgeCenterSync;
    private LinearLayout emptyDuoOverlay;

    public interface OnBrowseLibraryListener {
        void onBrowseLibrary();
    }
    private OnBrowseLibraryListener browseLibraryListener;
    private Button btnPickPair;
    private Button btnEmptyLibrary;

    public void setOnBrowseLibraryListener(OnBrowseLibraryListener listener) {
        this.browseLibraryListener = listener;
    }

    // Metadata & Dynamic Stream Geometries
    public static class VideoMeta {
        public int rawWidth = 0;
        public int rawHeight = 0;
        public int rotation = 0;
        public int displayWidth = 0;
        public int displayHeight = 0;
        public long durationMs = 0;
        public long fileSizeBytes = 0;
        public int bitrateKbps = 0;
        public float fps = 30.0f;
        public String codec = "AVC / H.264";
        public String aspectLabel = "16:9 Cinema";
    }
    private VideoMeta origMeta = new VideoMeta();
    private VideoMeta compMeta = new VideoMeta();

    // Hold-to-Peek Elements
    private FrameLayout holdToPeekOverlay;
    private Button btnHoldToPeek;

    // Scorecard Elements
    private LinearLayout scorecardOverlay;
    private TextView tvScorecardVmafVal;
    private TextView tvScorecardPsnrVal;
    private TextView tvScorecardSavedVal;
    private TextView tvScorecardSavedSub;
    private TextView tvScorecardExplanation;

    // Standard Non-Fullscreen Sections (Hidden during Fullscreen)
    private LinearLayout headerRow;
    private LinearLayout tabsRow;
    private LinearLayout curtainBar;
    private LinearLayout controlsCard;
    private LinearLayout telemetryCard;
    private TextView tvStatusBanner;

    // Cinema Mode Floating Overlays
    private LinearLayout cinemaTopBar;
    private LinearLayout cinemaBottomBar;
    private Button btnCinemaPlayPause;
    private SeekBar cinemaSeekBar;
    private TextView cinemaTimeDisplay;
    private Button btnCinemaAudio;

    // Standard Controls
    private TextView[] modeTabButtons;
    private Button btnPlayPause;
    private SeekBar masterSeekBar;
    private TextView tvTimeDisplay;
    private Button btnAudioToggle;
    private Button btnStepBack;
    private Button btnStepForward;
    private Button btnFullscreenToggle;

    // Speed Controls
    private TextView[] speedButtons;

    // Telemetry
    private TextView tvTelemetryOrig;
    private TextView tvTelemetryComp;

    private Handler handler;
    private Runnable progressUpdater;

    public DuoComparisonPlayerView(Context context) {
        super(context);
        init(context);
    }

    public DuoComparisonPlayerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        init(context);
    }

    public void setFullscreenListener(FullscreenListener listener) {
        this.fullscreenListener = listener;
    }

    public boolean isFullscreen() {
        return isFullscreen;
    }

    public Uri getOriginalUri() {
        return originalUri;
    }

    public Uri getCompressedUri() {
        return compressedUri;
    }

    private int dp(int value) {
        return ThemeStyles.dp(getContext(), value);
    }

    private GradientDrawable createRoundedDrawable(int bgColor, int strokeColor, int strokeWidth, float radiusPx) {
        return ThemeStyles.createRoundedDrawable(bgColor, strokeColor, strokeWidth, radiusPx);
    }

    private void init(Context context) {
        handler = new Handler(Looper.getMainLooper());
        setOrientation(VERTICAL);
        setBackgroundColor(ThemeStyles.COLOR_BG_CANVAS);
        setPadding(dp(14), dp(8), dp(14), dp(24));

        scaleGestureDetector = new ScaleGestureDetector(context, new ScaleGestureDetector.SimpleOnScaleGestureListener() {
            @Override
            public boolean onScale(ScaleGestureDetector detector) {
                float scaleFactor = detector.getScaleFactor();
                float newZoom = currentZoom * scaleFactor;
                if (newZoom < 1.0f) newZoom = 1.0f;
                if (newZoom > 8.0f) newZoom = 8.0f;
                if (Math.abs(newZoom - currentZoom) > 0.02f) {
                    setZoom(newZoom);
                }
                return true;
            }
        });

        // ==========================================
        // 1. TOP HEADER ROW (TITLE + SYNC BADGE + PAIR BTN + FULLSCREEN BTN)
        // ==========================================
        headerRow = new LinearLayout(context);
        headerRow.setOrientation(HORIZONTAL);
        headerRow.setGravity(Gravity.CENTER_VERTICAL);
        headerRow.setPadding(0, 0, 0, dp(10));

        TextView tvTitle = new TextView(context);
        tvTitle.setText("DUO COMPARISON DECK");
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        LinearLayout.LayoutParams titleParams = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        tvTitle.setLayoutParams(titleParams);
        headerRow.addView(tvTitle);

        TextView tvSyncBadge = new TextView(context);
        tvSyncBadge.setText("● LOCKSTEP SYNC");
        tvSyncBadge.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        tvSyncBadge.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        tvSyncBadge.setTypeface(Typeface.DEFAULT_BOLD);
        tvSyncBadge.setPadding(dp(8), dp(4), dp(8), dp(4));
        tvSyncBadge.setBackground(createRoundedDrawable(Color.parseColor("#062E22"), Color.parseColor("#059669"), dp(1), dp(10)));
        headerRow.addView(tvSyncBadge);

        btnPickPair = new Button(context);
        btnPickPair.setText("VIDEOS");
        btnPickPair.setTextColor(Color.WHITE);
        btnPickPair.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnPickPair.setTypeface(Typeface.DEFAULT_BOLD);
        btnPickPair.setPadding(dp(10), dp(4), dp(10), dp(4));
        LinearLayout.LayoutParams pairParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(34));
        pairParams.setMargins(dp(6), 0, 0, 0);
        btnPickPair.setLayoutParams(pairParams);
        btnPickPair.setBackground(createRoundedDrawable(Color.parseColor("#1E293B"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(12)));
        btnPickPair.setOnClickListener(v -> {
            if (browseLibraryListener != null) {
                browseLibraryListener.onBrowseLibrary();
            } else {
                openPairLibraryDialog();
            }
        });
        headerRow.addView(btnPickPair);

        btnFullscreenToggle = new Button(context);
        btnFullscreenToggle.setText("FULLSCREEN");
        btnFullscreenToggle.setTextColor(Color.WHITE);
        btnFullscreenToggle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnFullscreenToggle.setTypeface(Typeface.DEFAULT_BOLD);
        btnFullscreenToggle.setPadding(dp(10), dp(4), dp(10), dp(4));
        LinearLayout.LayoutParams fsParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(34));
        fsParams.setMargins(dp(6), 0, 0, 0);
        btnFullscreenToggle.setLayoutParams(fsParams);
        btnFullscreenToggle.setBackground(createRoundedDrawable(Color.parseColor("#1B2A4A"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(12)));
        btnFullscreenToggle.setOnClickListener(v -> toggleFullscreen());
        headerRow.addView(btnFullscreenToggle);
        addView(headerRow);

        // ==========================================
        // 2. COMPARISON MODE TABS (4 PILLS)
        // ==========================================
        tabsRow = new LinearLayout(context);
        tabsRow.setOrientation(HORIZONTAL);
        tabsRow.setWeightSum(5);
        tabsRow.setPadding(0, 0, 0, dp(10));

        String[] modeLabels = new String[]{"Curtain", "Dual", "Peek", "Score", "Solo"};
        modeTabButtons = new TextView[5];

        for (int i = 0; i < 5; i++) {
            final int index = i;
            TextView tab = new TextView(context);
            tab.setText(modeLabels[i]);
            tab.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
            tab.setTypeface(Typeface.DEFAULT_BOLD);
            tab.setGravity(Gravity.CENTER);
            tab.setPadding(dp(4), dp(8), dp(4), dp(8));

            LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
            p.setMargins(dp(2), 0, dp(2), 0);
            tab.setLayoutParams(p);

            tab.setOnClickListener(v -> setMode(ComparisonMode.values()[index]));
            modeTabButtons[i] = tab;
            tabsRow.addView(tab);
        }
        addView(tabsRow);

        // ==========================================
        // 3. MAIN CINEMA VIDEO VIEWPORT (270dp Normal / Match Parent Fullscreen)
        // ==========================================
        playerContainer = new FrameLayout(context);
        LinearLayout.LayoutParams playerParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(270));
        playerParams.setMargins(0, 0, 0, dp(10));
        playerContainer.setLayoutParams(playerParams);
        playerContainer.setBackground(createRoundedDrawable(Color.BLACK, ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(18)));
        playerContainer.setClipToOutline(true);

        // Layer 1: Compressed Video (Bottom Base)
        containerCompressed = new FrameLayout(context);
        containerCompressed.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        textureCompressed = new TextureView(context);
        textureCompressed.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setupTextureCompressed();
        textureCompressed.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> {
            if (r - l != or - ol || b - t != ob - ot) {
                applyUnifiedTransform();
            }
        });
        // If the view is already attached and the SurfaceTexture is immediately available
        // (e.g. view recycled), mark it ready now so initFromLatch() doesn't wait forever.
        if (textureCompressed.isAvailable() && textureCompressed.getSurfaceTexture() != null) {
            surfaceCompressed  = new Surface(textureCompressed.getSurfaceTexture());
            isSurfaceCompReady = true;
        }
        containerCompressed.addView(textureCompressed);
        playerContainer.addView(containerCompressed);

        // Layer 2: Original Video (Clipped by Wipe Curtain)
        containerOriginal = new FrameLayout(context);
        containerOriginal.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        containerOriginal.setClipChildren(true);
        textureOriginal = new TextureView(context);
        textureOriginal.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        setupTextureOriginal();
        textureOriginal.addOnLayoutChangeListener((v, l, t, r, b, ol, ot, or, ob) -> {
            if (r - l != or - ol || b - t != ob - ot) {
                applyUnifiedTransform();
            }
        });
        if (textureOriginal.isAvailable() && textureOriginal.getSurfaceTexture() != null) {
            surfaceOriginal   = new Surface(textureOriginal.getSurfaceTexture());
            isSurfaceOrigReady = true;
        }
        containerOriginal.addView(textureOriginal);
        playerContainer.addView(containerOriginal);

        // Layer 3: Neon Divider Line & Pill Handle
        dividerLine = new View(context);
        dividerLine.setBackgroundColor(ThemeStyles.COLOR_CYAN_NEON);
        FrameLayout.LayoutParams dividerParams = new FrameLayout.LayoutParams(dp(2), ViewGroup.LayoutParams.MATCH_PARENT);
        dividerLine.setLayoutParams(dividerParams);
        playerContainer.addView(dividerLine);

        dividerHandle = new LinearLayout(context);
        dividerHandle.setOrientation(HORIZONTAL);
        dividerHandle.setGravity(Gravity.CENTER);
        dividerHandle.setPadding(dp(14), dp(8), dp(14), dp(8));
        dividerHandle.setBackground(createRoundedDrawable(ThemeStyles.COLOR_CYAN_PRIMARY, Color.WHITE, dp(1), dp(16)));
        TextView tvHandleIcon = new TextView(context);
        tvHandleIcon.setText("◀   ▶");
        tvHandleIcon.setTextColor(Color.WHITE);
        tvHandleIcon.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvHandleIcon.setTypeface(Typeface.DEFAULT_BOLD);
        dividerHandle.addView(tvHandleIcon);

        FrameLayout.LayoutParams handleParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        handleParams.gravity = Gravity.CENTER_VERTICAL;
        dividerHandle.setLayoutParams(handleParams);
        playerContainer.addView(dividerHandle);

        // Layer 4: Studio Stream Telemetry Cards (Left & Right) & Center Sync Delta Badge
        cardOriginal = new LinearLayout(context);
        cardOriginal.setOrientation(VERTICAL);
        cardOriginal.setPadding(dp(8), dp(5), dp(8), dp(5));
        cardOriginal.setBackground(createRoundedDrawable(Color.parseColor("#CC091020"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(10)));
        FrameLayout.LayoutParams cardOrigParams = new FrameLayout.LayoutParams(
                dp(140), ViewGroup.LayoutParams.WRAP_CONTENT);
        cardOrigParams.setMargins(dp(10), dp(10), 0, 0);
        cardOrigParams.gravity = Gravity.TOP | Gravity.START;
        cardOriginal.setLayoutParams(cardOrigParams);

        tvLabelOriginal = new TextView(context);
        tvLabelOriginal.setText("● SOURCE MASTER");
        tvLabelOriginal.setTextColor(ThemeStyles.COLOR_CYAN_NEON);
        tvLabelOriginal.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvLabelOriginal.setTypeface(Typeface.DEFAULT_BOLD);
        tvLabelOriginal.setMaxLines(1);
        tvLabelOriginal.setEllipsize(android.text.TextUtils.TruncateAt.END);
        cardOriginal.addView(tvLabelOriginal);

        tvOrigTime = new TextView(context);
        tvOrigTime.setText("⏱ 00:00.000 [F:0]");
        tvOrigTime.setTextColor(Color.WHITE);
        tvOrigTime.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvOrigTime.setTypeface(Typeface.MONOSPACE);
        cardOriginal.addView(tvOrigTime);

        tvOrigSpecs = new TextView(context);
        tvOrigSpecs.setText("Ready to compare");
        tvOrigSpecs.setTextColor(Color.parseColor("#A5F3FC"));
        tvOrigSpecs.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        tvOrigSpecs.setTypeface(Typeface.MONOSPACE);
        tvOrigSpecs.setMaxLines(1);
        tvOrigSpecs.setEllipsize(android.text.TextUtils.TruncateAt.END);
        cardOriginal.addView(tvOrigSpecs);

        tvOrigBitrate = new TextView(context);
        tvOrigBitrate.setText("Source Master");
        tvOrigBitrate.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        tvOrigBitrate.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        tvOrigBitrate.setTypeface(Typeface.MONOSPACE);
        tvOrigBitrate.setMaxLines(1);
        tvOrigBitrate.setEllipsize(android.text.TextUtils.TruncateAt.END);
        cardOriginal.addView(tvOrigBitrate);

        playerContainer.addView(cardOriginal);

        cardCompressed = new LinearLayout(context);
        cardCompressed.setOrientation(VERTICAL);
        cardCompressed.setGravity(Gravity.END);
        cardCompressed.setPadding(dp(8), dp(5), dp(8), dp(5));
        cardCompressed.setBackground(createRoundedDrawable(Color.parseColor("#CC091020"), ThemeStyles.COLOR_EMERALD_NEON, dp(1), dp(10)));
        FrameLayout.LayoutParams cardCompParams = new FrameLayout.LayoutParams(
                dp(140), ViewGroup.LayoutParams.WRAP_CONTENT);
        cardCompParams.setMargins(0, dp(10), dp(10), 0);
        cardCompParams.gravity = Gravity.TOP | Gravity.END;
        cardCompressed.setLayoutParams(cardCompParams);

        tvLabelCompressed = new TextView(context);
        tvLabelCompressed.setText("● PRISM-V OPTIMIZED");
        tvLabelCompressed.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        tvLabelCompressed.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvLabelCompressed.setTypeface(Typeface.DEFAULT_BOLD);
        tvLabelCompressed.setMaxLines(1);
        tvLabelCompressed.setEllipsize(android.text.TextUtils.TruncateAt.END);
        cardCompressed.addView(tvLabelCompressed);

        tvCompTime = new TextView(context);
        tvCompTime.setText("⏱ 00:00.000 [F:0]");
        tvCompTime.setTextColor(Color.WHITE);
        tvCompTime.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvCompTime.setTypeface(Typeface.MONOSPACE);
        cardCompressed.addView(tvCompTime);

        tvCompSpecs = new TextView(context);
        tvCompSpecs.setText("Ready to compare");
        tvCompSpecs.setTextColor(Color.parseColor("#A7F3D0"));
        tvCompSpecs.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        tvCompSpecs.setTypeface(Typeface.MONOSPACE);
        tvCompSpecs.setMaxLines(1);
        tvCompSpecs.setEllipsize(android.text.TextUtils.TruncateAt.END);
        cardCompressed.addView(tvCompSpecs);

        tvCompBitrate = new TextView(context);
        tvCompBitrate.setText("Prism-V Optimized");
        tvCompBitrate.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        tvCompBitrate.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        tvCompBitrate.setTypeface(Typeface.MONOSPACE);
        tvCompBitrate.setMaxLines(1);
        tvCompBitrate.setEllipsize(android.text.TextUtils.TruncateAt.END);
        cardCompressed.addView(tvCompBitrate);

        playerContainer.addView(cardCompressed);

        badgeCenterSync = new TextView(context);
        badgeCenterSync.setText("● LOCKSTEP Δ 0ms [F: 0]");
        badgeCenterSync.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        badgeCenterSync.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        badgeCenterSync.setTypeface(Typeface.MONOSPACE);
        badgeCenterSync.setPadding(dp(8), dp(4), dp(8), dp(4));
        badgeCenterSync.setBackground(createRoundedDrawable(Color.parseColor("#E6091020"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(10)));
        FrameLayout.LayoutParams badgeParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        badgeParams.gravity = Gravity.TOP | Gravity.CENTER_HORIZONTAL;
        badgeParams.setMargins(0, dp(10), 0, 0);
        badgeCenterSync.setLayoutParams(badgeParams);
        badgeCenterSync.setOnClickListener(v -> forceResync());
        playerContainer.addView(badgeCenterSync);

        // Layer 5: Hold-to-Peek Overlay Button
        holdToPeekOverlay = new FrameLayout(context);
        holdToPeekOverlay.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        holdToPeekOverlay.setVisibility(GONE);

        btnHoldToPeek = new Button(context);
        btnHoldToPeek.setText(" PRESS & HOLD TO PEEK SOURCE");
        btnHoldToPeek.setTextColor(Color.WHITE);
        btnHoldToPeek.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        btnHoldToPeek.setTypeface(Typeface.DEFAULT_BOLD);
        btnHoldToPeek.setBackground(createRoundedDrawable(Color.parseColor("#E6091020"), ThemeStyles.COLOR_CYAN_NEON, dp(2), dp(22)));
        FrameLayout.LayoutParams btnPeekParams = new FrameLayout.LayoutParams(
                dp(290), dp(54));
        btnPeekParams.gravity = Gravity.CENTER;
        btnHoldToPeek.setLayoutParams(btnPeekParams);

        btnHoldToPeek.setOnTouchListener((v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                    containerOriginal.setVisibility(VISIBLE);
                    btnHoldToPeek.setText("SHOWING SOURCE MASTER");
                    btnHoldToPeek.setBackground(createRoundedDrawable(Color.parseColor("#F20284C7"), Color.WHITE, dp(2), dp(22)));
                    return true;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    containerOriginal.setVisibility(GONE);
                    btnHoldToPeek.setText(" PRESS & HOLD TO PEEK SOURCE");
                    btnHoldToPeek.setBackground(createRoundedDrawable(Color.parseColor("#E6091020"), ThemeStyles.COLOR_CYAN_NEON, dp(2), dp(22)));
                    return true;
            }
            return false;
        });
        holdToPeekOverlay.addView(btnHoldToPeek);
        playerContainer.addView(holdToPeekOverlay);

        // Layer 6: Perceptual Difference Scorecard
        scorecardOverlay = createScorecardLayout(context);
        scorecardOverlay.setVisibility(GONE);
        playerContainer.addView(scorecardOverlay);

        // Layer 7: Floating Zoom Indicator Badge
        tvZoomBadge = new TextView(context);
        tvZoomBadge.setText("1.0x (FIT)");
        tvZoomBadge.setTextColor(Color.WHITE);
        tvZoomBadge.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvZoomBadge.setTypeface(Typeface.DEFAULT_BOLD);
        tvZoomBadge.setPadding(dp(8), dp(4), dp(8), dp(4));
        tvZoomBadge.setBackground(createRoundedDrawable(Color.parseColor("#CC0F172A"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(10)));
        FrameLayout.LayoutParams zoomBadgeParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        zoomBadgeParams.gravity = Gravity.BOTTOM | Gravity.START;
        zoomBadgeParams.setMargins(dp(12), 0, 0, dp(12));
        tvZoomBadge.setLayoutParams(zoomBadgeParams);
        tvZoomBadge.setVisibility(GONE);
        tvZoomBadge.setOnClickListener(v -> setZoom(1.0f));
        playerContainer.addView(tvZoomBadge);

        // Layer 8: Empty State Overlay (when no video pair is active)
        emptyDuoOverlay = new LinearLayout(context);
        emptyDuoOverlay.setOrientation(VERTICAL);
        emptyDuoOverlay.setGravity(Gravity.CENTER);
        emptyDuoOverlay.setPadding(dp(20), dp(20), dp(20), dp(20));
        emptyDuoOverlay.setBackgroundColor(Color.parseColor("#F5060D1A"));
        emptyDuoOverlay.setLayoutParams(new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));

        TextView tvEmptyIcon = new TextView(context);
        tvEmptyIcon.setText("");
        tvEmptyIcon.setTextSize(TypedValue.COMPLEX_UNIT_SP, 34);
        tvEmptyIcon.setGravity(Gravity.CENTER);
        emptyDuoOverlay.addView(tvEmptyIcon);

        TextView tvEmptyTitle = new TextView(context);
        tvEmptyTitle.setText("DUO COMPARISON READY");
        tvEmptyTitle.setTextColor(Color.WHITE);
        tvEmptyTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvEmptyTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        tvEmptyTitle.setPadding(0, dp(4), 0, dp(2));
        tvEmptyTitle.setGravity(Gravity.CENTER);
        emptyDuoOverlay.addView(tvEmptyTitle);

        TextView tvEmptyDesc = new TextView(context);
        tvEmptyDesc.setText("No video pair loaded yet. Compress a video in Studio or choose a compressed video from the list below:");
        tvEmptyDesc.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        tvEmptyDesc.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvEmptyDesc.setGravity(Gravity.CENTER);
        tvEmptyDesc.setPadding(0, 0, 0, dp(12));
        emptyDuoOverlay.addView(tvEmptyDesc);

        LinearLayout rowEmptyBtns = new LinearLayout(context);
        rowEmptyBtns.setOrientation(HORIZONTAL);
        rowEmptyBtns.setGravity(Gravity.CENTER);

        btnEmptyLibrary = new Button(context);
        btnEmptyLibrary.setText("BROWSE COMPRESSED VIDEOS");
        btnEmptyLibrary.setTextColor(Color.WHITE);
        btnEmptyLibrary.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnEmptyLibrary.setTypeface(Typeface.DEFAULT_BOLD);
        btnEmptyLibrary.setBackground(createRoundedDrawable(Color.parseColor("#1E293B"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(10)));
        btnEmptyLibrary.setPadding(dp(12), dp(6), dp(12), dp(6));
        btnEmptyLibrary.setOnClickListener(v -> {
            if (browseLibraryListener != null) {
                browseLibraryListener.onBrowseLibrary();
            } else {
                openPairLibraryDialog();
            }
        });
        rowEmptyBtns.addView(btnEmptyLibrary);



        emptyDuoOverlay.addView(rowEmptyBtns);
        playerContainer.addView(emptyDuoOverlay);

        // Layer 8: Fullscreen Cinema HUD Overlay (Floating Controls)
        setupCinemaOverlay(context);

        // Touch Listener: Handles Curtain drag and Pan when zoomed
        setupTouchInteractions();

        addView(playerContainer);

        // ==========================================
        // 4. ZOOM CONTROLS ROW & QUICK SNAPPERS
        // ==========================================
        curtainBar = new LinearLayout(context);
        curtainBar.setOrientation(HORIZONTAL);
        curtainBar.setGravity(Gravity.CENTER_VERTICAL);
        curtainBar.setPadding(0, 0, 0, dp(8));

        tvSplitRatio = new TextView(context);
        tvSplitRatio.setText("SOURCE 50%  ◀━━━ ● ━━━▶  PRISM-V 50%");
        tvSplitRatio.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvSplitRatio.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvSplitRatio.setTypeface(Typeface.MONOSPACE);
        LinearLayout.LayoutParams ratioParams = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        tvSplitRatio.setLayoutParams(ratioParams);
        curtainBar.addView(tvSplitRatio);

        // Zoom Selector Pills: [1.0x] [2.0x] [4.0x] [8.0x]
        float[] zoomLevels = new float[]{1.0f, 2.0f, 4.0f, 8.0f};
        String[] zoomLabels = new String[]{"1x", "2x Loupe", "4x Micro", "8x Ultra"};
        zoomButtons = new TextView[zoomLevels.length];
        for (int i = 0; i < zoomLevels.length; i++) {
            final float z = zoomLevels[i];
            final int zIdx = i;
            TextView btnZ = new TextView(context);
            btnZ.setText(zoomLabels[i]);
            btnZ.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
            btnZ.setTypeface(Typeface.DEFAULT_BOLD);
            btnZ.setPadding(dp(8), dp(4), dp(8), dp(4));
            updateZoomPillStyle(btnZ, z == currentZoom);
            LinearLayout.LayoutParams zp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            zp.setMargins(dp(4), 0, 0, 0);
            btnZ.setLayoutParams(zp);
            btnZ.setOnClickListener(v -> setZoom(z));
            zoomButtons[i] = btnZ;
            curtainBar.addView(btnZ);
        }
        addView(curtainBar);

        // ==========================================
        // 5. MASTER TRANSPORT DECK & FRAME STEPPERS
        // ==========================================
        controlsCard = new LinearLayout(context);
        controlsCard.setOrientation(VERTICAL);
        controlsCard.setPadding(dp(14), dp(12), dp(14), dp(12));
        controlsCard.setBackground(ThemeStyles.createCardDrawable(context, false));

        // Time Scrubber Row
        LinearLayout timeRow = new LinearLayout(context);
        timeRow.setOrientation(HORIZONTAL);
        timeRow.setGravity(Gravity.CENTER_VERTICAL);
        timeRow.setPadding(0, 0, 0, dp(4));

        masterSeekBar = new SeekBar(context);
        masterSeekBar.setMax(1000);
        masterSeekBar.setProgress(0);
        LinearLayout.LayoutParams seekParams = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        masterSeekBar.setLayoutParams(seekParams);
        masterSeekBar.setOnSeekBarChangeListener(createScrubListener());
        masterSeekBar.setOnTouchListener(createSeekBarTouchGuard()); // Bug 2 fix
        timeRow.addView(masterSeekBar);

        tvTimeDisplay = new TextView(context);
        tvTimeDisplay.setText("00:00 / 00:00");
        tvTimeDisplay.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvTimeDisplay.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvTimeDisplay.setTypeface(Typeface.MONOSPACE);
        tvTimeDisplay.setPadding(dp(8), 0, 0, 0);
        timeRow.addView(tvTimeDisplay);
        controlsCard.addView(timeRow);

        // Transport Buttons Row
        LinearLayout transportRow = new LinearLayout(context);
        transportRow.setOrientation(HORIZONTAL);
        transportRow.setGravity(Gravity.CENTER_VERTICAL);
        transportRow.setPadding(0, dp(4), 0, dp(6));

        Button btnFrameBack = createTransportButton(context, "⏮ 1F", v -> stepFrame(-33));
        transportRow.addView(btnFrameBack);

        btnStepBack = createTransportButton(context, "⏪ 100m", v -> stepFrame(-100));
        transportRow.addView(btnStepBack);

        btnPlayPause = new Button(context);
        btnPlayPause.setText("▶  PLAY SYNC");
        btnPlayPause.setTextColor(Color.WHITE);
        btnPlayPause.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnPlayPause.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        LinearLayout.LayoutParams playParams = new LinearLayout.LayoutParams(0, dp(46), 1.5f);
        playParams.setMargins(dp(4), 0, dp(4), 0);
        btnPlayPause.setLayoutParams(playParams);
        btnPlayPause.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#059669"), Color.parseColor("#10B981"), dp(14)));
        btnPlayPause.setOnClickListener(v -> togglePlayPause());
        transportRow.addView(btnPlayPause);

        btnStepForward = createTransportButton(context, "100m ⏩", v -> stepFrame(100));
        transportRow.addView(btnStepForward);

        Button btnFrameForward = createTransportButton(context, "1F ⏭", v -> stepFrame(33));
        transportRow.addView(btnFrameForward);
        controlsCard.addView(transportRow);

        // Speed & Audio Row
        LinearLayout actionRow = new LinearLayout(context);
        actionRow.setOrientation(HORIZONTAL);
        actionRow.setGravity(Gravity.CENTER_VERTICAL);
        actionRow.setPadding(0, dp(4), 0, 0);

        btnAudioToggle = new Button(context);
        btnAudioToggle.setText("Audio: Compressed (Opus)");
        btnAudioToggle.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        btnAudioToggle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnAudioToggle.setTypeface(Typeface.DEFAULT_BOLD);
        LinearLayout.LayoutParams audioParams = new LinearLayout.LayoutParams(0, dp(40), 1.3f);
        btnAudioToggle.setLayoutParams(audioParams);
        btnAudioToggle.setBackground(createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(12)));
        btnAudioToggle.setOnClickListener(v -> toggleAudioTrack());
        actionRow.addView(btnAudioToggle);

        // Playback Speed Selector Pills
        LinearLayout speedRow = new LinearLayout(context);
        speedRow.setOrientation(HORIZONTAL);
        speedRow.setGravity(Gravity.CENTER_VERTICAL);
        LinearLayout.LayoutParams spContParams = new LinearLayout.LayoutParams(0, dp(40), 1.2f);
        spContParams.setMargins(dp(6), 0, 0, 0);
        speedRow.setLayoutParams(spContParams);

        float[] speeds = new float[]{0.25f, 0.5f, 1.0f, 2.0f};
        String[] speedLabels = new String[]{"0.25x", "0.5x", "1.0x", "2.0x"};
        speedButtons = new TextView[speeds.length];

        for (int i = 0; i < speeds.length; i++) {
            final float spd = speeds[i];
            final int spdIdx = i;
            TextView btnSpd = new TextView(context);
            btnSpd.setText(speedLabels[i]);
            btnSpd.setTextColor(spd == 1.0f ? Color.WHITE : ThemeStyles.COLOR_TEXT_MUTED);
            btnSpd.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
            btnSpd.setTypeface(Typeface.DEFAULT_BOLD);
            btnSpd.setGravity(Gravity.CENTER);
            btnSpd.setPadding(dp(4), dp(8), dp(4), dp(8));
            btnSpd.setBackground(createRoundedDrawable(spd == 1.0f ? ThemeStyles.COLOR_CYAN_PRIMARY : Color.parseColor("#121B2D"),
                    ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(10)));
            LinearLayout.LayoutParams spdParams = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f);
            spdParams.setMargins(dp(2), 0, dp(2), 0);
            btnSpd.setLayoutParams(spdParams);
            btnSpd.setOnClickListener(v -> setPlaybackSpeed(spd, spdIdx));
            speedButtons[i] = btnSpd;
            speedRow.addView(btnSpd);
        }
        actionRow.addView(speedRow);
        controlsCard.addView(actionRow);
        addView(controlsCard);

        // ==========================================
        // 6. DUAL STREAM TELEMETRY STUDIO DECK
        // ==========================================
        telemetryCard = new LinearLayout(context);
        telemetryCard.setOrientation(VERTICAL);
        telemetryCard.setPadding(dp(14), dp(12), dp(14), dp(12));
        LinearLayout.LayoutParams teleCardParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        teleCardParams.setMargins(0, dp(10), 0, 0);
        telemetryCard.setLayoutParams(teleCardParams);
        telemetryCard.setBackground(createRoundedDrawable(Color.parseColor("#091020"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(16)));

        TextView tvTeleTitle = new TextView(context);
        tvTeleTitle.setText("STREAM TELEMETRY & SPECIFICATION");
        tvTeleTitle.setTextColor(ThemeStyles.COLOR_CYAN_NEON);
        tvTeleTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvTeleTitle.setTypeface(Typeface.DEFAULT_BOLD);
        telemetryCard.addView(tvTeleTitle);

        LinearLayout rowStreams = new LinearLayout(context);
        rowStreams.setOrientation(HORIZONTAL);
        rowStreams.setWeightSum(2);
        rowStreams.setPadding(0, dp(8), 0, dp(4));

        // Column 1: Source
        LinearLayout colOrig = new LinearLayout(context);
        colOrig.setOrientation(VERTICAL);
        colOrig.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        TextView tvOrigHdr = new TextView(context);
        tvOrigHdr.setText("● SOURCE MASTER");
        tvOrigHdr.setTextColor(Color.WHITE);
        tvOrigHdr.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvOrigHdr.setTypeface(Typeface.DEFAULT_BOLD);
        colOrig.addView(tvOrigHdr);

        tvTelemetryOrig = new TextView(context);
        tvTelemetryOrig.setText("• Master Source\n• Dynamic Resolution\n• Full Audio Fidelity\n• Lockstep Timing");
        tvTelemetryOrig.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvTelemetryOrig.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvTelemetryOrig.setTypeface(Typeface.MONOSPACE);
        tvTelemetryOrig.setPadding(0, dp(4), 0, 0);
        colOrig.addView(tvTelemetryOrig);
        rowStreams.addView(colOrig);

        // Column 2: Compressed
        LinearLayout colComp = new LinearLayout(context);
        colComp.setOrientation(VERTICAL);
        colComp.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        TextView tvCompHdr = new TextView(context);
        tvCompHdr.setText("● PRISM-V NEURAL");
        tvCompHdr.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        tvCompHdr.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvCompHdr.setTypeface(Typeface.DEFAULT_BOLD);
        colComp.addView(tvCompHdr);

        tvTelemetryComp = new TextView(context);
        tvTelemetryComp.setText("• Hardware Transcoded\n• High Storage Savings\n• Quality metrics: not measured");
        tvTelemetryComp.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvTelemetryComp.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvTelemetryComp.setTypeface(Typeface.MONOSPACE);
        tvTelemetryComp.setPadding(0, dp(4), 0, 0);
        colComp.addView(tvTelemetryComp);
        rowStreams.addView(colComp);

        telemetryCard.addView(rowStreams);
        addView(telemetryCard);

        // Status Banner
        tvStatusBanner = new TextView(context);
        tvStatusBanner.setText("Drag divider to wipe. Use 2x/4x Zoom to pixel-peep fine compression artifacts.");
        tvStatusBanner.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        tvStatusBanner.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvStatusBanner.setGravity(Gravity.CENTER);
        tvStatusBanner.setPadding(0, dp(10), 0, 0);
        addView(tvStatusBanner);

        // Set default mode (Side-by-Side Dual View Left/Right)
        setMode(ComparisonMode.SIDE_BY_SIDE);

        // Progress updater with smooth 30fps cadence (33ms) - eliminates Binder IPC starvation
        progressUpdater = new Runnable() {
            @Override
            public void run() {
                updateProgress();
                if (isPlaying) {
                    handler.postDelayed(this, 60);
                }
            }
        };

        playerContainer.post(() -> {
            int w = playerContainer.getWidth();
            if (w > 0) updateCurtainPosition(w / 2);
            applyUnifiedTransform();
        });

        playerContainer.addOnLayoutChangeListener((v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
            if (right - left != oldRight - oldLeft || bottom - top != oldBottom - oldTop) {
                applyUnifiedTransform();
            }
        });
        // Start in clean empty state with emptyDuoOverlay visible (no auto-selected video)
        updateLibraryCountBadge();
        showEmptyOverlay();
    }

    public void updateLibraryCountBadge() {
        int count = 0;
        try {
            java.util.List<File> diskFiles = com.prismv.storage.PrismStorageManager.listSavedVideos(getContext());
            count = diskFiles.size();
        } catch (Exception ignored) {}
        if (btnPickPair != null) {
            btnPickPair.setText(count > 0 ? ("VIDEOS (" + count + ")") : "VIDEOS");
        }
        if (btnEmptyLibrary != null) {
            btnEmptyLibrary.setText(count > 0 ? ("BROWSE COMPRESSED VIDEOS (" + count + ")") : "BROWSE COMPRESSED VIDEOS");
        }
    }

    public void showEmptyOverlay() {
        if (emptyDuoOverlay != null) {
            emptyDuoOverlay.setVisibility(VISIBLE);
            emptyDuoOverlay.bringToFront();
        }
        if (cardOriginal != null) cardOriginal.setVisibility(GONE);
        if (cardCompressed != null) cardCompressed.setVisibility(GONE);
        if (dividerLine != null) dividerLine.setVisibility(GONE);
        if (dividerHandle != null) dividerHandle.setVisibility(GONE);
        if (badgeCenterSync != null) badgeCenterSync.setVisibility(GONE);
        if (tvSplitRatio != null) tvSplitRatio.setVisibility(GONE);
    }

    public void hideEmptyOverlay() {
        if (emptyDuoOverlay != null) {
            emptyDuoOverlay.setVisibility(GONE);
        }
        if (cardOriginal != null) cardOriginal.setVisibility(VISIBLE);
        if (cardCompressed != null) cardCompressed.setVisibility(VISIBLE);
        if (dividerLine != null) dividerLine.setVisibility(VISIBLE);
        if (dividerHandle != null) dividerHandle.setVisibility(VISIBLE);
        if (badgeCenterSync != null) badgeCenterSync.setVisibility(VISIBLE);
        if (tvSplitRatio != null) tvSplitRatio.setVisibility(VISIBLE);
    }

    // ==========================================
    // CINEMA FULLSCREEN OVERLAYS
    // ==========================================
    private void setupCinemaOverlay(Context context) {
        // Floating Top Cinema Bar
        cinemaTopBar = new LinearLayout(context);
        cinemaTopBar.setOrientation(HORIZONTAL);
        cinemaTopBar.setGravity(Gravity.CENTER_VERTICAL);
        cinemaTopBar.setPadding(dp(14), dp(10), dp(14), dp(10));
        cinemaTopBar.setBackground(createRoundedDrawable(Color.parseColor("#D9091020"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(14)));
        cinemaTopBar.setClickable(true);
        cinemaTopBar.setFocusable(true);
        FrameLayout.LayoutParams topParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        topParams.gravity = Gravity.TOP;
        topParams.setMargins(dp(10), dp(10), dp(10), 0);
        cinemaTopBar.setLayoutParams(topParams);
        cinemaTopBar.setVisibility(GONE);

        TextView tvCinemaTitle = new TextView(context);
        tvCinemaTitle.setText("DUO CINEMA");
        tvCinemaTitle.setTextColor(Color.WHITE);
        tvCinemaTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvCinemaTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        cinemaTopBar.addView(tvCinemaTitle);

        View flexSpacer = new View(context);
        cinemaTopBar.addView(flexSpacer, new LinearLayout.LayoutParams(0, 0, 1.0f));

        // Cinema Zoom pills
        float[] zoomLevels = new float[]{1.0f, 2.0f, 4.0f, 8.0f};
        String[] zoomLabels = new String[]{"1x", "2x", "4x", "8x"};
        cinemaZoomButtons = new TextView[zoomLevels.length];
        for (int i = 0; i < zoomLevels.length; i++) {
            final float z = zoomLevels[i];
            TextView btnZ = new TextView(context);
            btnZ.setText(zoomLabels[i]);
            btnZ.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
            btnZ.setTypeface(Typeface.DEFAULT_BOLD);
            btnZ.setPadding(dp(8), dp(4), dp(8), dp(4));
            updateZoomPillStyle(btnZ, z == currentZoom);
            LinearLayout.LayoutParams zp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            zp.setMargins(dp(2), 0, dp(2), 0);
            btnZ.setLayoutParams(zp);
            btnZ.setOnClickListener(v -> setZoom(z));
            cinemaZoomButtons[i] = btnZ;
            cinemaTopBar.addView(btnZ);
        }

        Button btnCinemaPair = new Button(context);
        btnCinemaPair.setText("PAIR");
        btnCinemaPair.setTextColor(Color.WHITE);
        btnCinemaPair.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnCinemaPair.setTypeface(Typeface.DEFAULT_BOLD);
        btnCinemaPair.setPadding(dp(8), dp(4), dp(8), dp(4));
        LinearLayout.LayoutParams cpParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(34));
        cpParams.setMargins(dp(4), 0, 0, 0);
        btnCinemaPair.setLayoutParams(cpParams);
        btnCinemaPair.setBackground(createRoundedDrawable(Color.parseColor("#1E293B"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(10)));
        btnCinemaPair.setOnClickListener(v -> openPairLibraryDialog());
        cinemaTopBar.addView(btnCinemaPair);

        Button btnExitFs = new Button(context);
        btnExitFs.setText("EXIT");
        btnExitFs.setTextColor(Color.WHITE);
        btnExitFs.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnExitFs.setTypeface(Typeface.DEFAULT_BOLD);
        btnExitFs.setPadding(dp(14), dp(6), dp(14), dp(6));
        btnExitFs.setMinWidth(dp(80));
        btnExitFs.setMinHeight(dp(40));
        LinearLayout.LayoutParams exitParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        exitParams.setMargins(dp(8), 0, 0, 0);
        btnExitFs.setLayoutParams(exitParams);
        btnExitFs.setBackground(createRoundedDrawable(Color.parseColor("#B91C1C"), Color.parseColor("#EF4444"), dp(1), dp(10)));
        btnExitFs.setOnClickListener(v -> exitFullscreen());
        cinemaTopBar.addView(btnExitFs);
        playerContainer.addView(cinemaTopBar);

        // Floating Bottom Cinema Bar
        cinemaBottomBar = new LinearLayout(context);
        cinemaBottomBar.setOrientation(VERTICAL);
        cinemaBottomBar.setPadding(dp(14), dp(10), dp(14), dp(10));
        cinemaBottomBar.setBackground(createRoundedDrawable(Color.parseColor("#D9091020"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(16)));
        cinemaBottomBar.setClickable(true);
        cinemaBottomBar.setFocusable(true);
        FrameLayout.LayoutParams btmParams = new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        btmParams.gravity = Gravity.BOTTOM;
        btmParams.setMargins(dp(10), 0, dp(10), dp(10));
        cinemaBottomBar.setLayoutParams(btmParams);
        cinemaBottomBar.setVisibility(GONE);

        // Scrubber line
        LinearLayout cTimeRow = new LinearLayout(context);
        cTimeRow.setOrientation(HORIZONTAL);
        cTimeRow.setGravity(Gravity.CENTER_VERTICAL);

        cinemaSeekBar = new SeekBar(context);
        cinemaSeekBar.setMax(1000);
        cinemaSeekBar.setProgress(0);
        cinemaSeekBar.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
        cinemaSeekBar.setOnSeekBarChangeListener(createScrubListener());
        cinemaSeekBar.setOnTouchListener(createSeekBarTouchGuard()); // Bug 2 fix
        cTimeRow.addView(cinemaSeekBar);

        cinemaTimeDisplay = new TextView(context);
        cinemaTimeDisplay.setText("00:00 / 00:00");
        cinemaTimeDisplay.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        cinemaTimeDisplay.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        cinemaTimeDisplay.setTypeface(Typeface.MONOSPACE);
        cinemaTimeDisplay.setPadding(dp(8), 0, 0, 0);
        cTimeRow.addView(cinemaTimeDisplay);
        cinemaBottomBar.addView(cTimeRow);

        // Buttons line
        LinearLayout cBtnRow = new LinearLayout(context);
        cBtnRow.setOrientation(HORIZONTAL);
        cBtnRow.setGravity(Gravity.CENTER_VERTICAL);
        cBtnRow.setPadding(0, dp(4), 0, 0);

        Button cFrameBack = createTransportButton(context, "⏮ 1F", v -> stepFrame(-33));
        cBtnRow.addView(cFrameBack);

        Button cStepBack = createTransportButton(context, "⏪ 100m", v -> stepFrame(-100));
        cBtnRow.addView(cStepBack);

        btnCinemaPlayPause = new Button(context);
        btnCinemaPlayPause.setText("▶  PLAY");
        btnCinemaPlayPause.setTextColor(Color.WHITE);
        btnCinemaPlayPause.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnCinemaPlayPause.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        LinearLayout.LayoutParams cPlayParams = new LinearLayout.LayoutParams(0, dp(42), 1.5f);
        cPlayParams.setMargins(dp(4), 0, dp(4), 0);
        btnCinemaPlayPause.setLayoutParams(cPlayParams);
        btnCinemaPlayPause.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#059669"), Color.parseColor("#10B981"), dp(12)));
        btnCinemaPlayPause.setOnClickListener(v -> togglePlayPause());
        cBtnRow.addView(btnCinemaPlayPause);

        Button cStepForward = createTransportButton(context, "100m ⏩", v -> stepFrame(100));
        cBtnRow.addView(cStepForward);

        Button cFrameForward = createTransportButton(context, "1F ⏭", v -> stepFrame(33));
        cBtnRow.addView(cFrameForward);

        btnCinemaAudio = new Button(context);
        btnCinemaAudio.setText("Opus");
        btnCinemaAudio.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        btnCinemaAudio.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnCinemaAudio.setTypeface(Typeface.DEFAULT_BOLD);
        LinearLayout.LayoutParams cAudParams = new LinearLayout.LayoutParams(dp(80), dp(42));
        cAudParams.setMargins(dp(6), 0, 0, 0);
        btnCinemaAudio.setLayoutParams(cAudParams);
        btnCinemaAudio.setBackground(createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(10)));
        btnCinemaAudio.setOnClickListener(v -> toggleAudioTrack());
        cBtnRow.addView(btnCinemaAudio);

        cinemaBottomBar.addView(cBtnRow);
        playerContainer.addView(cinemaBottomBar);
    }

    // ==========================================
    // TOUCH, DRAG, PINCH-ZOOM & PAN INTERACTIONS
    // ==========================================
    private void setupTouchInteractions() {
        if (dividerHandle != null) {
            dividerHandle.setOnTouchListener((dhView, ev) -> {
                if (emptyDuoOverlay != null && emptyDuoOverlay.getVisibility() == View.VISIBLE) return false;
                if (dhView.getParent() != null) dhView.getParent().requestDisallowInterceptTouchEvent(true);
                switch (ev.getActionMasked()) {
                    case MotionEvent.ACTION_DOWN:
                    case MotionEvent.ACTION_MOVE:
                        int[] containerLoc = new int[2];
                        playerContainer.getLocationOnScreen(containerLoc);
                        int curX = (int) (ev.getRawX() - containerLoc[0]);
                        int parentW = playerContainer.getWidth();
                        if (parentW > 0) {
                            updateCurtainPosition(Math.max(10, Math.min(parentW - 10, curX)));
                        }
                        return true;
                    case MotionEvent.ACTION_UP:
                    case MotionEvent.ACTION_CANCEL:
                        if (dhView.getParent() != null) dhView.getParent().requestDisallowInterceptTouchEvent(false);
                        return true;
                }
                return false;
            });
        }

        playerContainer.setOnTouchListener((v, event) -> {
            if (emptyDuoOverlay != null && emptyDuoOverlay.getVisibility() == View.VISIBLE) {
                return false;
            }
            if (scaleGestureDetector != null) {
                scaleGestureDetector.onTouchEvent(event);
            }
            if (scaleGestureDetector != null && scaleGestureDetector.isInProgress()) {
                if (v.getParent() != null) v.getParent().requestDisallowInterceptTouchEvent(true);
                return true;
            }

            int w = playerContainer.getWidth();
            int h = playerContainer.getHeight();

            switch (event.getActionMasked()) {
                case MotionEvent.ACTION_DOWN:
                    lastTouchX = event.getX();
                    lastTouchY = event.getY();
                    isPanning = (currentZoom > 1.05f);
                    if (v.getParent() != null) {
                        v.getParent().requestDisallowInterceptTouchEvent(true);
                    }
                    if (!isPanning && (currentMode == ComparisonMode.SPLIT_CURTAIN || currentMode == ComparisonMode.SIDE_BY_SIDE) && w > 0) {
                        int x = (int) event.getX();
                        updateCurtainPosition(Math.max(10, Math.min(w - 10, x)));
                        return true;
                    }
                    if (isPanning) {
                        return true;
                    }
                    return false;

                case MotionEvent.ACTION_MOVE:
                    float dx = event.getX() - lastTouchX;
                    float dy = event.getY() - lastTouchY;
                    lastTouchX = event.getX();
                    lastTouchY = event.getY();

                    if (v.getParent() != null) {
                        v.getParent().requestDisallowInterceptTouchEvent(true);
                    }

                    if (currentZoom > 1.05f && event.getPointerCount() == 1) {
                        // Pan zoomed textures
                        panX += dx;
                        panY += dy;
                        applyZoomTransform();
                        return true;
                    } else if ((currentMode == ComparisonMode.SPLIT_CURTAIN || currentMode == ComparisonMode.SIDE_BY_SIDE) && w > 0 && event.getPointerCount() == 1) {
                        int x = (int) event.getX();
                        updateCurtainPosition(Math.max(10, Math.min(w - 10, x)));
                        return true;
                    }
                    break;

                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (v.getParent() != null) {
                        v.getParent().requestDisallowInterceptTouchEvent(false);
                    }
                    isPanning = false;
                    break;
            }
            return false;
        });
    }

    // ==========================================
    // FULLSCREEN TOGGLE ENGINE
    // ==========================================
    public void toggleFullscreen() {
        if (isFullscreen) {
            exitFullscreen();
        } else {
            enterFullscreen();
        }
    }

    public void enterFullscreen() {
        if (isFullscreen) return;
        isFullscreen = true;

        // Notify parent activity to hide top navTabs
        if (fullscreenListener != null) {
            fullscreenListener.onFullscreenToggled(true);
        }

        // Hide standard outer cards
        setPadding(0, 0, 0, 0);
        headerRow.setVisibility(GONE);
        tabsRow.setVisibility(GONE);
        curtainBar.setVisibility(GONE);
        controlsCard.setVisibility(GONE);
        telemetryCard.setVisibility(GONE);
        tvStatusBanner.setVisibility(GONE);

        // Expand player container to fill viewport
        LinearLayout.LayoutParams playerParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        playerParams.setMargins(0, 0, 0, 0);
        playerContainer.setLayoutParams(playerParams);
        playerContainer.setBackgroundColor(Color.BLACK);

        // Adjust Telemetry Cards so they sit gracefully below Cinema Top Bar
        if (cardOriginal != null && cardOriginal.getLayoutParams() instanceof FrameLayout.LayoutParams) {
            FrameLayout.LayoutParams p = (FrameLayout.LayoutParams) cardOriginal.getLayoutParams();
            p.width = dp(132);
            p.setMargins(dp(12), dp(58), 0, 0);
            cardOriginal.setLayoutParams(p);
        }
        if (cardCompressed != null && cardCompressed.getLayoutParams() instanceof FrameLayout.LayoutParams) {
            FrameLayout.LayoutParams p = (FrameLayout.LayoutParams) cardCompressed.getLayoutParams();
            p.width = dp(132);
            p.setMargins(0, dp(58), dp(12), 0);
            cardCompressed.setLayoutParams(p);
        }
        if (badgeCenterSync != null && badgeCenterSync.getLayoutParams() instanceof FrameLayout.LayoutParams) {
            FrameLayout.LayoutParams p = (FrameLayout.LayoutParams) badgeCenterSync.getLayoutParams();
            p.setMargins(0, dp(58), 0, 0);
            badgeCenterSync.setLayoutParams(p);
        }

        // Show floating cinema HUD
        cinemaTopBar.setVisibility(VISIBLE);
        cinemaBottomBar.setVisibility(VISIBLE);
        cinemaTopBar.bringToFront();
        cinemaBottomBar.bringToFront();

        post(() -> {
            int w = playerContainer.getWidth();
            if (w > 0) updateCurtainPosition(w / 2);
            applyUnifiedTransform();
        });

        if (getContext() instanceof Activity) {
            Activity act = (Activity) getContext();
            act.setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
            PrismHud.showSuccess(act, "Widescreen Cinema Mode (Left/Right Compare)");
        }
    }

    public void exitFullscreen() {
        if (!isFullscreen) return;
        isFullscreen = false;

        if (getContext() instanceof Activity) {
            Activity act = (Activity) getContext();
            act.setRequestedOrientation(android.content.pm.ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
        }

        // Restore padding & standard views
        setPadding(dp(14), dp(8), dp(14), dp(24));
        headerRow.setVisibility(VISIBLE);
        tabsRow.setVisibility(VISIBLE);
        curtainBar.setVisibility(VISIBLE);
        controlsCard.setVisibility(VISIBLE);
        telemetryCard.setVisibility(VISIBLE);
        tvStatusBanner.setVisibility(VISIBLE);

        // Restore player container height
        LinearLayout.LayoutParams playerParams = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(270));
        playerParams.setMargins(0, 0, 0, dp(10));
        playerContainer.setLayoutParams(playerParams);
        playerContainer.setBackground(createRoundedDrawable(Color.BLACK, ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(18)));

        // Restore Telemetry Cards normal top margins
        if (cardOriginal != null && cardOriginal.getLayoutParams() instanceof FrameLayout.LayoutParams) {
            FrameLayout.LayoutParams p = (FrameLayout.LayoutParams) cardOriginal.getLayoutParams();
            p.width = dp(132);
            p.setMargins(dp(10), dp(10), 0, 0);
            cardOriginal.setLayoutParams(p);
        }
        if (cardCompressed != null && cardCompressed.getLayoutParams() instanceof FrameLayout.LayoutParams) {
            FrameLayout.LayoutParams p = (FrameLayout.LayoutParams) cardCompressed.getLayoutParams();
            p.width = dp(132);
            p.setMargins(0, dp(10), dp(10), 0);
            cardCompressed.setLayoutParams(p);
        }
        if (badgeCenterSync != null && badgeCenterSync.getLayoutParams() instanceof FrameLayout.LayoutParams) {
            FrameLayout.LayoutParams p = (FrameLayout.LayoutParams) badgeCenterSync.getLayoutParams();
            p.setMargins(0, dp(10), 0, 0);
            badgeCenterSync.setLayoutParams(p);
        }

        // Hide floating cinema HUD
        cinemaTopBar.setVisibility(GONE);
        cinemaBottomBar.setVisibility(GONE);

        // Notify parent activity to restore top navTabs
        if (fullscreenListener != null) {
            fullscreenListener.onFullscreenToggled(false);
        }

        post(() -> {
            int w = playerContainer.getWidth();
            if (w > 0) updateCurtainPosition(w / 2);
            applyZoomTransform();
        });
    }

    // ==========================================
    // ZOOM & PIXEL-PEEP ENGINE
    // ==========================================
    public void setZoom(float zoom) {
        this.currentZoom = zoom;
        if (zoom <= 1.0f) {
            panX = 0;
            panY = 0;
            tvZoomBadge.setVisibility(GONE);
        } else {
            tvZoomBadge.setVisibility(VISIBLE);
            tvZoomBadge.setText(String.format(Locale.US, "%.1fx LOUPE (DRAG TO PAN)", currentZoom));
        }

        // Update pills in both standard and cinema rows
        float[] zoomLevels = new float[]{1.0f, 2.0f, 4.0f, 8.0f};
        for (int i = 0; i < zoomLevels.length; i++) {
            boolean active = Math.abs(zoomLevels[i] - currentZoom) < 0.25f;
            if (zoomButtons != null && i < zoomButtons.length) {
                updateZoomPillStyle(zoomButtons[i], active);
            }
            if (cinemaZoomButtons != null && i < cinemaZoomButtons.length) {
                updateZoomPillStyle(cinemaZoomButtons[i], active);
            }
        }

        applyZoomTransform();

        if (getContext() instanceof Activity && zoom > 1.0f) {
            PrismHud.showInfo((Activity) getContext(), String.format(Locale.US, "Zoom: %.1fx (Inspect Pixels)", zoom));
        }
    }

    private void updateZoomPillStyle(TextView btn, boolean active) {
        btn.setTextColor(active ? Color.WHITE : ThemeStyles.COLOR_TEXT_MUTED);
        btn.setBackground(createRoundedDrawable(
                active ? ThemeStyles.COLOR_CYAN_PRIMARY : Color.parseColor("#121B2D"),
                active ? Color.WHITE : ThemeStyles.COLOR_BORDER_SUBTLE,
                dp(1), dp(10)));
    }

    public void applyUnifiedTransform() {
        int w = playerContainer.getWidth();
        int h = playerContainer.getHeight();
        if (w <= 0 || h <= 0) return;

        if (currentMode == ComparisonMode.SIDE_BY_SIDE) {
            int origW = (containerOriginal != null && containerOriginal.getWidth() > 0) ? containerOriginal.getWidth() : w / 2;
            int compW = (containerCompressed != null && containerCompressed.getWidth() > 0) ? containerCompressed.getWidth() : w / 2;
            applySingleTextureMatrix(textureOriginal, origMeta, origW, h);
            applySingleTextureMatrix(textureCompressed, compMeta, compW, h);
            return;
        }

        // Determine master aspect ratio: Prefer Source Master, fallback to Compressed or 16:9
        int refDW = (origMeta != null && origMeta.displayWidth > 0) ? origMeta.displayWidth :
                ((compMeta != null && compMeta.displayWidth > 0) ? compMeta.displayWidth : 1080);
        int refDH = (origMeta != null && origMeta.displayHeight > 0) ? origMeta.displayHeight :
                ((compMeta != null && compMeta.displayHeight > 0) ? compMeta.displayHeight : 1920);

        float refAspect = (float) refDW / (float) refDH;
        float containerAspect = (float) w / (float) h;
        float fitW, fitH;

        if (refAspect > containerAspect) {
            fitW = w;
            fitH = w / refAspect;
        } else {
            fitH = h;
            fitW = h * refAspect;
        }

        // Clamp pan offsets so video remains inside viewing canvas
        float maxPanX = (w * (currentZoom - 1.0f)) / 2f;
        float maxPanY = (h * (currentZoom - 1.0f)) / 2f;
        if (panX > maxPanX) panX = maxPanX;
        if (panX < -maxPanX) panX = -maxPanX;
        if (panY > maxPanY) panY = maxPanY;
        if (panY < -maxPanY) panY = -maxPanY;

        applyMatrixToTexture(textureOriginal, origMeta, w, h, fitW, fitH);
        applyMatrixToTexture(textureCompressed, compMeta, w, h, fitW, fitH);
    }

    public void refreshPausedFrames() {
        if (!isPlaying) {
            try {
                if (playerOriginal != null && isOriginalPrepared) {
                    int p = playerOriginal.getCurrentPosition();
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        playerOriginal.seekTo((long) p, MediaPlayer.SEEK_CLOSEST);
                    } else {
                        playerOriginal.seekTo(p);
                    }
                }
            } catch (Exception ignored) {}
            try {
                if (playerCompressed != null && isCompressedPrepared) {
                    int p = playerCompressed.getCurrentPosition();
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                        playerCompressed.seekTo((long) p, MediaPlayer.SEEK_CLOSEST);
                    } else {
                        playerCompressed.seekTo(p);
                    }
                }
            } catch (Exception ignored) {}
        }
    }

    private void applySingleTextureMatrix(TextureView tv, VideoMeta meta, int targetW, int targetH) {
        if (tv == null || targetW <= 0 || targetH <= 0) return;
        int dw = (meta != null && meta.displayWidth > 0) ? meta.displayWidth : 1080;
        int dh = (meta != null && meta.displayHeight > 0) ? meta.displayHeight : 1920;
        float aspect = (float) dw / (float) dh;
        float containerAspect = (float) targetW / (float) targetH;
        float fitW, fitH;
        if (aspect > containerAspect) {
            fitW = targetW;
            fitH = targetW / aspect;
        } else {
            fitH = targetH;
            fitW = targetH * aspect;
        }
        applyMatrixToTexture(tv, meta, targetW, targetH, fitW, fitH);
    }

    private void applyMatrixToTexture(TextureView tv, VideoMeta meta, int w, int h, float fitW, float fitH) {
        if (tv == null) return;
        int tvW = tv.getWidth() > 0 ? tv.getWidth() : w;
        int tvH = tv.getHeight() > 0 ? tv.getHeight() : h;
        if (tvW <= 0 || tvH <= 0) return;

        Matrix matrix = new Matrix();
        float cx = tvW / 2f;
        float cy = tvH / 2f;

        int rotation = (meta != null) ? meta.rotation : 0;
        if (rotation == 90 || rotation == 270) {
            // Buffer width maps to view X, buffer height maps to view Y.
            // After 90/270 deg rotation, scaleX scales fitW over tvH, scaleY scales fitH over tvW:
            float scaleX = fitW / (float) tvH;
            float scaleY = fitH / (float) tvW;
            matrix.postRotate(rotation, cx, cy);
            matrix.postScale(scaleX, scaleY, cx, cy);
        } else if (rotation == 180) {
            matrix.postRotate(180, cx, cy);
            matrix.postScale(fitW / (float) tvW, fitH / (float) tvH, cx, cy);
        } else {
            matrix.postScale(fitW / (float) tvW, fitH / (float) tvH, cx, cy);
        }

        if (currentZoom > 1.0f) {
            matrix.postScale(currentZoom, currentZoom, cx, cy);
            matrix.postTranslate(panX, panY);
        } else if (panX != 0 || panY != 0) {
            matrix.postTranslate(panX, panY);
        }

        tv.setTransform(matrix);
    }

    private void applyZoomTransform() {
        applyUnifiedTransform();
    }

    // ── Bug 2 Fix: Touch-intercept guard so SeekBar works inside ScrollView ──
    private View.OnTouchListener createSeekBarTouchGuard() {
        return (v, event) -> {
            switch (event.getAction()) {
                case MotionEvent.ACTION_DOWN:
                case MotionEvent.ACTION_MOVE:
                    // Tell every ScrollView ancestor "hands off — I'm handling this horizontally"
                    if (v.getParent() != null) v.getParent().requestDisallowInterceptTouchEvent(true);
                    break;
                case MotionEvent.ACTION_UP:
                case MotionEvent.ACTION_CANCEL:
                    if (v.getParent() != null) v.getParent().requestDisallowInterceptTouchEvent(false);
                    break;
            }
            return false; // let the SeekBar's own onTouchEvent fire normally
        };
    }

    private SeekBar.OnSeekBarChangeListener createScrubListener() {
        return new SeekBar.OnSeekBarChangeListener() {
            private boolean wasPlayingBeforeScrub = false;
            private long lastScrubSeek = 0L;

            @Override
            public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
                if (fromUser) {
                    long now = SystemClock.elapsedRealtime();
                    if (now - lastScrubSeek > 50) {   // tighter 50ms cadence feels snappier
                        lastScrubSeek = now;
                        seekToPercentFast(progress / 1000.0f);
                    }
                }
            }

            @Override
            public void onStartTrackingTouch(SeekBar seekBar) {
                isUserScrubbing = true;
                wasPlayingBeforeScrub = isPlaying;
                if (isPlaying) {
                    // Pause silently: don't update the badge yet, smoother UX
                    isPlaying = false;
                    handler.removeCallbacks(progressUpdater);
                    try { if (playerOriginal != null && isOriginalPrepared) playerOriginal.pause(); } catch (Exception ignored) {}
                    try { if (playerCompressed != null && isCompressedPrepared) playerCompressed.pause(); } catch (Exception ignored) {}
                }
            }

            @Override
            public void onStopTrackingTouch(SeekBar seekBar) {
                isUserScrubbing = false;
                seekToPercentExact(seekBar.getProgress() / 1000.0f);
                if (wasPlayingBeforeScrub) {
                    // 80ms settle window — decoders need time to render the seeked frame
                    postDelayed(() -> {
                        if (!isPlaying) togglePlayPause();
                    }, 80);
                }
            }
        };
    }

    // ==========================================
    // AUTO FRAME-BY-FRAME LOCKSTEP ENGINE
    // ==========================================
    private void applySpeed(MediaPlayer mp, float speed) {
        if (mp == null || !isPlaying || Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return;
        try {
            if (!mp.isPlaying()) return; // CRITICAL: MediaPlayer.setPlaybackParams() RESUMES playback if called when paused!
            PlaybackParams pp = mp.getPlaybackParams();
            if (Math.abs(pp.getSpeed() - speed) > 0.003f) {
                pp.setSpeed(speed);
                mp.setPlaybackParams(pp);
            }
        } catch (Exception ignored) {}
    }

    private void syncSeekFast(int targetMs) {
        if (!isOriginalPrepared && !isCompressedPrepared) return;

        int maxDur = 0;
        if (playerCompressed != null && isCompressedPrepared) maxDur = playerCompressed.getDuration();
        else if (playerOriginal != null && isOriginalPrepared) maxDur = playerOriginal.getDuration();
        if (maxDur > 0) {
            targetMs = Math.max(0, Math.min(targetMs, maxDur));
        }

        final int finalTargetMs = targetMs;
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (playerOriginal != null && isOriginalPrepared) {
                    playerOriginal.seekTo((long) finalTargetMs, MediaPlayer.SEEK_CLOSEST_SYNC);
                }
                if (playerCompressed != null && isCompressedPrepared) {
                    playerCompressed.seekTo((long) finalTargetMs, MediaPlayer.SEEK_CLOSEST_SYNC);
                }
            } else {
                if (playerOriginal != null && isOriginalPrepared) playerOriginal.seekTo(finalTargetMs);
                if (playerCompressed != null && isCompressedPrepared) playerCompressed.seekTo(finalTargetMs);
            }
        } catch (Exception ignored) {}
    }

    private void syncSeekExact(int targetMs) {
        if (!isOriginalPrepared && !isCompressedPrepared) return;

        int maxDur = 0;
        if (playerCompressed != null && isCompressedPrepared) maxDur = playerCompressed.getDuration();
        else if (playerOriginal != null && isOriginalPrepared) maxDur = playerOriginal.getDuration();
        if (maxDur > 0) {
            targetMs = Math.max(0, Math.min(targetMs, maxDur));
        }

        final int finalTargetMs = targetMs;
        try {
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                if (playerOriginal != null && isOriginalPrepared) {
                    playerOriginal.seekTo((long) finalTargetMs, MediaPlayer.SEEK_CLOSEST);
                }
                if (playerCompressed != null && isCompressedPrepared) {
                    playerCompressed.seekTo((long) finalTargetMs, MediaPlayer.SEEK_CLOSEST);
                }
            } else {
                if (playerOriginal != null && isOriginalPrepared) playerOriginal.seekTo(finalTargetMs);
                if (playerCompressed != null && isCompressedPrepared) playerCompressed.seekTo(finalTargetMs);
            }
        } catch (Exception ignored) {}
    }

    public synchronized void handlePlaybackCompletion() {
        if (!isPlaying) return;
        isPlaying = false;
        handler.removeCallbacks(progressUpdater);

        try {
            if (playerOriginal != null && isOriginalPrepared) {
                playerOriginal.pause();
            }
        } catch (Exception ignored) {}
        try {
            if (playerCompressed != null && isCompressedPrepared) {
                playerCompressed.pause();
            }
        } catch (Exception ignored) {}

        // Rewind to 0 so next play starts cleanly from frame 0
        syncSeekFast(0);
        masterSeekBar.setProgress(0);
        if (cinemaSeekBar != null) cinemaSeekBar.setProgress(0);

        int dur = (playerCompressed != null && isCompressedPrepared) ? playerCompressed.getDuration() :
                ((playerOriginal != null && isOriginalPrepared) ? playerOriginal.getDuration() : 0);
        updateTimeDisplays(0, 0, dur);
        updatePlayButtonUI("▶  REPLAY SYNC", "#059669", "#10B981");

        if (badgeCenterSync != null) {
            badgeCenterSync.setText("COMPLETED (READY)");
            badgeCenterSync.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        }
    }

    private synchronized void syncLoopToStart() {
        handlePlaybackCompletion();
    }

    public void pauseIfPlaying() {
        if (isPlaying) {
            togglePlayPause();
        } else {
            try { if (playerOriginal != null && isOriginalPrepared) playerOriginal.pause(); } catch (Exception ignored) {}
            try { if (playerCompressed != null && isCompressedPrepared) playerCompressed.pause(); } catch (Exception ignored) {}
        }
    }

    public synchronized void togglePlayPause() {
        if (!isOriginalPrepared && !isCompressedPrepared) {
            if (getContext() instanceof Activity) {
                PrismHud.showInfo((Activity) getContext(), "No media loaded in Duo Player");
            }
            return;
        }

        if (isPlaying) {
            // ── PAUSE ──
            isPlaying = false;
            handler.removeCallbacks(progressUpdater);

            // Capture master position BEFORE pausing
            int masterPos = 0;
            try {
                if (playerCompressed != null && isCompressedPrepared) {
                    masterPos = playerCompressed.getCurrentPosition();
                } else if (playerOriginal != null && isOriginalPrepared) {
                    masterPos = playerOriginal.getCurrentPosition();
                }
            } catch (Exception ignored) {}

            try { if (playerCompressed != null && isCompressedPrepared) playerCompressed.pause(); } catch (Exception ignored) {}
            try { if (playerOriginal   != null && isOriginalPrepared)   playerOriginal.pause();   } catch (Exception ignored) {}

            // Frame-accurate seek on both decoders using SEEK_CLOSEST (API 26+)
            final int snapPos = masterPos;
            try {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    if (playerOriginal != null && isOriginalPrepared) {
                        playerOriginal.seekTo((long) snapPos, MediaPlayer.SEEK_CLOSEST);
                    }
                    if (playerCompressed != null && isCompressedPrepared) {
                        playerCompressed.seekTo((long) snapPos, MediaPlayer.SEEK_CLOSEST);
                    }
                } else {
                    if (playerOriginal != null && isOriginalPrepared) playerOriginal.seekTo(snapPos);
                    if (playerCompressed != null && isCompressedPrepared) playerCompressed.seekTo(snapPos);
                }
            } catch (Exception ignored) {}

            updatePlayButtonUI("▶  PLAY SYNC", "#059669", "#10B981");

            // Reset nominal speed tracker without calling setPlaybackParams (which would resume playback!)
            currentOriginalSteerSpeed = currentPlaybackSpeed;

            int dur = getDuration();
            updateTimeDisplays(snapPos, snapPos, dur);

            if (badgeCenterSync != null) {
                float fps = compMeta != null && compMeta.fps > 0 ? compMeta.fps : 30.0f;
                int frameIdx = Math.max(0, Math.round((snapPos * fps) / 1000.0f));
                badgeCenterSync.setText("● LOCKSTEP Δ 0ms [F: " + frameIdx + "]");
                badgeCenterSync.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
            }

        } else {
            // ── PLAY ──
            // Guarantee any rogue decoder playback is halted before synchronous start
            try { if (playerOriginal != null && isOriginalPrepared && playerOriginal.isPlaying()) playerOriginal.pause(); } catch (Exception ignored) {}
            try { if (playerCompressed != null && isCompressedPrepared && playerCompressed.isPlaying()) playerCompressed.pause(); } catch (Exception ignored) {}

            int dur = getDuration();

            // If at or near end, rewind cleanly first
            int curPos = 0;
            try {
                if (playerCompressed != null && isCompressedPrepared) curPos = playerCompressed.getCurrentPosition();
                else if (playerOriginal != null && isOriginalPrepared) curPos = playerOriginal.getCurrentPosition();
            } catch (Exception ignored) {}

            if (dur > 0 && curPos >= dur - 100) {
                syncSeekExact(0);
                handler.postDelayed(this::startBothPlayers, 100);
                return;
            }

            int origPos = 0, compPos = 0;
            try {
                if (playerOriginal != null && isOriginalPrepared) origPos = playerOriginal.getCurrentPosition();
                if (playerCompressed != null && isCompressedPrepared) compPos = playerCompressed.getCurrentPosition();
            } catch (Exception ignored) {}

            // If already in sub-frame lockstep (<= 35ms), start immediately with zero delay
            if (Math.abs(origPos - compPos) <= 35) {
                startBothPlayers();
            } else {
                int alignPos = (isCompressedPrepared && playerCompressed != null) ? compPos : origPos;
                syncSeekExact(alignPos);
                handler.postDelayed(this::startBothPlayers, 120);
            }
        }
    }

    /**
     * Bug 3 fix: Atomic back-to-back start — minimises IPC gap between the two start() calls.
     * Starts decoders first, marks isPlaying = true, then applies playback speed safely.
     */
    private synchronized void startBothPlayers() {
        if (isPlaying) return; // guard against double-fire from postDelayed

        // 1. Back-to-back start with zero intervening work — tightest possible start gap
        try { if (playerOriginal   != null && isOriginalPrepared)   playerOriginal.start();   } catch (Exception e) {
            android.util.Log.w("DuoPlayer", "start orig failed: " + e.getMessage());
        }
        try { if (playerCompressed != null && isCompressedPrepared) playerCompressed.start(); } catch (Exception e) {
            android.util.Log.w("DuoPlayer", "start comp failed: " + e.getMessage());
        }

        isPlaying = true;

        // 2. Safely apply playback speeds now that players are actively started
        currentOriginalSteerSpeed = currentPlaybackSpeed;
        applySpeed(playerOriginal, currentPlaybackSpeed);
        applySpeed(playerCompressed, currentPlaybackSpeed);

        updatePlayButtonUI("⏸  PAUSE BOTH", "#B45309", "#F59E0B");
        lastDriftCheckTime = SystemClock.elapsedRealtime();
        handler.removeCallbacks(progressUpdater);
        handler.post(progressUpdater);
    }

    private int getDuration() {
        try {
            if (playerCompressed != null && isCompressedPrepared) return playerCompressed.getDuration();
            if (playerOriginal   != null && isOriginalPrepared)   return playerOriginal.getDuration();
        } catch (Exception ignored) {}
        return 0;
    }

    private void updatePlayButtonUI(String label, String c1, String c2) {
        if (btnPlayPause != null) {
            btnPlayPause.setText(label);
            btnPlayPause.setBackground(ThemeStyles.createGradientButton(Color.parseColor(c1), Color.parseColor(c2), dp(14)));
        }
        if (btnCinemaPlayPause != null) {
            btnCinemaPlayPause.setText(label.contains("PAUSE") ? "⏸ PAUSE" : "▶ PLAY");
            btnCinemaPlayPause.setBackground(ThemeStyles.createGradientButton(Color.parseColor(c1), Color.parseColor(c2), dp(12)));
        }
    }

    public void stepFrame(int deltaMs) {
        if (!isOriginalPrepared && !isCompressedPrepared) return;
        if (isPlaying) {
            togglePlayPause();
        }

        int frameIntervalMs = 33; // default 30fps
        if (compMeta != null && compMeta.fps > 0) {
            frameIntervalMs = Math.round(1000.0f / compMeta.fps);
        } else if (origMeta != null && origMeta.fps > 0) {
            frameIntervalMs = Math.round(1000.0f / origMeta.fps);
        }
        if (frameIntervalMs <= 0) frameIntervalMs = 33;

        int step = (Math.abs(deltaMs) <= 35) ? (deltaMs > 0 ? frameIntervalMs : -frameIntervalMs) : deltaMs;

        int current = (playerCompressed != null && isCompressedPrepared)
                ? playerCompressed.getCurrentPosition()
                : ((playerOriginal != null && isOriginalPrepared) ? playerOriginal.getCurrentPosition() : 0);
        int target = Math.max(0, current + step);
        syncSeekExact(target);

        int dur = (playerCompressed != null && isCompressedPrepared) ? playerCompressed.getDuration() : 0;
        updateTimeDisplays(target, target, dur);
    }

    private void seekToPercentFast(float pct) {
        if (!isCompressedPrepared && !isOriginalPrepared) return;
        int dur = (playerCompressed != null && isCompressedPrepared) ? playerCompressed.getDuration() :
                (playerOriginal != null && isOriginalPrepared ? playerOriginal.getDuration() : 0);
        if (dur > 0) {
            int targetMs = (int) (dur * pct);
            syncSeekFast(targetMs);
            updateTimeDisplays(targetMs, targetMs, dur);
        }
    }

    private void seekToPercentExact(float pct) {
        if (!isCompressedPrepared && !isOriginalPrepared) return;
        int dur = (playerCompressed != null && isCompressedPrepared) ? playerCompressed.getDuration() :
                (playerOriginal != null && isOriginalPrepared ? playerOriginal.getDuration() : 0);
        if (dur > 0) {
            int targetMs = (int) (dur * pct);
            syncSeekExact(targetMs);
            updateTimeDisplays(targetMs, targetMs, dur);
        }
    }

    private void updateProgress() {
        if (!isPlaying || isUserScrubbing) return;

        int dur = 0;
        int posComp = 0;
        int posOrig = 0;

        try {
            if (isCompressedPrepared && playerCompressed != null) {
                posComp = playerCompressed.getCurrentPosition();
                dur = playerCompressed.getDuration();
            }
            if (isOriginalPrepared && playerOriginal != null) {
                posOrig = playerOriginal.getCurrentPosition();
                if (dur <= 0) dur = playerOriginal.getDuration();
            }
        } catch (Exception ignored) {}

        if (!isCompressedPrepared && !isOriginalPrepared) return;

        int masterPos = (isCompressedPrepared && playerCompressed != null) ? posComp : posOrig;

        if (dur > 0) {
            // Clean completion detection: stop cleanly at end and present REPLAY SYNC
            if (masterPos >= dur - 50) {
                handlePlaybackCompletion();
                return;
            }
            int progress = (int) ((masterPos / (float) dur) * 1000);
            masterSeekBar.setProgress(progress);
            if (cinemaSeekBar != null) cinemaSeekBar.setProgress(progress);
        }

        // ── DRIFT WATCHDOG (Continuous Proportional Speed Steering) ──────────
        if (isPlaying && isOriginalPrepared && playerOriginal != null
                && isCompressedPrepared && playerCompressed != null
                && Build.VERSION.SDK_INT >= Build.VERSION_CODES.M) {
            long now = SystemClock.elapsedRealtime();
            if (now - lastDriftCheckTime >= 200) {
                lastDriftCheckTime = now;
                int drift    = posOrig - posComp; // +ve = original ahead, -ve = original behind
                int absDrift = Math.abs(drift);

                if (absDrift <= 15) {
                    // Sub-frame lockstep — restore nominal playback speed
                    if (Math.abs(currentOriginalSteerSpeed - currentPlaybackSpeed) > 0.002f) {
                        currentOriginalSteerSpeed = currentPlaybackSpeed;
                        applySpeed(playerOriginal, currentOriginalSteerSpeed);
                    }
                } else if (absDrift <= 100) {
                    // Imperceptible micro-steer (±2%)
                    float targetSpeed = (drift < 0)
                            ? (currentPlaybackSpeed * 1.02f)
                            : (currentPlaybackSpeed * 0.98f);
                    if (Math.abs(currentOriginalSteerSpeed - targetSpeed) > 0.003f) {
                        currentOriginalSteerSpeed = targetSpeed;
                        applySpeed(playerOriginal, currentOriginalSteerSpeed);
                    }
                } else if (absDrift <= 400) {
                    // Gentle steer (±5%)
                    float targetSpeed = (drift < 0)
                            ? (currentPlaybackSpeed * 1.05f)
                            : (currentPlaybackSpeed * 0.95f);
                    if (Math.abs(currentOriginalSteerSpeed - targetSpeed) > 0.003f) {
                        currentOriginalSteerSpeed = targetSpeed;
                        applySpeed(playerOriginal, currentOriginalSteerSpeed);
                    }
                } else if (absDrift <= 1200) {
                    // Strong steer (±10%) - smoothly converges within 400ms without ANY seek
                    float targetSpeed = (drift < 0)
                            ? (currentPlaybackSpeed * 1.10f)
                            : (currentPlaybackSpeed * 0.90f);
                    if (Math.abs(currentOriginalSteerSpeed - targetSpeed) > 0.005f) {
                        currentOriginalSteerSpeed = targetSpeed;
                        applySpeed(playerOriginal, currentOriginalSteerSpeed);
                    }
                } else {
                    // Extreme drift (>1.2s): Smooth keyframe seek (last resort, throttled)
                    if (now - lastHardSeekTime > 2500) {
                        lastHardSeekTime = now;
                        try {
                            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                                playerOriginal.seekTo((long) posComp, MediaPlayer.SEEK_CLOSEST);
                            } else {
                                playerOriginal.seekTo(posComp);
                            }
                        } catch (Exception ignored) {}
                        currentOriginalSteerSpeed = currentPlaybackSpeed;
                        applySpeed(playerOriginal, currentOriginalSteerSpeed);
                    }
                }
            }
        }
        // ── end drift watchdog ───────────────────────────────────────────────

        updateTimeDisplays(posOrig, posComp, dur);
    }

    private void updateTimeDisplay() {
        if (!isCompressedPrepared && !isOriginalPrepared) return;
        try {
            int posComp = (playerCompressed != null && isCompressedPrepared) ? playerCompressed.getCurrentPosition() : 0;
            int dur = (playerCompressed != null && isCompressedPrepared) ? playerCompressed.getDuration() : 
                    (playerOriginal != null && isOriginalPrepared ? playerOriginal.getDuration() : 0);
            int posOrig = (playerOriginal != null && isOriginalPrepared) ? playerOriginal.getCurrentPosition() : posComp;
            updateTimeDisplays(posOrig, posComp, dur);
        } catch (Exception ignored) {}
    }

    private void updateTimeDisplays(int posOrig, int posComp, int dur) {
        String formatted = String.format(Locale.US, "%02d:%02d / %02d:%02d",
                posComp / 60000, (posComp % 60000) / 1000,
                dur / 60000, (dur % 60000) / 1000);
        tvTimeDisplay.setText(formatted);
        if (cinemaTimeDisplay != null) cinemaTimeDisplay.setText(formatted);

        float fps = 30.0f;
        if (compMeta != null && compMeta.fps > 0) {
            fps = compMeta.fps;
        } else if (origMeta != null && origMeta.fps > 0) {
            fps = origMeta.fps;
        }
        float frameDurationMs = 1000.0f / fps;
        float halfFrame = frameDurationMs / 2.0f; // e.g. 16.6ms at 30fps

        int drift = posOrig - posComp;
        int absDrift = Math.abs(drift);
        int frameIdx = Math.max(0, Math.round((posComp * fps) / 1000.0f));

        // When paused or within half-frame tolerance, both video decoders are physically rendering the exact same frame.
        // Therefore, lock both displayed timestamps to posComp for true 0 ms delay!
        int displayOrigPos = (!isPlaying || absDrift <= halfFrame) ? posComp : posOrig;
        int displayCompPos = posComp;

        // Update live dual millisecond timestamps with frame indices
        if (tvOrigTime != null) {
            tvOrigTime.setText("⏱ " + formatTimeMillis(displayOrigPos) + " [F:" + frameIdx + "]");
        }
        if (tvCompTime != null) {
            tvCompTime.setText("⏱ " + formatTimeMillis(displayCompPos) + " [F:" + frameIdx + "]");
        }

        // Update live sync delta badge
        if (badgeCenterSync != null) {
            if (!isPlaying || absDrift <= halfFrame) {
                badgeCenterSync.setText("● LOCKSTEP Δ 0ms [F: " + frameIdx + "]");
                badgeCenterSync.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
            } else if (absDrift <= frameDurationMs * 2.0f) {
                badgeCenterSync.setText(String.format(Locale.US, "● SYNC Δ %+dms", drift));
                badgeCenterSync.setTextColor(ThemeStyles.COLOR_CYAN_NEON);
            } else {
                badgeCenterSync.setText(String.format(Locale.US, "FRAME-LOCK Δ %+dms", drift));
                badgeCenterSync.setTextColor(ThemeStyles.COLOR_AMBER_SUNSET);
            }
        }
    }

    private String formatTimeMillis(int ms) {
        if (ms < 0) ms = 0;
        int min = ms / 60000;
        int sec = (ms % 60000) / 1000;
        int millis = ms % 1000;
        return String.format(Locale.US, "%02d:%02d.%03d", min, sec, millis);
    }

    public void forceResync() {
        if (playerOriginal != null && isOriginalPrepared && playerCompressed != null && isCompressedPrepared) {
            try {
                int pos = playerCompressed.getCurrentPosition();
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    playerOriginal.seekTo((long) pos, MediaPlayer.SEEK_CLOSEST);
                    playerCompressed.seekTo((long) pos, MediaPlayer.SEEK_CLOSEST);
                } else {
                    playerOriginal.seekTo(pos);
                    playerCompressed.seekTo(pos);
                }
                currentOriginalSteerSpeed = currentPlaybackSpeed;
                if (isPlaying) {
                    applySpeed(playerOriginal, currentPlaybackSpeed);
                }
                updateTimeDisplays(pos, pos, playerCompressed.getDuration());
                if (getContext() instanceof Activity) {
                    PrismHud.showSuccess((Activity) getContext(), "Frames locked in sync (Δ 0ms)");
                }
            } catch (Exception ignored) {}
        }
    }

    // ==========================================
    // AUDIO TOGGLE & SPEED
    // ==========================================
    public void toggleAudioTrack() {
        isAudioOriginal = !isAudioOriginal;
        applyAudioRouting();
        if (getContext() instanceof Activity) {
            Activity act = (Activity) getContext();
            if (isAudioOriginal) {
                btnAudioToggle.setText("Audio: Source (Master)");
                btnAudioToggle.setTextColor(ThemeStyles.COLOR_MINT_TEXT);
                if (btnCinemaAudio != null) {
                    btnCinemaAudio.setText("Master");
                    btnCinemaAudio.setTextColor(ThemeStyles.COLOR_MINT_TEXT);
                }
                PrismHud.showSuccess(act, "Listening to: Uncompressed Master Audio");
            } else {
                btnAudioToggle.setText("Audio: Compressed (Opus)");
                btnAudioToggle.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
                if (btnCinemaAudio != null) {
                    btnCinemaAudio.setText("Opus");
                    btnCinemaAudio.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
                }
                PrismHud.showInfo(act, "Listening to: Optimized Opus Audio");
            }
        }
    }

    private void applyAudioRouting() {
        if (playerOriginal != null) {
            float vol = isAudioOriginal ? 1.0f : 0.0f;
            playerOriginal.setVolume(vol, vol);
        }
        if (playerCompressed != null) {
            float vol = isAudioOriginal ? 0.0f : 1.0f;
            playerCompressed.setVolume(vol, vol);
        }
    }

    private void setPlaybackSpeed(float speed, int btnIdx) {
        currentPlaybackSpeed = speed;
        currentOriginalSteerSpeed = speed;
        for (int i = 0; i < speedButtons.length; i++) {
            boolean active = (i == btnIdx);
            speedButtons[i].setBackground(createRoundedDrawable(active ? ThemeStyles.COLOR_CYAN_PRIMARY : Color.parseColor("#121B2D"),
                    ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(10)));
            speedButtons[i].setTextColor(active ? Color.WHITE : ThemeStyles.COLOR_TEXT_MUTED);
        }

        if (isPlaying) {
            applySpeed(playerOriginal, speed);
            applySpeed(playerCompressed, speed);
        }

        if (getContext() instanceof Activity) {
            PrismHud.showInfo((Activity) getContext(), "Playback Speed: " + speed + "x");
        }
    }

    // ==========================================
    // COMPARISON MODES
    // ==========================================
    public void setMode(ComparisonMode mode) {
        currentMode = mode;
        for (int i = 0; i < modeTabButtons.length; i++) {
            boolean active = (i == mode.ordinal());
            modeTabButtons[i].setBackground(ThemeStyles.createPillDrawable(getContext(), active));
            modeTabButtons[i].setTextColor(active ? Color.WHITE : ThemeStyles.COLOR_TEXT_SECONDARY);
        }

        if (originalUri == null && compressedUri == null) {
            showEmptyOverlay();
            return;
        }

        int parentW = playerContainer.getWidth();
        if (parentW <= 0) parentW = 1000;

        switch (mode) {
            case SPLIT_CURTAIN:
                containerCompressed.setVisibility(VISIBLE);
                containerOriginal.setVisibility(VISIBLE);
                dividerLine.setVisibility(VISIBLE);
                dividerHandle.setVisibility(VISIBLE);
                holdToPeekOverlay.setVisibility(GONE);
                scorecardOverlay.setVisibility(GONE);
                if (cardOriginal != null) cardOriginal.setVisibility(VISIBLE);
                if (cardCompressed != null) cardCompressed.setVisibility(VISIBLE);
                if (badgeCenterSync != null) badgeCenterSync.setVisibility(VISIBLE);
                if (tvSplitRatio != null) tvSplitRatio.setVisibility(VISIBLE);
                updateCurtainPosition(parentW / 2);
                tvStatusBanner.setText("Drag the cyan divider across the video to wipe between Source and Compressed.");
                break;

            case SIDE_BY_SIDE:
                containerCompressed.setVisibility(VISIBLE);
                containerOriginal.setVisibility(VISIBLE);
                dividerLine.setVisibility(VISIBLE);
                dividerHandle.setVisibility(GONE);
                holdToPeekOverlay.setVisibility(GONE);
                scorecardOverlay.setVisibility(GONE);
                if (cardOriginal != null) cardOriginal.setVisibility(VISIBLE);
                if (cardCompressed != null) cardCompressed.setVisibility(VISIBLE);
                if (badgeCenterSync != null) badgeCenterSync.setVisibility(VISIBLE);
                if (tvSplitRatio != null) tvSplitRatio.setVisibility(GONE);
                updateCurtainPosition(parentW / 2);
                tvStatusBanner.setText("Dual View Active: Left = Source Master, Right = Compressed.");
                break;

            case HOLD_TO_PEEK:
                containerCompressed.setVisibility(VISIBLE);
                containerOriginal.setVisibility(GONE);
                dividerLine.setVisibility(GONE);
                dividerHandle.setVisibility(GONE);
                holdToPeekOverlay.setVisibility(VISIBLE);
                scorecardOverlay.setVisibility(GONE);
                if (cardOriginal != null) cardOriginal.setVisibility(GONE);
                if (cardCompressed != null) cardCompressed.setVisibility(VISIBLE);
                if (badgeCenterSync != null) badgeCenterSync.setVisibility(GONE);
                if (tvSplitRatio != null) tvSplitRatio.setVisibility(GONE);
                tvStatusBanner.setText("Hold the center button to inspect raw source frames.");
                break;

            case ERROR_SCORECARD:
                containerCompressed.setVisibility(VISIBLE);
                containerOriginal.setVisibility(GONE);
                dividerLine.setVisibility(GONE);
                dividerHandle.setVisibility(GONE);
                holdToPeekOverlay.setVisibility(GONE);
                scorecardOverlay.setVisibility(VISIBLE);
                if (cardOriginal != null) cardOriginal.setVisibility(GONE);
                if (cardCompressed != null) cardCompressed.setVisibility(GONE);
                if (badgeCenterSync != null) badgeCenterSync.setVisibility(GONE);
                if (tvSplitRatio != null) tvSplitRatio.setVisibility(GONE);
                tvStatusBanner.setText("Quality metrics are not calculated by this view.");
                break;

            case SOLO_OUTPUT:
                containerCompressed.setVisibility(VISIBLE);
                containerOriginal.setVisibility(GONE);
                dividerLine.setVisibility(GONE);
                dividerHandle.setVisibility(GONE);
                holdToPeekOverlay.setVisibility(GONE);
                scorecardOverlay.setVisibility(GONE);
                if (cardOriginal != null) cardOriginal.setVisibility(GONE);
                if (cardCompressed != null) cardCompressed.setVisibility(VISIBLE);
                if (badgeCenterSync != null) badgeCenterSync.setVisibility(GONE);
                if (tvSplitRatio != null) tvSplitRatio.setVisibility(GONE);
                tvStatusBanner.setText("Solo Cinema View: 120Hz smooth playback with zoom & loupe inspection.");
                break;

            case SOLO_ORIGINAL:
                containerOriginal.setVisibility(VISIBLE);
                containerCompressed.setVisibility(GONE);
                dividerLine.setVisibility(GONE);
                dividerHandle.setVisibility(GONE);
                holdToPeekOverlay.setVisibility(GONE);
                scorecardOverlay.setVisibility(GONE);
                if (cardOriginal != null) cardOriginal.setVisibility(VISIBLE);
                if (cardCompressed != null) cardCompressed.setVisibility(GONE);
                if (badgeCenterSync != null) badgeCenterSync.setVisibility(GONE);
                if (tvSplitRatio != null) tvSplitRatio.setVisibility(GONE);
                tvStatusBanner.setText("Solo Source Master: Inspecting original stream.");
                break;
        }

        post(() -> {
            applyUnifiedTransform();
            refreshPausedFrames();
        });
    }

    private void updateCurtainPosition(int x) {
        curtainPositionX = x;
        int parentW = playerContainer.getWidth();
        if (parentW <= 0) parentW = 1000;

        if (currentMode == ComparisonMode.SIDE_BY_SIDE) {
            int halfW = parentW / 2;
            FrameLayout.LayoutParams origParams = new FrameLayout.LayoutParams(halfW, ViewGroup.LayoutParams.MATCH_PARENT);
            containerOriginal.setLayoutParams(origParams);
            containerOriginal.setTranslationX(0);

            FrameLayout.LayoutParams texOrigParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            textureOriginal.setLayoutParams(texOrigParams);

            FrameLayout.LayoutParams compParams = new FrameLayout.LayoutParams(halfW, ViewGroup.LayoutParams.MATCH_PARENT);
            containerCompressed.setLayoutParams(compParams);
            containerCompressed.setTranslationX(halfW);

            FrameLayout.LayoutParams texCompParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
            textureCompressed.setLayoutParams(texCompParams);

            dividerLine.setVisibility(VISIBLE);
            dividerLine.setTranslationX(halfW);
            dividerHandle.setVisibility(GONE);
            post(() -> {
                applyUnifiedTransform();
                refreshPausedFrames();
            });
            return;
        }

        // Standard Split Curtain or other modes
        containerCompressed.setTranslationX(0);
        FrameLayout.LayoutParams compParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        containerCompressed.setLayoutParams(compParams);

        FrameLayout.LayoutParams texCompParams = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        textureCompressed.setLayoutParams(texCompParams);

        containerOriginal.setTranslationX(0);
        FrameLayout.LayoutParams origParams = new FrameLayout.LayoutParams(x, ViewGroup.LayoutParams.MATCH_PARENT);
        containerOriginal.setLayoutParams(origParams);

        FrameLayout.LayoutParams texParams = new FrameLayout.LayoutParams(parentW, ViewGroup.LayoutParams.MATCH_PARENT);
        textureOriginal.setLayoutParams(texParams);

        dividerLine.setTranslationX(x);
        dividerHandle.setTranslationX(x - (dividerHandle.getWidth() > 0 ? dividerHandle.getWidth() / 2 : dp(24)));
        if (currentMode == ComparisonMode.SPLIT_CURTAIN) {
            dividerLine.setVisibility(VISIBLE);
            dividerHandle.setVisibility(VISIBLE);
        }

        if (tvSplitRatio != null && parentW > 0) {
            int pct = (int) ((x / (float) parentW) * 100);
            tvSplitRatio.setText(String.format(Locale.US, "SOURCE %d%%  ◀━━━ ● ━━━▶  PRISM-V %d%%", pct, 100 - pct));

            // Center 50% Tactile Haptic Snap
            if (Math.abs(pct - 50) <= 1) {
                if (lastHapticPct != 50) {
                    lastHapticPct = 50;
                    ThemeStyles.performHaptic(dividerHandle);
                }
            } else if (Math.abs(pct - 50) >= 4) {
                lastHapticPct = pct;
            }
        }
    }

    // ==========================================
    // VIDEO LOADING & SAMPLES
    // ==========================================
    public void loadVideoPair(Uri origUri, Uri compUri, String origName, String compName) {
        pauseIfPlaying();
        hideEmptyOverlay();
        this.originalUri   = origUri;
        this.compressedUri = compUri;
        // Store as pending so surface callbacks can pick them up (Bug 7 fix)
        pendingOrigUri = origUri;
        pendingCompUri = compUri;
        initPending    = true;
        isArmed        = false;

        origMeta = extractVideoMeta(origUri);
        compMeta = extractVideoMeta(compUri);

        if (origName != null) tvLabelOriginal.setText(origName);
        if (compName != null) tvLabelCompressed.setText(compName);

        updateTelemetryCardSpecs();

        isOriginalPrepared   = false;
        isCompressedPrepared = false;

        masterSeekBar.setProgress(0);
        if (cinemaSeekBar != null) cinemaSeekBar.setProgress(0);
        updatePlayButtonUI("▶  PLAY SYNC", "#059669", "#10B981");

        // Bug 1 fix: only init if surfaces are actually ready; otherwise the
        // onSurfaceTextureAvailable callbacks will fire initFromLatch() later.
        initFromLatch();

        setMode(currentMode);
        post(() -> applyUnifiedTransform());
    }

    public void loadSourceVideoOnly(Uri origUri, String origName) {
        pauseIfPlaying();
        hideEmptyOverlay();
        if (playerCompressed != null) {
            try { playerCompressed.release(); } catch (Exception ignored) {}
            playerCompressed = null;
        }
        isCompressedPrepared = false;
        isArmed = false;
        this.compressedUri = null;
        pendingCompUri     = null;

        this.originalUri = origUri;
        pendingOrigUri   = origUri;
        initPending      = true;
        origMeta = extractVideoMeta(origUri);
        if (origName != null) tvLabelOriginal.setText(origName);
        isOriginalPrepared = false;

        masterSeekBar.setProgress(0);
        if (cinemaSeekBar != null) cinemaSeekBar.setProgress(0);
        updatePlayButtonUI("▶  PLAY SYNC", "#059669", "#10B981");

        initFromLatch();

        setMode(ComparisonMode.SOLO_ORIGINAL);
        updateTelemetryCardSpecs();
        post(() -> applyUnifiedTransform());
    }

    public void loadOutputVideoSolo(Uri compUri, String compName) {
        pauseIfPlaying();
        hideEmptyOverlay();
        if (playerOriginal != null) {
            try { playerOriginal.release(); } catch (Exception ignored) {}
            playerOriginal = null;
        }
        isOriginalPrepared = false;
        isArmed = false;
        this.originalUri = null;
        pendingOrigUri   = null;

        this.compressedUri = compUri;
        pendingCompUri     = compUri;
        initPending        = true;
        compMeta = extractVideoMeta(compUri);
        if (compName != null) tvLabelCompressed.setText(compName);
        isCompressedPrepared = false;

        masterSeekBar.setProgress(0);
        if (cinemaSeekBar != null) cinemaSeekBar.setProgress(0);
        updatePlayButtonUI("▶  PLAY SYNC", "#059669", "#10B981");

        initFromLatch();
        updateTelemetryCardSpecs();
        setMode(ComparisonMode.SOLO_OUTPUT);
        post(() -> applyUnifiedTransform());
    }

    public void openPairLibraryDialog() {
        CompressedVideosView.showLibraryDialog(getContext(), this::loadFromHistoryRecord);
    }

    public void loadFromHistoryRecord(CompressionHistoryManager.Record r) {
        if (r == null) return;
        Uri origUri = r.getOriginalUri();
        Uri compUri = r.getCompressedUri();
        if (compUri == null || !r.isCompressedFileExisting()) {
            if (getContext() instanceof Activity) {
                PrismHud.showWarning((Activity) getContext(), "Compressed video file not found on disk");
            }
            return;
        }

        boolean origAccessible = false;
        if (origUri != null) {
            if ("file".equals(origUri.getScheme())) {
                origAccessible = new File(origUri.getPath()).exists();
            } else if ("content".equals(origUri.getScheme())) {
                try (android.content.res.AssetFileDescriptor afd = getContext().getContentResolver().openAssetFileDescriptor(origUri, "r")) {
                    origAccessible = (afd != null);
                } catch (Exception ignored) {}
            }
        }
        if (!origAccessible && r.title != null) {
            File possibleSource = CompressedVideosView.findPossibleSourceFile(r.title);
            if (possibleSource != null && possibleSource.exists()) {
                origUri = Uri.fromFile(possibleSource);
                origAccessible = true;
            }
        }
        if (!origAccessible && r.compressedPath != null) {
            File possibleSource = CompressedVideosView.findPossibleSourceFile(new File(r.compressedPath).getName());
            if (possibleSource != null && possibleSource.exists()) {
                origUri = Uri.fromFile(possibleSource);
                origAccessible = true;
            }
        }

        if (!origAccessible) {
            String compTitle = String.format(Locale.US, "● PRISM-V: %s (%.1f MB)",
                    r.title != null ? r.title : "Compressed",
                    r.compressedSizeBytes / (1024.0 * 1024.0));
            loadOutputVideoSolo(compUri, compTitle);
            if (getContext() instanceof Activity) {
                PrismHud.showInfo((Activity) getContext(), "Master source not on storage; playing compressed output.");
            }
            return;
        }

        String origTitle = "● SOURCE: " + (r.title != null ? r.title : "Master");
        String compTitle = String.format(Locale.US, "● PRISM-V (%.1f MB • -%.1f%%)",
                r.compressedSizeBytes / (1024.0 * 1024.0), r.percentReduction);
        loadVideoPair(origUri, compUri, origTitle, compTitle);
        if (getContext() instanceof Activity) {
            PrismHud.showSuccess((Activity) getContext(), "Loaded pair: " + (r.title != null ? r.title : "Comparison"));
        }
    }

    public void loadDefaultSampleFiles() {
        // Priority 1: Check CompressionHistoryManager for latest valid recorded compression pair
        try {
            CompressionHistoryManager history = CompressionHistoryManager.get(getContext());
            CompressionHistoryManager.Record latest = history.getLatestRecord();
            if (latest != null && latest.isCompressedFileExisting()) {
                boolean origValid = false;
                if (latest.getOriginalUri() != null) {
                    if ("file".equals(latest.getOriginalUri().getScheme())) {
                        origValid = new File(latest.getOriginalUri().getPath()).exists();
                    } else if ("content".equals(latest.getOriginalUri().getScheme())) {
                        try (android.content.res.AssetFileDescriptor afd = getContext().getContentResolver().openAssetFileDescriptor(latest.getOriginalUri(), "r")) {
                            origValid = (afd != null);
                        } catch (Exception ignored) {}
                    }
                }
                if (!origValid && latest.title != null) {
                    File possibleSource = new File("/sdcard/sample_video/" + latest.title);
                    if (possibleSource.exists()) origValid = true;
                    else {
                        File camSource = new File("/sdcard/DCIM/Camera/" + latest.title);
                        if (camSource.exists()) origValid = true;
                    }
                }
                if (origValid) {
                    loadFromHistoryRecord(latest);
                    return;
                }
            }
        } catch (Exception ignored) {}

        // Priority 2: Check standard sample reel pair in /sdcard/sample_video/
        File actionReelOrig = new File("/sdcard/sample_video/sample_action_reel.mp4");
        File actionReelComp = new File("/storage/emulated/0/Movies/Prism-V/sample_action_reel_compressed.mp4");
        if (!actionReelComp.exists() || actionReelComp.length() == 0) {
            actionReelComp = new File("/storage/emulated/0/Movies/Prism-V/sample_action_reel_optimized.mp4");
        }
        if (!actionReelOrig.exists()) {
            actionReelOrig = new File("/sdcard/sample_video/sample.mp4");
            actionReelComp = new File("/storage/emulated/0/Movies/Prism-V/sample_optimized.mp4");
        }
        if (actionReelOrig.exists() && actionReelComp.exists() && actionReelComp.length() > 0) {
            double pct = (1.0 - (double) actionReelComp.length() / actionReelOrig.length()) * 100.0;
            loadVideoPair(
                    Uri.fromFile(actionReelOrig),
                    Uri.fromFile(actionReelComp),
                    String.format(Locale.US, "● SOURCE: %s (%.1f MB)", actionReelOrig.getName(), actionReelOrig.length() / (1024.0 * 1024.0)),
                    String.format(Locale.US, "● PRISM-V (%.1f MB • -%.1f%%)", actionReelComp.length() / (1024.0 * 1024.0), pct)
            );
            return;
        }

        // Priority 2: Look for compressed_sample.mp4 and sample.mp4 in external files or sdcard
        File origFile = new File("/sdcard/Android/data/com.prismv/files/sample.mp4");
        File compFile = new File("/sdcard/Android/data/com.prismv/files/compressed_sample.mp4");

        if (!compFile.exists()) {
            compFile = new File("/sdcard/Android/data/com.prismv/files/compressed_sample.mp4.mp4");
        }
        if (!origFile.exists()) {
            origFile = new File(getContext().getExternalFilesDir(null), "sample.mp4");
        }
        if (!compFile.exists()) {
            compFile = new File(getContext().getExternalFilesDir(null), "compressed_sample.mp4");
        }

        if (origFile.exists() && compFile.exists()) {
            double pct = (origFile.length() > 0) ? (1.0 - (double) compFile.length() / (double) origFile.length()) * 100.0 : 0.0;
            loadVideoPair(
                    Uri.fromFile(origFile),
                    Uri.fromFile(compFile),
                    String.format(Locale.US, "● SOURCE (%.1f MB)", origFile.length() / (1024.0 * 1024.0)),
                    String.format(Locale.US, "● PRISM-V (%.1f MB • -%.1f%%)", compFile.length() / (1024.0 * 1024.0), Math.max(0, pct))
            );
        }
    }

    private VideoMeta extractVideoMeta(Uri uri) {
        VideoMeta meta = new VideoMeta();
        if (uri == null) return meta;
        MediaMetadataRetriever mmr = null;
        try {
            mmr = new MediaMetadataRetriever();
            mmr.setDataSource(getContext(), uri);

            String wStr = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_WIDTH);
            String hStr = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_HEIGHT);
            String rStr = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_VIDEO_ROTATION);
            String dStr = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_DURATION);
            String bStr = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_BITRATE);
            String mime = mmr.extractMetadata(MediaMetadataRetriever.METADATA_KEY_MIMETYPE);

            if (wStr != null) meta.rawWidth = Integer.parseInt(wStr);
            if (hStr != null) meta.rawHeight = Integer.parseInt(hStr);
            if (rStr != null) meta.rotation = Integer.parseInt(rStr);
            if (dStr != null) meta.durationMs = Long.parseLong(dStr);
            if (bStr != null) meta.bitrateKbps = (int) (Long.parseLong(bStr) / 1000);
            if (mime != null) {
                meta.codec = (mime.contains("hevc") || mime.contains("h265")) ? "HEVC / H.265" : "AVC / H.264";
            }

            FpsExtractor.FpsResult fpsRes = FpsExtractor.detectFps(getContext(), uri,
                    (uri != null && "file".equalsIgnoreCase(uri.getScheme())) ? uri.getPath() : null,
                    mmr, meta.durationMs);
            meta.fps = fpsRes.exactFps;

            if (meta.rotation == 90 || meta.rotation == 270) {
                meta.displayWidth = meta.rawHeight;
                meta.displayHeight = meta.rawWidth;
            } else {
                meta.displayWidth = meta.rawWidth;
                meta.displayHeight = meta.rawHeight;
            }

            if (meta.displayWidth > 0 && meta.displayHeight > 0) {
                float ar = (float) meta.displayWidth / (float) meta.displayHeight;
                if (Math.abs(ar - 9f / 16f) < 0.05f) meta.aspectLabel = "9:16 Reel";
                else if (Math.abs(ar - 16f / 9f) < 0.05f) meta.aspectLabel = "16:9 Cinema";
                else if (Math.abs(ar - 4f / 3f) < 0.05f) meta.aspectLabel = "4:3 TV";
                else if (Math.abs(ar - 3f / 4f) < 0.05f) meta.aspectLabel = "3:4 Social";
                else if (Math.abs(ar - 1.0f) < 0.05f) meta.aspectLabel = "1:1 Square";
                else if (ar < 1.0f) meta.aspectLabel = String.format(Locale.US, "%.2f:1 Port", 1.0f / ar);
                else meta.aspectLabel = String.format(Locale.US, "%.2f:1 Land", ar);
            }

            // Determine file size
            if ("file".equalsIgnoreCase(uri.getScheme()) && uri.getPath() != null) {
                File f = new File(uri.getPath());
                if (f.exists()) meta.fileSizeBytes = f.length();
            } else {
                try {
                    android.content.res.AssetFileDescriptor afd = getContext().getContentResolver().openAssetFileDescriptor(uri, "r");
                    if (afd != null) {
                        meta.fileSizeBytes = afd.getLength();
                        afd.close();
                    }
                } catch (Exception ignored) {}
            }
        } catch (Exception e) {
            e.printStackTrace();
        } finally {
            if (mmr != null) {
                try { mmr.release(); } catch (Exception ignored) {}
            }
        }
        return meta;
    }

    private void updateTelemetryCardSpecs() {
        if (origMeta.displayWidth > 0) {
            if (tvOrigSpecs != null) {
                tvOrigSpecs.setText("" + origMeta.displayWidth + "x" + origMeta.displayHeight + " • " + origMeta.aspectLabel);
            }
            if (tvOrigBitrate != null) {
                String sizeStr = (origMeta.fileSizeBytes > 0) ? String.format(Locale.US, "%.1f MB", origMeta.fileSizeBytes / (1024.0 * 1024.0)) : "";
                String bitStr = (origMeta.bitrateKbps > 0) ? String.format(Locale.US, " • %.1fM", origMeta.bitrateKbps / 1000.0) : "";
                tvOrigBitrate.setText("" + sizeStr + bitStr + " • " + (origMeta.codec.contains("HEVC") ? "HEVC" : "AVC"));
            }
        }
        if (compMeta.displayWidth > 0) {
            if (tvCompSpecs != null) {
                tvCompSpecs.setText("" + compMeta.displayWidth + "x" + compMeta.displayHeight + " • " + compMeta.aspectLabel);
            }
            if (tvCompBitrate != null) {
                String sizeStr = (compMeta.fileSizeBytes > 0) ? String.format(Locale.US, "%.1f MB", compMeta.fileSizeBytes / (1024.0 * 1024.0)) : "";
                double savedPct = (origMeta.fileSizeBytes > 0 && compMeta.fileSizeBytes > 0) 
                        ? (1.0 - (double) compMeta.fileSizeBytes / origMeta.fileSizeBytes) * 100.0 : 0.0;
                String pctStr = (savedPct > 0) ? String.format(Locale.US, " • -%.0f%%", savedPct) : "";
                tvCompBitrate.setText("" + sizeStr + pctStr + " • " + (compMeta.codec.contains("HEVC") ? "HEVC" : "AVC"));
            }
        }
        updateBottomTelemetryCard();
    }

    private void updateBottomTelemetryCard() {
        if (tvTelemetryOrig != null) {
            if (origMeta.displayWidth > 0) {
                String sizeStr = (origMeta.fileSizeBytes > 0) 
                        ? String.format(Locale.US, "%.1f MB", origMeta.fileSizeBytes / (1024.0 * 1024.0)) : "Unknown";
                String durStr = (origMeta.durationMs > 0) 
                        ? String.format(Locale.US, "%02d:%02d", origMeta.durationMs / 60000, (origMeta.durationMs % 60000) / 1000) : "--:--";
                tvTelemetryOrig.setText(
                        "• Resolution: " + origMeta.displayWidth + "x" + origMeta.displayHeight + " (" + origMeta.aspectLabel + ")\n" +
                        "• Codec: " + origMeta.codec + " (Master)\n" +
                        "• Size: " + sizeStr + " • Bitrate: " + (origMeta.bitrateKbps > 0 ? (origMeta.bitrateKbps / 1000.0 + " Mbps") : "Dynamic") + "\n" +
                        "• Duration: " + durStr + " • Sync Clock: Strict Master"
                );
            } else {
                tvTelemetryOrig.setText("• Master Source\n• Dynamic Resolution\n• Full Audio Fidelity\n• Lockstep Timing");
            }
        }
        if (tvTelemetryComp != null) {
            if (compMeta.displayWidth > 0) {
                String sizeStr = (compMeta.fileSizeBytes > 0) 
                        ? String.format(Locale.US, "%.1f MB", compMeta.fileSizeBytes / (1024.0 * 1024.0)) : "Unknown";
                double savedPct = (origMeta.fileSizeBytes > 0 && compMeta.fileSizeBytes > 0)
                        ? (1.0 - (double) compMeta.fileSizeBytes / origMeta.fileSizeBytes) * 100.0 : 0.0;
                String pctStr = (savedPct > 0) ? String.format(Locale.US, " [-%.1f%%]", savedPct) : "";

                // File-size savings are real; perceptual scores are not measured here.
                long savedBytes = Math.max(0, origMeta.fileSizeBytes - compMeta.fileSizeBytes);
                double savedMb = savedBytes / (1024.0 * 1024.0);

                if (tvScorecardVmafVal != null) {
                    tvScorecardVmafVal.setText("n/a");
                }
                if (tvScorecardPsnrVal != null) {
                    tvScorecardPsnrVal.setText("n/a");
                }
                if (tvScorecardSavedVal != null) {
                    tvScorecardSavedVal.setText(String.format(Locale.US, "-%.1f%%", Math.max(0, savedPct)));
                }
                if (tvScorecardSavedSub != null) {
                    tvScorecardSavedSub.setText(String.format(Locale.US, "%.1f MB Freed", savedMb));
                }
                if (tvScorecardExplanation != null) {
                    tvScorecardExplanation.setText(String.format(Locale.US,
                            "Perceptual quality not measured. Compressed %.1f MB with %s.",
                            compMeta.fileSizeBytes / (1024.0 * 1024.0), compMeta.codec));
                }

                tvTelemetryComp.setText(
                        "• Resolution: " + compMeta.displayWidth + "x" + compMeta.displayHeight + " (" + compMeta.aspectLabel + ")\n" +
                        "• Codec: " + compMeta.codec + " (Hardware Accelerated)\n" +
                        "• Size: " + sizeStr + pctStr + " • Bitrate: " + (compMeta.bitrateKbps > 0 ? (compMeta.bitrateKbps / 1000.0 + " Mbps") : "Dynamic") + "\n" +
                        "• Quality metrics: not measured"
                );
            } else {
                tvTelemetryComp.setText("• Hardware Transcoded\n• High Storage Savings\n• Quality metrics: not measured");
            }
        }
    }

    // ==========================================
    // READINESS LATCH (Bug 1 & 7 fix)
    // ==========================================

    /**
     * Central gate: called whenever any of the 4 readiness bits changes.
     * Kicks off player init only once all required bits for a given side are true.
     * This prevents the race where initPlayer* is called before the SurfaceTexture exists.
     */
    private synchronized void initFromLatch() {
        // --- Original side ---
        if (pendingOrigUri != null && isSurfaceOrigReady && surfaceOriginal != null
                && !isOriginalPrepared && playerOriginal == null) {
            initPlayerOriginal();
        }
        // --- Compressed side ---
        if (pendingCompUri != null && isSurfaceCompReady && surfaceCompressed != null
                && !isCompressedPrepared && playerCompressed == null) {
            initPlayerCompressed();
        }
    }

    /**
     * Called when BOTH players are prepared. Seeks both to 0, then arms the player
     * so the next PLAY tap starts them back-to-back with zero buffering stall.
     */
    private synchronized void onBothPrepared() {
        boolean origOk = !isSurfaceOrigReady || (isOriginalPrepared && playerOriginal != null);
        boolean compOk = !isSurfaceCompReady || (isCompressedPrepared && playerCompressed != null);

        // Also handle solo-only modes (one side absent)
        boolean origNeeded = (pendingOrigUri != null);
        boolean compNeeded = (pendingCompUri != null);

        if (origNeeded && !isOriginalPrepared) return;
        if (compNeeded && !isCompressedPrepared) return;

        isArmed = true;
        initPending = false;

        // Seek both decoders to position 0 so they're pre-rendered at frame 0.
        // This eliminates the "first frame lag" on first Play press.
        try {
            if (playerOriginal != null && isOriginalPrepared) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    playerOriginal.seekTo(0L, MediaPlayer.SEEK_CLOSEST);
                } else {
                    playerOriginal.seekTo(0);
                }
            }
        } catch (Exception ignored) {}
        try {
            if (playerCompressed != null && isCompressedPrepared) {
                if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                    playerCompressed.seekTo(0L, MediaPlayer.SEEK_CLOSEST);
                } else {
                    playerCompressed.seekTo(0);
                }
            }
        } catch (Exception ignored) {}

        post(() -> {
            applyUnifiedTransform();
            updateTimeDisplay();
        });
    }

    private void initPlayerOriginal() {
        if (originalUri == null || surfaceOriginal == null) return;
        try {
            if (playerOriginal != null) {
                try { playerOriginal.release(); } catch (Exception ignored) {}
                playerOriginal = null;
            }
            isOriginalPrepared = false;
            isArmed = false;

            playerOriginal = new MediaPlayer();
            playerOriginal.setOnErrorListener((mp, what, extra) -> {
                android.util.Log.w("DuoPlayer", "playerOriginal error: what=" + what + ", extra=" + extra);
                isOriginalPrepared = false;
                if (origRetryCount < MAX_RECOVER_ATTEMPTS) {
                    origRetryCount++;
                    postDelayed(() -> {
                        try { if (playerOriginal != null) playerOriginal.release(); } catch (Exception ignored) {}
                        playerOriginal = null;
                        initFromLatch();
                    }, 200L * origRetryCount);
                }
                return true;
            });
            playerOriginal.setDataSource(getContext(), originalUri);
            playerOriginal.setSurface(surfaceOriginal);
            playerOriginal.setLooping(false);
            playerOriginal.setOnCompletionListener(mp -> post(this::handlePlaybackCompletion));
            playerOriginal.setOnSeekCompleteListener(mp -> {
                if (!isPlaying) post(this::updateTimeDisplay);
            });
            playerOriginal.setOnPreparedListener(mp -> {
                isOriginalPrepared = true;
                origRetryCount = 0;
                int vw = mp.getVideoWidth();
                int vh = mp.getVideoHeight();
                if (vw > 0 && vh > 0) {
                    if (origMeta.rawWidth <= 0) origMeta.rawWidth = vw;
                    if (origMeta.rawHeight <= 0) origMeta.rawHeight = vh;
                    if (origMeta.rotation == 90 || origMeta.rotation == 270) {
                        origMeta.displayWidth  = origMeta.rawHeight;
                        origMeta.displayHeight = origMeta.rawWidth;
                    } else {
                        origMeta.displayWidth  = origMeta.rawWidth;
                        origMeta.displayHeight = origMeta.rawHeight;
                    }
                }
                applyAudioRouting();
                updateTimeDisplay();
                updateTelemetryCardSpecs();
                onBothPrepared(); // latch gate
            });
            playerOriginal.prepareAsync();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void initPlayerCompressed() {
        if (compressedUri == null || surfaceCompressed == null) return;
        try {
            if (playerCompressed != null) {
                try { playerCompressed.release(); } catch (Exception ignored) {}
                playerCompressed = null;
            }
            isCompressedPrepared = false;
            isArmed = false;

            playerCompressed = new MediaPlayer();
            playerCompressed.setOnErrorListener((mp, what, extra) -> {
                android.util.Log.w("DuoPlayer", "playerCompressed error: what=" + what + ", extra=" + extra);
                isCompressedPrepared = false;
                if (compRetryCount < MAX_RECOVER_ATTEMPTS) {
                    compRetryCount++;
                    postDelayed(() -> {
                        try { if (playerCompressed != null) playerCompressed.release(); } catch (Exception ignored) {}
                        playerCompressed = null;
                        initFromLatch();
                    }, 200L * compRetryCount);
                }
                return true;
            });
            playerCompressed.setDataSource(getContext(), compressedUri);
            playerCompressed.setSurface(surfaceCompressed);
            playerCompressed.setLooping(false);
            playerCompressed.setOnCompletionListener(mp -> post(this::handlePlaybackCompletion));
            playerCompressed.setOnSeekCompleteListener(mp -> {
                if (!isPlaying) post(this::updateTimeDisplay);
            });
            playerCompressed.setOnPreparedListener(mp -> {
                isCompressedPrepared = true;
                compRetryCount = 0;
                int vw = mp.getVideoWidth();
                int vh = mp.getVideoHeight();
                if (vw > 0 && vh > 0) {
                    if (compMeta.rawWidth <= 0) compMeta.rawWidth = vw;
                    if (compMeta.rawHeight <= 0) compMeta.rawHeight = vh;
                    if (compMeta.rotation == 90 || compMeta.rotation == 270) {
                        compMeta.displayWidth  = compMeta.rawHeight;
                        compMeta.displayHeight = compMeta.rawWidth;
                    } else {
                        compMeta.displayWidth  = compMeta.rawWidth;
                        compMeta.displayHeight = compMeta.rawHeight;
                    }
                }
                applyAudioRouting();
                updateTimeDisplay();
                updateTelemetryCardSpecs();
                onBothPrepared(); // latch gate
            });
            playerCompressed.prepareAsync();
        } catch (Exception e) {
            e.printStackTrace();
        }
    }

    private void setupTextureCompressed() {
        textureCompressed.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
            @Override
            public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int width, int height) {
                // Release any stale wrapper first (Bug 7 fix)
                if (surfaceCompressed != null) {
                    try { surfaceCompressed.release(); } catch (Exception ignored) {}
                }
                surfaceCompressed   = new Surface(surfaceTexture);
                isSurfaceCompReady  = true;
                // A recycled TextureView can arrive while an existing player still
                // points at the released Surface -> rebind so playback stays visible.
                if (playerCompressed != null && isCompressedPrepared) {
                    try { playerCompressed.setSurface(surfaceCompressed); } catch (Exception ignored) {}
                }
                // If a URI is pending but player isn't running yet, kick off init now
                if (pendingCompUri != null && compressedUri != null && !isCompressedPrepared && playerCompressed == null) {
                    initFromLatch();
                }
                post(() -> applyUnifiedTransform());
            }

            @Override
            public void onSurfaceTextureSizeChanged(SurfaceTexture surfaceTexture, int width, int height) {
                post(() -> applyUnifiedTransform());
            }

            @Override
            public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
                isSurfaceCompReady = false;
                // Don't release the Surface here — the MediaPlayer still holds a reference.
                // Setting null prevents initFromLatch from using a stale handle.
                surfaceCompressed = null;
                return true;
            }

            @Override
            public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {}
        });
    }

    private void setupTextureOriginal() {
        textureOriginal.setSurfaceTextureListener(new TextureView.SurfaceTextureListener() {
            @Override
            public void onSurfaceTextureAvailable(SurfaceTexture surfaceTexture, int width, int height) {
                // Release any stale wrapper first (Bug 7 fix)
                if (surfaceOriginal != null) {
                    try { surfaceOriginal.release(); } catch (Exception ignored) {}
                }
                surfaceOriginal   = new Surface(surfaceTexture);
                isSurfaceOrigReady = true;
                // Rebind an existing player to the recycled Surface to avoid black frames.
                if (playerOriginal != null && isOriginalPrepared) {
                    try { playerOriginal.setSurface(surfaceOriginal); } catch (Exception ignored) {}
                }
                if (pendingOrigUri != null && originalUri != null && !isOriginalPrepared && playerOriginal == null) {
                    initFromLatch();
                }
                post(() -> applyUnifiedTransform());
            }

            @Override
            public void onSurfaceTextureSizeChanged(SurfaceTexture surfaceTexture, int width, int height) {
                post(() -> applyUnifiedTransform());
            }

            @Override
            public boolean onSurfaceTextureDestroyed(SurfaceTexture surfaceTexture) {
                isSurfaceOrigReady = false;
                surfaceOriginal    = null;
                return true;
            }

            @Override
            public void onSurfaceTextureUpdated(SurfaceTexture surfaceTexture) {}
        });
    }

    private Button createTransportButton(Context context, String text, OnClickListener onClickListener) {
        Button btn = new Button(context);
        btn.setText(text);
        btn.setTextColor(ThemeStyles.COLOR_TEXT_PRIMARY);
        btn.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btn.setTypeface(Typeface.DEFAULT_BOLD);
        LinearLayout.LayoutParams params = new LinearLayout.LayoutParams(0, dp(46), 0.9f);
        btn.setLayoutParams(params);
        btn.setBackground(createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(14)));
        btn.setOnClickListener(onClickListener);
        return btn;
    }

    private LinearLayout createScorecardLayout(Context context) {
        LinearLayout scorecard = new LinearLayout(context);
        scorecard.setOrientation(VERTICAL);
        scorecard.setPadding(dp(18), dp(18), dp(18), dp(18));
        scorecard.setBackgroundColor(Color.parseColor("#F0050811"));
        scorecard.setGravity(Gravity.CENTER);

        TextView tvScTitle = new TextView(context);
        tvScTitle.setText("LIVE PERCEPTUAL FIDELITY SCORECARD");
        tvScTitle.setTextColor(ThemeStyles.COLOR_CYAN_NEON);
        tvScTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvScTitle.setTypeface(Typeface.DEFAULT_BOLD);
        scorecard.addView(tvScTitle);

        LinearLayout rowMetrics = new LinearLayout(context);
        rowMetrics.setOrientation(HORIZONTAL);
        rowMetrics.setWeightSum(3);
        rowMetrics.setPadding(0, dp(16), 0, dp(16));

        tvScorecardVmafVal = addScorecardMetricReturningVal(context, rowMetrics, "VMAF-NEG", "--", ThemeStyles.COLOR_MINT_TEXT, "Awaiting Telemetry");
        tvScorecardPsnrVal = addScorecardMetricReturningVal(context, rowMetrics, "PSNR", "-- dB", ThemeStyles.COLOR_CYAN_TEXT, "Awaiting Telemetry");
        
        TextView[] savedPair = addScorecardMetricReturningBoth(context, rowMetrics, "SAVED", "--%", ThemeStyles.COLOR_AMBER_SUNSET, "Awaiting Pair");
        tvScorecardSavedVal = savedPair[0];
        tvScorecardSavedSub = savedPair[1];

        scorecard.addView(rowMetrics);

        tvScorecardExplanation = new TextView(context);
        tvScorecardExplanation.setText("Quality metrics are not measured by this view.");
        tvScorecardExplanation.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvScorecardExplanation.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvScorecardExplanation.setGravity(Gravity.CENTER);
        scorecard.addView(tvScorecardExplanation);

        return scorecard;
    }

    private TextView addScorecardMetricReturningVal(Context context, LinearLayout parent, String label, String val, int color, String sub) {
        TextView[] pair = addScorecardMetricReturningBoth(context, parent, label, val, color, sub);
        return pair[0];
    }

    private TextView[] addScorecardMetricReturningBoth(Context context, LinearLayout parent, String label, String val, int color, String sub) {
        LinearLayout col = new LinearLayout(context);
        col.setOrientation(VERTICAL);
        col.setGravity(Gravity.CENTER);
        col.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        TextView tvVal = new TextView(context);
        tvVal.setText(val);
        tvVal.setTextColor(color);
        tvVal.setTextSize(TypedValue.COMPLEX_UNIT_SP, 19);
        tvVal.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        col.addView(tvVal);

        TextView tvLbl = new TextView(context);
        tvLbl.setText(label);
        tvLbl.setTextColor(Color.WHITE);
        tvLbl.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvLbl.setTypeface(Typeface.DEFAULT_BOLD);
        col.addView(tvLbl);

        TextView tvSub = new TextView(context);
        tvSub.setText(sub);
        tvSub.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        tvSub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        col.addView(tvSub);

        parent.addView(col);
        return new TextView[]{tvVal, tvSub};
    }

    public void releasePlayers() {
        isPlaying            = false;
        isOriginalPrepared   = false;
        isCompressedPrepared = false;
        isArmed              = false;
        handler.removeCallbacks(progressUpdater);

        if (playerOriginal != null) {
            try { playerOriginal.stop();    } catch (Exception ignored) {}
            try { playerOriginal.release(); } catch (Exception ignored) {}
            playerOriginal = null;
        }
        if (playerCompressed != null) {
            try { playerCompressed.stop();    } catch (Exception ignored) {}
            try { playerCompressed.release(); } catch (Exception ignored) {}
            playerCompressed = null;
        }
        // Don't release the Surface wrappers here — the TextureView SurfaceTexture
        // is still alive and will re-notify us via onSurfaceTextureAvailable if needed.
        // Releasing them would orphan the native window handle.
    }
}
