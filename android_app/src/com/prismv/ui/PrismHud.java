package com.prismv.ui;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.os.Handler;
import android.os.Looper;
import android.util.TypedValue;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;

/**
 * PrismHud:
 * High-precision, cinema-grade floating HUD notification pills.
 * Replaces system gray Toasts with sleek animated obsidian-emerald/cyan
 * floating status pills that seamlessly integrate with the studio aesthetic.
 */
public final class PrismHud {

    private PrismHud() {}

    private static int dp(Context context, int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, context.getResources().getDisplayMetrics());
    }

    public static void showSuccess(Activity activity, String message) {
        show(activity, "● " + message, ThemeStyles.COLOR_EMERALD_NEON, Color.parseColor("#062E22"), Color.parseColor("#059669"));
    }

    public static void showInfo(Activity activity, String message) {
        show(activity, "ℹ " + message, ThemeStyles.COLOR_CYAN_NEON, Color.parseColor("#0C2A4A"), Color.parseColor("#0284C7"));
    }

    public static void showWarning(Activity activity, String message) {
        show(activity, "▲ " + message, ThemeStyles.COLOR_AMBER_SUNSET, Color.parseColor("#382305"), Color.parseColor("#D97706"));
    }

    public static void showError(Activity activity, String message) {
        show(activity, "✖ " + message, Color.parseColor("#FCA5A5"), Color.parseColor("#3B1219"), Color.parseColor("#EF4444"));
    }

    private static void show(Activity activity, String message, int textColor, int bgColor, int strokeColor) {
        if (activity == null || activity.isFinishing()) return;

        activity.runOnUiThread(() -> {
            ViewGroup decor = (ViewGroup) activity.getWindow().getDecorView();
            FrameLayout hudContainer = null;
            for (int i = 0; i < decor.getChildCount(); i++) {
                View child = decor.getChildAt(i);
                if ("prism_hud_container".equals(child.getTag())) {
                    hudContainer = (FrameLayout) child;
                    break;
                }
            }

            if (hudContainer == null) {
                hudContainer = new FrameLayout(activity);
                hudContainer.setTag("prism_hud_container");
                hudContainer.setLayoutParams(new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT));
                hudContainer.setClickable(false);
                decor.addView(hudContainer);
            }

            final LinearLayout pill = new LinearLayout(activity);
            pill.setOrientation(LinearLayout.HORIZONTAL);
            pill.setGravity(Gravity.CENTER);
            int padH = dp(activity, 20);
            int padV = dp(activity, 12);
            pill.setPadding(padH, padV, padH, padV);

            GradientDrawable bg = new GradientDrawable();
            bg.setColor(bgColor);
            bg.setStroke(dp(activity, 1), strokeColor);
            bg.setCornerRadius(dp(activity, 24));
            pill.setBackground(bg);

            TextView tv = new TextView(activity);
            tv.setText(message);
            tv.setTextColor(textColor);
            tv.setTextSize(TypedValue.COMPLEX_UNIT_SP, 12);
            tv.setTypeface(Typeface.create("sans-serif-medium", Typeface.BOLD));
            pill.addView(tv);

            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams(
                    ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT);
            lp.gravity = Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL;
            lp.setMargins(0, 0, 0, dp(activity, 70));
            pill.setLayoutParams(lp);

            pill.setAlpha(0.0f);
            pill.setTranslationY(dp(activity, 20));
            final FrameLayout finalContainer = hudContainer;
            finalContainer.addView(pill);

            pill.animate()
                    .alpha(1.0f)
                    .translationY(0)
                    .setDuration(220)
                    .start();

            new Handler(Looper.getMainLooper()).postDelayed(() -> {
                pill.animate()
                        .alpha(0.0f)
                        .translationY(-dp(activity, 15))
                        .setDuration(220)
                        .withEndAction(() -> finalContainer.removeView(pill))
                        .start();
            }, 2600);
        });
    }
}
