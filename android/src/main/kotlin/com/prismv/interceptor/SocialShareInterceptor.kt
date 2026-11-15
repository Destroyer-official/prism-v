package com.prismv.interceptor

import android.content.Context
import android.content.Intent
import android.net.Uri
import android.util.Log
import java.io.File

/**
 * Social Export Share-Sheet Pre-Filter.
 *
 * Solves Red-Team Failure Mode #10:
 * Social media platforms (WhatsApp, Instagram, TikTok) strip non-standard auxiliary tracks
 * ('prmr') during backend transcoding, losing the enhancement layer.
 *
 * This interceptor composites Track 1 (HW Base) and Track 2 (Micro-Residual) in RAM
 * with a calibrated 5x5 debanding dither shader and emits a standard single-track MP4.
 */
class SocialShareInterceptor(private val context: Context) {

    companion object {
        private const val TAG = "SocialShareInterceptor"
        private val SOCIAL_PACKAGES = setOf(
            "com.whatsapp",
            "com.instagram.android",
            "com.zhiliaoapp.musically", // TikTok
            "com.google.android.youtube"
        )
    }

    /**
     * Check if target intent points to a known social media app that strips auxiliary tracks.
     */
    fun isSocialMediaTarget(intent: Intent): Boolean {
        val targetPackage = intent.`package` ?: intent.component?.packageName
        return targetPackage != null && SOCIAL_PACKAGES.contains(targetPackage)
    }

    /**
     * Prepare optimized single-track social media export from a dual-track Prism MP4.
     */
    fun prepareSocialExport(sourcePrismMp4: File): File {
        Log.i(TAG, "Compositing Track 1 + Track 2 for social media export: ${sourcePrismMp4.name}")

        val exportFile = File(context.cacheDir, "social_export_${System.currentTimeMillis()}.mp4")
        val success = nativeCompositeSocialExport(sourcePrismMp4.absolutePath, exportFile.absolutePath)

        return if (success && exportFile.exists() && exportFile.length() > 0) {
            Log.i(TAG, "Social export composite ready: ${exportFile.length()} bytes")
            exportFile
        } else {
            Log.w(TAG, "Social composite failed, falling back to original")
            sourcePrismMp4
        }
    }

    /**
     * Launch clean Android share intent with the composite single-track video.
     */
    fun shareToSocial(file: File, targetPackage: String? = null) {
        val uri = androidx.core.content.FileProvider.getUriForFile(
            context,
            "${context.packageName}.fileprovider",
            file
        )

        val shareIntent = Intent(Intent.ACTION_SEND).apply {
            type = "video/mp4"
            putExtra(Intent.EXTRA_STREAM, uri)
            addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION)
            if (targetPackage != null) {
                setPackage(targetPackage)
            }
        }

        context.startActivity(Intent.createChooser(shareIntent, "Share Enhanced Video"))
    }

    private external fun nativeCompositeSocialExport(inputPath: String, outputPath: String): Boolean
}
