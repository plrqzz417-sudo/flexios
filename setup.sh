#!/data/data/com.termux/files/usr/bin/bash
set -e

# ============================================================
# FlexCityCheat — setup completo (NDK nativo Termux)
# ============================================================

echo "[*] instalando dependencias base..."
pkg install -y git clang wget unzip
# ── NDK ──────────────────────────────────────────────────────
# O pacote "android-ndk" do Termux foi descontinuado.
# O toolchain aarch64 real vem de lzhiyong/termux-ndk.
# Asset: android-ndk-r29-aarch64.tar.xz (tag: android-ndk)

TERMUX_NDK="$PREFIX/opt/android-ndk"

if [ -n "$ANDROID_NDK" ] && [ -d "$ANDROID_NDK" ]; then
    echo "[*] usando ANDROID_NDK=$ANDROID_NDK"
elif [ -d "$TERMUX_NDK/toolchains/llvm/prebuilt/linux-aarch64" ]; then
    ANDROID_NDK="$TERMUX_NDK"
    echo "[*] usando NDK existente: $ANDROID_NDK"
else
    echo "[*] baixando NDK r29 aarch64 ..."

    NDK_TARBALL="$HOME/android-ndk-r29-aarch64.tar.xz"
    NDK_URL="https://github.com/lzhiyong/termux-ndk/releases/download/android-ndk/android-ndk-r29-aarch64.tar.xz"

    wget -O "$NDK_TARBALL" "$NDK_URL"

    echo "[*] extraindo em $PREFIX/opt ..."
    mkdir -p "$PREFIX/opt"
    rm -rf "$PREFIX/opt/android-ndk-r29"
    tar -xf "$NDK_TARBALL" -C "$PREFIX/opt"
    rm -f "$NDK_TARBALL"

    # o tarball extrai como android-ndk-r29; normaliza pra android-ndk
    if [ -d "$PREFIX/opt/android-ndk-r29" ]; then
        rm -rf "$TERMUX_NDK"
        mv "$PREFIX/opt/android-ndk-r29" "$TERMUX_NDK"
    fi

    ANDROID_NDK="$TERMUX_NDK"
    [ -d "$ANDROID_NDK/toolchains/llvm/prebuilt/linux-aarch64" ] || {
        echo "[!] toolchain nao encontrada apos extracao"
        ls "$ANDROID_NDK/toolchains/llvm/prebuilt/" 2>/dev/null
        exit 1
    }
    echo "[+] NDK em: $ANDROID_NDK"
fi

ANDROID_NDK="$(realpath "$ANDROID_NDK")"
export ANDROID_NDK

# valida toolchain
HOST_TAG="linux-aarch64"
[ ! -d "$ANDROID_NDK/toolchains/llvm/prebuilt/$HOST_TAG" ] && HOST_TAG="linux-x86_64"
[ -d "$ANDROID_NDK/toolchains/llvm/prebuilt/$HOST_TAG" ] || {
    echo "[!] toolchain nao encontrada em $ANDROID_NDK/toolchains/llvm/prebuilt/"
    ls "$ANDROID_NDK/toolchains/llvm/prebuilt/" 2>/dev/null || true
    exit 1
}
echo "[+] host tag: $HOST_TAG"

echo "[*] criando estrutura em ~/FlexCityCheat ..."
mkdir -p ~/FlexCityCheat/jni
mkdir -p ~/FlexCityCheat/app/src/main/java/com/flexcity/cheat
cd ~/FlexCityCheat

if [ ! -d imgui ]; then
    echo "[*] clonando imgui ..."
    git clone --depth=1 https://github.com/ocornut/imgui.git
fi

# ─── build.sh ────────────────────────────────────────────────
echo "[*] escrevendo build.sh ..."
cat > build.sh <<'BUILD_EOF'
#!/data/data/com.termux/files/usr/bin/bash
set -e

ANDROID_NDK="${ANDROID_NDK:-$PREFIX/opt/android-ndk}"
[ -d "$ANDROID_NDK" ] || { echo "[!] NDK nao encontrado em $ANDROID_NDK"; exit 1; }
ANDROID_NDK="$(realpath "$ANDROID_NDK")"

HOST_TAG="linux-aarch64"
[ ! -d "$ANDROID_NDK/toolchains/llvm/prebuilt/$HOST_TAG" ] && HOST_TAG="linux-x86_64"
TOOLCHAIN="$ANDROID_NDK/toolchains/llvm/prebuilt/$HOST_TAG"
[ -d "$TOOLCHAIN" ] || {
    echo "[!] toolchain ausente: $TOOLCHAIN"
    ls "$ANDROID_NDK/toolchains/llvm/prebuilt/" 2>/dev/null
    exit 1
}

CLANG="$TOOLCHAIN/bin/clang++"
SYSROOT="$TOOLCHAIN/sysroot"
STRIP="$TOOLCHAIN/bin/llvm-strip"
[ -x "$CLANG" ] || { echo "[!] clang nao executavel: $CLANG"; exit 1; }
[ -d "$SYSROOT" ] || { echo "[!] sysroot ausente: $SYSROOT"; exit 1; }

if ! "$CLANG" --version >/dev/null 2>&1; then
    echo "[!] $CLANG nao roda neste host."
    echo "    isso geralmente significa NDK x86_64 em device ARM64."
    echo "    use: pkg install android-ndk   (nativo aarch64)"
    exit 1
fi

API=24
TRIPLE="aarch64-linux-android${API}"

SRC="$(cd "$(dirname "$0")" && pwd)"
JNI="$SRC/jni"
IMGUI="$SRC/imgui"
OUT="$SRC/libs/arm64-v8a"
mkdir -p "$OUT"

OUR_SRCS=( "$JNI/Main.cpp" "$JNI/Cheat.cpp" )
IMGUI_SRCS=(
    "$IMGUI/imgui.cpp"
    "$IMGUI/imgui_draw.cpp"
    "$IMGUI/imgui_tables.cpp"
    "$IMGUI/imgui_widgets.cpp"
    "$IMGUI/backends/imgui_impl_opengl3.cpp"
    "$IMGUI/backends/imgui_impl_android.cpp"
)

for f in "${OUR_SRCS[@]}" "${IMGUI_SRCS[@]}"; do
    [ -f "$f" ] || { echo "[!] fonte faltando: $f"; exit 1; }
done

echo "[*] compilando ..."
"$CLANG" \
    --target="$TRIPLE" \
    --sysroot="$SYSROOT" \
    -std=c++17 -O2 -fPIC -shared \
    -fvisibility=hidden \
    -I"$JNI" -I"$IMGUI" -I"$IMGUI/backends" \
    -Wl,-soname,libnative-lib.so \
    "${OUR_SRCS[@]}" "${IMGUI_SRCS[@]}" \
    -o "$OUT/libnative-lib.so" \
    -llog -landroid -lEGL -lGLESv3 -ldl -lm

[ -x "$STRIP" ] && "$STRIP" --strip-unneeded "$OUT/libnative-lib.so" || true
ls -la "$OUT/libnative-lib.so"
echo "[+] OK: $OUT/libnative-lib.so"
BUILD_EOF
chmod +x build.sh

# ─── jni/Cheat.h ─────────────────────────────────────────────
cat > jni/Cheat.h <<'CHEATH_EOF'
#pragma once
#include <cstdint>
#include <string>
#include <cmath>
#include <android/log.h>

#define LOG_TAG "FlexCity"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

struct Vector3 {
    float x, y, z;
    Vector3 operator-(const Vector3& o) const { return {x-o.x, y-o.y, z-o.z}; }
    float Length() const { return sqrtf(x*x + y*y + z*z); }
};

struct CheatState {
    bool  espEnabled  = true;
    bool  espBox      = true;
    bool  espLine     = true;
    bool  espHealth   = true;
    bool  espArmor    = true;
    bool  espName     = true;
    bool  espDistance = true;
    float maxDistance = 500.0f;
};
extern CheatState g_cheat;

void DrawESP();
void DrawMenu();
CHEATH_EOF

# ─── jni/il2cpp_api.h ────────────────────────────────────────
cat > jni/il2cpp_api.h <<'IL2CPP_EOF'
#pragma once
#include <dlfcn.h>
#include <link.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <android/log.h>

namespace il2cpp_api {

    struct Il2CppObject;
    struct Il2CppClass;
    struct Il2CppImage;
    struct Il2CppAssembly;
    struct Il2CppDomain;
    struct MethodInfo;
    struct FieldInfo;
    struct Il2CppType;
    struct Il2CppException;
    struct Il2CppArray;

    using domain_get_fn            = Il2CppDomain* (*)();
    using domain_get_assemblies_fn = const Il2CppAssembly** (*)(const Il2CppDomain*, size_t*);
    using assembly_get_image_fn    = Il2CppImage* (*)(const Il2CppAssembly*);
    using image_get_name_fn        = const char* (*)(const Il2CppImage*);
    using class_from_name_fn       = Il2CppClass* (*)(const Il2CppImage*, const char*, const char*);
    using class_get_type_fn        = const Il2CppType* (*)(Il2CppClass*);
    using type_get_object_fn       = Il2CppObject* (*)(const Il2CppType*);
    using class_get_method_fn      = MethodInfo* (*)(Il2CppClass*, const char*, int);
    using class_get_field_fn       = FieldInfo* (*)(Il2CppClass*, const char*);
    using field_get_offset_fn      = size_t (*)(FieldInfo*);
    using runtime_invoke_fn        = Il2CppObject* (*)(const MethodInfo*, void*, void**, Il2CppException**);
    using array_length_fn          = uint32_t (*)(Il2CppArray*);

    inline domain_get_fn            domain_get            = nullptr;
    inline domain_get_assemblies_fn domain_get_assemblies = nullptr;
    inline assembly_get_image_fn    assembly_get_image    = nullptr;
    inline image_get_name_fn        image_get_name        = nullptr;
    inline class_from_name_fn       class_from_name       = nullptr;
    inline class_get_type_fn        class_get_type        = nullptr;
    inline type_get_object_fn       type_get_object       = nullptr;
    inline class_get_method_fn      class_get_method      = nullptr;
    inline class_get_field_fn       class_get_field       = nullptr;
    inline field_get_offset_fn      field_get_offset      = nullptr;
    inline runtime_invoke_fn        runtime_invoke        = nullptr;
    inline array_length_fn          array_length          = nullptr;

    inline uintptr_t libil2cpp_base = 0;

    inline void* find_il2cpp() {
        void* h = dlopen("libil2cpp.so", RTLD_LAZY | RTLD_NOLOAD);
        if (!h) h = dlopen("libil2cpp.so", RTLD_LAZY);
        return h;
    }

    inline void init() {
        if (libil2cpp_base) return;
        dl_iterate_phdr([](dl_phdr_info* info, size_t, void*) -> int {
            if (info->dlpi_name && strstr(info->dlpi_name, "libil2cpp.so")) {
                libil2cpp_base = info->dlpi_addr;
                return 1;
            }
            return 0;
        }, nullptr);

        void* h = find_il2cpp();
        if (!h || !libil2cpp_base) {
            __android_log_print(ANDROID_LOG_ERROR, "FlexCity", "il2cpp nao carregado");
            return;
        }

        #define R(name) name = (decltype(name))dlsym(h, #name)
        R(domain_get);
        R(domain_get_assemblies);
        R(assembly_get_image);
        R(image_get_name);
        R(class_from_name);
        R(class_get_type);
        R(type_get_object);
        R(class_get_method);
        R(class_get_field);
        R(field_get_offset);
        R(runtime_invoke);
        R(array_length);
        #undef R

        __android_log_print(ANDROID_LOG_INFO, "FlexCity", "il2cpp base=%p", (void*)libil2cpp_base);
    }

    inline Il2CppImage* get_game_image() {
        if (!domain_get || !domain_get_assemblies) return nullptr;
        Il2CppDomain* d = domain_get();
        if (!d) return nullptr;
        size_t n = 0;
        const Il2CppAssembly** asms = domain_get_assemblies(d, &n);
        for (size_t i = 0; i < n; i++) {
            Il2CppImage* img = assembly_get_image(asms[i]);
            const char* name = image_get_name(img);
            if (name && !strcmp(name, "Assembly-CSharp.dll")) return img;
        }
        return nullptr;
    }

    inline void* rva(uintptr_t r) {
        return (void*)(libil2cpp_base + r);
    }
}
IL2CPP_EOF

# ─── jni/Cheat.cpp ───────────────────────────────────────────
cat > jni/Cheat.cpp <<'CHEATCPP_EOF'
#include "Cheat.h"
#include "il2cpp_api.h"
#include "imgui.h"

using namespace il2cpp_api;

CheatState g_cheat;

namespace RVAs {
    // Image 2 — UnityEngine.CoreModule.dll — class Camera : Behaviour
    // public Vector3 WorldToScreenPoint(Vector3 position)
    constexpr uintptr_t CAMERA_W2S = 0x31C98B0;
}
using Camera_W2S_t = Vector3 (*)(void*, Vector3);

template<typename T>
static inline T FRead(void* obj, size_t off) {
    if (!obj) return T{};
    return *(T*)((char*)obj + off);
}

static std::string ReadIl2CppString(void* strObj) {
    if (!strObj) return "";
    int32_t len = FRead<int32_t>(strObj, 0x10);
    if (len <= 0 || len > 64) return "";
    std::string out;
    out.reserve((size_t)len);
    const uint16_t* chars = (const uint16_t*)((char*)strObj + 0x14);
    for (int32_t i = 0; i < len; i++) {
        uint16_t c = chars[i];
        out.push_back(c < 0x80 ? (char)c : '?');
    }
    return out;
}

static constexpr uintptr_t CL_BASE_CHARACTER    = 0x30;
static constexpr uintptr_t CL_HP_SYNC           = 0x68;
static constexpr uintptr_t CL_CHARACTER_CANVAS  = 0x80;
static constexpr uintptr_t BC_CAMERA_TARGET     = 0x20;
static constexpr uintptr_t HP_HP                = 0x68;
static constexpr uintptr_t HP_ARMOR             = 0x6A;
static constexpr uintptr_t HP_MAX_HP            = 0x6C;
static constexpr uintptr_t HP_IS_DEAD           = 0x6E;
static constexpr uintptr_t CANVAS_NICKNAME_TMP  = 0xD0;
static constexpr uintptr_t TMP_TEXT_STR         = 0x48;
static constexpr uintptr_t SWITCHCAM_MAINCAM    = 0x28;

static constexpr uintptr_t NATIVE_TRANSFORM_POS_OFF = 0x90;

static bool read_native_localpos(void* transformIl2Cpp, Vector3& out) {
    if (!transformIl2Cpp) return false;
    void* native = FRead<void*>(transformIl2Cpp, 0x10);
    if (!native || (uintptr_t)native < 0x10000) return false;
    out = FRead<Vector3>(native, NATIVE_TRANSFORM_POS_OFF);
    return true;
}

namespace Cache {
    Il2CppImage*  gameImage       = nullptr;
    Il2CppClass*  charLinkerKlass = nullptr;
    Il2CppClass*  switchCamKlass  = nullptr;
    Il2CppClass*  objectKlass     = nullptr;
    Il2CppObject* typeCharLinker  = nullptr;
    Il2CppObject* typeSwitchCam   = nullptr;
    MethodInfo*   findObjectsM    = nullptr;
    Camera_W2S_t  w2sFn           = nullptr;
    bool ok = false;

    static Il2CppClass* find_object_class(Il2CppImage* gameImg) {
        Il2CppClass* k = class_from_name(gameImg, "UnityEngine", "Object");
        if (k) return k;
        Il2CppDomain* d = domain_get();
        size_t n = 0;
        const Il2CppAssembly** asms = domain_get_assemblies(d, &n);
        for (size_t i = 0; i < n; i++) {
            Il2CppImage* img = assembly_get_image(asms[i]);
            const char* nm = image_get_name(img);
            if (nm && strstr(nm, "UnityEngine.CoreModule")) {
                return class_from_name(img, "UnityEngine", "Object");
            }
        }
        return nullptr;
    }

    inline bool Init() {
        if (ok) return true;
        il2cpp_api::init();
        gameImage = il2cpp_api::get_game_image();
        if (!gameImage) { LOGI("game image nil"); return false; }

        charLinkerKlass = class_from_name(gameImage, "", "CharacterLinker");
        switchCamKlass  = class_from_name(gameImage, "", "SwitchCamera");
        objectKlass     = find_object_class(gameImage);

        if (!charLinkerKlass || !switchCamKlass || !objectKlass) {
            LOGI("klass nil: linker=%p swcam=%p obj=%p",
                 charLinkerKlass, switchCamKlass, objectKlass);
            return false;
        }

        typeCharLinker = type_get_object(class_get_type(charLinkerKlass));
        typeSwitchCam  = type_get_object(class_get_type(switchCamKlass));
        findObjectsM   = class_get_method(objectKlass, "FindObjectsOfType", 1);
        if (!findObjectsM) { LOGI("FindObjectsOfType nil"); return false; }

        w2sFn = (Camera_W2S_t)rva(RVAs::CAMERA_W2S);
        if (!w2sFn) { LOGI("W2S RVA zero"); return false; }

        ok = true;
        LOGI("cache ok");
        return true;
    }

    inline Il2CppArray* FindAll(Il2CppObject* type) {
        if (!findObjectsM || !type) return nullptr;
        void* params[1] = { type };
        Il2CppException* exc = nullptr;
        Il2CppArray* arr = (Il2CppArray*)runtime_invoke(findObjectsM, nullptr, params, &exc);
        if (exc) return nullptr;
        return arr;
    }

    template<typename Fn>
    inline void IterateArray(Il2CppArray* arr, Fn fn) {
        if (!arr) return;
        uint32_t len = array_length(arr);
        void** data = (void**)((char*)arr + 0x20);
        for (uint32_t i = 0; i < len; i++) {
            if (data[i]) fn(data[i]);
        }
    }
}

static void* get_main_camera() {
    Il2CppArray* arr = Cache::FindAll(Cache::typeSwitchCam);
    if (!arr) return nullptr;
    void* cam = nullptr;
    Cache::IterateArray(arr, [&](void* swCam) {
        if (!cam) cam = FRead<void*>(swCam, SWITCHCAM_MAINCAM);
    });
    return cam;
}

static bool get_world_pos(void* linker, Vector3& out) {
    void* baseChar = FRead<void*>(linker, CL_BASE_CHARACTER);
    if (!baseChar) return false;
    void* tr = FRead<void*>(baseChar, BC_CAMERA_TARGET);
    if (!tr) return false;
    return read_native_localpos(tr, out);
}

static std::string get_name(void* linker) {
    void* canvas = FRead<void*>(linker, CL_CHARACTER_CANVAS);
    if (!canvas) return "";
    void* tmp = FRead<void*>(canvas, CANVAS_NICKNAME_TMP);
    if (!tmp) return "";
    void* str = FRead<void*>(tmp, TMP_TEXT_STR);
    return ReadIl2CppString(str);
}

static void get_health(void* linker,
                       int16_t& hp, int16_t& maxHp,
                       int16_t& armor, bool& dead)
{
    void* sync = FRead<void*>(linker, CL_HP_SYNC);
    if (!sync) { hp = maxHp = armor = 0; dead = true; return; }
    hp    = FRead<int16_t>(sync, HP_HP);
    armor = FRead<int16_t>(sync, HP_ARMOR);
    maxHp = FRead<int16_t>(sync, HP_MAX_HP);
    dead  = FRead<bool>   (sync, HP_IS_DEAD);
}

void DrawESP() {
    if (!g_cheat.espEnabled) return;
    if (!Cache::Init()) return;

    void* cam = get_main_camera();
    if (!cam) return;

    ImDrawList* dl = ImGui::GetForegroundDrawList();
    ImVec2 screen = ImGui::GetIO().DisplaySize;

    Il2CppArray* links = Cache::FindAll(Cache::typeCharLinker);
    Cache::IterateArray(links, [&](void* linker) {
        Vector3 worldPos;
        if (!get_world_pos(linker, worldPos)) return;
        if (worldPos.x == 0.f && worldPos.y == 0.f && worldPos.z == 0.f) return;

        int16_t hp = 0, maxHp = 0, armor = 0;
        bool dead = false;
        get_health(linker, hp, maxHp, armor, dead);
        if (dead || hp <= 0) return;

        Vector3 sp = Cache::w2sFn(cam, worldPos);
        if (sp.z <= 0.05f) return;

        float screenY = screen.y - sp.y;
        if (sp.x < 0 || sp.x > screen.x ||
            screenY < 0 || screenY > screen.y) return;

        float boxH = 200.0f / sp.z;
        float boxW = boxH * 0.5f;
        if (boxH < 6.0f || boxH > screen.y) return;

        ImVec2 top(sp.x - boxW, screenY - boxH);
        ImVec2 bot(sp.x + boxW, screenY);

        float ratio = maxHp > 0 ? (float)hp / (float)maxHp : 1.0f;
        ImU32 col = IM_COL32(
            (int)(255 * (1.0f - ratio)),
            (int)(255 * ratio),
            60, 220);

        if (g_cheat.espLine) {
            dl->AddLine(ImVec2(screen.x * 0.5f, screen.y),
                        ImVec2(sp.x, top.y),
                        IM_COL32(200, 200, 200, 180), 1.0f);
        }
        if (g_cheat.espBox) {
            dl->AddRect(top, bot, col, 0.0f, 0, 1.5f);
        }
        if (g_cheat.espHealth) {
            float barW = 3.0f;
            float barH = boxH * ratio;
            dl->AddRectFilled(
                ImVec2(top.x - 6.0f, bot.y - barH),
                ImVec2(top.x - 6.0f + barW, bot.y),
                IM_COL32(60, 220, 60, 220));
            dl->AddRect(
                ImVec2(top.x - 6.0f, top.y),
                ImVec2(top.x - 6.0f + barW, bot.y),
                IM_COL32(20, 20, 20, 220), 0.0f, 0, 1.0f);
        }
        if (g_cheat.espArmor && armor > 0) {
            float barW = 3.0f;
            float barH = boxH * ((float)armor / 100.0f);
            dl->AddRectFilled(
                ImVec2(top.x - 11.0f, bot.y - barH),
                ImVec2(top.x - 11.0f + barW, bot.y),
                IM_COL32(80, 160, 255, 220));
        }
        if (g_cheat.espName) {
            std::string nm = get_name(linker);
            if (!nm.empty()) {
                ImVec2 tsz = ImGui::CalcTextSize(nm.c_str());
                dl->AddText(ImVec2(sp.x - tsz.x * 0.5f, top.y - tsz.y - 4.0f),
                            IM_COL32(255, 255, 255, 230), nm.c_str());
            }
        }
        if (g_cheat.espDistance) {
            char buf[32];
            snprintf(buf, sizeof(buf), "[%dm]", (int)sp.z);
            dl->AddText(ImVec2(sp.x - 14.0f, bot.y + 2.0f),
                        IM_COL32(200, 200, 200, 200), buf);
        }
    });
}

void DrawMenu() {
    ImGui::SetNextWindowSize(ImVec2(360, 260), ImGuiCond_Once);
    ImGui::Begin("Flex City", nullptr, ImGuiWindowFlags_NoCollapse);

    ImGui::Checkbox("ESP", &g_cheat.espEnabled);
    if (g_cheat.espEnabled) {
        ImGui::Indent();
        ImGui::Checkbox("Box",      &g_cheat.espBox);
        ImGui::Checkbox("Line",     &g_cheat.espLine);
        ImGui::Checkbox("Health",   &g_cheat.espHealth);
        ImGui::Checkbox("Armor",    &g_cheat.espArmor);
        ImGui::Checkbox("Name",     &g_cheat.espName);
        ImGui::Checkbox("Distance", &g_cheat.espDistance);
        ImGui::Unindent();
    }
    ImGui::End();
}
CHEATCPP_EOF

# ─── jni/Main.cpp ────────────────────────────────────────────
cat > jni/Main.cpp <<'MAIN_EOF'
#include "Cheat.h"
#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <android/native_window.h>
#include <android/native_window_jni.h>
#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_android.h"

static ANativeWindow* g_window  = nullptr;
static EGLDisplay     g_display = EGL_NO_DISPLAY;
static EGLSurface     g_surface = EGL_NO_SURFACE;
static EGLContext     g_context = EGL_NO_CONTEXT;
static bool           g_running = false;

static bool setup_egl(ANativeWindow* win) {
    g_display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (g_display == EGL_NO_DISPLAY) return false;
    if (!eglInitialize(g_display, nullptr, nullptr)) return false;

    EGLint attribs[] = {
        EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT,
        EGL_RED_SIZE,   8,
        EGL_GREEN_SIZE, 8,
        EGL_BLUE_SIZE,  8,
        EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 0,
        EGL_STENCIL_SIZE, 0,
        EGL_NONE
    };
    EGLConfig cfg; EGLint num = 0;
    if (!eglChooseConfig(g_display, attribs, &cfg, 1, &num) || num == 0) return false;

    EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    g_context = eglCreateContext(g_display, cfg, EGL_NO_CONTEXT, ctxAttribs);
    if (g_context == EGL_NO_CONTEXT) return false;

    g_surface = eglCreateWindowSurface(g_display, cfg, (EGLNativeWindowType)win, nullptr);
    if (g_surface == EGL_NO_SURFACE) return false;

    return eglMakeCurrent(g_display, g_surface, g_surface, g_context);
}

static void* render_thread(void*) {
    while (g_running) {
        if (!g_window) { usleep(10000); continue; }

        eglMakeCurrent(g_display, g_surface, g_surface, g_context);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();

        DrawMenu();
        DrawESP();

        ImGui::Render();

        int w = (int)ImGui::GetIO().DisplaySize.x;
        int h = (int)ImGui::GetIO().DisplaySize.y;
        glViewport(0, 0, w, h);
        glClearColor(0.f, 0.f, 0.f, 0.f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        eglSwapBuffers(g_display, g_surface);

        usleep(16000);
    }
    return nullptr;
}

extern "C" JNIEXPORT void JNICALL
Java_com_flexcity_cheat_MainActivity_nativeInit(JNIEnv* env, jobject, jobject surface) {
    g_window = ANativeWindow_fromSurface(env, surface);
    if (!g_window) return;

    if (!setup_egl(g_window)) return;

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;

    ImGui_ImplAndroid_Init(g_window);
    ImGui_ImplOpenGL3_Init("#version 300 es");

    g_running = true;
    pthread_t t;
    pthread_create(&t, nullptr, render_thread, nullptr);
}

extern "C" JNIEXPORT void JNICALL
Java_com_flexcity_cheat_MainActivity_nativeShutdown(JNIEnv*, jobject) {
    g_running = false;
    usleep(120000);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplAndroid_Shutdown();
    ImGui::DestroyContext();

    if (g_display != EGL_NO_DISPLAY) {
        eglMakeCurrent(g_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (g_context != EGL_NO_CONTEXT) eglDestroyContext(g_display, g_context);
        if (g_surface != EGL_NO_SURFACE) eglDestroySurface(g_display, g_surface);
        eglTerminate(g_display);
    }
    if (g_window) {
        ANativeWindow_release(g_window);
        g_window = nullptr;
    }
}
MAIN_EOF

# ─── MainActivity.java ───────────────────────────────────────
cat > app/src/main/java/com/flexcity/cheat/MainActivity.java <<'JAVA_EOF'
package com.flexcity.cheat;

import android.app.Activity;
import android.os.Bundle;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
    static { System.loadLibrary("native-lib"); }

    private SurfaceView surfaceView;

    @Override
    protected void onCreate(Bundle b) {
        super.onCreate(b);
        surfaceView = new SurfaceView(this);
        surfaceView.getHolder().addCallback(this);
        setContentView(surfaceView);
    }

    @Override public void surfaceCreated(SurfaceHolder h) {
        nativeInit(h.getSurface());
    }
    @Override public void surfaceChanged(SurfaceHolder h, int f, int w, int ht) {}
    @Override public void surfaceDestroyed(SurfaceHolder h) {
        nativeShutdown();
    }

    private native void nativeInit(Surface s);
    private native void nativeShutdown();
}
JAVA_EOF

echo "[*] compilando..."
./build.sh

echo ""
echo "[+] Pronto."
echo "    .so gerado em: ~/FlexCityCheat/libs/arm64-v8a/libnative-lib.so"
echo "    para recompilar depois:  cd ~/FlexCityCheat && ./build.sh"
echo "    se quiser mudar offset de Transform nativo:  nano jni/Cheat.cpp  (procure NATIVE_TRANSFORM_POS_OFF)"
