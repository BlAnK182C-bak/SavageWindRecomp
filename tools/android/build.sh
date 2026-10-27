#!/bin/bash
# tools/android/build.sh: build/android/SavageWind-<version>.apk, Savage Wind for arm64 Android 13+ - the runtime
# (libmain.so: the Makefile's PLATFORM=android), SDL3 with its Java activity, the fonts. No game code: the player builds
# it from their own game files on a PC (tools/android/gamelib.sh) and copies it to the phone with them.
# Needs NDK=<Android NDK r28c>, ANDROID_HOME=<Android SDK> (build-tools, a platform), a JDK, cmake, ninja, python3, zip.
# Signed with tools/android/debug.p12, a test key anyone has: one key, so a newer build installs over an older one.
set -euo pipefail
cd "$(dirname "$0")/../.."
: "${NDK:?NDK=<Android NDK r28c>}" "${ANDROID_HOME:?ANDROID_HOME=<Android SDK>}"
export NDK
tools/linux/build_deps.sh android
D=$PWD/build/linux/android/deps
make -j"$(getconf _NPROCESSORS_ONLN)" PLATFORM=android NDK="$NDK" DEPS="$D" build/android/libmain.so

BT=$(ls -d "$ANDROID_HOME"/build-tools/* | sort -V | tail -1)
JAR=$(ls -d "$ANDROID_HOME"/platforms/android-* | sort -V | tail -1)/android.jar
NB=$(ls -d "$NDK"/toolchains/llvm/prebuilt/*/bin | head -1)
VERSION=$(sed -n 's/^#define SAVAGEWIND_VERSION "\(.*\)"/\1/p' runtime/ui/launcher.mm)
O=build/android/apk
rm -rf "$O"
mkdir -p "$O/root/lib/arm64-v8a" "$O/root/assets" "$O/classes" "$O/res/mipmap-xxxhdpi"

# the libraries (debug info stripped: it's most of libmain.so); the files the runtime opens by name, read from the
# assets (runtime/paths.c rt_resource), with their licenses
cp build/android/libmain.so "$D/lib/libSDL3.so" "$O/root/lib/arm64-v8a/"
"$NB/llvm-strip" --strip-debug "$O/root/lib/arm64-v8a/"*.so
cp third_party/fonts/FiraSans/FiraSans-SemiBold.ttf third_party/fonts/FiraSans/FiraSans-Medium.ttf \
   third_party/fonts/NotoSansThai/NotoSansThai-Medium.ttf "$O/root/assets/"
cp third_party/fonts/FiraSans/OFL.txt "$O/root/assets/FiraSans-OFL.txt"
cp third_party/fonts/NotoSansThai/OFL.txt "$O/root/assets/NotoSansThai-OFL.txt"
cp third_party/FidelityFX/LICENSE.txt "$O/root/assets/FidelityFX-LICENSE.txt"
cp README.md LICENSE "$O/root/assets/"
cp docs/icon/icon_1024.png "$O/root/assets/icon.png"
cp docs/icon/icon_1024.png "$O/res/mipmap-xxxhdpi/ic_launcher.png"

# Java: SDL's activity, from its source release (build_deps.sh fetched it), and ours
mkdir -p "$O/sdl"
tar -xzf "$(ls build/linux/src/SDL3-*.tar.gz | head -1)" -C "$O/sdl" --strip-components 1 --wildcards '*/android-project/app/src/main/java/*'
javac --release 11 -nowarn -cp "$JAR" -d "$O/classes" "$O"/sdl/android-project/app/src/main/java/org/libsdl/app/*.java \
    tools/android/SavageWindActivity.java
"$BT/d8" --release --min-api 33 --lib "$JAR" --output "$O/root" $(find "$O/classes" -name '*.class')

"$BT/aapt2" compile --dir "$O/res" -o "$O/res.zip"
"$BT/aapt2" link -o "$O/base.apk" -I "$JAR" --manifest tools/android/AndroidManifest.xml -R "$O/res.zip" \
    --version-name "$VERSION" --version-code 1
(cd "$O/root" && zip -qr ../base.apk .)
"$BT/zipalign" -f 4 "$O/base.apk" "$O/aligned.apk"
"$BT/apksigner" sign --ks tools/android/debug.p12 --ks-type PKCS12 --ks-pass pass:android --ks-key-alias savagewind \
    --out "build/android/SavageWind-$VERSION.apk" "$O/aligned.apk"
echo "build/android/SavageWind-$VERSION.apk"
