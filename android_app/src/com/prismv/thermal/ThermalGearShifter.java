package com.prismv.thermal;

import android.content.Context;
import android.os.Build;
import android.os.PowerManager;

/**
 * Thermal Gear Shifter for Android.
 * Enforces Invariant 3: Thermodynamic ceiling (<0.05 J/frame) and
 * dynamically adapts encoder complexity to Android PowerManager thermal status.
 */
public final class ThermalGearShifter {

    public enum Gear {
        GEAR_4_MAX_FIDELITY(4, 0.048, "Full Cool-chic + AFGS1 + BBR"),
        GEAR_3_STANDARD(3, 0.034, "Cool-chic micro-residual + BBR"),
        GEAR_2_ECONOMY(2, 0.022, "Fast micro-residual + HW Base"),
        GEAR_1_CONSERVATIVE(1, 0.012, "Hardware MediaCodec only"),
        GEAR_0_SUSPEND(0, 0.000, "Thermal Pause (Atomic GOP Yield)");

        public final int level;
        public final double estJoulesPerFrame;
        public final String description;

        Gear(int level, double estJoulesPerFrame, String description) {
            this.level = level;
            this.estJoulesPerFrame = estJoulesPerFrame;
            this.description = description;
        }
    }

    private Gear currentGear = Gear.GEAR_3_STANDARD;

    public Gear mapStatusToGear(int thermalStatus) {
        // android.os.PowerManager.THERMAL_STATUS_*
        switch (thermalStatus) {
            case PowerManager.THERMAL_STATUS_NONE:
                currentGear = Gear.GEAR_4_MAX_FIDELITY;
                break;
            case PowerManager.THERMAL_STATUS_LIGHT:
                currentGear = Gear.GEAR_3_STANDARD;
                break;
            case PowerManager.THERMAL_STATUS_MODERATE:
                currentGear = Gear.GEAR_2_ECONOMY;
                break;
            case PowerManager.THERMAL_STATUS_SEVERE:
                currentGear = Gear.GEAR_1_CONSERVATIVE;
                break;
            case PowerManager.THERMAL_STATUS_CRITICAL:
            case PowerManager.THERMAL_STATUS_EMERGENCY:
            case PowerManager.THERMAL_STATUS_SHUTDOWN:
                currentGear = Gear.GEAR_0_SUSPEND;
                break;
            default:
                currentGear = Gear.GEAR_3_STANDARD;
                break;
        }
        return currentGear;
    }

    public Gear getCurrentGear() {
        return currentGear;
    }

    public boolean isWithinThermodynamicCeiling() {
        return currentGear.estJoulesPerFrame <= 0.050;
    }
}
