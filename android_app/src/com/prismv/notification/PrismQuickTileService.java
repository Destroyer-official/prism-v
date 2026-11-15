package com.prismv.notification;

import android.content.Intent;
import android.os.Build;
import android.service.quicksettings.Tile;
import android.service.quicksettings.TileService;
import android.util.Log;

import com.prismv.ui.MainActivity;
import com.prismv.ui.PrismPreferences;

/**
 * Android 16 Quick Settings Live Tile for Prism-V.
 *
 * Appears in the Android Notification / Quick Settings shade.
 * Displays real-time storage reclamation status and triggers 1-tap Smart Clean.
 */
public class PrismQuickTileService extends TileService {
    private static final String TAG = "PrismQuickTile";

    @Override
    public void onStartListening() {
        super.onStartListening();
        updateTileState();
    }

    @Override
    public void onClick() {
        super.onClick();
        try {
            Intent intent = new Intent(this, MainActivity.class);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_SINGLE_TOP);
            intent.putExtra("TAB_TARGET", 0);
            startActivityAndCollapse(intent);
        } catch (Exception e) {
            Log.e(TAG, "Failed to launch from Quick Tile: " + e.getMessage());
        }
    }

    private void updateTileState() {
        try {
            Tile tile = getQsTile();
            if (tile == null) return;

            tile.setState(Tile.STATE_ACTIVE);
            tile.setLabel("Prism-V Clean");
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                tile.setSubtitle("Ready to Optimize");
            }
            tile.updateTile();
        } catch (Exception ignored) {}
    }
}
