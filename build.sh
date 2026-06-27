#!/bin/bash
NDK=/opt/android-ndk-r25c
CC=$NDK/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang

$CC -shared -fPIC -I. main.c game.c world.c math.c \
    $NDK/sources/android/native_app_glue/android_native_app_glue.c \
    -o libmain.so -landroid -llog -lGLESv2 -lEGL -lm

mkdir -p lib/arm64-v8a
cp libmain.so lib/arm64-v8a/

aapt package -f -M AndroidManifest.xml -I $ANDROID_HOME/platforms/android-34/android.jar -F app.apk lib/
apksigner sign --ks debug.keystore app.apk
