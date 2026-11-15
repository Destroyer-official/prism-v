$ErrorActionPreference = "Stop"

$JAVAC = "D:\program_software\sdks\jdk-25.0.5.8-hotspo\bin\javac.exe"
$JAVA = "D:\program_software\sdks\jdk-25.0.5.8-hotspo\bin\java.exe"
$TOOLS_DIR = "D:\code\Main_projects\forensic_search\military_intelligence"

Write-Host "1. Compiling Java sources..."
$javaFiles = Get-ChildItem -Path "android_app\src" -Recurse -Filter "*.java" | ForEach-Object { $_.FullName }
& $JAVAC --release 17 -encoding UTF-8 -cp "$TOOLS_DIR\android.jar" -d "android_app\bin" $javaFiles

Write-Host "2. Running D8 Dexer..."
$classFiles = Get-ChildItem -Path "android_app\bin" -Recurse -Filter "*.class" | ForEach-Object { $_.FullName }
& $JAVA -cp "$TOOLS_DIR\r8lib.jar" com.android.tools.r8.D8 --lib "$TOOLS_DIR\android.jar" --output "android_app\dex" $classFiles

Write-Host "3. Building base APK with Apktool..."
if (Test-Path "android_app\unsigned.apk") { Remove-Item -Force "android_app\unsigned.apk" }
if (Test-Path "android_app\apk_root\build") { Remove-Item -Recurse -Force "android_app\apk_root\build" }
& $JAVA -jar "$TOOLS_DIR\apktool.jar" b -f "android_app\apk_root" -o "android_app\unsigned.apk"

Write-Host "4. Injecting classes.dex..."
python -c "import zipfile; z = zipfile.ZipFile('android_app/unsigned.apk', 'a'); z.write('android_app/dex/classes.dex', 'classes.dex'); z.close()"

Write-Host "5. Signing and Aligning APK..."
if (Test-Path "android_app\signed") { Remove-Item -Recurse -Force "android_app\signed" }
& $JAVA -jar "$TOOLS_DIR\uber-apk-signer.jar" -a "android_app\unsigned.apk" -o "android_app\signed" --allowResign

Write-Host ">>> BUILD & SIGN SUCCESSFUL! <<<"
