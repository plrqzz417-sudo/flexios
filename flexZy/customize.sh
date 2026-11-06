#!/system/bin/sh
SKIPUNZIP=0

mkdir -p "$MODPATH/zygisk"
cp "$MODPATH/lib/arm64-v8a/libflexzy.so" "$MODPATH/zygisk/arm64-v8a.so"
rm -rf "$MODPATH/lib"