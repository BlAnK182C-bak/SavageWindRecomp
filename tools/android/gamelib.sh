#!/bin/bash
# tools/android/gamelib.sh [exefs folder]: the game code for Savage Wind on Android, built on a Linux or macOS PC from
# your own game files (the phone has no compiler): build/android/swgame.so and swgame.so.id. Copy both, with the game's
# exefs/ and romfs/ folders, into the phone's game folder (SavageWind/extracted/: the app's start screen shows it).
# Run it from a checkout of the same commit as the APK: the app loads only game code with its own id.
# exefs folder: by default a desktop setup's (~/.local/share/savagewind/extracted/exefs), else extracted/exefs.
# Downloads the Android NDK r28c into build/android/ndk unless NDK=<it> is set; makes tools/venv if missing.
set -euo pipefail
cd "$(dirname "$0")/../.."
EXEFS=${1:-$([ -d ~/.local/share/savagewind/extracted/exefs ] && echo ~/.local/share/savagewind/extracted/exefs || echo extracted/exefs)}
[ -f "$EXEFS/main" ] || { echo "no exefs/main in $EXEFS: give the game's exefs folder"; exit 1; }

if [ -z "${NDK:-}" ]; then
    NDK=$PWD/build/android/ndk/android-ndk-r28c
    if [ ! -d "$NDK" ]; then
        case "$(uname -s)" in
            Linux) f=android-ndk-r28c-linux.zip sum=a7b54a5de87fecd125a17d54f73c446199e72a64 ;;
            Darwin) f=android-ndk-r28c-darwin.zip sum=fc20a6bf15a30fb3428c9b60a7308793a362dc6d ;;
            *) echo "Linux or macOS only (or set NDK=<Android NDK r28c>)"; exit 1 ;;
        esac
        mkdir -p build/android/ndk
        echo "downloading the Android NDK r28c (about 1 GB)..."
        curl -fL -o "build/android/ndk/$f" "https://dl.google.com/android/repository/$f"
        echo "$sum  build/android/ndk/$f" | (sha1sum -c - 2>/dev/null || shasum -c -)
        (cd build/android/ndk && unzip -q "$f" && rm "$f")
    fi
fi
if [ ! -x tools/venv/bin/python ]; then
    python3 -m venv tools/venv
    tools/venv/bin/pip install -q capstone==5.0.9 unicorn==2.1.4 lz4==4.4.5
fi

JOBS=$(getconf _NPROCESSORS_ONLN)
make PLATFORM=android NDK="$NDK" EXEFS="$EXEFS" gen
make -j"$JOBS" PLATFORM=android NDK="$NDK" build/android/swgame.so
cat <<EOF

Done: build/android/swgame.so and build/android/swgame.so.id
Copy them, with your game's exefs and romfs folders (and aoc1, aoc2 for the DLC), into SavageWind/extracted/ on the
phone, then tap Check again on Savage Wind's start screen.
EOF
