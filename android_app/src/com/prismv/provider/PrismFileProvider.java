package com.prismv.provider;

import android.content.ContentProvider;
import android.content.ContentValues;
import android.database.Cursor;
import android.database.MatrixCursor;
import android.net.Uri;
import android.os.Environment;
import android.os.ParcelFileDescriptor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Clean-room ContentProvider for sharing videos and compressed containers
 * with external apps (Samsung Gallery, VLC, WhatsApp, Google Photos, etc.)
 */
public class PrismFileProvider extends ContentProvider {
    public static final String AUTHORITY = "com.prismv.provider";

    @Override
    public boolean onCreate() {
        return true;
    }

    public static Uri getUriForFile(File file) {
        return Uri.parse("content://" + AUTHORITY + "/" + file.getName());
    }

    public static Uri getUriForFile(android.content.Context context, String authority, File file) {
        return getUriForFile(file);
    }

    private File resolveFile(String fileName) {
        if (fileName == null || getContext() == null) return null;

        // 1. Check public Movies/Prism-V/
        File pubFile = new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_MOVIES), "Prism-V/" + fileName);
        if (pubFile.exists()) return pubFile;

        // 2. Check app-specific Movies/Prism-V/
        File appMovieFile = new File(getContext().getExternalFilesDir(Environment.DIRECTORY_MOVIES), "Prism-V/" + fileName);
        if (appMovieFile.exists()) return appMovieFile;

        // 3. Check external files dir
        File extFile = new File(getContext().getExternalFilesDir(null), fileName);
        if (extFile.exists()) return extFile;

        // 4. Check cache dir
        File cacheFile = new File(getContext().getCacheDir(), fileName);
        if (cacheFile.exists()) return cacheFile;

        return pubFile; // default
    }

    @Override
    public ParcelFileDescriptor openFile(Uri uri, String mode) throws FileNotFoundException {
        if (getContext() == null) throw new FileNotFoundException("Context is null");
        String fileName = uri.getLastPathSegment();
        if (fileName == null) throw new FileNotFoundException("Invalid URI path");

        File file = resolveFile(fileName);
        if (file == null || !file.exists()) {
            throw new FileNotFoundException("File not found: " + fileName);
        }

        return ParcelFileDescriptor.open(file, ParcelFileDescriptor.MODE_READ_ONLY);
    }

    @Override
    public String getType(Uri uri) {
        String path = uri.getPath();
        if (path != null) {
            if (path.endsWith(".mp4")) return "video/mp4";
            if (path.endsWith(".mkv")) return "video/x-matroska";
            if (path.endsWith(".mov")) return "video/quicktime";
        }
        return "application/octet-stream";
    }

    @Override
    public Cursor query(Uri uri, String[] projection, String selection, String[] selectionArgs, String sortOrder) {
        String fileName = uri.getLastPathSegment();
        File file = resolveFile(fileName);

        String[] cols = projection != null ? projection : new String[]{OpenableColumns.DISPLAY_NAME, OpenableColumns.SIZE};
        MatrixCursor cursor = new MatrixCursor(cols);
        MatrixCursor.RowBuilder row = cursor.newRow();
        for (String col : cols) {
            if (OpenableColumns.DISPLAY_NAME.equals(col)) {
                row.add(file != null ? file.getName() : fileName);
            } else if (OpenableColumns.SIZE.equals(col)) {
                row.add(file != null ? file.length() : 0L);
            } else {
                row.add(null);
            }
        }
        return cursor;
    }

    @Override
    public Uri insert(Uri uri, ContentValues values) { return null; }
    @Override
    public int delete(Uri uri, String selection, String[] selectionArgs) { return 0; }
    @Override
    public int update(Uri uri, ContentValues values, String selection, String[] selectionArgs) { return 0; }
}
