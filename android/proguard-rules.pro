# Prism-V Proguard & R8 Shrinking Rules

# Keep native JNI bridge methods
-keepclasseswithmembernames class * {
    native <methods>;
}

# Keep Prism-V data models and callbacks
-keep class com.prismv.** { *; }

# WorkManager Workers
-keep class * extends androidx.work.Worker { *; }
-keep class * extends androidx.work.CoroutineWorker { *; }

# Jetpack Compose
-keepclassmembers class * {
    @androidx.compose.runtime.Composable *;
}
