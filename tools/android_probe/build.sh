#!/bin/sh
# tools/android_probe/build.sh: build/android_probe/SavageWind-probe.apk, an app that shows what a phone has of what
# the runtime needs (probe.c). No Gradle: the NDK's clang, javac, and d8 / aapt2 / apksigner from an Android SDK
# ($ANDROID_HOME, the NDK at $ANDROID_NDK_HOME or the newest under $ANDROID_HOME/ndk). Debug-signed.
set -eu
SDK=${ANDROID_HOME:?set ANDROID_HOME to the Android SDK}
NDK=${ANDROID_NDK_HOME:-$(ls -d "$SDK"/ndk/* | sort -V | tail -1)}
BT=$(ls -d "$SDK"/build-tools/* | sort -V | tail -1)
JAR=$(ls -d "$SDK"/platforms/android-* | sort -V | tail -1)/android.jar
CC=$(ls -d "$NDK"/toolchains/llvm/prebuilt/*/bin | head -1)/aarch64-linux-android28-clang
SRC=tools/android_probe
O=build/android_probe
rm -rf "$O"
mkdir -p "$O/apk/lib/arm64-v8a" "$O/classes"

# (16 KB alignment: some Android 15 phones use 16 KB pages, and a library aligned for 4 KB doesn't load there)
"$CC" -shared -fPIC -O2 -Wall -Wextra -Wl,-z,max-page-size=16384 -o "$O/apk/lib/arm64-v8a/libprobe.so" "$SRC/probe.c" -lvulkan
javac --release 8 -cp "$JAR" -d "$O/classes" "$SRC/Probe.java"
"$BT/d8" --release --min-api 28 --lib "$JAR" --output "$O/apk" $(find "$O/classes" -name '*.class')
"$BT/aapt2" link -o "$O/base.apk" -I "$JAR" --manifest "$SRC/AndroidManifest.xml"
(cd "$O/apk" && zip -qr ../base.apk .)
"$BT/zipalign" -f -p 4 "$O/base.apk" "$O/aligned.apk"
keytool -genkeypair -keystore "$O/debug.jks" -storepass android -keypass android -alias probe -keyalg RSA \
    -validity 10000 -dname CN=probe -noprompt
"$BT/apksigner" sign --ks "$O/debug.jks" --ks-pass pass:android --out "$O/SavageWind-probe.apk" "$O/aligned.apk"
echo "$O/SavageWind-probe.apk"
