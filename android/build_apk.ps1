# Builds a debug-signed arm64 APK of Jam Sandbox without Gradle (NDK clang + aapt2 + apksigner).
# Prerequisites live in ..\toolchain\android (shared with RougeFleet): sdk\, jdk17\, raylib-5.5\.
param([string]$Version = "0.1.0", [int]$VersionCode = 1)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path "$PSScriptRoot\..").Path
$tc = (Resolve-Path "$root\..\toolchain\android").Path
$sdk = "$tc\sdk"
$jdk = (Get-ChildItem "$tc\jdk17" | Select-Object -First 1).FullName
$env:JAVA_HOME = $jdk; $env:PATH = "$jdk\bin;$env:PATH"
$ndk = (Get-ChildItem "$sdk\ndk" | Select-Object -First 1).FullName
$bt = (Get-ChildItem "$sdk\build-tools" | Select-Object -First 1).FullName
$llvm = "$ndk\toolchains\llvm\prebuilt\windows-x86_64\bin"
$clang = "$llvm\clang.exe"
$ar = "$llvm\llvm-ar.exe"
$target = "aarch64-linux-android29"
$rl = "$tc\raylib-5.5\src"
$out = "$root\build\android"
$obj = "$out\obj"
New-Item -ItemType Directory -Force $obj, "$out\lib\arm64-v8a" | Out-Null
$glue = "$ndk\sources\android\native_app_glue"

# 1. raylib static library (cached)
$libray = "$out\libraylib.a"
if (-not (Test-Path $libray)) {
    $flags = @("--target=$target", "-O2", "-ffunction-sections", "-funwind-tables", "-fstack-protector-strong", "-fPIC", "-std=gnu99",
               "-D__ANDROID__", "-DPLATFORM_ANDROID", "-D__ANDROID_API__=29", "-DGRAPHICS_API_OPENGL_ES2", "-w",
               "-I$rl", "-I$rl\external\glfw\include", "-I$glue")
    $objs = @()
    foreach ($f in "rcore", "rshapes", "rtextures", "rtext", "rmodels", "raudio", "utils") {
        & $clang @flags -c "$rl\$f.c" -o "$obj\ray_$f.o"; if ($LASTEXITCODE) { throw "raylib: $f failed" }
        $objs += "$obj\ray_$f.o"
    }
    & $ar rcs $libray @objs
}

# 2. game shared library
$gsrc = @("$root\src\main.c", "$glue\android_native_app_glue.c")
& $clang "--target=$target" -O2 -fPIC -std=gnu99 -shared -D__ANDROID__ -DPLATFORM_ANDROID -DGRAPHICS_API_OPENGL_ES2 -w `
    "-I$rl" "-I$glue" "-I$root\src" "-Wl,-z,max-page-size=16384" -u ANativeActivity_onCreate `
    -o "$out\lib\arm64-v8a\libmain.so" @gsrc $libray -llog -landroid -lEGL -lGLESv2 -lOpenSLES -lm -ldl
if ($LASTEXITCODE) { throw "game link failed" }

# 3. manifest, theme (fullscreen, drawn under the display cutout on API 28+) and APK packaging
$manifest = @"
<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android" package="com.fleetinglabs.jamsandbox" android:versionCode="$VersionCode" android:versionName="$Version">
  <uses-sdk android:minSdkVersion="24" android:targetSdkVersion="34" />
  <uses-feature android:glEsVersion="0x00020000" android:required="true" />
  <application android:label="Jam Sandbox" android:icon="@mipmap/ic_launcher" android:roundIcon="@mipmap/ic_launcher" android:hasCode="false" android:allowBackup="true">
    <activity android:name="android.app.NativeActivity" android:exported="true" android:screenOrientation="landscape"
              android:configChanges="orientation|keyboardHidden|screenSize|smallestScreenSize|screenLayout|uiMode"
              android:theme="@style/AppTheme">
      <meta-data android:name="android.app.lib_name" android:value="main" />
      <intent-filter><action android:name="android.intent.action.MAIN" /><category android:name="android.intent.category.LAUNCHER" /></intent-filter>
    </activity>
  </application>
</manifest>
"@
Set-Content "$out\AndroidManifest.xml" $manifest -Encoding UTF8
$res = "$out\res"
New-Item -ItemType Directory -Force "$res\values", "$res\values-v28" | Out-Null
Copy-Item "$PSScriptRoot\icons\mipmap-*" $res -Recurse -Force   # app icon (v6), one ic_launcher.png per density
Set-Content "$res\values\styles.xml" '<resources><style name="AppTheme" parent="android:Theme.NoTitleBar.Fullscreen"/></resources>' -Encoding UTF8
Set-Content "$res\values-v28\styles.xml" '<resources><style name="AppTheme" parent="android:Theme.NoTitleBar.Fullscreen"><item name="android:windowLayoutInDisplayCutoutMode">shortEdges</item></style></resources>' -Encoding UTF8
$android = "$sdk\platforms\android-34\android.jar"
& "$bt\aapt2.exe" compile --dir $res -o "$out\resources.zip"
if ($LASTEXITCODE) { throw "aapt2 compile failed" }
& "$bt\aapt2.exe" link -o "$out\base.apk" --manifest "$out\AndroidManifest.xml" -I $android "$out\resources.zip" --min-sdk-version 24 --target-sdk-version 34 --version-code $VersionCode --version-name $Version
if ($LASTEXITCODE) { throw "aapt2 link failed" }

$py = @"
import zipfile, os, sys
out, root = sys.argv[1], sys.argv[2]
zin = zipfile.ZipFile(out + '/base.apk')
zout = zipfile.ZipFile(out + '/unaligned.apk', 'w', zipfile.ZIP_DEFLATED)
for i in zin.infolist(): zout.writestr(i, zin.read(i.filename))
zout.write(out + '/lib/arm64-v8a/libmain.so', 'lib/arm64-v8a/libmain.so', zipfile.ZIP_STORED)
for d, _, fs in os.walk(root + '/assets'):
    for f in fs:
        p = os.path.join(d, f); rel = os.path.relpath(p, root + '/assets').replace(os.sep, '/')
        zout.write(p, 'assets/' + rel)
zout.close()
"@
Set-Content "$out\pack.py" $py -Encoding UTF8
python "$out\pack.py" $out $root
if ($LASTEXITCODE) { throw "packing failed" }

# 4. align and sign (debug keystore, created once; keep it so updates install over older builds)
$ks = "$tc\jamsandbox-debug.keystore"
if (-not (Test-Path $ks)) {
    $ErrorActionPreference = "Continue"; & "$jdk\bin\keytool.exe" -genkeypair -keystore $ks -storepass android -keypass android -alias jamsandbox -keyalg RSA -keysize 2048 -validity 20000 -dname "CN=Jam Sandbox Debug" 2>$null; $ErrorActionPreference = "Stop"
}
& "$bt\zipalign.exe" -f -p 4 "$out\unaligned.apk" "$out\aligned.apk"
if ($LASTEXITCODE) { throw "zipalign failed" }
& "$bt\apksigner.bat" sign --ks $ks --ks-pass pass:android --key-pass pass:android --out "$out\JamSandbox-v$Version.apk" "$out\aligned.apk"
if ($LASTEXITCODE) { throw "apksigner failed" }
Write-Host "APK: $out\JamSandbox-v$Version.apk"



