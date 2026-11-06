#!/data/data/com.termux/files/usr/bin/bash
# setup_internal.sh — cria projeto FlexCityInternal do zero
# baixa imgui + xdl, prepara estrutura de pastas
# uso: bash setup_internal.sh          (mantém pastas existentes)
#      bash setup_internal.sh --fresh  (apaga tudo e refaz)

set -e

PROJECT=~/FlexCityInternal
NDK="${NDK:-$HOME/android-ndk}"

if [ ! -d "$NDK" ]; then
    echo "[!] NDK não encontrado em $NDK"
    echo "    export NDK=/caminho/do/ndk"
    echo "    (o NDK que você tem é o ~/android-ndk-r29 ou similar)"
    exit 1
fi
echo "[*] NDK: $NDK"

FRESH=0
if [ "$1" == "--fresh" ]; then
    FRESH=1
fi

# 1. estrutura base
if [ "$FRESH" -eq 1 ]; then
    echo "[*] --fresh: apagando $PROJECT"
    rm -rf "$PROJECT"
fi
mkdir -p "$PROJECT/jni"
cd "$PROJECT"

# 2. ImGui
if [ ! -f jni/imgui/imgui.cpp ]; then
    echo "[*] baixando ImGui..."
    rm -rf jni/imgui_tmp
    git clone --depth 1 https://github.com/ocornut/imgui.git jni/imgui_tmp
    mkdir -p jni/imgui/backends
    cp jni/imgui_tmp/imgui.cpp jni/imgui/
    cp jni/imgui_tmp/imgui_draw.cpp jni/imgui/
    cp jni/imgui_tmp/imgui_tables.cpp jni/imgui/
    cp jni/imgui_tmp/imgui_widgets.cpp jni/imgui/
    cp jni/imgui_tmp/imgui.h jni/imgui/
    cp jni/imgui_tmp/imgui_internal.h jni/imgui/
    cp jni/imgui_tmp/imconfig.h jni/imgui/
    cp jni/imgui_tmp/imstb_rectpack.h jni/imgui/
    cp jni/imgui_tmp/imstb_textedit.h jni/imgui/
    cp jni/imgui_tmp/imstb_truetype.h jni/imgui/
    cp jni/imgui_tmp/backends/imgui_impl_opengl3.cpp jni/imgui/backends/
    cp jni/imgui_tmp/backends/imgui_impl_opengl3.h jni/imgui/backends/
    cp jni/imgui_tmp/backends/imgui_impl_opengl3_loader.h jni/imgui/backends/ 2>/dev/null || true
    cp jni/imgui_tmp/backends/imgui_impl_android.cpp jni/imgui/backends/ 2>/dev/null || true
    cp jni/imgui_tmp/backends/imgui_impl_android.h jni/imgui/backends/ 2>/dev/null || true
    rm -rf jni/imgui_tmp
    echo "[*] imgui baixado"
else
    echo "[*] imgui já existe, pulando"
fi

# 3. xDL — usa find pra achar os arquivos independente da estrutura do repo
if [ ! -f jni/xdl/xdl.c ]; then
    echo "[*] baixando xDL..."
    rm -rf jni/xdl_tmp
    git clone --depth 1 https://github.com/hexhacking/xDL.git jni/xdl_tmp
    mkdir -p jni/xdl
    
    # acha todos os .c e .h em qualquer subpasta do repo
    XDL_C=$(find jni/xdl_tmp -name "*.c" | grep -v sample | grep -v test | head -20)
    XDL_H=$(find jni/xdl_tmp -name "*.h" | grep -v sample | grep -v test | head -20)
    
    if [ -z "$XDL_C" ]; then
        echo "[!] nao achei nenhum .c no repo do xdl"
        ls -R jni/xdl_tmp | head -50
        exit 1
    fi
    
    for f in $XDL_C; do cp "$f" jni/xdl/; done
    for f in $XDL_H; do cp "$f" jni/xdl/; done
    
    rm -rf jni/xdl_tmp
    echo "[*] xdl: $(ls jni/xdl/*.c 2>/dev/null | wc -l) .c, $(ls jni/xdl/*.h 2>/dev/null | wc -l) .h"
else
    echo "[*] xdl já existe, pulando"
fi

# 4. Dobby prebuilt
if [ ! -f jni/dobby/lib/arm64-v8a/libdobby.a ]; then
    if [ -f "$HOME/FlexCityCheat/dobby/lib/arm64-v8a/libdobby.a" ]; then
        echo "[*] copiando Dobby prebuilt de FlexCityCheat"
        mkdir -p jni/dobby/lib/arm64-v8a jni/dobby/include
        cp "$HOME/FlexCityCheat/dobby/lib/arm64-v8a/libdobby.a" jni/dobby/lib/arm64-v8a/
        cp "$HOME/FlexCityCheat/dobby/include/"*.h jni/dobby/include/ 2>/dev/null || true
    else
        echo "[!] Dobby NAO encontrado em ~/FlexCityCheat/dobby/"
        echo "    coloca o libdobby.a em: $PROJECT/jni/dobby/lib/arm64-v8a/"
        echo "    e os headers em: $PROJECT/jni/dobby/include/"
    fi
else
    echo "[*] dobby já existe, pulando"
fi

# 5. .gitignore
cat > .gitignore <<'EOF'
obj/
libs/
jni/imgui/
jni/xdl/
jni/dobby/
EOF

echo
echo "[✓] estrutura pronta em $PROJECT"
echo
echo "estrutura:"
find . -maxdepth 3 -type d | grep -v '\.git' | sort
echo
echo "arquivos xdl:"
ls jni/xdl/ 2>/dev/null | head -15
echo
echo "proximo passo: colar os .cpp/.h do cheat em $PROJECT/jni/"
echo "e rodar ./build.sh"