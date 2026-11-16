# Build the standalone Java Android APK without Gradle.
#
# Required environment variables (or pass the matching parameter):
#   JAVA_HOME          JDK with bin\javac.exe and bin\java.exe
#   PRISM_ANDROID_JAR  Android platform android.jar
#   PRISM_APKTOOL_JAR  apktool jar
#   PRISM_R8_JAR       r8lib jar (contains D8)
#   PRISM_SIGNER_JAR   uber-apk-signer jar
#
# Paths may also be placed in <repo>/tools/vendor/ with the same file names.

[CmdletBinding()]
param(
    [string]$JavaHome = $env:JAVA_HOME,
    [string]$AndroidJar = $env:PRISM_ANDROID_JAR,
    [string]$ApktoolJar = $env:PRISM_APKTOOL_JAR,
    [string]$R8Jar = $env:PRISM_R8_JAR,
    [string]$SignerJar = $env:PRISM_SIGNER_JAR
)

$ErrorActionPreference = "Stop"
$RepoRoot = Split-Path -Parent $PSScriptRoot
$Vendor = Join-Path $RepoRoot "tools\vendor"
$AppRoot = Join-Path $RepoRoot "android_app"
$SrcDir = Join-Path $AppRoot "src"
$BinDir = Join-Path $AppRoot "bin"
$DexDir = Join-Path $AppRoot "dex"
$SignedDir = Join-Path $AppRoot "signed"
$UnsignedApk = Join-Path $AppRoot "unsigned.apk"

function Find-Tool {
    param([string]$Value, [string]$FileName, [string]$EnvName)
    if ($Value -and (Test-Path $Value)) { return (Resolve-Path $Value).Path }
    $vendorPath = Join-Path $Vendor $FileName
    if (Test-Path $vendorPath) { return $vendorPath }
    throw "Missing $FileName. Set $EnvName or place the file in tools/vendor/."
}

function Invoke-Tool {
    param([string]$Exe, [string[]]$Arguments, [string]$Description)
    # PowerShell 5.1 turns native stderr into error records; run these with
    # Continue and check the exit code explicitly.
    $previous = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    & $Exe @Arguments
    $code = $LASTEXITCODE
    $ErrorActionPreference = $previous
    if ($code -ne 0) { throw "$Description failed with exit code $code" }
}

if (-not $JavaHome -or -not (Test-Path (Join-Path $JavaHome "bin\javac.exe"))) {
    $javac = Get-Command javac -ErrorAction SilentlyContinue
    if (-not $javac) { throw "JAVA_HOME is not set and javac was not found on PATH." }
    $JavaHome = Split-Path -Parent (Split-Path -Parent $javac.Source)
}

$Javac = Join-Path $JavaHome "bin\javac.exe"
$Java = Join-Path $JavaHome "bin\java.exe"
$AndroidJar = Find-Tool $AndroidJar "android.jar" "PRISM_ANDROID_JAR"
$ApktoolJar = Find-Tool $ApktoolJar "apktool.jar" "PRISM_APKTOOL_JAR"
$R8Jar = Find-Tool $R8Jar "r8lib.jar" "PRISM_R8_JAR"
$SignerJar = Find-Tool $SignerJar "uber-apk-signer.jar" "PRISM_SIGNER_JAR"

Write-Host "1. Compiling Java sources..."
if (Test-Path $BinDir) { Remove-Item -Recurse -Force $BinDir }
New-Item -ItemType Directory -Force -Path $BinDir | Out-Null
$javaFiles = Get-ChildItem -Path $SrcDir -Recurse -Filter "*.java" | ForEach-Object { $_.FullName }
if (-not $javaFiles) { throw "No Java sources found under $SrcDir" }
$javacArgs = @("--release", "17", "-encoding", "UTF-8", "-cp", $AndroidJar, "-d", $BinDir) + $javaFiles
Invoke-Tool -Exe $Javac -Arguments $javacArgs -Description "javac"

Write-Host "2. Dexing class files with D8..."
if (Test-Path $DexDir) { Remove-Item -Recurse -Force $DexDir }
New-Item -ItemType Directory -Force -Path $DexDir | Out-Null
$classFiles = Get-ChildItem -Path $BinDir -Recurse -Filter "*.class" | ForEach-Object { $_.FullName }
$d8Args = @("-cp", $R8Jar, "com.android.tools.r8.D8", "--lib", $AndroidJar, "--output", $DexDir) + $classFiles
Invoke-Tool -Exe $Java -Arguments $d8Args -Description "D8"

Write-Host "3. Building the base APK with apktool..."
if (Test-Path $UnsignedApk) { Remove-Item -Force $UnsignedApk }
$ApkRoot = Join-Path $AppRoot "apk_root"
$ApkRootBuild = Join-Path $ApkRoot "build"
if (Test-Path $ApkRootBuild) { Remove-Item -Recurse -Force $ApkRootBuild }
Invoke-Tool -Exe $Java -Arguments @("-jar", $ApktoolJar, "b", "-f", $ApkRoot, "-o", $UnsignedApk) -Description "apktool"

Write-Host "4. Adding classes.dex to the APK..."
$dex = Join-Path $DexDir "classes.dex"
if (-not (Test-Path $dex)) { throw "D8 did not produce $dex" }
Add-Type -AssemblyName System.IO.Compression.FileSystem
$zip = [System.IO.Compression.ZipFile]::Open($UnsignedApk, "Update")
try {
    $entry = $zip.GetEntry("classes.dex")
    if ($entry) { $entry.Delete() }
    [System.IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
        $zip, $dex, "classes.dex", [System.IO.Compression.CompressionLevel]::Optimal) | Out-Null
} finally {
    $zip.Dispose()
}

Write-Host "5. Signing and aligning the APK..."
if (Test-Path $SignedDir) { Remove-Item -Recurse -Force $SignedDir }
Invoke-Tool -Exe $Java -Arguments @("-jar", $SignerJar, "-a", $UnsignedApk, "-o", $SignedDir, "--allowResign") -Description "APK signing"

$signed = Get-ChildItem -Path $SignedDir -Filter "*.apk" | Select-Object -First 1
if (-not $signed) { throw "No signed APK was produced in $SignedDir" }

$ReleaseDir = Join-Path $RepoRoot "release"
New-Item -ItemType Directory -Force -Path $ReleaseDir | Out-Null
$ReleaseApk = Join-Path $ReleaseDir "prism-v-v1.1.2.apk"
Copy-Item -Force $signed.FullName $ReleaseApk

Write-Host "Build complete: $($signed.FullName)"
Write-Host "Release copy ready: $ReleaseApk"
