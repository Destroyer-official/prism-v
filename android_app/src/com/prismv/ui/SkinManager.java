package com.prismv.ui;

import android.graphics.Color;

/**
 * SkinManager:
 * Central Studio Design System & Hardware-Matched Theme Manager for Prism-V.
 * Provides 6 professional studio skins inspired by DaVinci Resolve, Teenage Engineering,
 * Kodak 35mm Celluloid, Material 3 Expressive, and Sunlight Field Monitoring.
 */
public final class SkinManager {

    public enum Skin {
        LIQUID_GLASS(
                "Liquid Frost Glass",
                "STUDIO DEFAULT",
                "2027 Flagship titanium depth, refractive glass cards & neon cyan telemetry.",
                Color.parseColor("#040711"), // Canvas
                Color.parseColor("#0C1527"), // Surface
                Color.parseColor("#0284C7"), // Primary
                Color.parseColor("#00E5FF"), // Accent
                false
        ),
        AMOLED_BLACK(
                "Pitch-Black AMOLED Cyber",
                "42% OLED POWER SAVE",
                "Pure 0% black OLED baseline with cyber mint telemetry. Zero light bleed for colorists.",
                Color.parseColor("#000000"),
                Color.parseColor("#121212"),
                Color.parseColor("#059669"),
                Color.parseColor("#00FF9D"),
                false
        ),
        NEO_BRUTALIST(
                "Tactile Neo-Brutalist",
                "TE-INDUSTRIAL",
                "Teenage Engineering industrial basalt chassis, safety orange & cobalt accents.",
                Color.parseColor("#121214"),
                Color.parseColor("#26262E"),
                Color.parseColor("#E64A19"),
                Color.parseColor("#FF5500"),
                false
        ),
        SUNSET_GOLD(
                "Hollywood Sunset Gold",
                "35MM FILM STOCK",
                "Warm analog tungsten charcoal, molten amber & sepia accents for cinematic editing.",
                Color.parseColor("#0C0A08"),
                Color.parseColor("#251E18"),
                Color.parseColor("#D97706"),
                Color.parseColor("#F59E0B"),
                false
        ),
        MONET_INDIGO(
                "Cosmic Monet Indigo",
                "M3 EXPRESSIVE",
                "Material 3 Expressive cosmic navy with radiant royal indigo & lavender luminescence.",
                Color.parseColor("#0B0F19"),
                Color.parseColor("#243047"),
                Color.parseColor("#6366F1"),
                Color.parseColor("#818CF8"),
                false
        ),
        DAYLIGHT_PAPER(
                "Daylight Paper Studio",
                "WCAG AAA 14:1 FIELD",
                "Low-blue-light alabaster paper with deep slate ink. Glare-free for outdoor film shoots.",
                Color.parseColor("#F7F4EE"),
                Color.parseColor("#DDD6C5"),
                Color.parseColor("#0369A1"),
                Color.parseColor("#0284C7"),
                true
        );

        public final String displayName;
        public final String tag;
        public final String description;
        public final int colorCanvas;
        public final int colorSurface;
        public final int colorPrimary;
        public final int colorAccent;
        public final boolean isLight;

        Skin(String displayName, String tag, String description,
             int colorCanvas, int colorSurface, int colorPrimary, int colorAccent, boolean isLight) {
            this.displayName = displayName;
            this.tag = tag;
            this.description = description;
            this.colorCanvas = colorCanvas;
            this.colorSurface = colorSurface;
            this.colorPrimary = colorPrimary;
            this.colorAccent = colorAccent;
            this.isLight = isLight;
        }

        public static Skin fromName(String name) {
            if (name == null) return LIQUID_GLASS;
            String lower = name.toLowerCase();
            if (lower.contains("amoled") || lower.contains("black") || lower.contains("pitch")) {
                return AMOLED_BLACK;
            } else if (lower.contains("brutalist") || lower.contains("tactile") || lower.contains("teenage")) {
                return NEO_BRUTALIST;
            } else if (lower.contains("sunset") || lower.contains("gold") || lower.contains("amber") || lower.contains("film")) {
                return SUNSET_GOLD;
            } else if (lower.contains("monet") || lower.contains("indigo") || lower.contains("cosmic")) {
                return MONET_INDIGO;
            } else if (lower.contains("paper") || lower.contains("daylight") || lower.contains("alabaster")) {
                return DAYLIGHT_PAPER;
            }
            return LIQUID_GLASS;
        }
    }

    private static Skin activeSkin = Skin.LIQUID_GLASS;

    public static Skin getActiveSkin() {
        return activeSkin;
    }

    public static void setActiveSkin(Skin skin) {
        if (skin != null) {
            activeSkin = skin;
            applySkinToTheme();
        }
    }

    public static void applySkinToTheme() {
        switch (activeSkin) {
            case AMOLED_BLACK:
                ThemeStyles.COLOR_BG_CANVAS = Color.parseColor("#000000");
                ThemeStyles.COLOR_BG_TOPBAR = Color.parseColor("#060606");
                ThemeStyles.COLOR_BG_SURFACE = Color.parseColor("#0A0A0A");
                ThemeStyles.COLOR_BG_CARD = Color.parseColor("#121212");
                ThemeStyles.COLOR_BG_CARD_ACTIVE = Color.parseColor("#1C1C1C");
                ThemeStyles.COLOR_BORDER_SUBTLE = Color.parseColor("#262626");
                ThemeStyles.COLOR_BORDER_ACTIVE = Color.parseColor("#00FF9D");
                ThemeStyles.COLOR_CYAN_NEON = Color.parseColor("#00FF9D");
                ThemeStyles.COLOR_CYAN_PRIMARY = Color.parseColor("#059669");
                ThemeStyles.COLOR_CYAN_TEXT = Color.parseColor("#34D399");
                ThemeStyles.COLOR_EMERALD_NEON = Color.parseColor("#00FF9D");
                ThemeStyles.COLOR_TEXT_PRIMARY = Color.WHITE;
                ThemeStyles.COLOR_TEXT_SECONDARY = Color.parseColor("#A1A1AA");
                ThemeStyles.COLOR_TEXT_MUTED = Color.parseColor("#71717A");
                ThemeStyles.IS_LIGHT_THEME = false;
                break;

            case NEO_BRUTALIST:
                ThemeStyles.COLOR_BG_CANVAS = Color.parseColor("#121214");
                ThemeStyles.COLOR_BG_TOPBAR = Color.parseColor("#18181C");
                ThemeStyles.COLOR_BG_SURFACE = Color.parseColor("#1E1E24");
                ThemeStyles.COLOR_BG_CARD = Color.parseColor("#26262E");
                ThemeStyles.COLOR_BG_CARD_ACTIVE = Color.parseColor("#34343E");
                ThemeStyles.COLOR_BORDER_SUBTLE = Color.parseColor("#444452");
                ThemeStyles.COLOR_BORDER_ACTIVE = Color.parseColor("#FF5500");
                ThemeStyles.COLOR_CYAN_NEON = Color.parseColor("#FF5500"); // Signal Orange
                ThemeStyles.COLOR_CYAN_PRIMARY = Color.parseColor("#E64A19");
                ThemeStyles.COLOR_CYAN_TEXT = Color.parseColor("#FF8A65");
                ThemeStyles.COLOR_EMERALD_NEON = Color.parseColor("#2979FF"); // Cobalt
                ThemeStyles.COLOR_TEXT_PRIMARY = Color.parseColor("#F4F4F5");
                ThemeStyles.COLOR_TEXT_SECONDARY = Color.parseColor("#B0B0C0");
                ThemeStyles.COLOR_TEXT_MUTED = Color.parseColor("#71717A");
                ThemeStyles.IS_LIGHT_THEME = false;
                break;

            case SUNSET_GOLD:
                ThemeStyles.COLOR_BG_CANVAS = Color.parseColor("#0C0A08");
                ThemeStyles.COLOR_BG_TOPBAR = Color.parseColor("#14100D");
                ThemeStyles.COLOR_BG_SURFACE = Color.parseColor("#1C1612");
                ThemeStyles.COLOR_BG_CARD = Color.parseColor("#251E18");
                ThemeStyles.COLOR_BG_CARD_ACTIVE = Color.parseColor("#342A22");
                ThemeStyles.COLOR_BORDER_SUBTLE = Color.parseColor("#4A3B2C");
                ThemeStyles.COLOR_BORDER_ACTIVE = Color.parseColor("#F59E0B");
                ThemeStyles.COLOR_CYAN_NEON = Color.parseColor("#F59E0B"); // Warm Amber
                ThemeStyles.COLOR_CYAN_PRIMARY = Color.parseColor("#D97706");
                ThemeStyles.COLOR_CYAN_TEXT = Color.parseColor("#FBBF24");
                ThemeStyles.COLOR_EMERALD_NEON = Color.parseColor("#10B981");
                ThemeStyles.COLOR_TEXT_PRIMARY = Color.parseColor("#FFFBEB");
                ThemeStyles.COLOR_TEXT_SECONDARY = Color.parseColor("#D4B996");
                ThemeStyles.COLOR_TEXT_MUTED = Color.parseColor("#8C7764");
                ThemeStyles.IS_LIGHT_THEME = false;
                break;

            case MONET_INDIGO:
                ThemeStyles.COLOR_BG_CANVAS = Color.parseColor("#0B0F19");
                ThemeStyles.COLOR_BG_TOPBAR = Color.parseColor("#111827");
                ThemeStyles.COLOR_BG_SURFACE = Color.parseColor("#1E293B");
                ThemeStyles.COLOR_BG_CARD = Color.parseColor("#243047");
                ThemeStyles.COLOR_BG_CARD_ACTIVE = Color.parseColor("#314160");
                ThemeStyles.COLOR_BORDER_SUBTLE = Color.parseColor("#3B4D6E");
                ThemeStyles.COLOR_BORDER_ACTIVE = Color.parseColor("#818CF8");
                ThemeStyles.COLOR_CYAN_NEON = Color.parseColor("#818CF8"); // Periwinkle
                ThemeStyles.COLOR_CYAN_PRIMARY = Color.parseColor("#6366F1"); // Royal Indigo
                ThemeStyles.COLOR_CYAN_TEXT = Color.parseColor("#A5B4FC");
                ThemeStyles.COLOR_EMERALD_NEON = Color.parseColor("#38BDF8");
                ThemeStyles.COLOR_TEXT_PRIMARY = Color.WHITE;
                ThemeStyles.COLOR_TEXT_SECONDARY = Color.parseColor("#94A3B8");
                ThemeStyles.COLOR_TEXT_MUTED = Color.parseColor("#64748B");
                ThemeStyles.IS_LIGHT_THEME = false;
                break;

            case DAYLIGHT_PAPER:
                ThemeStyles.COLOR_BG_CANVAS = Color.parseColor("#F7F4EE");
                ThemeStyles.COLOR_BG_TOPBAR = Color.parseColor("#EFEAE0");
                ThemeStyles.COLOR_BG_SURFACE = Color.parseColor("#E6E0D2");
                ThemeStyles.COLOR_BG_CARD = Color.parseColor("#DDD6C5");
                ThemeStyles.COLOR_BG_CARD_ACTIVE = Color.parseColor("#D0C7B4");
                ThemeStyles.COLOR_BORDER_SUBTLE = Color.parseColor("#B8AD96");
                ThemeStyles.COLOR_BORDER_ACTIVE = Color.parseColor("#0284C7");
                ThemeStyles.COLOR_CYAN_NEON = Color.parseColor("#0284C7"); // Technical Sapphire
                ThemeStyles.COLOR_CYAN_PRIMARY = Color.parseColor("#0369A1");
                ThemeStyles.COLOR_CYAN_TEXT = Color.parseColor("#0284C7");
                ThemeStyles.COLOR_EMERALD_NEON = Color.parseColor("#059669");
                ThemeStyles.COLOR_TEXT_PRIMARY = Color.parseColor("#0F172A"); // Deep Ink
                ThemeStyles.COLOR_TEXT_SECONDARY = Color.parseColor("#334155");
                ThemeStyles.COLOR_TEXT_MUTED = Color.parseColor("#64748B");
                ThemeStyles.IS_LIGHT_THEME = true;
                break;

            case LIQUID_GLASS:
            default:
                ThemeStyles.COLOR_BG_CANVAS = Color.parseColor("#040711");
                ThemeStyles.COLOR_BG_TOPBAR = Color.parseColor("#070D1B");
                ThemeStyles.COLOR_BG_SURFACE = Color.parseColor("#091020");
                ThemeStyles.COLOR_BG_CARD = Color.parseColor("#0C1527");
                ThemeStyles.COLOR_BG_CARD_ACTIVE = Color.parseColor("#0F233E");
                ThemeStyles.COLOR_BORDER_SUBTLE = Color.parseColor("#1B2942");
                ThemeStyles.COLOR_BORDER_ACTIVE = Color.parseColor("#00E5FF");
                ThemeStyles.COLOR_CYAN_NEON = Color.parseColor("#00E5FF");
                ThemeStyles.COLOR_CYAN_PRIMARY = Color.parseColor("#0284C7");
                ThemeStyles.COLOR_CYAN_TEXT = Color.parseColor("#38BDF8");
                ThemeStyles.COLOR_EMERALD_NEON = Color.parseColor("#10B981");
                ThemeStyles.COLOR_TEXT_PRIMARY = Color.WHITE;
                ThemeStyles.COLOR_TEXT_SECONDARY = Color.parseColor("#94A3B8");
                ThemeStyles.COLOR_TEXT_MUTED = Color.parseColor("#64748B");
                ThemeStyles.IS_LIGHT_THEME = false;
                break;
        }
    }
}
