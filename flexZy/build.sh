#!/bin/bash
set -e

# Gera o .dex do FlexHandler
mkdir -p jni/java/flexzy
javac -d jni/java jni/java/flexzy/FlexHandler.java
d8 --output jni/java jni/java/flexzy/FlexHandler.class
xxd -i jni/java/classes.dex > jni/flexhandler_dex.h

# Build do .so
ndk-build NDK_PROJECT_PATH=. APP_BUILD_SCRIPT=./jni/Android.mk NDK_APPLICATION_MK=./jni/Application.mk

# Empacota no ZIP do Magisk
mkdir -p module/lib/arm64-v8a
cp libs/arm64-v8a/libflexzy.so module/lib/arm64-v8a/
cd module
zip -r ../flexzy.zip *
cd ..
echo "ZIP gerado: flexzy.zip"