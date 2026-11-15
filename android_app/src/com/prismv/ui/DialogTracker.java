package com.prismv.ui;

import android.app.Dialog;
import android.util.Log;
import java.util.ArrayList;
import java.util.List;

/**
 * DialogTracker:
 * Centralized active dialog manager for Prism-V.
 * Tracks all modal dialogs, sheets, and popups to enable seamless hierarchical back navigation.
 */
public final class DialogTracker {
    private static final String TAG = "DialogTracker";
    private static final List<Dialog> activeDialogs = new ArrayList<>();

    private DialogTracker() {}

    public static synchronized void register(Dialog dialog) {
        if (dialog == null) return;
        activeDialogs.remove(dialog);
        activeDialogs.add(dialog);
        Log.d(TAG, "[DIALOG] Registered dialog: " + dialog + " (active count: " + activeDialogs.size() + ")");
    }

    public static synchronized void unregister(Dialog dialog) {
        if (dialog == null) return;
        activeDialogs.remove(dialog);
        Log.d(TAG, "[DIALOG] Unregistered dialog: " + dialog + " (remaining count: " + activeDialogs.size() + ")");
    }

    public static synchronized boolean dismissTopDialog() {
        Log.d(TAG, "[DIALOG] dismissTopDialog called, active count=" + activeDialogs.size());
        for (int i = activeDialogs.size() - 1; i >= 0; i--) {
            Dialog d = activeDialogs.get(i);
            if (d != null && d.isShowing()) {
                try {
                    Log.d(TAG, "[DIALOG] Dismissing top dialog: " + d);
                    activeDialogs.remove(i);
                    d.dismiss();
                    return true;
                } catch (Exception e) {
                    Log.w(TAG, "[DIALOG] Failed to dismiss dialog: " + e.getMessage());
                }
            } else {
                activeDialogs.remove(i);
            }
        }
        return false;
    }

    public static synchronized boolean hasActiveDialog() {
        for (int i = activeDialogs.size() - 1; i >= 0; i--) {
            Dialog d = activeDialogs.get(i);
            if (d != null && d.isShowing()) {
                return true;
            } else if (d == null || !d.isShowing()) {
                activeDialogs.remove(i);
            }
        }
        return false;
    }
}
