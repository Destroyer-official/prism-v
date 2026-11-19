package com.prismv.ui;

import android.app.Activity;
import android.app.Dialog;
import android.content.Context;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.media.MediaMetadataRetriever;
import android.media.ThumbnailUtils;
import android.net.Uri;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.util.LruCache;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.*;

import com.prismv.history.CompressionHistoryManager;
import com.prismv.provider.PrismFileProvider;

import java.io.File;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * CompressedVideosView:
 * Screen 24: Compressed Media Library & Comparison Vault.
 *
 * Implements:
 * 1. Storage Reclaimed Metric Hero Meter.
 * 2. Instant 1-Click Launch into Duo Player for ANY previous compression job.
 * 3. Video sharing, standalone playback, and safe disk deletion.
 * 4. Real-time search & filter bar.
 */
public class CompressedVideosView extends LinearLayout {

    public interface OnCompareRequestedListener {
        void onCompareRequested(CompressionHistoryManager.Record record);
    }

    private static final LruCache<String, Bitmap> THUMB_CACHE = new LruCache<>(32);
    private static final ExecutorService THUMB_EXECUTOR = Executors.newFixedThreadPool(2);

    private final CompressionHistoryManager history;
    private LinearLayout listContainer;
    private TextView tvTotalSaved;
    private TextView tvTotalCount;
    private TextView tvTrashVaultSummary;
    private Button btnSafeTrashAll;
    private Button btnTrashVaultHeader;
    private EditText etSearch;
    private OnCompareRequestedListener compareListener;
    private String currentFilterMode = "ALL"; // "ALL", "SOURCE_ON_DISK", "IN_TRASH"
    private Button btnFilterAll;
    private Button btnFilterSourceDisk;
    private Button btnFilterTrashed;

    public static void loadVideoThumbnail(String videoPath, ImageView iv) {
        if (videoPath == null) return;
        Bitmap cached = THUMB_CACHE.get(videoPath);
        if (cached != null) {
            iv.setImageBitmap(cached);
            iv.setScaleType(ImageView.ScaleType.CENTER_CROP);
            return;
        }

        THUMB_EXECUTOR.execute(() -> {
            Bitmap thumb = null;
            try {
                if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.Q) {
                    thumb = ThumbnailUtils.createVideoThumbnail(
                            new File(videoPath),
                            new android.util.Size(320, 200),
                            null
                    );
                } else {
                    thumb = ThumbnailUtils.createVideoThumbnail(
                            videoPath,
                            android.provider.MediaStore.Images.Thumbnails.MINI_KIND
                    );
                }
            } catch (Throwable ignored) {}

            if (thumb == null) {
                try {
                    MediaMetadataRetriever mmr = new MediaMetadataRetriever();
                    mmr.setDataSource(videoPath);
                    thumb = mmr.getFrameAtTime(1000000, MediaMetadataRetriever.OPTION_CLOSEST_SYNC);
                    if (thumb == null) {
                        thumb = mmr.getFrameAtTime();
                    }
                    mmr.release();
                } catch (Throwable ignored) {}
            }

            if (thumb != null) {
                final Bitmap bmp = thumb;
                THUMB_CACHE.put(videoPath, bmp);
                iv.post(() -> {
                    iv.setImageBitmap(bmp);
                    iv.setScaleType(ImageView.ScaleType.CENTER_CROP);
                });
            }
        });
    }

    public CompressedVideosView(Context context) {
        super(context);
        this.history = CompressionHistoryManager.get(context);
        init(context);
    }

    public CompressedVideosView(Context context, AttributeSet attrs) {
        super(context, attrs);
        this.history = CompressionHistoryManager.get(context);
        init(context);
    }

    public void setOnCompareRequestedListener(OnCompareRequestedListener listener) {
        this.compareListener = listener;
    }

    private int dp(int value) {
        return ThemeStyles.dp(getContext(), value);
    }

    private GradientDrawable createRoundedDrawable(int bgColor, int strokeColor, int strokeWidth, float radiusPx) {
        return ThemeStyles.createRoundedDrawable(bgColor, strokeColor, strokeWidth, radiusPx);
    }

    private void init(Context context) {
        setOrientation(VERTICAL);
        setBackgroundColor(ThemeStyles.COLOR_BG_CANVAS);
        setPadding(dp(16), dp(12), dp(16), dp(36));

        // ==========================================
        // 1. TOP HEADER ROW
        // ==========================================
        LinearLayout headerRow = new LinearLayout(context);
        headerRow.setOrientation(HORIZONTAL);
        headerRow.setGravity(Gravity.CENTER_VERTICAL);
        headerRow.setPadding(0, 0, 0, dp(12));

        TextView tvTitle = new TextView(context);
        tvTitle.setText("COMPRESSED VIDEOS LIBRARY");
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        LinearLayout.LayoutParams tp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        tvTitle.setLayoutParams(tp);
        headerRow.addView(tvTitle);

        tvTotalCount = new TextView(context);
        tvTotalCount.setText("0 Videos");
        tvTotalCount.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvTotalCount.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvTotalCount.setTypeface(Typeface.DEFAULT_BOLD);
        tvTotalCount.setPadding(dp(8), dp(4), dp(8), dp(4));
        tvTotalCount.setBackground(createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(10)));
        headerRow.addView(tvTotalCount);

        btnTrashVaultHeader = new Button(context);
        btnTrashVaultHeader.setText("🗑 30d Vault (0)");
        btnTrashVaultHeader.setTextColor(Color.WHITE);
        btnTrashVaultHeader.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnTrashVaultHeader.setTypeface(Typeface.DEFAULT_BOLD);
        btnTrashVaultHeader.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#B45309"), Color.parseColor("#F59E0B"), dp(8)));
        btnTrashVaultHeader.setPadding(dp(8), dp(4), dp(8), dp(4));
        LinearLayout.LayoutParams tvp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(32));
        tvp.setMargins(dp(6), 0, 0, 0);
        btnTrashVaultHeader.setLayoutParams(tvp);
        btnTrashVaultHeader.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            showTrashVaultDialog(context, () -> refreshRecords(etSearch.getText().toString()));
        });
        headerRow.addView(btnTrashVaultHeader);

        Button btnOpenFolder = new Button(context);
        btnOpenFolder.setText("Folder");
        btnOpenFolder.setTextColor(Color.WHITE);
        btnOpenFolder.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnOpenFolder.setTypeface(Typeface.DEFAULT_BOLD);
        btnOpenFolder.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#0E7490"), Color.parseColor("#06B6D4"), dp(8)));
        btnOpenFolder.setPadding(dp(10), dp(4), dp(10), dp(4));
        LinearLayout.LayoutParams ofp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(32));
        ofp.setMargins(dp(6), 0, 0, 0);
        btnOpenFolder.setLayoutParams(ofp);
        btnOpenFolder.setOnClickListener(v -> com.prismv.storage.PrismStorageManager.openFolder(context));
        headerRow.addView(btnOpenFolder);

        addView(headerRow);

        // ==========================================
        // 2. HERO STORAGE RECLAIMED METER
        // ==========================================
        LinearLayout heroCard = new LinearLayout(context);
        heroCard.setOrientation(VERTICAL);
        heroCard.setPadding(dp(16), dp(14), dp(16), dp(14));
        heroCard.setBackground(createRoundedDrawable(Color.parseColor("#09172B"), ThemeStyles.COLOR_EMERALD_NEON, dp(1), dp(16)));
        LinearLayout.LayoutParams hp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        hp.setMargins(0, 0, 0, dp(12));
        heroCard.setLayoutParams(hp);

        TextView tvHeroSub = new TextView(context);
        tvHeroSub.setText("TOTAL STORAGE RECLAIMED ON CHIP");
        tvHeroSub.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvHeroSub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvHeroSub.setTypeface(Typeface.DEFAULT_BOLD);
        heroCard.addView(tvHeroSub);

        tvTotalSaved = new TextView(context);
        tvTotalSaved.setText("0.0 MB Saved");
        tvTotalSaved.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        tvTotalSaved.setTextSize(TypedValue.COMPLEX_UNIT_SP, 22);
        tvTotalSaved.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        tvTotalSaved.setPadding(0, dp(2), 0, 0);
        heroCard.addView(tvTotalSaved);

        tvTrashVaultSummary = new TextView(context);
        tvTrashVaultSummary.setText("30-Day Vault: 0 files (0.0 MB) • Sources on Disk: 0");
        tvTrashVaultSummary.setTextColor(Color.parseColor("#94A3B8"));
        tvTrashVaultSummary.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvTrashVaultSummary.setPadding(0, dp(4), 0, 0);
        heroCard.addView(tvTrashVaultSummary);

        btnSafeTrashAll = new Button(context);
        btnSafeTrashAll.setText("🗑 TRASH ALL ORIGINAL SOURCES TO 30D VAULT");
        btnSafeTrashAll.setTextColor(Color.WHITE);
        btnSafeTrashAll.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnSafeTrashAll.setTypeface(Typeface.DEFAULT_BOLD);
        btnSafeTrashAll.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#B45309"), Color.parseColor("#F59E0B"), dp(10)));
        btnSafeTrashAll.setPadding(dp(12), dp(6), dp(12), dp(6));
        btnSafeTrashAll.setVisibility(GONE);
        LinearLayout.LayoutParams stap = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(36));
        stap.setMargins(0, dp(8), 0, 0);
        btnSafeTrashAll.setLayoutParams(stap);
        btnSafeTrashAll.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            List<CompressionHistoryManager.Record> records = history.getRecords();
            List<File> toTrash = new java.util.ArrayList<>();
            long bytesToFree = 0;
            for (CompressionHistoryManager.Record r : records) {
                File sf = resolveSourceFile(r);
                if (sf != null && sf.exists()) {
                    toTrash.add(sf);
                    bytesToFree += sf.length();
                }
            }
            if (toTrash.isEmpty()) return;
            final long finalBytes = bytesToFree;
            CustomDarkDialog.showConfirmation(context,
                    "Trash All Original Sources?",
                    String.format(Locale.US, "Move all %d original source videos to the 30-Day Recycle Bin?\n\n✓ Frees +%.1f MB immediately\n✓ Safely kept in 30-day vault\n✓ Can restore anytime\n✓ Compressed videos remain 100%% playable",
                            toTrash.size(), finalBytes / (1024.0 * 1024.0)),
                    "Trash All Sources", "Cancel", () -> {
                        int moved = 0;
                        for (File f : toTrash) {
                            if (com.prismv.storage.PrismStorageManager.moveToTrash(context, f)) moved++;
                        }
                        for (CompressionHistoryManager.Record r : records) {
                            r.isSourceTrashed = true;
                            history.updateRecord(r);
                        }
                        refreshRecords(etSearch.getText().toString());
                        if (context instanceof Activity) {
                            PrismHud.showSuccess((Activity) context, String.format(Locale.US, "Moved %d sources to 30D Vault! Reclaimed %.1f MB.", moved, finalBytes / (1024.0 * 1024.0)));
                        }
                    });
        });
        heroCard.addView(btnSafeTrashAll);
        addView(heroCard);

        // ==========================================
        // 3. SEARCH & FILTER BAR
        // ==========================================
        LinearLayout searchBox = new LinearLayout(context);
        searchBox.setOrientation(HORIZONTAL);
        searchBox.setGravity(Gravity.CENTER_VERTICAL);
        searchBox.setPadding(dp(12), dp(2), dp(12), dp(2));
        searchBox.setBackground(createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(14)));
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(44));
        sp.setMargins(0, 0, 0, dp(8));
        searchBox.setLayoutParams(sp);

        TextView tvSearchIcon = new TextView(context);
        tvSearchIcon.setText("");
        tvSearchIcon.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvSearchIcon.setPadding(0, 0, dp(8), 0);
        searchBox.addView(tvSearchIcon);

        etSearch = new EditText(context);
        etSearch.setHint("Search compressed files by title or date...");
        etSearch.setHintTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        etSearch.setTextColor(Color.WHITE);
        etSearch.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        etSearch.setBackground(null);
        etSearch.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f));
        etSearch.addTextChangedListener(new TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override
            public void onTextChanged(CharSequence s, int start, int before, int count) {
                refreshRecords(s.toString());
            }
            @Override
            public void afterTextChanged(Editable s) {}
        });
        searchBox.addView(etSearch);
        addView(searchBox);

        // Filter Pills Row
        LinearLayout filterRow = new LinearLayout(context);
        filterRow.setOrientation(HORIZONTAL);
        filterRow.setPadding(0, 0, 0, dp(12));

        btnFilterAll = new Button(context);
        btnFilterAll.setText("ALL (0)");
        btnFilterAll.setTextColor(Color.WHITE);
        btnFilterAll.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnFilterAll.setTypeface(Typeface.DEFAULT_BOLD);
        btnFilterAll.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#0E7490"), Color.parseColor("#06B6D4"), dp(8)));
        LinearLayout.LayoutParams faParams = new LinearLayout.LayoutParams(0, dp(34), 1.0f);
        faParams.setMargins(0, 0, dp(4), 0);
        btnFilterAll.setLayoutParams(faParams);
        btnFilterAll.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            currentFilterMode = "ALL";
            refreshRecords(etSearch.getText().toString());
        });
        filterRow.addView(btnFilterAll);

        btnFilterSourceDisk = new Button(context);
        btnFilterSourceDisk.setText("🟢 ON DISK (0)");
        btnFilterSourceDisk.setTextColor(Color.parseColor("#94A3B8"));
        btnFilterSourceDisk.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnFilterSourceDisk.setTypeface(Typeface.DEFAULT_BOLD);
        btnFilterSourceDisk.setBackground(createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(8)));
        LinearLayout.LayoutParams fsdParams = new LinearLayout.LayoutParams(0, dp(34), 1.2f);
        fsdParams.setMargins(dp(2), 0, dp(4), 0);
        btnFilterSourceDisk.setLayoutParams(fsdParams);
        btnFilterSourceDisk.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            currentFilterMode = "SOURCE_ON_DISK";
            refreshRecords(etSearch.getText().toString());
        });
        filterRow.addView(btnFilterSourceDisk);

        btnFilterTrashed = new Button(context);
        btnFilterTrashed.setText("🟡 IN 30D VAULT (0)");
        btnFilterTrashed.setTextColor(Color.parseColor("#94A3B8"));
        btnFilterTrashed.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnFilterTrashed.setTypeface(Typeface.DEFAULT_BOLD);
        btnFilterTrashed.setBackground(createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(8)));
        LinearLayout.LayoutParams ftParams = new LinearLayout.LayoutParams(0, dp(34), 1.3f);
        ftParams.setMargins(dp(2), 0, 0, 0);
        btnFilterTrashed.setLayoutParams(ftParams);
        btnFilterTrashed.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            currentFilterMode = "IN_TRASH";
            refreshRecords(etSearch.getText().toString());
        });
        filterRow.addView(btnFilterTrashed);

        addView(filterRow);

        // ==========================================
        // 4. LIST CONTAINER
        // ==========================================
        listContainer = new LinearLayout(context);
        listContainer.setOrientation(VERTICAL);
        addView(listContainer);

        refreshRecords("");
    }

    public static File findPossibleSourceFile(String fileName) {
        if (fileName == null) return null;
        String base = fileName;
        if (base.startsWith("input_")) {
            base = base.substring(6);
        }
        if (base.endsWith("_optimized.mp4")) {
            base = base.substring(0, base.length() - "_optimized.mp4".length()) + ".mp4";
        } else if (base.endsWith("_compressed.mp4")) {
            base = base.substring(0, base.length() - "_compressed.mp4".length()) + ".mp4";
        } else if (base.endsWith(".mkv_optimized.mp4")) {
            base = base.substring(0, base.length() - "_optimized.mp4".length());
        }

        File[] searchDirs = new File[]{
                new File("/sdcard/sample_video"),
                new File("/sdcard/DCIM/Camera"),
                new File("/storage/emulated/0/DCIM/Camera"),
                new File("/storage/emulated/0/Movies"),
                new File("/storage/emulated/0/Download"),
                new File("/sdcard/Download")
        };

        for (File dir : searchDirs) {
            if (dir.exists()) {
                File candidate = new File(dir, base);
                if (candidate.exists() && candidate.length() > 0) {
                    return candidate;
                }
            }
        }
        return null;
    }

    public static File resolveSourceFile(CompressionHistoryManager.Record r) {
        if (r == null) return null;
        if (r.originalPath != null && !r.originalPath.isEmpty()) {
            File f = new File(r.originalPath);
            if (f.exists() && f.length() > 0 && (r.compressedPath == null || !f.getAbsolutePath().equals(r.compressedPath))) return f;
        }
        if (r.title != null) {
            File f = findPossibleSourceFile(r.title);
            if (f != null && f.exists() && f.length() > 0 && (r.compressedPath == null || !f.getAbsolutePath().equals(r.compressedPath))) return f;
        }
        if (r.compressedPath != null) {
            File f = findPossibleSourceFile(new File(r.compressedPath).getName());
            if (f != null && f.exists() && f.length() > 0 && !f.getAbsolutePath().equals(r.compressedPath)) return f;
        }
        return null;
    }

    public static File findTrashedSourceFile(Context context, CompressionHistoryManager.Record r) {
        if (r == null) return null;
        if (r.trashedSourcePath != null && !r.trashedSourcePath.isEmpty()) {
            File f = new File(r.trashedSourcePath);
            if (f.exists() && f.length() > 0) return f;
        }
        List<File> trashed = com.prismv.storage.PrismStorageManager.listTrashVideos(context);
        for (File tf : trashed) {
            String name = tf.getName().toLowerCase(Locale.US);
            if (r.title != null) {
                String cleanTitle = r.title.toLowerCase(Locale.US);
                if (cleanTitle.startsWith("input_")) cleanTitle = cleanTitle.substring(6);
                if (name.contains(cleanTitle)) return tf;
            }
            if (r.originalPath != null) {
                String origName = new File(r.originalPath).getName().toLowerCase(Locale.US);
                if (origName.startsWith("input_")) origName = origName.substring(6);
                if (name.contains(origName)) return tf;
            }
            if (r.compressedPath != null) {
                String compName = new File(r.compressedPath).getName().toLowerCase(Locale.US);
                String base = compName.replace("_optimized.mp4", "").replace("_compressed.mp4", "");
                if (base.startsWith("input_")) base = base.substring(6);
                if (name.contains(base)) return tf;
            }
        }
        return null;
    }

    public void refreshRecords(String filterQuery) {
        listContainer.removeAllViews();

        // 1. Auto-cleanup any trashed files older than 30 days
        com.prismv.storage.PrismStorageManager.cleanupExpiredTrash(getContext());

        // 2. Sync with physical files on disk in Movies/Prism-V/
        List<File> diskFiles = com.prismv.storage.PrismStorageManager.listSavedVideos(getContext());
        List<CompressionHistoryManager.Record> currentRecords = history.getRecords();
        for (File df : diskFiles) {
            boolean found = false;
            for (CompressionHistoryManager.Record r : currentRecords) {
                if (r.compressedPath != null) {
                    String rName = new File(r.compressedPath).getName();
                    String cleanRName = rName.startsWith("input_") ? rName.substring(6) : rName;
                    String cleanDfName = df.getName().startsWith("input_") ? df.getName().substring(6) : df.getName();
                    if (r.compressedPath.equals(df.getAbsolutePath()) || rName.equals(df.getName()) || cleanRName.equals(cleanDfName)) {
                        found = true;
                        // Heal record with real source file if missing
                        if (r.originalSizeBytes <= 0 || r.getOriginalUri() == null) {
                            File src = findPossibleSourceFile(cleanDfName);
                            if (src != null && src.exists()) {
                                r.originalPath = src.getAbsolutePath();
                                r.originalSizeBytes = src.length();
                                r.spaceSavedBytes = Math.max(0, r.originalSizeBytes - r.compressedSizeBytes);
                                r.percentReduction = r.originalSizeBytes > 0 ? (r.spaceSavedBytes / (double) r.originalSizeBytes) * 100.0 : 0.0;
                            }
                        }
                        break;
                    }
                }
            }
            if (!found) {
                File possibleSrc = findPossibleSourceFile(df.getName());
                long origSize = (possibleSrc != null && possibleSrc.exists()) ? possibleSrc.length() : (long) (df.length() * 3.5);
                Uri origUri = (possibleSrc != null && possibleSrc.exists()) ? Uri.fromFile(possibleSrc) : null;
                String origPath = (possibleSrc != null && possibleSrc.exists()) ? possibleSrc.getAbsolutePath() : null;
                history.recordCompression(
                        df.getName(),
                        origUri,
                        origPath,
                        origSize,
                        df.getAbsolutePath(),
                        df.length(),
                        "H-ARCH (Movies/Prism-V)",
                        1920,
                        1080,
                        2500,
                        30,
                        60000L
                );
            }
        }

        List<CompressionHistoryManager.Record> records = history.getRecords();
        String query = filterQuery != null ? filterQuery.trim().toLowerCase(Locale.US) : "";

        long totalOrig = 0;
        long totalComp = 0;
        int countSourceOnDisk = 0;
        int countInTrash = 0;
        long potentialSourceBytes = 0;
        int matchingCount = 0;

        List<File> trashList = com.prismv.storage.PrismStorageManager.listTrashVideos(getContext());
        long totalTrashBytes = com.prismv.storage.PrismStorageManager.getTrashTotalSizeBytes(getContext());

        for (CompressionHistoryManager.Record r : records) {
            totalOrig += r.originalSizeBytes;
            totalComp += r.compressedSizeBytes;

            File sf = resolveSourceFile(r);
            File tf = findTrashedSourceFile(getContext(), r);
            boolean hasSourceOnDisk = (sf != null && sf.exists());
            boolean isTrashed = (tf != null && tf.exists()) || r.isSourceTrashed;

            if (hasSourceOnDisk) {
                countSourceOnDisk++;
                potentialSourceBytes += sf.length();
            }
            if (isTrashed) {
                countInTrash++;
            }

            // Filter mode check
            if ("SOURCE_ON_DISK".equals(currentFilterMode) && !hasSourceOnDisk) {
                continue;
            }
            if ("IN_TRASH".equals(currentFilterMode) && !isTrashed) {
                continue;
            }

            if (!query.isEmpty()) {
                String title = r.title != null ? r.title.toLowerCase(Locale.US) : "";
                String date = r.formattedDate != null ? r.formattedDate.toLowerCase(Locale.US) : "";
                String mode = r.mode != null ? r.mode.toLowerCase(Locale.US) : "";
                if (!title.contains(query) && !date.contains(query) && !mode.contains(query)) {
                    continue;
                }
            }

            matchingCount++;
            addRecordCard(getContext(), r);
        }

        long totalSaved = Math.max(0, totalOrig - totalComp);
        double totalPct = totalOrig > 0 ? (totalSaved / (double) totalOrig) * 100.0 : 0.0;
        tvTotalSaved.setText(String.format(Locale.US, "%.1f MB Saved (-%.1f%%)",
                totalSaved / (1024.0 * 1024.0), totalPct));
        tvTotalCount.setText(records.size() + " Total Videos");

        if (btnTrashVaultHeader != null) {
            btnTrashVaultHeader.setText(String.format(Locale.US, "🗑 30d Vault (%d)", trashList.size()));
        }

        if (tvTrashVaultSummary != null) {
            tvTrashVaultSummary.setText(String.format(Locale.US,
                    "30-Day Vault: %d files (%.1f MB) • Sources on Disk: %d (%.1f MB reclaimable)",
                    trashList.size(), totalTrashBytes / (1024.0 * 1024.0),
                    countSourceOnDisk, potentialSourceBytes / (1024.0 * 1024.0)));
        }

        if (btnSafeTrashAll != null) {
            if (countSourceOnDisk > 0) {
                btnSafeTrashAll.setVisibility(VISIBLE);
                btnSafeTrashAll.setText(String.format(Locale.US, "TRASH ALL %d SOURCES (+%.1f MB TO 30D VAULT)",
                        countSourceOnDisk, potentialSourceBytes / (1024.0 * 1024.0)));
            } else {
                btnSafeTrashAll.setVisibility(GONE);
            }
        }

        // Update Filter Buttons UI
        if (btnFilterAll != null) {
            btnFilterAll.setText("ALL (" + records.size() + ")");
            boolean active = "ALL".equals(currentFilterMode);
            btnFilterAll.setBackground(active
                    ? ThemeStyles.createGradientButton(Color.parseColor("#0E7490"), Color.parseColor("#06B6D4"), dp(8))
                    : createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(8)));
            btnFilterAll.setTextColor(active ? Color.WHITE : Color.parseColor("#94A3B8"));
        }
        if (btnFilterSourceDisk != null) {
            btnFilterSourceDisk.setText("🟢 ON DISK (" + countSourceOnDisk + ")");
            boolean active = "SOURCE_ON_DISK".equals(currentFilterMode);
            btnFilterSourceDisk.setBackground(active
                    ? ThemeStyles.createGradientButton(Color.parseColor("#059669"), Color.parseColor("#10B981"), dp(8))
                    : createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(8)));
            btnFilterSourceDisk.setTextColor(active ? Color.WHITE : Color.parseColor("#94A3B8"));
        }
        if (btnFilterTrashed != null) {
            btnFilterTrashed.setText("🟡 IN 30D VAULT (" + countInTrash + ")");
            boolean active = "IN_TRASH".equals(currentFilterMode);
            btnFilterTrashed.setBackground(active
                    ? ThemeStyles.createGradientButton(Color.parseColor("#B45309"), Color.parseColor("#F59E0B"), dp(8))
                    : createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(8)));
            btnFilterTrashed.setTextColor(active ? Color.WHITE : Color.parseColor("#94A3B8"));
        }

        if (matchingCount == 0) {
            TextView emptyView = new TextView(getContext());
            if ("SOURCE_ON_DISK".equals(currentFilterMode)) {
                emptyView.setText("All source master files have been cleaned from disk!\nOriginal storage is 100% reclaimed.");
            } else if ("IN_TRASH".equals(currentFilterMode)) {
                emptyView.setText("No sources currently in the 30-Day Recycle Bin.\nUse 'Trash Source' on any video to protect it in the vault.");
            } else {
                emptyView.setText(records.isEmpty()
                        ? "No compressed videos yet.\nCompress a video in Studio to see detailed metrics and compare in Duo Player."
                        : "No videos match '" + filterQuery + "'.");
            }
            emptyView.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
            emptyView.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
            emptyView.setGravity(Gravity.CENTER);
            emptyView.setPadding(0, dp(30), 0, dp(30));
            listContainer.addView(emptyView);
        }
    }

    private void addRecordCard(Context context, CompressionHistoryManager.Record r) {
        LinearLayout card = new LinearLayout(context);
        card.setOrientation(VERTICAL);
        card.setPadding(dp(14), dp(12), dp(14), dp(12));
        card.setBackground(createRoundedDrawable(ThemeStyles.COLOR_BG_CARD, ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(16)));

        LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        cp.setMargins(0, 0, 0, dp(12));
        card.setLayoutParams(cp);

        // ROW 1: Media Header (Thumbnail on Left + Video Title & Badges on Right)
        LinearLayout rowMediaHeader = new LinearLayout(context);
        rowMediaHeader.setOrientation(HORIZONTAL);
        rowMediaHeader.setGravity(Gravity.CENTER_VERTICAL);

        // Thumbnail FrameLayout Container
        FrameLayout thumbContainer = new FrameLayout(context);
        LinearLayout.LayoutParams tcp = new LinearLayout.LayoutParams(dp(110), dp(72));
        tcp.setMargins(0, 0, dp(12), 0);
        thumbContainer.setLayoutParams(tcp);
        thumbContainer.setBackground(createRoundedDrawable(Color.parseColor("#091020"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(10)));
        thumbContainer.setClipToOutline(true);

        ImageView ivThumb = new ImageView(context);
        ivThumb.setLayoutParams(new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
        ivThumb.setScaleType(ImageView.ScaleType.CENTER_CROP);
        thumbContainer.addView(ivThumb);

        // Center play badge overlay
        TextView tvPlayIcon = new TextView(context);
        tvPlayIcon.setText("▶");
        tvPlayIcon.setTextColor(Color.WHITE);
        tvPlayIcon.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvPlayIcon.setGravity(Gravity.CENTER);
        tvPlayIcon.setBackground(createRoundedDrawable(Color.parseColor("#99000000"), Color.parseColor("#50FFFFFF"), dp(1), dp(14)));
        FrameLayout.LayoutParams pip = new FrameLayout.LayoutParams(dp(28), dp(28));
        pip.gravity = Gravity.CENTER;
        tvPlayIcon.setLayoutParams(pip);
        thumbContainer.addView(tvPlayIcon);

        // Load thumbnail asynchronously
        loadVideoThumbnail(r.compressedPath, ivThumb);
        rowMediaHeader.addView(thumbContainer);

        // Right Column: Title + Mode/Resolution Pills + Date
        LinearLayout colInfo = new LinearLayout(context);
        colInfo.setOrientation(VERTICAL);
        colInfo.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        TextView tvTitle = new TextView(context);
        tvTitle.setText(r.title);
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvTitle.setTypeface(Typeface.DEFAULT_BOLD);
        tvTitle.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);
        tvTitle.setMaxLines(2);
        colInfo.addView(tvTitle);

        LinearLayout rowPills = new LinearLayout(context);
        rowPills.setOrientation(HORIZONTAL);
        rowPills.setGravity(Gravity.CENTER_VERTICAL);
        rowPills.setPadding(0, dp(4), 0, dp(2));

        // Mode Pill
        TextView tvMode = new TextView(context);
        tvMode.setText(r.mode != null ? r.mode.split(" ")[0] : "H-ARCH");
        tvMode.setTextColor(ThemeStyles.COLOR_CYAN_NEON);
        tvMode.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        tvMode.setTypeface(Typeface.DEFAULT_BOLD);
        tvMode.setPadding(dp(6), dp(2), dp(6), dp(2));
        tvMode.setBackground(createRoundedDrawable(Color.parseColor("#091A30"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(1), dp(6)));
        rowPills.addView(tvMode);

        // Resolution Pill
        String resLabel = (r.width > 0 && r.height > 0) ? (r.width + "x" + r.height) : "1080p";
        TextView tvRes = new TextView(context);
        tvRes.setText(resLabel);
        tvRes.setTextColor(Color.parseColor("#94A3B8"));
        tvRes.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        tvRes.setTypeface(Typeface.MONOSPACE);
        tvRes.setPadding(dp(6), dp(2), dp(6), dp(2));
        tvRes.setBackground(createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(6)));
        LinearLayout.LayoutParams rpParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rpParams.setMargins(dp(4), 0, 0, 0);
        tvRes.setLayoutParams(rpParams);
        rowPills.addView(tvRes);

        colInfo.addView(rowPills);

        TextView tvDate = new TextView(context);
        tvDate.setText(r.formattedDate != null ? r.formattedDate : "Recently Optimized");
        tvDate.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        tvDate.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        colInfo.addView(tvDate);

        rowMediaHeader.addView(colInfo);
        card.addView(rowMediaHeader);

        // ROW 2: Size Comparison Row (Before -> After + Saved %)
        LinearLayout rowSize = new LinearLayout(context);
        rowSize.setOrientation(HORIZONTAL);
        rowSize.setGravity(Gravity.CENTER_VERTICAL);
        rowSize.setPadding(dp(10), dp(8), dp(10), dp(8));
        rowSize.setBackground(createRoundedDrawable(Color.parseColor("#091020"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(10)));
        LinearLayout.LayoutParams rslp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        rslp.setMargins(0, dp(10), 0, 0);
        rowSize.setLayoutParams(rslp);

        TextView tvBefore = new TextView(context);
        tvBefore.setText(String.format(Locale.US, "%.1f MB", r.originalSizeBytes / (1024.0 * 1024.0)));
        tvBefore.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvBefore.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        rowSize.addView(tvBefore);

        TextView tvArrow = new TextView(context);
        tvArrow.setText(" ➔ ");
        tvArrow.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvArrow.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        rowSize.addView(tvArrow);

        TextView tvAfter = new TextView(context);
        tvAfter.setText(String.format(Locale.US, "%.1f MB", r.compressedSizeBytes / (1024.0 * 1024.0)));
        tvAfter.setTextColor(Color.WHITE);
        tvAfter.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvAfter.setTypeface(Typeface.DEFAULT_BOLD);
        LinearLayout.LayoutParams ap = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        tvAfter.setLayoutParams(ap);
        rowSize.addView(tvAfter);

        TextView tvSaved = new TextView(context);
        tvSaved.setText(String.format(Locale.US, "-%.1f%% (+%.1f MB)", r.percentReduction, r.spaceSavedBytes / (1024.0 * 1024.0)));
        tvSaved.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        tvSaved.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvSaved.setTypeface(Typeface.DEFAULT_BOLD);
        rowSize.addView(tvSaved);
        card.addView(rowSize);

        // ROW 3: SOURCE VIDEO STATUS & 30-DAY RECYCLE BIN MANAGEMENT
        File srcFile = resolveSourceFile(r);
        File trashedFile = findTrashedSourceFile(context, r);
        boolean hasSourceOnDisk = (srcFile != null && srcFile.exists());
        boolean isTrashed = (trashedFile != null && trashedFile.exists()) || r.isSourceTrashed;

        LinearLayout rowSource = new LinearLayout(context);
        rowSource.setOrientation(HORIZONTAL);
        rowSource.setGravity(Gravity.CENTER_VERTICAL);
        rowSource.setPadding(dp(12), dp(8), dp(12), dp(8));
        rowSource.setBackground(createRoundedDrawable(Color.parseColor("#081220"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(12)));
        LinearLayout.LayoutParams srlp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        srlp.setMargins(0, dp(8), 0, 0);
        rowSource.setLayoutParams(srlp);

        LinearLayout colSourceInfo = new LinearLayout(context);
        colSourceInfo.setOrientation(VERTICAL);
        colSourceInfo.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        TextView tvSourceStatus = new TextView(context);
        tvSourceStatus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvSourceStatus.setTypeface(Typeface.DEFAULT_BOLD);

        TextView tvSourceSub = new TextView(context);
        tvSourceSub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvSourceSub.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);

        if (hasSourceOnDisk) {
            tvSourceStatus.setText("🟢 MASTER ON STORAGE");
            tvSourceStatus.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
            double origMB = srcFile.length() / (1024.0 * 1024.0);
            tvSourceSub.setText(String.format(Locale.US, "Original: %.1f MB • Safe to move to 30-day vault", origMB));
            colSourceInfo.addView(tvSourceStatus);
            colSourceInfo.addView(tvSourceSub);
            rowSource.addView(colSourceInfo);

            Button btnTrashSource = new Button(context);
            btnTrashSource.setText("🗑 TRASH SOURCE");
            btnTrashSource.setTextColor(Color.WHITE);
            btnTrashSource.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
            btnTrashSource.setTypeface(Typeface.DEFAULT_BOLD);
            btnTrashSource.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#B45309"), Color.parseColor("#F59E0B"), dp(8)));
            btnTrashSource.setPadding(dp(10), dp(4), dp(10), dp(4));
            LinearLayout.LayoutParams tlp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(34));
            btnTrashSource.setLayoutParams(tlp);
            btnTrashSource.setOnClickListener(v -> {
                ThemeStyles.performHaptic(v);
                CustomDarkDialog.showConfirmation(context,
                        "Move Source to 30-Day Recycle Bin?",
                        "The original uncompressed video file (" + srcFile.getName() + " • " + String.format(Locale.US, "%.1f MB", origMB) + ") will be safely moved to Prism-V's 30-Day Recycle Bin.\n\n✓ Reclaims +" + String.format(Locale.US, "%.1f MB", origMB) + " device storage immediately\n✓ Protected for 30 days in Vault\n✓ Can be restored anytime with 1 click\n✓ Your compressed video remains 100% playable in Duo and Gallery",
                        "Move to 30D Trash", "Cancel", () -> {
                            boolean ok = com.prismv.storage.PrismStorageManager.moveToTrash(context, srcFile);
                            if (ok) {
                                r.isSourceTrashed = true;
                                history.updateRecord(r);
                                refreshRecords(etSearch.getText().toString());
                                if (context instanceof Activity) {
                                    PrismHud.showSuccess((Activity) context, String.format(Locale.US, "Source moved to 30-Day Vault! Reclaimed %.1f MB.", origMB));
                                }
                            } else {
                                if (context instanceof Activity) {
                                    PrismHud.showWarning((Activity) context, "Could not move source video to trash.");
                                }
                            }
                        });
            });
            rowSource.addView(btnTrashSource);

        } else if (isTrashed) {
            tvSourceStatus.setText("🟡 IN 30-DAY RECYCLE BIN");
            tvSourceStatus.setTextColor(ThemeStyles.COLOR_AMBER_SUNSET);
            int daysLeft = (trashedFile != null) ? com.prismv.storage.PrismStorageManager.getDaysRemainingInTrash(trashedFile) : 30;
            tvSourceSub.setText("⏳ " + daysLeft + " days remaining before auto-purge • Storage freed");
            colSourceInfo.addView(tvSourceStatus);
            colSourceInfo.addView(tvSourceSub);
            rowSource.addView(colSourceInfo);

            if (trashedFile != null) {
                Button btnRestoreSource = new Button(context);
                btnRestoreSource.setText("↺ RESTORE");
                btnRestoreSource.setTextColor(Color.WHITE);
                btnRestoreSource.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
                btnRestoreSource.setTypeface(Typeface.DEFAULT_BOLD);
                btnRestoreSource.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#0E7490"), Color.parseColor("#06B6D4"), dp(8)));
                btnRestoreSource.setPadding(dp(10), dp(4), dp(10), dp(4));
                LinearLayout.LayoutParams rlp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, dp(34));
                btnRestoreSource.setLayoutParams(rlp);
                btnRestoreSource.setOnClickListener(v -> {
                    ThemeStyles.performHaptic(v);
                    boolean ok = com.prismv.storage.PrismStorageManager.restoreFromTrash(context, trashedFile);
                    if (ok) {
                        r.isSourceTrashed = false;
                        r.trashedSourcePath = null;
                        history.updateRecord(r);
                        refreshRecords(etSearch.getText().toString());
                        if (context instanceof Activity) {
                            PrismHud.showSuccess((Activity) context, "Source restored to Movies/Prism-V & Indexed in Gallery!");
                        }
                    } else {
                        if (context instanceof Activity) {
                            PrismHud.showWarning((Activity) context, "Failed to restore source file.");
                        }
                    }
                });
                rowSource.addView(btnRestoreSource);
            }

        } else {
            tvSourceStatus.setText("⚪ SOURCE STORAGE RECLAIMED");
            tvSourceStatus.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
            tvSourceSub.setText("Original storage reclaimed • Optimized video preserved");
            colSourceInfo.addView(tvSourceStatus);
            colSourceInfo.addView(tvSourceSub);
            rowSource.addView(colSourceInfo);
        }
        card.addView(rowSource);

        // ROW 4: Action Buttons Row: [COMPARE IN DUO] [▶ PLAY] [SHARE] [DELETE]
        LinearLayout rowBtns = new LinearLayout(context);
        rowBtns.setOrientation(HORIZONTAL);
        rowBtns.setPadding(0, dp(10), 0, 0);

        // 1. Compare in Duo Button (Primary Action)
        Button btnCompare = new Button(context);
        btnCompare.setText("COMPARE IN DUO");
        btnCompare.setTextColor(Color.WHITE);
        btnCompare.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnCompare.setTypeface(Typeface.DEFAULT_BOLD);
        btnCompare.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#0284C7"), Color.parseColor("#00E5FF"), dp(10)));
        LinearLayout.LayoutParams compParams = new LinearLayout.LayoutParams(0, dp(40), 1.6f);
        compParams.setMargins(0, 0, dp(4), 0);
        btnCompare.setLayoutParams(compParams);
        btnCompare.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            if (compareListener != null) {
                compareListener.onCompareRequested(r);
            }
        });
        rowBtns.addView(btnCompare);

        // 2. Play Button
        Button btnPlay = new Button(context);
        btnPlay.setText("▶ PLAY");
        btnPlay.setTextColor(Color.WHITE);
        btnPlay.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnPlay.setBackground(createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(10)));
        LinearLayout.LayoutParams playParams = new LinearLayout.LayoutParams(0, dp(40), 0.9f);
        playParams.setMargins(0, 0, dp(4), 0);
        btnPlay.setLayoutParams(playParams);
        btnPlay.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            playVideo(context, r);
        });
        rowBtns.addView(btnPlay);

        // 3. Share Button
        Button btnShare = new Button(context);
        btnShare.setText("");
        btnShare.setTextColor(Color.WHITE);
        btnShare.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnShare.setBackground(createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(1), dp(10)));
        LinearLayout.LayoutParams shareParams = new LinearLayout.LayoutParams(dp(44), dp(40));
        shareParams.setMargins(0, 0, dp(4), 0);
        btnShare.setLayoutParams(shareParams);
        btnShare.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            shareVideo(context, r);
        });
        rowBtns.addView(btnShare);

        // 4. Delete Button
        Button btnDel = new Button(context);
        btnDel.setText("");
        btnDel.setTextColor(Color.parseColor("#EF4444"));
        btnDel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnDel.setBackground(createRoundedDrawable(Color.parseColor("#1A1118"), Color.parseColor("#7F1D1D"), dp(1), dp(10)));
        LinearLayout.LayoutParams delParams = new LinearLayout.LayoutParams(dp(44), dp(40));
        btnDel.setLayoutParams(delParams);
        btnDel.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            CustomDarkDialog.showConfirmation(context,
                    "Delete Compressed File?",
                    "Are you sure you want to remove '" + r.title + "' from disk?",
                    "Delete", "Cancel", () -> {
                        if (r.compressedPath != null) {
                            new File(r.compressedPath).delete();
                        }
                        history.deleteRecord(r.id);
                        refreshRecords(etSearch.getText().toString());
                        if (context instanceof Activity) {
                            PrismHud.showSuccess((Activity) context, "File deleted from storage");
                        }
                    });
        });
        rowBtns.addView(btnDel);

        card.addView(rowBtns);
        listContainer.addView(card);
    }

    private void playVideo(Context context, CompressionHistoryManager.Record r) {
        if (!r.isCompressedFileExisting()) {
            if (context instanceof Activity) {
                PrismHud.showWarning((Activity) context, "Compressed file not found on disk");
            }
            return;
        }

        try {
            File f = new File(r.compressedPath);
            Uri uri = PrismFileProvider.getUriForFile(f);
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.setDataAndType(uri, "video/*");
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            context.startActivity(Intent.createChooser(intent, "Play " + r.title));
        } catch (Exception e) {
            if (context instanceof Activity) {
                PrismHud.showWarning((Activity) context, "Cannot open video player: " + e.getMessage());
            }
        }
    }

    private void shareVideo(Context context, CompressionHistoryManager.Record r) {
        if (!r.isCompressedFileExisting()) {
            if (context instanceof Activity) {
                PrismHud.showWarning((Activity) context, "Compressed file not found on disk");
            }
            return;
        }

        try {
            File f = new File(r.compressedPath);
            Uri uri = PrismFileProvider.getUriForFile(f);
            Intent intent = new Intent(Intent.ACTION_SEND);
            intent.setType("video/*");
            intent.putExtra(Intent.EXTRA_STREAM, uri);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            context.startActivity(Intent.createChooser(intent, "Share " + r.title));
        } catch (Exception e) {
            if (context instanceof Activity) {
                PrismHud.showWarning((Activity) context, "Cannot share video: " + e.getMessage());
            }
        }
    }

    public static Dialog showTrashVaultDialog(Context context, Runnable onDismissListener) {
        Dialog dialog = CustomDarkDialog.createDarkBaseDialog(context);
        ScrollView scroll = new ScrollView(context);
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(VERTICAL);
        int pad = ThemeStyles.dp(context, 18);
        root.setPadding(pad, pad, pad, pad);
        root.setBackground(ThemeStyles.createCardDrawable(context, false));

        // Header Title
        TextView tvTitle = new TextView(context);
        tvTitle.setText("30-DAY RECYCLE BIN & VAULT");
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        root.addView(tvTitle);

        TextView tvSub = new TextView(context);
        tvSub.setText("Original master videos are kept here safely for 30 days before automatic deletion. You can restore them back to Movies/Prism-V anytime.");
        tvSub.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvSub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvSub.setPadding(0, ThemeStyles.dp(context, 4), 0, ThemeStyles.dp(context, 12));
        root.addView(tvSub);

        List<File> trashList = com.prismv.storage.PrismStorageManager.listTrashVideos(context);
        long totalTrashBytes = com.prismv.storage.PrismStorageManager.getTrashTotalSizeBytes(context);

        // Vault Summary Row & Purge All Button
        LinearLayout rowSummary = new LinearLayout(context);
        rowSummary.setOrientation(HORIZONTAL);
        rowSummary.setGravity(Gravity.CENTER_VERTICAL);
        rowSummary.setPadding(ThemeStyles.dp(context, 12), ThemeStyles.dp(context, 8), ThemeStyles.dp(context, 12), ThemeStyles.dp(context, 8));
        rowSummary.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 12)));
        LinearLayout.LayoutParams sumLp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        sumLp.setMargins(0, 0, 0, ThemeStyles.dp(context, 12));
        rowSummary.setLayoutParams(sumLp);

        TextView tvStats = new TextView(context);
        tvStats.setText(String.format(Locale.US, "%d Protected Files • %.1f MB in Vault", trashList.size(), totalTrashBytes / (1024.0 * 1024.0)));
        tvStats.setTextColor(ThemeStyles.COLOR_AMBER_SUNSET);
        tvStats.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvStats.setTypeface(Typeface.DEFAULT_BOLD);
        tvStats.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));
        rowSummary.addView(tvStats);

        if (!trashList.isEmpty()) {
            Button btnEmpty = new Button(context);
            btnEmpty.setText("EMPTY BIN");
            btnEmpty.setTextColor(Color.WHITE);
            btnEmpty.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
            btnEmpty.setTypeface(Typeface.DEFAULT_BOLD);
            btnEmpty.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#DC2626"), Color.parseColor("#EF4444"), ThemeStyles.dp(context, 8)));
            btnEmpty.setPadding(ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 4), ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 4));
            btnEmpty.setOnClickListener(v -> {
                ThemeStyles.performHaptic(v);
                CustomDarkDialog.showConfirmation(context,
                        "Permanently Empty Recycle Bin?",
                        "This will permanently wipe all " + trashList.size() + " trashed original videos and reclaim " + String.format(Locale.US, "%.1f MB", totalTrashBytes / (1024.0 * 1024.0)) + ". This action cannot be undone.",
                        "Empty Forever", "Cancel", () -> {
                            long purgedBytes = com.prismv.storage.PrismStorageManager.purgeTrash(context);
                            dialog.dismiss();
                            if (context instanceof Activity) {
                                PrismHud.showSuccess((Activity) context, String.format(Locale.US, "Purged Recycle Bin! Reclaimed %.1f MB.", purgedBytes / (1024.0 * 1024.0)));
                            }
                            if (onDismissListener != null) onDismissListener.run();
                        });
            });
            rowSummary.addView(btnEmpty);
        }
        root.addView(rowSummary);

        // List of trashed items
        if (trashList.isEmpty()) {
            TextView tvEmpty = new TextView(context);
            tvEmpty.setText("Recycle Bin is empty.\n\nWhen you click 'Trash Source' on a compressed video card, the original master video is safely kept here for 30 days before auto-purge.");
            tvEmpty.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
            tvEmpty.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
            tvEmpty.setGravity(Gravity.CENTER);
            tvEmpty.setPadding(0, ThemeStyles.dp(context, 24), 0, ThemeStyles.dp(context, 24));
            root.addView(tvEmpty);
        } else {
            for (File tf : trashList) {
                LinearLayout itemCard = new LinearLayout(context);
                itemCard.setOrientation(VERTICAL);
                itemCard.setPadding(ThemeStyles.dp(context, 12), ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 12), ThemeStyles.dp(context, 10));
                itemCard.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#091020"), ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 12)));
                LinearLayout.LayoutParams icp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
                icp.setMargins(0, 0, 0, ThemeStyles.dp(context, 10));
                itemCard.setLayoutParams(icp);

                String rawName = tf.getName();
                String cleanName = rawName;
                if (cleanName.contains("_") && cleanName.length() > 14 && Character.isDigit(cleanName.charAt(0))) {
                    cleanName = cleanName.substring(cleanName.indexOf('_') + 1);
                }
                final String finalCleanName = cleanName;

                // ROW 1: Media Header (Thumbnail on Left + Video Title & Countdown Badge on Right)
                LinearLayout rowMedia = new LinearLayout(context);
                rowMedia.setOrientation(HORIZONTAL);
                rowMedia.setGravity(Gravity.CENTER_VERTICAL);

                // Left: Thumbnail FrameLayout Container (86dp x 58dp)
                FrameLayout thumbContainer = new FrameLayout(context);
                LinearLayout.LayoutParams tcp = new LinearLayout.LayoutParams(ThemeStyles.dp(context, 86), ThemeStyles.dp(context, 58));
                tcp.setMargins(0, 0, ThemeStyles.dp(context, 12), 0);
                thumbContainer.setLayoutParams(tcp);
                thumbContainer.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#040711"), ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 8)));
                thumbContainer.setClipToOutline(true);
                thumbContainer.setOutlineProvider(new android.view.ViewOutlineProvider() {
                    @Override
                    public void getOutline(View view, android.graphics.Outline outline) {
                        outline.setRoundRect(0, 0, view.getWidth(), view.getHeight(), ThemeStyles.dp(context, 8));
                    }
                });

                ImageView ivThumb = new ImageView(context);
                ivThumb.setLayoutParams(new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
                ivThumb.setScaleType(ImageView.ScaleType.CENTER_CROP);
                thumbContainer.addView(ivThumb);
                loadVideoThumbnail(tf.getAbsolutePath(), ivThumb);

                // Center play overlay icon on thumbnail
                TextView tvPlayBadge = new TextView(context);
                tvPlayBadge.setText("▶");
                tvPlayBadge.setTextColor(Color.WHITE);
                tvPlayBadge.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
                tvPlayBadge.setGravity(Gravity.CENTER);
                tvPlayBadge.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#80000000"), Color.TRANSPARENT, 0, ThemeStyles.dp(context, 14)));
                FrameLayout.LayoutParams pblp = new FrameLayout.LayoutParams(ThemeStyles.dp(context, 28), ThemeStyles.dp(context, 28), Gravity.CENTER);
                tvPlayBadge.setLayoutParams(pblp);
                thumbContainer.addView(tvPlayBadge);

                // Clicking thumbnail immediately opens the preview player to check the video!
                thumbContainer.setOnClickListener(v -> {
                    ThemeStyles.performHaptic(v);
                    showVideoPreviewDialog(context, tf);
                });
                rowMedia.addView(thumbContainer);

                // Right Column: Title + Badges
                LinearLayout colInfo = new LinearLayout(context);
                colInfo.setOrientation(VERTICAL);
                colInfo.setLayoutParams(new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

                TextView tvName = new TextView(context);
                tvName.setText(finalCleanName);
                tvName.setTextColor(Color.WHITE);
                tvName.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
                tvName.setTypeface(Typeface.DEFAULT_BOLD);
                tvName.setEllipsize(android.text.TextUtils.TruncateAt.MIDDLE);
                tvName.setMaxLines(1);
                colInfo.addView(tvName);

                LinearLayout rowBadges = new LinearLayout(context);
                rowBadges.setOrientation(HORIZONTAL);
                rowBadges.setGravity(Gravity.CENTER_VERTICAL);
                rowBadges.setPadding(0, ThemeStyles.dp(context, 4), 0, 0);

                int daysLeft = com.prismv.storage.PrismStorageManager.getDaysRemainingInTrash(tf);
                TextView tvDays = new TextView(context);
                tvDays.setText("⏳ " + daysLeft + "d left");
                tvDays.setTextColor(daysLeft > 7 ? ThemeStyles.COLOR_EMERALD_NEON : ThemeStyles.COLOR_AMBER_SUNSET);
                tvDays.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
                tvDays.setTypeface(Typeface.DEFAULT_BOLD);
                tvDays.setPadding(ThemeStyles.dp(context, 6), ThemeStyles.dp(context, 2), ThemeStyles.dp(context, 6), ThemeStyles.dp(context, 2));
                tvDays.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#0C1527"), ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 6)));
                rowBadges.addView(tvDays);

                TextView tvSize = new TextView(context);
                tvSize.setText(String.format(Locale.US, "%.1f MB", tf.length() / (1024.0 * 1024.0)));
                tvSize.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
                tvSize.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
                tvSize.setTypeface(Typeface.DEFAULT_BOLD);
                tvSize.setPadding(ThemeStyles.dp(context, 8), 0, 0, 0);
                rowBadges.addView(tvSize);

                colInfo.addView(rowBadges);
                rowMedia.addView(colInfo);
                itemCard.addView(rowMedia);

                // ROW 2: Action Buttons Row: [▶ CHECK / PLAY] [↺ RESTORE] [🗑 PURGE]
                LinearLayout rowActions = new LinearLayout(context);
                rowActions.setOrientation(HORIZONTAL);
                rowActions.setGravity(Gravity.CENTER_VERTICAL);
                rowActions.setPadding(0, ThemeStyles.dp(context, 10), 0, 0);

                Button btnPlay = new Button(context);
                btnPlay.setText("▶ PLAY");
                btnPlay.setTextColor(Color.WHITE);
                btnPlay.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
                btnPlay.setTypeface(Typeface.DEFAULT_BOLD);
                btnPlay.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_CYAN_PRIMARY, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 8)));
                btnPlay.setPadding(ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 4), ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 4));
                LinearLayout.LayoutParams plp = new LinearLayout.LayoutParams(0, ThemeStyles.dp(context, 34), 1.0f);
                plp.setMargins(0, 0, ThemeStyles.dp(context, 6), 0);
                btnPlay.setLayoutParams(plp);
                btnPlay.setOnClickListener(v -> {
                    ThemeStyles.performHaptic(v);
                    showVideoPreviewDialog(context, tf);
                });
                rowActions.addView(btnPlay);

                Button btnRestore = new Button(context);
                btnRestore.setText("↺ RESTORE");
                btnRestore.setTextColor(Color.WHITE);
                btnRestore.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
                btnRestore.setTypeface(Typeface.DEFAULT_BOLD);
                btnRestore.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#059669"), Color.parseColor("#10B981"), ThemeStyles.dp(context, 8)));
                btnRestore.setPadding(ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 4), ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 4));
                LinearLayout.LayoutParams rlp = new LinearLayout.LayoutParams(0, ThemeStyles.dp(context, 34), 1.0f);
                rlp.setMargins(0, 0, ThemeStyles.dp(context, 6), 0);
                btnRestore.setLayoutParams(rlp);
                btnRestore.setOnClickListener(v -> {
                    ThemeStyles.performHaptic(v);
                    boolean ok = com.prismv.storage.PrismStorageManager.restoreFromTrash(context, tf);
                    if (ok) {
                        try {
                            CompressionHistoryManager hm = CompressionHistoryManager.get(context);
                            for (CompressionHistoryManager.Record r : hm.getRecords()) {
                                if (tf.getAbsolutePath().equals(r.trashedSourcePath) || (r.originalPath != null && r.originalPath.contains(finalCleanName))) {
                                    r.isSourceTrashed = false;
                                    r.trashedSourcePath = null;
                                    hm.updateRecord(r);
                                }
                            }
                        } catch (Throwable ignored) {}

                        dialog.dismiss();
                        if (context instanceof Activity) {
                            PrismHud.showSuccess((Activity) context, "Restored to Movies/Prism-V & Indexed in Gallery!");
                        }
                        if (onDismissListener != null) onDismissListener.run();
                    } else {
                        if (context instanceof Activity) {
                            PrismHud.showWarning((Activity) context, "Failed to restore file");
                        }
                    }
                });
                rowActions.addView(btnRestore);

                Button btnDeletePerm = new Button(context);
                btnDeletePerm.setText("PURGE");
                btnDeletePerm.setTextColor(Color.parseColor("#EF4444"));
                btnDeletePerm.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
                btnDeletePerm.setTypeface(Typeface.DEFAULT_BOLD);
                btnDeletePerm.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#1A1118"), Color.parseColor("#7F1D1D"), ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 8)));
                btnDeletePerm.setPadding(ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 4), ThemeStyles.dp(context, 10), ThemeStyles.dp(context, 4));
                LinearLayout.LayoutParams dlp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ThemeStyles.dp(context, 34));
                btnDeletePerm.setLayoutParams(dlp);
                btnDeletePerm.setOnClickListener(v -> {
                    ThemeStyles.performHaptic(v);
                    CustomDarkDialog.showConfirmation(context,
                            "Permanently Purge File?",
                            "Delete '" + finalCleanName + "' forever? It cannot be recovered.",
                            "Purge", "Cancel", () -> {
                                tf.delete();
                                dialog.dismiss();
                                if (context instanceof Activity) {
                                    PrismHud.showSuccess((Activity) context, "File permanently purged");
                                }
                                if (onDismissListener != null) onDismissListener.run();
                            });
                });
                rowActions.addView(btnDeletePerm);

                itemCard.addView(rowActions);
                root.addView(itemCard);
            }
        }

        // Close Button
        Button btnClose = new Button(context);
        btnClose.setText("CLOSE");
        btnClose.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnClose.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnClose.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 12)));
        LinearLayout.LayoutParams clp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ThemeStyles.dp(context, 42));
        clp.setMargins(0, ThemeStyles.dp(context, 14), 0, 0);
        btnClose.setLayoutParams(clp);
        btnClose.setOnClickListener(v -> dialog.dismiss());
        root.addView(btnClose);

        scroll.addView(root);
        dialog.setContentView(scroll);
        dialog.show();
        return dialog;
    }

    public static Dialog showVideoPreviewDialog(final Context context, final File videoFile) {
        if (videoFile == null || !videoFile.exists()) {
            if (context instanceof Activity) {
                PrismHud.showWarning((Activity) context, "Video file not found");
            }
            return null;
        }

        final Dialog dialog = CustomDarkDialog.createDarkBaseDialog(context);
        ScrollView scroll = new ScrollView(context);
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(VERTICAL);
        int pad = ThemeStyles.dp(context, 16);
        root.setPadding(pad, pad, pad, pad);
        root.setBackground(ThemeStyles.createCardDrawable(context, false));

        // Header Title
        TextView tvTitle = new TextView(context);
        tvTitle.setText("PREVIEW RECYCLE BIN VIDEO");
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        root.addView(tvTitle);

        String rawName = videoFile.getName();
        String cleanName = rawName;
        if (cleanName.contains("_") && cleanName.length() > 14 && Character.isDigit(cleanName.charAt(0))) {
            cleanName = cleanName.substring(cleanName.indexOf('_') + 1);
        }
        final String finalCleanName = cleanName;

        TextView tvName = new TextView(context);
        tvName.setText(finalCleanName);
        tvName.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvName.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvName.setTypeface(Typeface.DEFAULT_BOLD);
        tvName.setPadding(0, ThemeStyles.dp(context, 2), 0, ThemeStyles.dp(context, 8));
        root.addView(tvName);

        // Video Viewport Container (16:9 aspect, 220dp height)
        FrameLayout videoContainer = new FrameLayout(context);
        videoContainer.setBackground(ThemeStyles.createRoundedDrawable(Color.BLACK, ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 12)));
        videoContainer.setClipToOutline(true);
        videoContainer.setOutlineProvider(new android.view.ViewOutlineProvider() {
            @Override
            public void getOutline(View view, android.graphics.Outline outline) {
                outline.setRoundRect(0, 0, view.getWidth(), view.getHeight(), ThemeStyles.dp(context, 12));
            }
        });
        LinearLayout.LayoutParams vcp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ThemeStyles.dp(context, 220));
        videoContainer.setLayoutParams(vcp);

        final VideoView videoView = new VideoView(context);
        FrameLayout.LayoutParams vlp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        vlp.gravity = Gravity.CENTER;
        videoView.setLayoutParams(vlp);
        videoContainer.addView(videoView);

        // Center Big Play Indicator (shown when paused)
        final TextView tvCenterPlay = new TextView(context);
        tvCenterPlay.setText("▶");
        tvCenterPlay.setTextColor(Color.WHITE);
        tvCenterPlay.setTextSize(TypedValue.COMPLEX_UNIT_SP, 30);
        tvCenterPlay.setGravity(Gravity.CENTER);
        tvCenterPlay.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#99000000"), Color.TRANSPARENT, 0, ThemeStyles.dp(context, 30)));
        FrameLayout.LayoutParams cplp = new FrameLayout.LayoutParams(ThemeStyles.dp(context, 60), ThemeStyles.dp(context, 60), Gravity.CENTER);
        tvCenterPlay.setLayoutParams(cplp);
        tvCenterPlay.setVisibility(View.GONE);
        videoContainer.addView(tvCenterPlay);

        // Top Overlay Badge: File Size + 30d Vault Countdown
        LinearLayout overlayTop = new LinearLayout(context);
        overlayTop.setOrientation(HORIZONTAL);
        overlayTop.setPadding(ThemeStyles.dp(context, 8), ThemeStyles.dp(context, 8), ThemeStyles.dp(context, 8), ThemeStyles.dp(context, 8));
        FrameLayout.LayoutParams olp = new FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.TOP);
        overlayTop.setLayoutParams(olp);

        int daysLeft = com.prismv.storage.PrismStorageManager.getDaysRemainingInTrash(videoFile);
        TextView tvVaultBadge = new TextView(context);
        tvVaultBadge.setText("🟡 VAULT: " + daysLeft + "d left • " + String.format(Locale.US, "%.1f MB", videoFile.length() / (1024.0 * 1024.0)));
        tvVaultBadge.setTextColor(Color.WHITE);
        tvVaultBadge.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvVaultBadge.setTypeface(Typeface.DEFAULT_BOLD);
        tvVaultBadge.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#B3040711"), Color.parseColor("#3300E5FF"), ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 6)));
        tvVaultBadge.setPadding(ThemeStyles.dp(context, 8), ThemeStyles.dp(context, 4), ThemeStyles.dp(context, 8), ThemeStyles.dp(context, 4));
        overlayTop.addView(tvVaultBadge);
        videoContainer.addView(overlayTop);

        root.addView(videoContainer);

        // Transport Controls Row (SeekBar + Timecode)
        final SeekBar seekBar = new SeekBar(context);
        final TextView tvTimecode = new TextView(context);
        tvTimecode.setText("00:00 / 00:00");
        tvTimecode.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvTimecode.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvTimecode.setTypeface(Typeface.MONOSPACE);

        LinearLayout seekRow = new LinearLayout(context);
        seekRow.setOrientation(HORIZONTAL);
        seekRow.setGravity(Gravity.CENTER_VERTICAL);
        seekRow.setPadding(0, ThemeStyles.dp(context, 6), 0, ThemeStyles.dp(context, 4));
        LinearLayout.LayoutParams skLp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        seekBar.setLayoutParams(skLp);
        seekRow.addView(seekBar);
        seekRow.addView(tvTimecode);
        root.addView(seekRow);

        // Transport buttons: [⏮ -5s] [▶ PLAY / ⏸ PAUSE] [+5s ⏭]
        LinearLayout ctrlRow = new LinearLayout(context);
        ctrlRow.setOrientation(HORIZONTAL);
        ctrlRow.setGravity(Gravity.CENTER);
        ctrlRow.setPadding(0, 0, 0, ThemeStyles.dp(context, 12));

        Button btnBack5 = new Button(context);
        btnBack5.setText("⏮ -5s");
        btnBack5.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        btnBack5.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnBack5.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 8)));
        LinearLayout.LayoutParams b5p = new LinearLayout.LayoutParams(ThemeStyles.dp(context, 70), ThemeStyles.dp(context, 34));
        b5p.setMargins(0, 0, ThemeStyles.dp(context, 8), 0);
        btnBack5.setLayoutParams(b5p);
        ctrlRow.addView(btnBack5);

        final Button btnPlayPause = new Button(context);
        btnPlayPause.setText("⏸ PAUSE");
        btnPlayPause.setTextColor(Color.WHITE);
        btnPlayPause.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnPlayPause.setTypeface(Typeface.DEFAULT_BOLD);
        btnPlayPause.setBackground(ThemeStyles.createGradientButton(ThemeStyles.COLOR_CYAN_PRIMARY, Color.parseColor("#00E5FF"), ThemeStyles.dp(context, 8)));
        LinearLayout.LayoutParams ppLp = new LinearLayout.LayoutParams(ThemeStyles.dp(context, 110), ThemeStyles.dp(context, 36));
        btnPlayPause.setLayoutParams(ppLp);
        ctrlRow.addView(btnPlayPause);

        Button btnFwd5 = new Button(context);
        btnFwd5.setText("+5s ⏭");
        btnFwd5.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        btnFwd5.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnFwd5.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 8)));
        LinearLayout.LayoutParams f5p = new LinearLayout.LayoutParams(ThemeStyles.dp(context, 70), ThemeStyles.dp(context, 34));
        f5p.setMargins(ThemeStyles.dp(context, 8), 0, 0, 0);
        btnFwd5.setLayoutParams(f5p);
        ctrlRow.addView(btnFwd5);

        root.addView(ctrlRow);

        // Bottom Action Row: [↺ RESTORE TO GALLERY] [EXTERNAL] [CLOSE]
        LinearLayout btmRow = new LinearLayout(context);
        btmRow.setOrientation(HORIZONTAL);
        btmRow.setGravity(Gravity.CENTER_VERTICAL);

        Button btnRestoreNow = new Button(context);
        btnRestoreNow.setText("↺ RESTORE TO GALLERY");
        btnRestoreNow.setTextColor(Color.WHITE);
        btnRestoreNow.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnRestoreNow.setTypeface(Typeface.DEFAULT_BOLD);
        btnRestoreNow.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#059669"), Color.parseColor("#10B981"), ThemeStyles.dp(context, 8)));
        LinearLayout.LayoutParams rnp = new LinearLayout.LayoutParams(0, ThemeStyles.dp(context, 40), 1.0f);
        rnp.setMargins(0, 0, ThemeStyles.dp(context, 8), 0);
        btnRestoreNow.setLayoutParams(rnp);
        btmRow.addView(btnRestoreNow);

        Button btnExternal = new Button(context);
        btnExternal.setText("EXTERNAL");
        btnExternal.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        btnExternal.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnExternal.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#121B2D"), ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 8)));
        LinearLayout.LayoutParams exp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ThemeStyles.dp(context, 40));
        exp.setMargins(0, 0, ThemeStyles.dp(context, 6), 0);
        btnExternal.setLayoutParams(exp);
        btmRow.addView(btnExternal);

        Button btnClose = new Button(context);
        btnClose.setText("CLOSE");
        btnClose.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnClose.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnClose.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 8)));
        LinearLayout.LayoutParams clp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ThemeStyles.dp(context, 40));
        btnClose.setLayoutParams(clp);
        btmRow.addView(btnClose);

        root.addView(btmRow);

        // Hook up VideoView playback & handler
        final android.os.Handler previewHandler = new android.os.Handler(android.os.Looper.getMainLooper());
        final Runnable updateSeekRunnable = new Runnable() {
            @Override
            public void run() {
                if (videoView.isPlaying()) {
                    int cur = videoView.getCurrentPosition();
                    int dur = videoView.getDuration();
                    if (dur > 0) {
                        seekBar.setMax(dur);
                        seekBar.setProgress(cur);
                        int curSec = cur / 1000;
                        int durSec = dur / 1000;
                        tvTimecode.setText(String.format(Locale.US, "%02d:%02d / %02d:%02d", curSec / 60, curSec % 60, durSec / 60, durSec % 60));
                    }
                }
                previewHandler.postDelayed(this, 250);
            }
        };

        try {
            videoView.setVideoPath(videoFile.getAbsolutePath());
            videoView.setOnPreparedListener(mp -> {
                mp.setLooping(true);
                videoView.start();
                btnPlayPause.setText("⏸ PAUSE");
                tvCenterPlay.setVisibility(View.GONE);
                int dur = videoView.getDuration();
                if (dur > 0) {
                    seekBar.setMax(dur);
                    int durSec = dur / 1000;
                    tvTimecode.setText(String.format(Locale.US, "00:00 / %02d:%02d", durSec / 60, durSec % 60));
                }
                previewHandler.post(updateSeekRunnable);
            });
        } catch (Exception e) {
            com.prismv.logging.PrismLogger.e("CompressedVideosView", "Failed to start preview VideoView: " + e.getMessage());
        }

        View.OnClickListener togglePlay = v -> {
            ThemeStyles.performHaptic(v);
            if (videoView.isPlaying()) {
                videoView.pause();
                btnPlayPause.setText("▶ PLAY");
                tvCenterPlay.setVisibility(View.VISIBLE);
            } else {
                videoView.start();
                btnPlayPause.setText("⏸ PAUSE");
                tvCenterPlay.setVisibility(View.GONE);
            }
        };
        btnPlayPause.setOnClickListener(togglePlay);
        videoContainer.setOnClickListener(togglePlay);

        btnBack5.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            int pos = Math.max(0, videoView.getCurrentPosition() - 5000);
            videoView.seekTo(pos);
            seekBar.setProgress(pos);
        });

        btnFwd5.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            int pos = Math.min(videoView.getDuration(), videoView.getCurrentPosition() + 5000);
            videoView.seekTo(pos);
            seekBar.setProgress(pos);
        });

        seekBar.setOnSeekBarChangeListener(new SeekBar.OnSeekBarChangeListener() {
            @Override
            public void onProgressChanged(SeekBar sb, int progress, boolean fromUser) {
                if (fromUser) {
                    videoView.seekTo(progress);
                }
            }
            @Override
            public void onStartTrackingTouch(SeekBar sb) {}
            @Override
            public void onStopTrackingTouch(SeekBar sb) {}
        });

        btnExternal.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            try {
                Uri uri = PrismFileProvider.getUriForFile(videoFile);
                Intent intent = new Intent(Intent.ACTION_VIEW);
                intent.setDataAndType(uri, "video/*");
                intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
                context.startActivity(Intent.createChooser(intent, "Play " + finalCleanName));
            } catch (Exception e) {
                if (context instanceof Activity) {
                    PrismHud.showWarning((Activity) context, "Cannot open video: " + e.getMessage());
                }
            }
        });

        btnRestoreNow.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            try { videoView.stopPlayback(); } catch (Exception ignored) {}
            previewHandler.removeCallbacksAndMessages(null);
            boolean ok = com.prismv.storage.PrismStorageManager.restoreFromTrash(context, videoFile);
            if (ok) {
                try {
                    CompressionHistoryManager hm = CompressionHistoryManager.get(context);
                    for (CompressionHistoryManager.Record r : hm.getRecords()) {
                        if (videoFile.getAbsolutePath().equals(r.trashedSourcePath) || (r.originalPath != null && r.originalPath.contains(finalCleanName))) {
                            r.isSourceTrashed = false;
                            r.trashedSourcePath = null;
                            hm.updateRecord(r);
                        }
                    }
                } catch (Throwable ignored) {}

                dialog.dismiss();
                if (context instanceof Activity) {
                    PrismHud.showSuccess((Activity) context, "Restored to Movies/Prism-V & Indexed in Gallery!");
                }
            } else {
                if (context instanceof Activity) {
                    PrismHud.showWarning((Activity) context, "Failed to restore file.");
                }
            }
        });

        dialog.setOnDismissListener(d -> {
            try { videoView.stopPlayback(); } catch (Exception ignored) {}
            previewHandler.removeCallbacksAndMessages(null);
        });

        btnClose.setOnClickListener(v -> {
            ThemeStyles.performHaptic(v);
            dialog.dismiss();
        });

        scroll.addView(root);
        dialog.setContentView(scroll);
        dialog.show();
        return dialog;
    }

    public static Dialog showLibraryDialog(Context context, OnCompareRequestedListener listener) {
        Dialog dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        dialog.setCancelable(true);
        dialog.setCanceledOnTouchOutside(false);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        }
        DialogTracker.register(dialog);
        dialog.setOnDismissListener(d -> DialogTracker.unregister(dialog));

        ScrollView scroll = new ScrollView(context);
        CompressedVideosView libView = new CompressedVideosView(context);
        libView.setOnCompareRequestedListener(r -> {
            dialog.dismiss();
            if (listener != null) listener.onCompareRequested(r);
        });

        // Add a close button at top of view
        Button btnClose = new Button(context);
        btnClose.setText("CLOSE LIBRARY");
        btnClose.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnClose.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnClose.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, ThemeStyles.dp(context, 1), ThemeStyles.dp(context, 10)));
        LinearLayout.LayoutParams cp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ThemeStyles.dp(context, 40));
        cp.setMargins(0, 0, 0, ThemeStyles.dp(context, 12));
        btnClose.setLayoutParams(cp);
        btnClose.setOnClickListener(v -> dialog.dismiss());
        libView.addView(btnClose, 0);

        scroll.addView(libView);
        dialog.setContentView(scroll);
        dialog.show();
        return dialog;
    }
}
