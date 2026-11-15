package com.prismv.thermal

import android.content.Context
import android.os.Build
import android.os.PowerManager
import android.util.Log

/**
 * Thermal Dynamic Gear-Shifter for Prism-V on Android.
 *
 * Hooks PowerManager.OnThermalStatusChangedListener to prevent thermal saturation
 * and survive passive mobile chassis limits (3.0-3.8 W sustained).
 */
class ThermalGearShifter(context: Context, private val listener: OnGearChangeListener) {

    companion object {
        private const val TAG = "ThermalGearShifter"
    }

    enum class Gear(val value: Int) {
        GEAR_1_HIGH_QUALITY(1), // Full 3-level micro-residual
        GEAR_2_BALANCED(2),     // 2-level micro-residual, cap fps to 30
        GEAR_3_POWER_SAVER(3),  // Bypass micro-residual, pure HW MediaCodec
        GEAR_4_THERMAL_PAUSE(4) // Atomic GOP flush, sleep until cooled to <37°C
    }

    interface OnGearChangeListener {
        fun onGearChanged(newGear: Gear, thermalStatus: Int)
    }

    private val powerManager = context.getSystemService(Context.POWER_SERVICE) as? PowerManager
    var currentGear: Gear = Gear.GEAR_1_HIGH_QUALITY
        private set

    private val thermalListener = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
        PowerManager.OnThermalStatusChangedListener { status ->
            val nextGear = when (status) {
                PowerManager.THERMAL_STATUS_NONE,
                PowerManager.THERMAL_STATUS_LIGHT -> Gear.GEAR_1_HIGH_QUALITY

                PowerManager.THERMAL_STATUS_MODERATE -> Gear.GEAR_2_BALANCED

                PowerManager.THERMAL_STATUS_SEVERE -> Gear.GEAR_3_POWER_SAVER

                PowerManager.THERMAL_STATUS_CRITICAL,
                PowerManager.THERMAL_STATUS_EMERGENCY,
                PowerManager.THERMAL_STATUS_SHUTDOWN -> Gear.GEAR_4_THERMAL_PAUSE

                else -> Gear.GEAR_1_HIGH_QUALITY
            }

            if (nextGear != currentGear) {
                Log.i(TAG, "Thermal shift: $currentGear -> $nextGear (Status: $status)")
                currentGear = nextGear
                listener.onGearChanged(nextGear, status)
            }
        }
    } else null

    fun register() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && thermalListener != null) {
            powerManager?.addThermalStatusListener(thermalListener)
            Log.d(TAG, "Thermal listener registered successfully")
        }
    }

    fun unregister() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q && thermalListener != null) {
            powerManager?.removeThermalStatusListener(thermalListener)
            Log.d(TAG, "Thermal listener unregistered")
        }
    }
}
