[CmdletBinding()]
param()

$vendorDir = "D:\code\Main_projects\forensic_search\military_intelligence"
& powershell -ExecutionPolicy Bypass -File "$PSScriptRoot\scripts\build_apk.ps1" `
    -AndroidJar "$vendorDir\android.jar" `
    -ApktoolJar "$vendorDir\apktool.jar" `
    -R8Jar "$vendorDir\r8lib.jar" `
    -SignerJar "$vendorDir\uber-apk-signer.jar"
