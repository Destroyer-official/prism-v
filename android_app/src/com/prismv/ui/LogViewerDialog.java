package com.prismv.ui;

import android.app.Activity;
import android.app.Dialog;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.os.Handler;
import android.os.Looper;
import android.text.Editable;
import android.text.TextWatcher;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.Button;
import android.widget.EditText;
import android.widget.HorizontalScrollView;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

import com.prismv.logging.PrismLogger;

import java.util.List;
import java.util.Locale;

/**
 * LogViewerDialog:
 * In-app full diagnostic log inspector with level filters, keyword search,
 * 1-tap Download Full Logs, Share / Send Logs, and Copy to Clipboard.
 */
public final class LogViewerDialog {
    private static final String TAG = "LogViewerDialog";

    private LogViewerDialog() {}

    public static void show(Context context) {
        if (context == null) return;
        PrismLogger.event(TAG, "OPEN", "User opened in-app Live Log Viewer");

        Dialog dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        }

        final Handler mainHandler = new Handler(Looper.getMainLooper());
        final String[] activeFilter = new String[]{"ALL"};
        final String[] searchQuery = new String[]{""};

        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setPadding(dp(context, 14), dp(context, 14), dp(context, 14), dp(context, 14));
        root.setBackground(ThemeStyles.createRoundedDrawable(
                Color.parseColor("#080E1A"), ThemeStyles.COLOR_CYAN_PRIMARY, dp(context, 1), dp(context, 18)));

        // Header
        LinearLayout header = new LinearLayout(context);
        header.setOrientation(LinearLayout.HORIZONTAL);
        header.setGravity(Gravity.CENTER_VERTICAL);
        header.setPadding(0, 0, 0, dp(context, 8));

        TextView tvTitle = new TextView(context);
        tvTitle.setText("FULL DIAGNOSTIC LOGS");
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        header.addView(tvTitle, new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f));

        TextView tvClose = new TextView(context);
        tvClose.setText("CLOSE");
        tvClose.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvClose.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvClose.setTypeface(Typeface.DEFAULT_BOLD);
        tvClose.setPadding(dp(context, 10), dp(context, 4), dp(context, 10), dp(context, 4));
        tvClose.setBackground(ThemeStyles.createRoundedDrawable(
                Color.parseColor("#111C30"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 10)));
        tvClose.setOnClickListener(v -> dialog.dismiss());
        header.addView(tvClose);
        root.addView(header);

        // Stats line
        PrismLogger.LogStats stats = PrismLogger.getStats(context);
        TextView tvStats = new TextView(context);
        tvStats.setText(String.format(Locale.US,
                "Stored: %.1f KB across %d file(s) • %d session entries • %d crash report(s) • Level: %s",
                stats.totalBytes / 1024.0, stats.logFileCount, stats.sessionLines, stats.crashCount, stats.verbosity));
        tvStats.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvStats.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        tvStats.setPadding(0, 0, 0, dp(context, 8));
        root.addView(tvStats);

        // Search input
        EditText etFilter = new EditText(context);
        etFilter.setHint("Filter logs (component, error, codec, path...)");
        etFilter.setHintTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        etFilter.setTextColor(Color.WHITE);
        etFilter.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        etFilter.setSingleLine(true);
        etFilter.setPadding(dp(context, 10), dp(context, 8), dp(context, 10), dp(context, 8));
        etFilter.setBackground(ThemeStyles.createRoundedDrawable(
                Color.parseColor("#0D1628"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 10)));
        LinearLayout.LayoutParams searchLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        searchLp.setMargins(0, 0, 0, dp(context, 8));
        etFilter.setLayoutParams(searchLp);
        root.addView(etFilter);

        // Filter pills
        String[] levels = new String[]{"ALL", "E", "W", "I", "D"};
        String[] labels = new String[]{"ALL", "ERRORS", "WARN+", "INFO", "DEBUG"};
        TextView[] pills = new TextView[levels.length];

        HorizontalScrollView pillScroll = new HorizontalScrollView(context);
        pillScroll.setHorizontalScrollBarEnabled(false);
        LinearLayout pillRow = new LinearLayout(context);
        pillRow.setOrientation(LinearLayout.HORIZONTAL);
        pillScroll.addView(pillRow);
        LinearLayout.LayoutParams pillScrollLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        pillScrollLp.setMargins(0, 0, 0, dp(context, 8));
        pillScroll.setLayoutParams(pillScrollLp);
        root.addView(pillScroll);

        // Log output scroll view
        ScrollView logScroll = new ScrollView(context);
        logScroll.setBackground(ThemeStyles.createRoundedDrawable(
                Color.parseColor("#040812"), Color.parseColor("#1E293B"), dp(context, 1), dp(context, 10)));
        logScroll.setPadding(dp(context, 8), dp(context, 8), dp(context, 8), dp(context, 8));
        LinearLayout.LayoutParams scrollLp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 320));
        scrollLp.setMargins(0, 0, 0, dp(context, 10));
        logScroll.setLayoutParams(scrollLp);

        TextView tvLogs = new TextView(context);
        tvLogs.setTextColor(Color.parseColor("#CBD5E1"));
        tvLogs.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9.5f);
        tvLogs.setTypeface(Typeface.MONOSPACE);
        tvLogs.setTextIsSelectable(true);
        logScroll.addView(tvLogs);
        root.addView(logScroll);

        Runnable refreshLogs = () -> {
            List<String> lines = PrismLogger.getRecentLines(600, activeFilter[0]);
            String q = searchQuery[0].trim().toLowerCase(Locale.US);
            StringBuilder sb = new StringBuilder();
            int shown = 0;
            for (String line : lines) {
                if (!q.isEmpty() && !line.toLowerCase(Locale.US).contains(q)) continue;
                sb.append(line).append('\n');
                shown++;
            }
            if (shown == 0) {
                tvLogs.setText("(No log lines match current filter)");
            } else {
                tvLogs.setText(sb.toString());
            }
            logScroll.post(() -> logScroll.fullScroll(ScrollView.FOCUS_DOWN));
        };

        for (int i = 0; i < levels.length; i++) {
            final int idx = i;
            final String lvl = levels[i];
            TextView pill = new TextView(context);
            pill.setText(labels[i]);
            pill.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
            pill.setTypeface(Typeface.DEFAULT_BOLD);
            pill.setPadding(dp(context, 10), dp(context, 5), dp(context, 10), dp(context, 5));
            stylePill(context, pill, i == 0);
            LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.setMargins(0, 0, dp(context, 6), 0);
            pill.setLayoutParams(lp);
            pill.setOnClickListener(v -> {
                activeFilter[0] = lvl;
                for (int j = 0; j < pills.length; j++) {
                    stylePill(context, pills[j], j == idx);
                }
                refreshLogs.run();
            });
            pills[i] = pill;
            pillRow.addView(pill);
        }

        etFilter.addTextChangedListener(new TextWatcher() {
            @Override public void beforeTextChanged(CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged(CharSequence s, int start, int before, int count) {
                searchQuery[0] = s != null ? s.toString() : "";
                refreshLogs.run();
            }
            @Override public void afterTextChanged(Editable s) {}
        });

        // Action Buttons Row 1: DOWNLOAD FULL LOGS + SHARE / SEND
        LinearLayout btnRow1 = new LinearLayout(context);
        btnRow1.setOrientation(LinearLayout.HORIZONTAL);
        LinearLayout.LayoutParams br1Lp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        br1Lp.setMargins(0, 0, 0, dp(context, 8));
        btnRow1.setLayoutParams(br1Lp);

        Button btnDownload = new Button(context);
        btnDownload.setText("DOWNLOAD LOGS");
        btnDownload.setTextColor(Color.WHITE);
        btnDownload.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnDownload.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        btnDownload.setBackground(ThemeStyles.createGradientButton(
                Color.parseColor("#0284C7"), Color.parseColor("#06B6D4"), dp(context, 10)));
        LinearLayout.LayoutParams b1 = new LinearLayout.LayoutParams(0, dp(context, 42), 1.0f);
        b1.setMargins(0, 0, dp(context, 6), 0);
        btnDownload.setLayoutParams(b1);
        btnDownload.setOnClickListener(v -> triggerDownloadLogs(context, false));
        btnRow1.addView(btnDownload);

        Button btnShare = new Button(context);
        btnShare.setText("SHARE / SEND");
        btnShare.setTextColor(Color.WHITE);
        btnShare.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnShare.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        btnShare.setBackground(ThemeStyles.createGradientButton(
                Color.parseColor("#059669"), Color.parseColor("#10B981"), dp(context, 10)));
        LinearLayout.LayoutParams b2 = new LinearLayout.LayoutParams(0, dp(context, 42), 1.0f);
        b2.setMargins(dp(context, 6), 0, 0, 0);
        btnShare.setLayoutParams(b2);
        btnShare.setOnClickListener(v -> triggerDownloadLogs(context, true));
        btnRow1.addView(btnShare);
        root.addView(btnRow1);

        // Action Buttons Row 2: COPY + REFRESH
        LinearLayout btnRow2 = new LinearLayout(context);
        btnRow2.setOrientation(LinearLayout.HORIZONTAL);

        Button btnCopy = new Button(context);
        btnCopy.setText("COPY VISIBLE");
        btnCopy.setTextColor(Color.WHITE);
        btnCopy.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnCopy.setTypeface(Typeface.DEFAULT_BOLD);
        btnCopy.setBackground(ThemeStyles.createRoundedDrawable(
                Color.parseColor("#1E293B"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 10)));
        LinearLayout.LayoutParams b3 = new LinearLayout.LayoutParams(0, dp(context, 38), 1.0f);
        b3.setMargins(0, 0, dp(context, 6), 0);
        btnCopy.setLayoutParams(b3);
        btnCopy.setOnClickListener(v -> {
            try {
                ClipboardManager cm = (ClipboardManager) context.getSystemService(Context.CLIPBOARD_SERVICE);
                if (cm != null) {
                    cm.setPrimaryClip(ClipData.newPlainText("Prism-V Logs", tvLogs.getText().toString()));
                    if (context instanceof Activity) {
                        PrismHud.showSuccess((Activity) context, "Copied visible logs to clipboard!");
                    }
                }
            } catch (Throwable t) {
                PrismLogger.e(TAG, "Copy logs failed: " + t.getMessage(), t);
            }
        });
        btnRow2.addView(btnCopy);

        Button btnRefresh = new Button(context);
        btnRefresh.setText("REFRESH");
        btnRefresh.setTextColor(Color.WHITE);
        btnRefresh.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
        btnRefresh.setTypeface(Typeface.DEFAULT_BOLD);
        btnRefresh.setBackground(ThemeStyles.createRoundedDrawable(
                Color.parseColor("#1E293B"), ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 10)));
        LinearLayout.LayoutParams b4 = new LinearLayout.LayoutParams(0, dp(context, 38), 1.0f);
        b4.setMargins(dp(context, 6), 0, 0, 0);
        btnRefresh.setLayoutParams(b4);
        btnRefresh.setOnClickListener(v -> {
            PrismLogger.LogStats s = PrismLogger.getStats(context);
            tvStats.setText(String.format(Locale.US,
                    "Stored: %.1f KB across %d file(s) • %d session entries • %d crash report(s) • Level: %s",
                    s.totalBytes / 1024.0, s.logFileCount, s.sessionLines, s.crashCount, s.verbosity));
            refreshLogs.run();
        });
        btnRow2.addView(btnRefresh);
        root.addView(btnRow2);

        refreshLogs.run();

        dialog.setContentView(root);
        dialog.setOnDismissListener(d -> DialogTracker.unregister(dialog));
        DialogTracker.register(dialog);
        dialog.show();

        if (dialog.getWindow() != null) {
            int width = (int) (context.getResources().getDisplayMetrics().widthPixels * 0.94);
            dialog.getWindow().setLayout(width, ViewGroup.LayoutParams.WRAP_CONTENT);
        }
    }

    /**
     * Reusable helper to export full diagnostic logs on a background thread, save to Downloads/Prism-V/,
     * and optionally open the Share Sheet immediately or show a confirmation dialog with Share button.
     */
    public static void triggerDownloadLogs(Context context, boolean shareImmediately) {
        if (context == null) return;
        if (context instanceof Activity) {
            PrismHud.showInfo((Activity) context, "Collecting full diagnostic logs across all components...");
        }
        Handler mainHandler = new Handler(Looper.getMainLooper());
        new Thread(() -> {
            PrismLogger.ExportResult res = PrismLogger.exportFullDiagnosticLog(context);
            mainHandler.post(() -> {
                if (!res.success) {
                    if (context instanceof Activity) {
                        PrismHud.showError((Activity) context, "Log export failed: " + res.errorMessage);
                    }
                    return;
                }

                if (shareImmediately) {
                    if (context instanceof Activity) {
                        PrismHud.showSuccess((Activity) context,
                                String.format(Locale.US, "Saved %.1f KB to %s", res.sizeBytes / 1024.0, res.publicDisplayPath));
                    }
                    PrismLogger.shareLogFile(context, res);
                } else {
                    String summary = String.format(Locale.US,
                            "Full diagnostic log saved!\n\n" +
                            "• Saved to: %s\n" +
                            "• Size: %.1f KB (%d lines)\n" +
                            "• Crash dumps included: %d\n" +
                            "• Includes: Device specs, MediaCodec list, settings, work journal, history, crash reports, process logcat, and all component logs.\n\n" +
                            "Tap SHARE / SEND LOGS to send this file directly to the developer so we can find and fix any issue.",
                            res.publicDisplayPath, res.sizeBytes / 1024.0, res.totalLines, res.crashCount);

                    CustomDarkDialog.showConfirmation(
                            context,
                            "Full Logs Downloaded",
                            summary,
                            "SHARE / SEND LOGS",
                            "DONE",
                            () -> PrismLogger.shareLogFile(context, res)
                    );
                }
            });
        }, "PrismLogger-Export").start();
    }

    private static void stylePill(Context context, TextView pill, boolean active) {
        pill.setTextColor(active ? Color.WHITE : ThemeStyles.COLOR_TEXT_MUTED);
        pill.setBackground(ThemeStyles.createRoundedDrawable(
                active ? ThemeStyles.COLOR_CYAN_PRIMARY : Color.parseColor("#121B2D"),
                active ? Color.WHITE : ThemeStyles.COLOR_BORDER_SUBTLE,
                dp(context, 1), dp(context, 10)));
    }

    private static int dp(Context context, int value) {
        return ThemeStyles.dp(context, value);
    }
}
