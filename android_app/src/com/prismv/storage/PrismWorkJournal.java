package com.prismv.storage;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Log;

import com.prismv.logging.PrismLogger;

import java.util.ArrayList;
import java.util.List;

/**
 * 100% Production-Grade SQLite WorkJournal for Prism-V.
 *
 * Implements:
 * 1. GOP & PTS Checkpoint Persistence to recover from crashes, low memory kills, or phone reboots.
 * 2. Android 15 FGS timeout checkpoint handoff.
 * 3. Atomic journal tracking of batch compression state.
 */
public final class PrismWorkJournal extends SQLiteOpenHelper {
    private static final String TAG = "PrismV_WorkJournal";
    private static final String DB_NAME = "prism_work_journal.db";
    private static final int DB_VERSION = 1;

    public static final String TABLE_JOBS = "work_journal";
    public static final String COL_JOB_ID = "job_id";
    public static final String COL_INPUT_PATH = "input_path";
    public static final String COL_OUTPUT_PATH = "output_path";
    public static final String COL_PTS_US = "pts_checkpoint_us";
    public static final String COL_GOP_INDEX = "gop_index";
    public static final String COL_PERCENT = "percent";
    public static final String COL_STATUS = "status";
    public static final String COL_UPDATED_MS = "last_updated_ms";
    public static final String COL_ERROR = "error_message";

    public static final String STATUS_PENDING = "PENDING";
    public static final String STATUS_RUNNING = "RUNNING";
    public static final String STATUS_COMPLETED = "COMPLETED";
    public static final String STATUS_TIMEOUT = "TIMEOUT";
    public static final String STATUS_FAILED = "FAILED";

    private static PrismWorkJournal instance;

    public static class JournalEntry {
        public String jobId;
        public String inputPath;
        public String outputPath;
        public long ptsCheckpointUs;
        public int gopIndex;
        public int percent;
        public String status;
        public long lastUpdatedMs;
        public String errorMessage;
    }

    public static synchronized PrismWorkJournal get(Context context) {
        if (instance == null) {
            instance = new PrismWorkJournal(context.getApplicationContext());
        }
        return instance;
    }

    private PrismWorkJournal(Context context) {
        super(context, DB_NAME, null, DB_VERSION);
    }

    @Override
    public void onCreate(SQLiteDatabase db) {
        String createTable = "CREATE TABLE " + TABLE_JOBS + " (" +
                COL_JOB_ID + " TEXT PRIMARY KEY, " +
                COL_INPUT_PATH + " TEXT, " +
                COL_OUTPUT_PATH + " TEXT, " +
                COL_PTS_US + " INTEGER DEFAULT 0, " +
                COL_GOP_INDEX + " INTEGER DEFAULT 0, " +
                COL_PERCENT + " INTEGER DEFAULT 0, " +
                COL_STATUS + " TEXT, " +
                COL_UPDATED_MS + " INTEGER, " +
                COL_ERROR + " TEXT" +
                ")";
        db.execSQL(createTable);
        PrismLogger.i(TAG, "Created SQLite work_journal database table.");
    }

    @Override
    public void onUpgrade(SQLiteDatabase db, int oldVersion, int newVersion) {
        PrismLogger.w(TAG, "Upgrading work_journal DB from v" + oldVersion + " to v" + newVersion);
        db.execSQL("DROP TABLE IF EXISTS " + TABLE_JOBS);
        onCreate(db);
    }

    public synchronized void recordJobStart(String jobId, String inputPath, String outputPath) {
        try {
            SQLiteDatabase db = getWritableDatabase();
            ContentValues cv = new ContentValues();
            cv.put(COL_JOB_ID, jobId);
            cv.put(COL_INPUT_PATH, inputPath);
            cv.put(COL_OUTPUT_PATH, outputPath);
            cv.put(COL_PTS_US, 0);
            cv.put(COL_GOP_INDEX, 0);
            cv.put(COL_PERCENT, 0);
            cv.put(COL_STATUS, STATUS_RUNNING);
            cv.put(COL_UPDATED_MS, System.currentTimeMillis());
            db.insertWithOnConflict(TABLE_JOBS, null, cv, SQLiteDatabase.CONFLICT_REPLACE);
            PrismLogger.i(TAG, "recordJobStart: jobId=" + jobId + " in=" + inputPath + " out=" + outputPath);
        } catch (Exception e) {
            PrismLogger.e(TAG, "recordJobStart failed: " + e.getMessage(), e);
        }
    }

    public synchronized void updateCheckpoint(String jobId, long ptsUs, int gopIndex, int percent) {
        try {
            SQLiteDatabase db = getWritableDatabase();
            ContentValues cv = new ContentValues();
            cv.put(COL_PTS_US, ptsUs);
            cv.put(COL_GOP_INDEX, gopIndex);
            cv.put(COL_PERCENT, percent);
            cv.put(COL_STATUS, STATUS_RUNNING);
            cv.put(COL_UPDATED_MS, System.currentTimeMillis());
            db.update(TABLE_JOBS, cv, COL_JOB_ID + "=?", new String[]{jobId});
            PrismLogger.v(TAG, "updateCheckpoint: jobId=" + jobId + " ptsUs=" + ptsUs + " gop=" + gopIndex + " pct=" + percent + "%");
        } catch (Exception e) {
            PrismLogger.e(TAG, "updateCheckpoint failed: " + e.getMessage(), e);
        }
    }

    public synchronized void recordJobCompleted(String jobId) {
        try {
            SQLiteDatabase db = getWritableDatabase();
            ContentValues cv = new ContentValues();
            cv.put(COL_PERCENT, 100);
            cv.put(COL_STATUS, STATUS_COMPLETED);
            cv.put(COL_UPDATED_MS, System.currentTimeMillis());
            db.update(TABLE_JOBS, cv, COL_JOB_ID + "=?", new String[]{jobId});
            PrismLogger.i(TAG, "recordJobCompleted: jobId=" + jobId);
        } catch (Exception e) {
            PrismLogger.e(TAG, "recordJobCompleted failed: " + e.getMessage(), e);
        }
    }

    public synchronized void recordJobTimeout(String jobId) {
        try {
            SQLiteDatabase db = getWritableDatabase();
            ContentValues cv = new ContentValues();
            cv.put(COL_STATUS, STATUS_TIMEOUT);
            cv.put(COL_UPDATED_MS, System.currentTimeMillis());
            db.update(TABLE_JOBS, cv, COL_JOB_ID + "=?", new String[]{jobId});
            PrismLogger.w(TAG, "recordJobTimeout: jobId=" + jobId);
        } catch (Exception e) {
            PrismLogger.e(TAG, "recordJobTimeout failed: " + e.getMessage(), e);
        }
    }

    public synchronized void recordJobFailed(String jobId, String errorMessage) {
        try {
            SQLiteDatabase db = getWritableDatabase();
            ContentValues cv = new ContentValues();
            cv.put(COL_STATUS, STATUS_FAILED);
            cv.put(COL_ERROR, errorMessage);
            cv.put(COL_UPDATED_MS, System.currentTimeMillis());
            db.update(TABLE_JOBS, cv, COL_JOB_ID + "=?", new String[]{jobId});
            PrismLogger.e(TAG, "recordJobFailed: jobId=" + jobId + " error=" + errorMessage);
        } catch (Exception e) {
            PrismLogger.e(TAG, "recordJobFailed failed: " + e.getMessage(), e);
        }
    }

    public synchronized List<JournalEntry> getRecentJobs(int limit) {
        List<JournalEntry> list = new ArrayList<>();
        try {
            SQLiteDatabase db = getReadableDatabase();
            Cursor c = db.query(TABLE_JOBS, null, null, null, null, null,
                    COL_UPDATED_MS + " DESC", limit > 0 ? String.valueOf(limit) : null);
            if (c != null) {
                while (c.moveToNext()) {
                    JournalEntry e = new JournalEntry();
                    e.jobId = c.getString(c.getColumnIndexOrThrow(COL_JOB_ID));
                    e.inputPath = c.getString(c.getColumnIndexOrThrow(COL_INPUT_PATH));
                    e.outputPath = c.getString(c.getColumnIndexOrThrow(COL_OUTPUT_PATH));
                    e.ptsCheckpointUs = c.getLong(c.getColumnIndexOrThrow(COL_PTS_US));
                    e.gopIndex = c.getInt(c.getColumnIndexOrThrow(COL_GOP_INDEX));
                    e.percent = c.getInt(c.getColumnIndexOrThrow(COL_PERCENT));
                    e.status = c.getString(c.getColumnIndexOrThrow(COL_STATUS));
                    e.lastUpdatedMs = c.getLong(c.getColumnIndexOrThrow(COL_UPDATED_MS));
                    int errIdx = c.getColumnIndex(COL_ERROR);
                    e.errorMessage = errIdx >= 0 ? c.getString(errIdx) : null;
                    list.add(e);
                }
                c.close();
            }
        } catch (Exception e) {
            PrismLogger.e(TAG, "getRecentJobs failed: " + e.getMessage(), e);
        }
        return list;
    }

    public synchronized List<JournalEntry> getInterruptedJobs() {
        List<JournalEntry> list = new ArrayList<>();
        try {
            SQLiteDatabase db = getReadableDatabase();
            Cursor c = db.query(TABLE_JOBS, null,
                    COL_STATUS + " IN (?, ?)",
                    new String[]{STATUS_RUNNING, STATUS_TIMEOUT},
                    null, null, COL_UPDATED_MS + " DESC");

            if (c != null) {
                while (c.moveToNext()) {
                    JournalEntry e = new JournalEntry();
                    e.jobId = c.getString(c.getColumnIndexOrThrow(COL_JOB_ID));
                    e.inputPath = c.getString(c.getColumnIndexOrThrow(COL_INPUT_PATH));
                    e.outputPath = c.getString(c.getColumnIndexOrThrow(COL_OUTPUT_PATH));
                    e.ptsCheckpointUs = c.getLong(c.getColumnIndexOrThrow(COL_PTS_US));
                    e.gopIndex = c.getInt(c.getColumnIndexOrThrow(COL_GOP_INDEX));
                    e.percent = c.getInt(c.getColumnIndexOrThrow(COL_PERCENT));
                    e.status = c.getString(c.getColumnIndexOrThrow(COL_STATUS));
                    e.lastUpdatedMs = c.getLong(c.getColumnIndexOrThrow(COL_UPDATED_MS));
                    int errIdx = c.getColumnIndex(COL_ERROR);
                    e.errorMessage = errIdx >= 0 ? c.getString(errIdx) : null;
                    list.add(e);
                }
                c.close();
            }
            PrismLogger.d(TAG, "getInterruptedJobs returned " + list.size() + " entries.");
        } catch (Exception e) {
            PrismLogger.e(TAG, "getInterruptedJobs failed: " + e.getMessage(), e);
        }
        return list;
    }

    public synchronized void purgeCompletedJobs() {
        try {
            SQLiteDatabase db = getWritableDatabase();
            int deleted = db.delete(TABLE_JOBS, COL_STATUS + "=?", new String[]{STATUS_COMPLETED});
            PrismLogger.i(TAG, "purgeCompletedJobs removed " + deleted + " completed rows.");
        } catch (Exception e) {
            PrismLogger.e(TAG, "purgeCompletedJobs failed: " + e.getMessage(), e);
        }
    }
}
