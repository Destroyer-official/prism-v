package com.prismv.transaction

import android.content.ContentResolver
import android.content.ContentValues
import android.content.Context
import android.net.Uri
import android.os.Build
import android.provider.MediaStore
import android.util.Log
import java.io.File
import java.io.FileInputStream
import java.io.FileOutputStream
import java.security.MessageDigest

/**
 * 5-Stage Verified Replacement Transaction Protocol for Android MediaStore.
 *
 * Invariant 6: Zero Media Loss Guarantee.
 * Destructive in-place overwrites on original media are strictly prohibited.
 */
class VerifiedReplacementEngine(private val context: Context) {

    companion object {
        private const val TAG = "VerifiedReplacement"
    }

    enum class State {
        CANDIDATE,
        LOCAL_VERIFY,
        COMMIT,
        PUBLISH,
        GRACE,
        FAILED
    }

    data class TransactionResult(
        val success: Boolean,
        val publishedUri: Uri?,
        val trashedOriginalFile: File?,
        val failureReason: String?
    )

    fun executeTransaction(
        originalMediaFile: File,
        compressionTask: (input: File, tempOutput: File) -> Boolean,
        verificationTask: (tempOutput: File) -> Boolean
    ): TransactionResult {
        var state = State.CANDIDATE

        // Stage 1: CANDIDATE
        if (!originalMediaFile.exists() || !originalMediaFile.canRead()) {
            return TransactionResult(false, null, null, "Original file unreadable: ${originalMediaFile.name}")
        }

        val tempFile = File(context.cacheDir, "prism_stage_${System.currentTimeMillis()}.mp4")
        if (tempFile.exists()) tempFile.delete()

        Log.d(TAG, "Stage 1: CANDIDATE -> Staging temp target: ${tempFile.absolutePath}")

        val compressedOk = try {
            compressionTask(originalMediaFile, tempFile)
        } catch (e: Exception) {
            Log.e(TAG, "Compression task failed", e)
            false
        }

        if (!compressedOk || !tempFile.exists() || tempFile.length() == 0L) {
            tempFile.delete()
            return TransactionResult(false, null, null, "Compression failed to generate valid output")
        }

        // Stage 2: LOCAL_VERIFY
        state = State.LOCAL_VERIFY
        Log.d(TAG, "Stage 2: LOCAL_VERIFY -> Performing independent verification...")

        val verifiedOk = try {
            verificationTask(tempFile)
        } catch (e: Exception) {
            Log.e(TAG, "Verification task exception", e)
            false
        }

        if (!verifiedOk) {
            tempFile.delete()
            state = State.FAILED
            Log.e(TAG, "Verification failed! Rolled back; original media preserved.")
            return TransactionResult(false, null, null, "Independent verification check failed")
        }

        // Stage 3: COMMIT
        state = State.COMMIT
        Log.d(TAG, "Stage 3: COMMIT -> Staged for atomic MediaStore publication")

        // Stage 4: PUBLISH (MediaStore IS_PENDING = 1 -> copy -> IS_PENDING = 0)
        state = State.PUBLISH
        val resolver: ContentResolver = context.contentResolver
        val contentValues = ContentValues().apply {
            put(MediaStore.Video.Media.DISPLAY_NAME, "Prism_${originalMediaFile.name}")
            put(MediaStore.Video.Media.MIME_TYPE, "video/mp4")
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                put(MediaStore.Video.Media.IS_PENDING, 1)
            }
        }

        val videoUri = resolver.insert(MediaStore.Video.Media.EXTERNAL_CONTENT_URI, contentValues)
        if (videoUri == null) {
            tempFile.delete()
            state = State.FAILED
            return TransactionResult(false, null, null, "Failed to allocate MediaStore entry")
        }

        try {
            resolver.openOutputStream(videoUri)?.use { out ->
                FileInputStream(tempFile).use { input ->
                    input.copyTo(out)
                }
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
                val publishValues = ContentValues().apply {
                    put(MediaStore.Video.Media.IS_PENDING, 0)
                }
                resolver.update(videoUri, publishValues, null, null)
            }
            Log.d(TAG, "Stage 4: PUBLISH -> Atomically published to MediaStore: $videoUri")
        } catch (e: Exception) {
            resolver.delete(videoUri, null, null)
            tempFile.delete()
            state = State.FAILED
            return TransactionResult(false, null, null, "Failed to copy payload to MediaStore: ${e.message}")
        }

        // Clean up temp file
        tempFile.delete()

        // Stage 5: GRACE (Move original to 7-day Prism Vault Trash; NEVER delete automatically)
        state = State.GRACE
        val trashVaultDir = File(context.filesDir, "prism_vault_trash").apply { mkdirs() }
        val trashedDest = File(trashVaultDir, "${System.currentTimeMillis()}_${originalMediaFile.name}")

        val moved = originalMediaFile.renameTo(trashedDest)
        val finalTrashedFile = if (moved) trashedDest else originalMediaFile

        Log.d(TAG, "Stage 5: GRACE -> Original file retained in trash vault: ${finalTrashedFile.absolutePath}")

        return TransactionResult(
            success = true,
            publishedUri = videoUri,
            trashedOriginalFile = finalTrashedFile,
            failureReason = null
        )
    }
}
