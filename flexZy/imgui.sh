#!/bin/bash
set -e

IMGUI_VERSION="v1.90.4"
IMGUI_REPO="https://github.com/ocornut/imgui.git"
BUILD_DIR="$PWD/.imgui_build"

echo "[1/3] Clonando ImGui $IMGUI_VERSION..."
rm -rf "$BUILD_DIR"
git clone --depth 1 --branch $IMGUI_VERSION $IMGUI_REPO "$BUILD_DIR"

echo "[2/3] Copiando arquivos pro jni/imgui..."
mkdir -p jni/imgui
cp "$BUILD_DIR/imgui.cpp"                     jni/imgui/
cp "$BUILD_DIR/imgui.h"                       jni/imgui/
cp "$BUILD_DIR/imgui_internal.h"              jni/imgui/
cp "$BUILD_DIR/imgui_draw.cpp"                jni/imgui/
cp "$BUILD_DIR/imgui_tables.cpp"              jni/imgui/
cp "$BUILD_DIR/imgui_widgets.cpp"             jni/imgui/
cp "$BUILD_DIR/imgui_demo.cpp"                jni/imgui/ 2>/dev/null || true
cp "$BUILD_DIR/imconfig.h"                    jni/imgui/
cp "$BUILD_DIR/imstb_rectpack.h"              jni/imgui/
cp "$BUILD_DIR/imstb_textedit.h"              jni/imgui/
cp "$BUILD_DIR/imstb_truetype.h"              jni/imgui/

cp "$BUILD_DIR/backends/imgui_impl_opengl3.cpp"        jni/imgui/
cp "$BUILD_DIR/backends/imgui_impl_opengl3.h"          jni/imgui/
cp "$BUILD_DIR/backends/imgui_impl_opengl3_loader.h"   jni/imgui/ 2>/dev/null || true
cp "$BUILD_DIR/backends/imgui_impl_android.cpp"        jni/imgui/
cp "$BUILD_DIR/backends/imgui_impl_android.h"          jni/imgui/

echo "[3/3] Verificando..."
ls -la jni/imgui/ | head -30

rm -rf "$BUILD_DIR"
echo "ImGui $IMGUI_VERSION pronto em jni/imgui/"