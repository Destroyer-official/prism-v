package com.prismv.worker;

import android.app.job.JobInfo;
import android.app.job.JobParameters;
import android.app.job.JobScheduler;
import android.app.job.JobService;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.os.Build;
import android.util.Log;

import com.prismv.scheduler.PrismScheduler;
import com.prismv.storage.PrismWorkJournal;

import java.util.List;

/**
 * Android 15/16 Resilient Execution Handoff JobService.
 *
 * When PrismMediaProcessingService reaches the 6-hour execution window timeout
 * (Android 15+ foregroundServiceType="mediaProcessing" quota), this JobService
 * is scheduled via JobScheduler with charging and battery constraints to resume
 * interrupted atomic jobs from PrismWorkJournal without data loss.
 */
public class PrismJobService extends JobService {
    private static final String TAG = "PrismJobService";
    public static final int JOB_ID_RESUME = 8801;

    private Thread workerThread;

    /**
     * Schedules execution handoff via Android JobScheduler.
     */
    public static void scheduleResumeJob(Context context) {
        try {
            JobScheduler js = (JobScheduler) context.getSystemService(Context.JOB_SCHEDULER_SERVICE);
            if (js == null) {
                Log.w(TAG, "JobScheduler unavailable on this device.");
                return;
            }

            ComponentName componentName = new ComponentName(context, PrismJobService.class);
            JobInfo.Builder builder = new JobInfo.Builder(JOB_ID_RESUME, componentName);

            boolean requireCharging = PrismScheduler.isRequireCharging(context);
            builder.setRequiresCharging(requireCharging);
            builder.setRequiredNetworkType(JobInfo.NETWORK_TYPE_NONE);

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                builder.setRequiresBatteryNotLow(true);
                builder.setRequiresStorageNotLow(true);
            }

            // Persist across reboots if permission granted
            try {
                builder.setPersisted(true);
            } catch (Exception ignored) {}

            // Set soft deadline of 30 minutes
            builder.setOverrideDeadline(30 * 60 * 1000L);

            int result = js.schedule(builder.build());
            Log.i(TAG, "[JobScheduler] Scheduled handoff job " + JOB_ID_RESUME + " (result=" + result + ")");
        } catch (Exception e) {
            Log.e(TAG, "[JobScheduler] Failed to schedule resume job: " + e.getMessage(), e);
        }
    }

    @Override
    public boolean onStartJob(final JobParameters params) {
        Log.i(TAG, "[JobScheduler] onStartJob triggered for job ID: " + params.getJobId());

        workerThread = new Thread(() -> {
            boolean reschedule = false;
            try {
                PrismWorkJournal journal = PrismWorkJournal.get(getApplicationContext());
                List<PrismWorkJournal.JournalEntry> interrupted = journal.getInterruptedJobs();

                Log.i(TAG, "[JobScheduler] Found " + interrupted.size() + " interrupted jobs in WorkJournal.");

                if (!interrupted.isEmpty()) {
                    // Check if conditions permit running right now
                    PrismScheduler.ScheduleStatus status = PrismScheduler.evaluateStatus(getApplicationContext());
                    if (status.canRunNow) {
                        Log.i(TAG, "[JobScheduler] Environment ready (" + status.statusMessage + "). Resuming foreground service.");
                        Intent serviceIntent = new Intent(getApplicationContext(), PrismMediaProcessingService.class);
                        serviceIntent.setAction(PrismMediaProcessingService.ACTION_START);
                        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                            try {
                                getApplicationContext().startForegroundService(serviceIntent);
                            } catch (Exception e) {
                                Log.w(TAG, "Cannot startForegroundService from background job: " + e.getMessage());
                                reschedule = true;
                            }
                        } else {
                            getApplicationContext().startService(serviceIntent);
                        }
                    } else {
                        Log.i(TAG, "[JobScheduler] Conditions not met (" + status.statusMessage + "). Will reschedule.");
                        reschedule = true;
                    }
                }
            } catch (Exception e) {
                Log.e(TAG, "[JobScheduler] Error processing interrupted jobs: " + e.getMessage(), e);
                reschedule = true;
            } finally {
                jobFinished(params, reschedule);
            }
        });

        workerThread.start();
        return true; // Work running asynchronously
    }

    @Override
    public boolean onStopJob(JobParameters params) {
        Log.w(TAG, "[JobScheduler] onStopJob triggered. Work interrupted; requesting reschedule.");
        if (workerThread != null && workerThread.isAlive()) {
            workerThread.interrupt();
        }
        return true; // Reschedule when constraints are met again
    }
}
