package com.prismv.ui;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.RectF;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.util.TypedValue;

/**
 * ThemeStyles:
 * Centralized Design System for Prism-V (Material 3 Expressive & Pro Studio Suites).
 * Provides unified color tokens, glassmorphic card backgrounds, glowing borders,
 * elevation drawables, and instant dynamic theme adaptation.
 */
public final class ThemeStyles {

    // Core Canvas & Surface (Pro Studio Obsidian & Glass)
    public static int COLOR_BG_CANVAS = Color.parseColor("#060A12"); // Deepest Titanium Obsidian
    public static int COLOR_BG_TOPBAR = Color.parseColor("#080E1C"); // Top bar surface
    public static int COLOR_BG_SURFACE = Color.parseColor("#0C1425"); // Surface card
    public static int COLOR_BG_CARD = Color.parseColor("#0F1A2F"); // Interactive card background
    public static int COLOR_BG_CARD_ACTIVE = Color.parseColor("#142646"); // Active card background
    public static int COLOR_BORDER_SUBTLE = Color.parseColor("#1C2B46"); // 1px glass border
    public static int COLOR_BORDER_ACTIVE = Color.parseColor("#00E5FF"); // Neon Cyan active border

    // Accents & Signals
    public static int COLOR_CYAN_NEON = Color.parseColor("#00E5FF");
    public static int COLOR_CYAN_PRIMARY = Color.parseColor("#0284C7");
    public static int COLOR_CYAN_TEXT = Color.parseColor("#38BDF8");
    public static int COLOR_EMERALD_NEON = Color.parseColor("#10B981");
    public static int COLOR_MINT_TEXT = Color.parseColor("#34D399");
    public static int COLOR_AMBER_SUNSET = Color.parseColor("#F59E0B");
    public static int COLOR_PURPLE_VIOLET = Color.parseColor("#8B5CF6");
    public static int COLOR_RED_ALERT = Color.parseColor("#EF4444");

    // Text & Content
    public static int COLOR_TEXT_PRIMARY = Color.parseColor("#F8FAFC"); // Slate 50
    public static int COLOR_TEXT_SECONDARY = Color.parseColor("#94A3B8"); // Slate 400
    public static int COLOR_TEXT_MUTED = Color.parseColor("#64748B"); // Slate 500

    // Theme Mode
    public static boolean IS_LIGHT_THEME = false;

    public static void applyDesignSkin(String skinName) {
        if (skinName == null) return;
        SkinManager.Skin skin = SkinManager.Skin.fromName(skinName);
        SkinManager.setActiveSkin(skin);
    }

    public static void applyDesignSkin(SkinManager.Skin skin) {
        if (skin != null) {
            SkinManager.setActiveSkin(skin);
        }
    }

    private ThemeStyles() {}

    public static int dp(Context context, int value) {
        return (int) TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, context.getResources().getDisplayMetrics());
    }

    public static float dp(Context context, float value) {
        return TypedValue.applyDimension(TypedValue.COMPLEX_UNIT_DIP, value, context.getResources().getDisplayMetrics());
    }

    public static Drawable createFolderIcon(Context context, int color, int sizeDp) {
        int sizePx = Math.max(16, dp(context, sizeDp));
        Bitmap bitmap = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(color);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1.8f, dp(context, 1.8f)));
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);

        float pad = sizePx * 0.12f;
        float w = sizePx - pad * 2f;
        float h = sizePx * 0.62f;
        float l = pad;
        float t = sizePx * 0.24f;

        Path path = new Path();
        path.moveTo(l, t + dp(context, 2.5f));
        path.lineTo(l, t);
        path.lineTo(l + w * 0.40f, t);
        path.lineTo(l + w * 0.52f, t + dp(context, 2.5f));
        path.lineTo(l + w, t + dp(context, 2.5f));
        path.lineTo(l + w, t + h);
        path.lineTo(l, t + h);
        path.close();
        canvas.drawPath(path, paint);

        canvas.drawLine(l, t + dp(context, 4.5f), l + w, t + dp(context, 4.5f), paint);
        return new BitmapDrawable(context.getResources(), bitmap);
    }

    public static Drawable createVaultIcon(Context context, int color, int sizeDp) {
        int sizePx = Math.max(16, dp(context, sizeDp));
        Bitmap bitmap = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(color);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1.8f, dp(context, 1.8f)));
        paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeJoin(Paint.Join.ROUND);

        float pad = sizePx * 0.12f;
        float l = pad;
        float t = pad + sizePx * 0.04f;
        float r = sizePx - pad;
        float b = sizePx - pad;

        RectF rect = new RectF(l, t, r, b);
        canvas.drawRoundRect(rect, dp(context, 3.5f), dp(context, 3.5f), paint);

        float cx = (l + r) / 2f;
        float cy = (t + b) / 2f;
        float radius = (r - l) * 0.22f;
        canvas.drawCircle(cx, cy, radius, paint);

        canvas.drawLine(cx - radius * 1.5f, cy, cx + radius * 1.5f, cy, paint);
        canvas.drawLine(cx, cy - radius * 1.5f, cx, cy + radius * 1.5f, paint);

        Paint fillPaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        fillPaint.setColor(color);
        fillPaint.setStyle(Paint.Style.FILL);
        canvas.drawCircle(cx, cy, Math.max(1.8f, dp(context, 1.8f)), fillPaint);

        return new BitmapDrawable(context.getResources(), bitmap);
    }

    public static Drawable createRefreshIcon(Context context, int color, int sizeDp) {
        int sizePx = Math.max(16, dp(context, sizeDp));
        Bitmap bitmap = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        Paint paint = new Paint(Paint.ANTI_ALIAS_FLAG);
        paint.setColor(color);
        paint.setStyle(Paint.Style.STROKE);
        paint.setStrokeWidth(Math.max(1.8f, dp(context, 1.8f)));
        paint.setStrokeCap(Paint.Cap.ROUND);

        float cx = sizePx / 2f;
        float cy = sizePx / 2f;
        float radius = sizePx * 0.35f;

        RectF oval = new RectF(cx - radius, cy - radius, cx + radius, cy + radius);
        canvas.drawArc(oval, 35, 280, false, paint);

        float arrowX = cx + (float) (radius * Math.cos(Math.toRadians(35)));
        float arrowY = cy + (float) (radius * Math.sin(Math.toRadians(35)));
        canvas.drawLine(arrowX, arrowY, arrowX - dp(context, 4f), arrowY, paint);
        canvas.drawLine(arrowX, arrowY, arrowX, arrowY - dp(context, 4f), paint);

        return new BitmapDrawable(context.getResources(), bitmap);
    }

    public static GradientDrawable createCardDrawable(Context context, boolean active) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(active ? COLOR_BG_CARD_ACTIVE : COLOR_BG_CARD);
        int strokeColor = active ? COLOR_BORDER_ACTIVE : COLOR_BORDER_SUBTLE;
        int strokeWidth = active ? dp(context, 2) : dp(context, 1);
        d.setStroke(strokeWidth, strokeColor);
        d.setCornerRadius(dp(context, 16));
        return d;
    }

    public static GradientDrawable createPillDrawable(Context context, boolean active) {
        GradientDrawable d = new GradientDrawable();
        if (active) {
            d.setColor(COLOR_CYAN_PRIMARY);
            d.setStroke(dp(context, 1), COLOR_CYAN_NEON);
        } else {
            d.setColor(IS_LIGHT_THEME ? Color.parseColor("#E6E0D2") : Color.parseColor("#0E1626"));
            d.setStroke(dp(context, 1), COLOR_BORDER_SUBTLE);
        }
        d.setCornerRadius(dp(context, 20));
        return d;
    }

    public static GradientDrawable createSegmentTrackDrawable(Context context) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(IS_LIGHT_THEME ? Color.parseColor("#E5DFC8") : Color.parseColor("#09101D"));
        d.setStroke(dp(context, 1), COLOR_BORDER_SUBTLE);
        d.setCornerRadius(dp(context, 24));
        return d;
    }

    public static GradientDrawable createSegmentPillDrawable(Context context, boolean active) {
        GradientDrawable d = new GradientDrawable();
        if (active) {
            d.setColor(COLOR_CYAN_PRIMARY);
            d.setCornerRadius(dp(context, 20));
        } else {
            d.setColor(Color.TRANSPARENT);
            d.setCornerRadius(dp(context, 20));
        }
        return d;
    }

    public static GradientDrawable createRoundedDrawable(int bgColor, int strokeColor, int strokeWidth, float radiusPx) {
        GradientDrawable d = new GradientDrawable();
        d.setColor(bgColor);
        if (strokeWidth > 0) d.setStroke(strokeWidth, strokeColor);
        d.setCornerRadius(radiusPx);
        return d;
    }

    public static GradientDrawable createGradientButton(int startColor, int endColor, float radiusPx) {
        GradientDrawable d = new GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT, new int[]{startColor, endColor});
        d.setCornerRadius(radiusPx);
        return d;
    }

    public static void performHaptic(android.view.View view) {
        if (view == null) return;
        try {
            view.performHapticFeedback(android.view.HapticFeedbackConstants.KEYBOARD_TAP);
        } catch (Exception ignored) {}
    }

    public static void performSuccessHaptic(android.view.View view) {
        if (view == null) return;
        try {
            if (android.os.Build.VERSION.SDK_INT >= android.os.Build.VERSION_CODES.R) {
                view.performHapticFeedback(android.view.HapticFeedbackConstants.CONFIRM);
            } else {
                view.performHapticFeedback(android.view.HapticFeedbackConstants.KEYBOARD_TAP);
            }
        } catch (Exception ignored) {}
    }
}
