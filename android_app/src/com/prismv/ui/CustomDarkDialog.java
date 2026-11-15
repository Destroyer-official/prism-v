package com.prismv.ui;

import android.app.Dialog;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.text.InputType;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.*;
import java.io.File;
import java.util.Locale;

/**
 * CustomDarkDialog:
 * Premium Glassmorphic Studio Dialog Engine.
 * Completely replaces all dated, stark-white Android AlertDialogs with
 * obsidian-card modal panels featuring neon cyan borders, crisp studio typography,
 * glowing choice chips, and tactile emerald CTA buttons.
 */
public final class CustomDarkDialog {

    private CustomDarkDialog() {}

    private static int dp(Context context, int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, context.getResources().getDisplayMetrics());
    }

    private static GradientDrawable createCardBackground(Context context) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(ThemeStyles.COLOR_BG_CARD);
        d.setStroke(dp(context, 2), ThemeStyles.COLOR_BORDER_ACTIVE);
        d.setCornerRadius(dp(context, 20));
        return d;
    }

    private static GradientDrawable createPillBackground(Context context, boolean active) {
        GradientDrawable d = new GradientDrawable();
        if (active) {
            d.setColor(Color.parseColor("#0F2D4A"));
            d.setStroke(dp(context, 1), ThemeStyles.COLOR_CYAN_NEON);
        } else {
            d.setColor(Color.parseColor("#091020"));
            d.setStroke(dp(context, 1), ThemeStyles.COLOR_BORDER_SUBTLE);
        }
        d.setCornerRadius(dp(context, 12));
        return d;
    }

    public interface OnItemSelectedListener {
        void onSelected(int which, String text);
    }

    public interface OnMultiChoiceConfirmedListener {
        void onConfirmed(boolean[] selectedIndices, java.util.List<String> selectedItems);
    }

    public interface OnInputConfirmedListener {
        void onConfirmed(String value);
    }

    /**
     * Display a dark glassmorphic single-choice selection dialog.
     */
    public static Dialog showSingleChoice(Context context, String title, String[] items, int selectedIndex, OnItemSelectedListener listener) {
        Dialog dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        ScrollView scroll = new ScrollView(context);
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(context, 20);
        root.setPadding(pad, pad, pad, pad);
        root.setBackground(createCardBackground(context));

        // Header Title
        TextView tvTitle = new TextView(context);
        tvTitle.setText(title);
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        root.addView(tvTitle);

        // Cyan Accent Line
        View divider = new View(context);
        divider.setBackgroundColor(ThemeStyles.COLOR_CYAN_PRIMARY);
        LinearLayout.LayoutParams divParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 2));
        divParams.setMargins(0, dp(context, 10), 0, dp(context, 14));
        divider.setLayoutParams(divParams);
        root.addView(divider);

        // List Items
        for (int i = 0; i < items.length; i++) {
            final int index = i;
            final String itemText = items[i];
            boolean isSelected = (i == selectedIndex);

            LinearLayout itemRow = new LinearLayout(context);
            itemRow.setOrientation(LinearLayout.HORIZONTAL);
            itemRow.setGravity(Gravity.CENTER_VERTICAL);
            itemRow.setPadding(dp(context, 14), dp(context, 12), dp(context, 14), dp(context, 12));
            itemRow.setBackground(createPillBackground(context, isSelected));

            LinearLayout.LayoutParams rowParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            rowParams.setMargins(0, 0, 0, dp(context, 8));
            itemRow.setLayoutParams(rowParams);

            TextView tvDot = new TextView(context);
            tvDot.setText(isSelected ? "● " : "○ ");
            tvDot.setTextColor(isSelected ? ThemeStyles.COLOR_CYAN_NEON : ThemeStyles.COLOR_TEXT_MUTED);
            tvDot.setTextSize(TypedValue.COMPLEX_UNIT_SP, 14);
            itemRow.addView(tvDot);

            TextView tvText = new TextView(context);
            tvText.setText(itemText);
            tvText.setTextColor(isSelected ? Color.WHITE : ThemeStyles.COLOR_TEXT_SECONDARY);
            tvText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            tvText.setTypeface(isSelected ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
            itemRow.addView(tvText);

            itemRow.setOnClickListener(v -> {
                dialog.dismiss();
                if (listener != null) listener.onSelected(index, itemText);
            });

            root.addView(itemRow);
        }

        // Cancel Button
        Button btnCancel = new Button(context);
        btnCancel.setText("Cancel");
        btnCancel.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnCancel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnCancel.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 12)));
        LinearLayout.LayoutParams cancelParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 40));
        cancelParams.setMargins(0, dp(context, 6), 0, 0);
        btnCancel.setLayoutParams(cancelParams);
        btnCancel.setOnClickListener(v -> dialog.dismiss());
        root.addView(btnCancel);

        scroll.addView(root);
        dialog.setContentView(scroll);
        dialog.show();
        return dialog;
    }

    /**
     * Display a dark glassmorphic multi-choice selection dialog with real checkboxes.
     */
    public static Dialog showMultiChoice(Context context, String title, String[] items, boolean[] initialChecked, OnMultiChoiceConfirmedListener listener) {
        Dialog dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        final boolean[] checked = new boolean[items.length];
        if (initialChecked != null) {
            System.arraycopy(initialChecked, 0, checked, 0, Math.min(initialChecked.length, checked.length));
        }

        ScrollView scroll = new ScrollView(context);
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(context, 20);
        root.setPadding(pad, pad, pad, pad);
        root.setBackground(createCardBackground(context));

        // Header Title
        TextView tvTitle = new TextView(context);
        tvTitle.setText(title);
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        root.addView(tvTitle);

        // Subtitle
        TextView tvSub = new TextView(context);
        tvSub.setText("Select one or more videos to include in the batch queue:");
        tvSub.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvSub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvSub.setPadding(0, dp(context, 4), 0, dp(context, 10));
        root.addView(tvSub);

        // Cyan Accent Line
        View divider = new View(context);
        divider.setBackgroundColor(ThemeStyles.COLOR_CYAN_PRIMARY);
        LinearLayout.LayoutParams divParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 2));
        divParams.setMargins(0, 0, 0, dp(context, 12));
        divider.setLayoutParams(divParams);
        root.addView(divider);

        final TextView[] checkBoxes = new TextView[items.length];
        final TextView[] textViews = new TextView[items.length];
        final LinearLayout[] rows = new LinearLayout[items.length];

        // List Items
        for (int i = 0; i < items.length; i++) {
            final int index = i;
            final String itemText = items[i];

            LinearLayout itemRow = new LinearLayout(context);
            rows[index] = itemRow;
            itemRow.setOrientation(LinearLayout.HORIZONTAL);
            itemRow.setGravity(Gravity.CENTER_VERTICAL);
            itemRow.setPadding(dp(context, 14), dp(context, 12), dp(context, 14), dp(context, 12));
            itemRow.setBackground(createPillBackground(context, checked[index]));

            LinearLayout.LayoutParams rowParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            rowParams.setMargins(0, 0, 0, dp(context, 8));
            itemRow.setLayoutParams(rowParams);

            TextView tvBox = new TextView(context);
            checkBoxes[index] = tvBox;
            tvBox.setText(checked[index] ? "☑ " : "☐ ");
            tvBox.setTextColor(checked[index] ? ThemeStyles.COLOR_CYAN_NEON : ThemeStyles.COLOR_TEXT_MUTED);
            tvBox.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
            itemRow.addView(tvBox);

            TextView tvText = new TextView(context);
            textViews[index] = tvText;
            tvText.setText(itemText);
            tvText.setTextColor(checked[index] ? Color.WHITE : ThemeStyles.COLOR_TEXT_SECONDARY);
            tvText.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            tvText.setTypeface(checked[index] ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
            itemRow.addView(tvText);

            itemRow.setOnClickListener(v -> {
                checked[index] = !checked[index];
                tvBox.setText(checked[index] ? "☑ " : "☐ ");
                tvBox.setTextColor(checked[index] ? ThemeStyles.COLOR_CYAN_NEON : ThemeStyles.COLOR_TEXT_MUTED);
                tvText.setTextColor(checked[index] ? Color.WHITE : ThemeStyles.COLOR_TEXT_SECONDARY);
                tvText.setTypeface(checked[index] ? Typeface.DEFAULT_BOLD : Typeface.DEFAULT);
                itemRow.setBackground(createPillBackground(context, checked[index]));
            });

            root.addView(itemRow);
        }

        // Confirm Button
        Button btnConfirm = new Button(context);
        btnConfirm.setText("✓  CONFIRM SELECTION");
        btnConfirm.setTextColor(Color.WHITE);
        btnConfirm.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        btnConfirm.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        btnConfirm.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#059669"), Color.parseColor("#10B981"), dp(context, 12)));
        LinearLayout.LayoutParams confirmParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 46));
        confirmParams.setMargins(0, dp(context, 12), 0, dp(context, 6));
        btnConfirm.setLayoutParams(confirmParams);
        btnConfirm.setOnClickListener(v -> {
            dialog.dismiss();
            if (listener != null) {
                java.util.List<String> selectedList = new java.util.ArrayList<>();
                for (int i = 0; i < items.length; i++) {
                    if (checked[i]) selectedList.add(items[i]);
                }
                listener.onConfirmed(checked, selectedList);
            }
        });
        root.addView(btnConfirm);

        // Cancel Button
        Button btnCancel = new Button(context);
        btnCancel.setText("Cancel");
        btnCancel.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnCancel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnCancel.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 12)));
        LinearLayout.LayoutParams cancelParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 40));
        cancelParams.setMargins(0, dp(context, 4), 0, 0);
        btnCancel.setLayoutParams(cancelParams);
        btnCancel.setOnClickListener(v -> dialog.dismiss());
        root.addView(btnCancel);

        scroll.addView(root);
        dialog.setContentView(scroll);
        dialog.show();
        return dialog;
    }

    /**
     * Display a dark glassmorphic text input dialog (for custom FPS, Bitrate, etc).
     */
    public static Dialog showInputPrompt(Context context, String title, String subtitle, String initialValue, int inputType, OnInputConfirmedListener listener) {
        Dialog dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(context, 20);
        root.setPadding(pad, pad, pad, pad);
        root.setBackground(createCardBackground(context));

        // Header Title
        TextView tvTitle = new TextView(context);
        tvTitle.setText(title);
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        root.addView(tvTitle);

        if (subtitle != null && !subtitle.isEmpty()) {
            TextView tvSub = new TextView(context);
            tvSub.setText(subtitle);
            tvSub.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
            tvSub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
            tvSub.setPadding(0, dp(context, 4), 0, dp(context, 10));
            root.addView(tvSub);
        } else {
            View divider = new View(context);
            divider.setBackgroundColor(ThemeStyles.COLOR_CYAN_PRIMARY);
            LinearLayout.LayoutParams divParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 2));
            divParams.setMargins(0, dp(context, 8), 0, dp(context, 12));
            divider.setLayoutParams(divParams);
            root.addView(divider);
        }

        // Input Field
        EditText etInput = new EditText(context);
        etInput.setText(initialValue != null ? initialValue : "");
        etInput.setTextColor(Color.WHITE);
        etInput.setTextSize(TypedValue.COMPLEX_UNIT_SP, 15);
        etInput.setInputType(inputType);
        etInput.setPadding(dp(context, 14), dp(context, 12), dp(context, 14), dp(context, 12));
        etInput.setBackground(ThemeStyles.createRoundedDrawable(Color.parseColor("#091020"), ThemeStyles.COLOR_CYAN_NEON, dp(context, 1), dp(context, 12)));
        LinearLayout.LayoutParams inputParams = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        inputParams.setMargins(0, 0, 0, dp(context, 16));
        etInput.setLayoutParams(inputParams);
        root.addView(etInput);

        // Buttons Row
        LinearLayout btnRow = new LinearLayout(context);
        btnRow.setOrientation(LinearLayout.HORIZONTAL);

        Button btnCancel = new Button(context);
        btnCancel.setText("Cancel");
        btnCancel.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnCancel.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnCancel.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 12)));
        LinearLayout.LayoutParams cancelP = new LinearLayout.LayoutParams(0, dp(context, 44), 1.0f);
        cancelP.setMargins(0, 0, dp(context, 8), 0);
        btnCancel.setLayoutParams(cancelP);
        btnCancel.setOnClickListener(v -> dialog.dismiss());
        btnRow.addView(btnCancel);

        Button btnSet = new Button(context);
        btnSet.setText("Apply Preset");
        btnSet.setTextColor(Color.WHITE);
        btnSet.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnSet.setTypeface(Typeface.DEFAULT_BOLD);
        btnSet.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#0284C7"), Color.parseColor("#00E5FF"), dp(context, 12)));
        LinearLayout.LayoutParams setP = new LinearLayout.LayoutParams(0, dp(context, 44), 1.4f);
        btnSet.setLayoutParams(setP);
        btnSet.setOnClickListener(v -> {
            dialog.dismiss();
            if (listener != null) listener.onConfirmed(etInput.getText().toString().trim());
        });
        btnRow.addView(btnSet);

        root.addView(btnRow);
        dialog.setContentView(root);
        dialog.show();
        return dialog;
    }



    /**
     * Display Premium Visual Skin Selection Dialog for the 6 Studio Design Skins.
     */
    public static Dialog showSkinSwitcher(Context context, OnItemSelectedListener listener) {
        SkinManager.Skin[] skins = SkinManager.Skin.values();
        SkinManager.Skin active = SkinManager.getActiveSkin();

        Dialog dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        ScrollView scroll = new ScrollView(context);
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(context, 18);
        root.setPadding(pad, pad, pad, pad);
        root.setBackground(createCardBackground(context));

        TextView tvTitle = new TextView(context);
        tvTitle.setText("🎨 STUDIO THEMES & HARDWARE SKINS");
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        root.addView(tvTitle);

        TextView tvSub = new TextView(context);
        tvSub.setText("Hardware display tuning, OLED energy optimization & studio color palettes.");
        tvSub.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvSub.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvSub.setPadding(0, dp(context, 4), 0, dp(context, 12));
        root.addView(tvSub);

        for (int i = 0; i < skins.length; i++) {
            final int index = i;
            final SkinManager.Skin skin = skins[i];
            boolean isSelected = (skin == active);

            LinearLayout card = new LinearLayout(context);
            card.setOrientation(LinearLayout.VERTICAL);
            card.setPadding(dp(context, 14), dp(context, 12), dp(context, 14), dp(context, 12));
            card.setBackground(ThemeStyles.createRoundedDrawable(
                    skin.colorSurface,
                    isSelected ? skin.colorAccent : ThemeStyles.COLOR_BORDER_SUBTLE,
                    isSelected ? dp(context, 2) : dp(context, 1),
                    dp(context, 14)
            ));

            LinearLayout.LayoutParams cardParams = new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            cardParams.setMargins(0, 0, 0, dp(context, 10));
            card.setLayoutParams(cardParams);

            // Row 1: Title + Tag Badge
            LinearLayout topRow = new LinearLayout(context);
            topRow.setOrientation(LinearLayout.HORIZONTAL);
            topRow.setGravity(Gravity.CENTER_VERTICAL);

            TextView tvT = new TextView(context);
            tvT.setText(skin.displayName);
            tvT.setTextColor(isSelected ? skin.colorAccent : Color.WHITE);
            tvT.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
            tvT.setTypeface(Typeface.DEFAULT_BOLD);
            LinearLayout.LayoutParams tParams = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
            tvT.setLayoutParams(tParams);
            topRow.addView(tvT);

            TextView tvTag = new TextView(context);
            tvTag.setText(skin.tag);
            tvTag.setTextColor(skin.colorAccent);
            tvTag.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
            tvTag.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
            tvTag.setPadding(dp(context, 6), dp(context, 2), dp(context, 6), dp(context, 2));
            tvTag.setBackground(ThemeStyles.createRoundedDrawable(
                    Color.argb(40, Color.red(skin.colorAccent), Color.green(skin.colorAccent), Color.blue(skin.colorAccent)),
                    skin.colorAccent,
                    dp(context, 1),
                    dp(context, 6)
            ));
            topRow.addView(tvTag);
            card.addView(topRow);

            // Row 2: Description
            TextView tvD = new TextView(context);
            tvD.setText(skin.description);
            tvD.setTextColor(skin.isLight ? Color.parseColor("#475569") : Color.parseColor("#94A3B8"));
            tvD.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
            tvD.setPadding(0, dp(context, 4), 0, dp(context, 8));
            card.addView(tvD);

            // Row 3: Live 4-Color Swatch Strip + Status Pill
            LinearLayout bottomRow = new LinearLayout(context);
            bottomRow.setOrientation(LinearLayout.HORIZONTAL);
            bottomRow.setGravity(Gravity.CENTER_VERTICAL);

            // 4 Swatch blocks
            int[] swatchColors = new int[]{skin.colorCanvas, skin.colorSurface, skin.colorPrimary, skin.colorAccent};
            String[] swatchLabels = new String[]{"BASE", "PANEL", "PRIMARY", "ACCENT"};
            LinearLayout swatchGroup = new LinearLayout(context);
            swatchGroup.setOrientation(LinearLayout.HORIZONTAL);
            LinearLayout.LayoutParams sgp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
            swatchGroup.setLayoutParams(sgp);

            for (int s = 0; s < 4; s++) {
                TextView sw = new TextView(context);
                sw.setText(swatchLabels[s]);
                sw.setTextSize(TypedValue.COMPLEX_UNIT_SP, 8);
                sw.setTypeface(Typeface.MONOSPACE, Typeface.BOLD);
                sw.setGravity(Gravity.CENTER);
                sw.setTextColor(s >= 2 ? Color.WHITE : (skin.isLight ? Color.BLACK : Color.WHITE));
                sw.setBackground(ThemeStyles.createRoundedDrawable(swatchColors[s], Color.parseColor("#445566"), dp(context, 1), dp(context, 4)));
                LinearLayout.LayoutParams swp = new LinearLayout.LayoutParams(dp(context, 46), dp(context, 20));
                swp.setMargins(0, 0, dp(context, 4), 0);
                sw.setLayoutParams(swp);
                swatchGroup.addView(sw);
            }
            bottomRow.addView(swatchGroup);

            // Status Pill
            TextView tvStatus = new TextView(context);
            tvStatus.setText(isSelected ? "● ACTIVE" : "○ APPLY");
            tvStatus.setTextColor(isSelected ? skin.colorAccent : Color.parseColor("#64748B"));
            tvStatus.setTextSize(TypedValue.COMPLEX_UNIT_SP, 10);
            tvStatus.setTypeface(Typeface.DEFAULT_BOLD);
            tvStatus.setPadding(dp(context, 8), dp(context, 4), dp(context, 8), dp(context, 4));
            tvStatus.setBackground(ThemeStyles.createRoundedDrawable(
                    isSelected ? Color.argb(45, Color.red(skin.colorAccent), Color.green(skin.colorAccent), Color.blue(skin.colorAccent)) : Color.TRANSPARENT,
                    isSelected ? skin.colorAccent : Color.parseColor("#334155"),
                    dp(context, 1),
                    dp(context, 10)
            ));
            bottomRow.addView(tvStatus);

            card.addView(bottomRow);

            card.setOnClickListener(v -> {
                dialog.dismiss();
                SkinManager.setActiveSkin(skin);
                if (listener != null) listener.onSelected(index, skin.displayName);
            });

            root.addView(card);
        }

        Button btnClose = new Button(context);
        btnClose.setText("Cancel");
        btnClose.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnClose.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnClose.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 12)));
        LinearLayout.LayoutParams closeP = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 40));
        closeP.setMargins(0, dp(context, 4), 0, 0);
        btnClose.setLayoutParams(closeP);
        btnClose.setOnClickListener(v -> dialog.dismiss());
        root.addView(btnClose);

        scroll.addView(root);
        dialog.setContentView(scroll);
        dialog.show();
        return dialog;
    }

    /**
     * Dark confirmation dialog with glowing accent buttons.
     */
    public static Dialog showConfirmation(Context context, String title, String message, String positiveText, String negativeText, Runnable onConfirmed) {
        Dialog dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(context, 20);
        root.setPadding(pad, pad, pad, pad);
        root.setBackground(createCardBackground(context));

        TextView tvTitle = new TextView(context);
        tvTitle.setText(title);
        tvTitle.setTextColor(Color.WHITE);
        tvTitle.setTextSize(TypedValue.COMPLEX_UNIT_SP, 16);
        tvTitle.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        root.addView(tvTitle);

        TextView tvMsg = new TextView(context);
        tvMsg.setText(message);
        tvMsg.setTextColor(ThemeStyles.COLOR_TEXT_SECONDARY);
        tvMsg.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        tvMsg.setPadding(0, dp(context, 10), 0, dp(context, 16));
        root.addView(tvMsg);

        LinearLayout rowBtns = new LinearLayout(context);
        rowBtns.setOrientation(LinearLayout.HORIZONTAL);
        rowBtns.setWeightSum(2);

        Button btnNeg = new Button(context);
        btnNeg.setText(negativeText != null ? negativeText : "Cancel");
        btnNeg.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnNeg.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnNeg.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 12)));
        LinearLayout.LayoutParams negParams = new LinearLayout.LayoutParams(0, dp(context, 44), 1.0f);
        negParams.setMargins(0, 0, dp(context, 6), 0);
        btnNeg.setLayoutParams(negParams);
        btnNeg.setOnClickListener(v -> dialog.dismiss());
        rowBtns.addView(btnNeg);

        Button btnPos = new Button(context);
        btnPos.setText(positiveText != null ? positiveText : "Confirm");
        btnPos.setTextColor(Color.WHITE);
        btnPos.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnPos.setTypeface(Typeface.DEFAULT_BOLD);
        btnPos.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#DC2626"), Color.parseColor("#EF4444"), dp(context, 12)));
        LinearLayout.LayoutParams posParams = new LinearLayout.LayoutParams(0, dp(context, 44), 1.0f);
        posParams.setMargins(dp(context, 6), 0, 0, 0);
        btnPos.setLayoutParams(posParams);
        btnPos.setOnClickListener(v -> {
            dialog.dismiss();
            if (onConfirmed != null) onConfirmed.run();
        });
        rowBtns.addView(btnPos);
        root.addView(rowBtns);

        dialog.setContentView(root);
        dialog.show();
        return dialog;
    }

    /**
     * Display a flagship celebratory Completion Dossier modal sheet
     * showing exact storage reclaimed, VMAF fidelity badge, and 1-tap actions.
     */
    public static Dialog showCompressionCompleteDossier(
            Context context,
            String videoName,
            double origMb,
            double compMb,
            double freedMb,
            double pctSaved,
            String outPath,
            Runnable onCompareInDuo,
            Runnable onPlaySolo,
            Runnable onShare,
            Runnable onOpenGallery
    ) {
        Dialog dialog = new Dialog(context);
        dialog.requestWindowFeature(Window.FEATURE_NO_TITLE);
        if (dialog.getWindow() != null) {
            dialog.getWindow().setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
            dialog.getWindow().setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);
        }

        ScrollView scroll = new ScrollView(context);
        LinearLayout root = new LinearLayout(context);
        root.setOrientation(LinearLayout.VERTICAL);
        int pad = dp(context, 20);
        root.setPadding(pad, pad, pad, pad);
        root.setBackground(createCardBackground(context));

        // Header Title: Glowing Badge
        LinearLayout headerRow = new LinearLayout(context);
        headerRow.setOrientation(LinearLayout.HORIZONTAL);
        headerRow.setGravity(Gravity.CENTER_VERTICAL);

        TextView tvBadge = new TextView(context);
        tvBadge.setText("✨ OPTIMIZATION COMPLETED & VAULTED");
        tvBadge.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        tvBadge.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvBadge.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        headerRow.addView(tvBadge);
        root.addView(headerRow);

        // Huge Neon Reclaimed Percentage
        TextView tvPct = new TextView(context);
        tvPct.setText(String.format(Locale.US, "-%.1f%%", pctSaved));
        tvPct.setTextColor(ThemeStyles.COLOR_EMERALD_NEON);
        tvPct.setTextSize(TypedValue.COMPLEX_UNIT_SP, 44);
        tvPct.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        tvPct.setPadding(0, dp(context, 4), 0, 0);
        root.addView(tvPct);

        TextView tvFreed = new TextView(context);
        tvFreed.setText(String.format(Locale.US, "RECLAIMED +%.1f MB FROM FLASH STORAGE", freedMb));
        tvFreed.setTextColor(Color.WHITE);
        tvFreed.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvFreed.setTypeface(Typeface.DEFAULT_BOLD);
        tvFreed.setPadding(0, 0, 0, dp(context, 12));
        root.addView(tvFreed);

        // Metrics Grid (Original -> Optimized -> Fidelity)
        LinearLayout metricsGrid = new LinearLayout(context);
        metricsGrid.setOrientation(LinearLayout.HORIZONTAL);
        metricsGrid.setWeightSum(3);
        metricsGrid.setPadding(dp(context, 12), dp(context, 10), dp(context, 12), dp(context, 10));
        metricsGrid.setBackground(createPillBackground(context, false));

        addDossierCol(context, metricsGrid, "ORIGINAL", String.format(Locale.US, "%.1f MB", origMb), "#94A3B8");
        addDossierCol(context, metricsGrid, "OPTIMIZED", String.format(Locale.US, "%.1f MB", compMb), "#38BDF8");
        addDossierCol(context, metricsGrid, "FIDELITY", "VMAF > 95", "#34D399");
        root.addView(metricsGrid);

        // Output File Details
        TextView tvPath = new TextView(context);
        File f = (outPath != null) ? new File(outPath) : null;
        String name = (f != null) ? f.getName() : videoName;
        tvPath.setText("📁 Saved to Movies/Prism-V/" + name + "\n✓ Indexed in Android MediaStore & Samsung Gallery");
        tvPath.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        tvPath.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        tvPath.setTypeface(Typeface.MONOSPACE);
        tvPath.setPadding(0, dp(context, 10), 0, dp(context, 14));
        root.addView(tvPath);

        // Action 1: Compare in Duo Player (Primary Emerald/Cyan Gradient)
        Button btnDuo = new Button(context);
        btnDuo.setText("⚖️  COMPARE IN DUO PLAYER (0ms Sync)");
        btnDuo.setTextColor(Color.WHITE);
        btnDuo.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        btnDuo.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        btnDuo.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#4338CA"), Color.parseColor("#7C3AED"), dp(context, 12)));
        LinearLayout.LayoutParams duoP = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 48));
        duoP.setMargins(0, 0, 0, dp(context, 8));
        btnDuo.setLayoutParams(duoP);
        btnDuo.setOnClickListener(v -> {
            dialog.dismiss();
            if (onCompareInDuo != null) onCompareInDuo.run();
        });
        root.addView(btnDuo);

        // Action 2: Play Solo Video
        Button btnPlay = new Button(context);
        btnPlay.setText("▶  PLAY OPTIMIZED VIDEO");
        btnPlay.setTextColor(Color.WHITE);
        btnPlay.setTextSize(TypedValue.COMPLEX_UNIT_SP, 13);
        btnPlay.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        btnPlay.setBackground(ThemeStyles.createGradientButton(Color.parseColor("#059669"), Color.parseColor("#10B981"), dp(context, 12)));
        LinearLayout.LayoutParams playP = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 44));
        playP.setMargins(0, 0, 0, dp(context, 8));
        btnPlay.setLayoutParams(playP);
        btnPlay.setOnClickListener(v -> {
            dialog.dismiss();
            if (onPlaySolo != null) onPlaySolo.run();
        });
        root.addView(btnPlay);

        // Row Action 3: Gallery & Share
        LinearLayout rowAux = new LinearLayout(context);
        rowAux.setOrientation(LinearLayout.HORIZONTAL);
        rowAux.setWeightSum(2);
        LinearLayout.LayoutParams auxLp = new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 42));
        auxLp.setMargins(0, 0, 0, dp(context, 10));
        rowAux.setLayoutParams(auxLp);

        Button btnGal = new Button(context);
        btnGal.setText("🎬 GALLERY");
        btnGal.setTextColor(ThemeStyles.COLOR_CYAN_TEXT);
        btnGal.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnGal.setTypeface(Typeface.DEFAULT_BOLD);
        btnGal.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 10)));
        LinearLayout.LayoutParams gp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f);
        gp.setMargins(0, 0, dp(context, 4), 0);
        btnGal.setLayoutParams(gp);
        btnGal.setOnClickListener(v -> {
            dialog.dismiss();
            if (onOpenGallery != null) onOpenGallery.run();
        });
        rowAux.addView(btnGal);

        Button btnShare = new Button(context);
        btnShare.setText("↗ SHARE");
        btnShare.setTextColor(Color.WHITE);
        btnShare.setTextSize(TypedValue.COMPLEX_UNIT_SP, 11);
        btnShare.setTypeface(Typeface.DEFAULT_BOLD);
        btnShare.setBackground(ThemeStyles.createRoundedDrawable(Color.TRANSPARENT, ThemeStyles.COLOR_BORDER_SUBTLE, dp(context, 1), dp(context, 10)));
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.MATCH_PARENT, 1.0f);
        sp.setMargins(dp(context, 4), 0, 0, 0);
        btnShare.setLayoutParams(sp);
        btnShare.setOnClickListener(v -> {
            dialog.dismiss();
            if (onShare != null) onShare.run();
        });
        rowAux.addView(btnShare);
        root.addView(rowAux);

        // Done Button
        Button btnClose = new Button(context);
        btnClose.setText("Done");
        btnClose.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        btnClose.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        btnClose.setBackground(null);
        btnClose.setLayoutParams(new LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(context, 36)));
        btnClose.setOnClickListener(v -> dialog.dismiss());
        root.addView(btnClose);

        scroll.addView(root);
        dialog.setContentView(scroll);
        dialog.show();
        return dialog;
    }

    private static void addDossierCol(Context context, LinearLayout parent, String label, String value, String colorHex) {
        LinearLayout col = new LinearLayout(context);
        col.setOrientation(LinearLayout.VERTICAL);
        col.setGravity(Gravity.CENTER);
        LinearLayout.LayoutParams p = new LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1.0f);
        col.setLayoutParams(p);

        TextView tvL = new TextView(context);
        tvL.setText(label);
        tvL.setTextColor(ThemeStyles.COLOR_TEXT_MUTED);
        tvL.setTextSize(TypedValue.COMPLEX_UNIT_SP, 9);
        tvL.setTypeface(Typeface.DEFAULT_BOLD);
        col.addView(tvL);

        TextView tvV = new TextView(context);
        tvV.setText(value);
        tvV.setTextColor(Color.parseColor(colorHex));
        tvV.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
        tvV.setTypeface(Typeface.create("sans-serif-black", Typeface.BOLD));
        tvV.setPadding(0, dp(context, 2), 0, 0);
        col.addView(tvV);

        parent.addView(col);
    }
}
