#include "Cheat.h"
#include "il2cpp_api.h"
#include <mutex>
#include <atomic>
#include <thread>
#include <unistd.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <signal.h>
#include <setjmp.h>

using namespace il2cpp_api;

AimbotConfig g_cfg;

static uint64_t g_local_player = 0;
static std::vector<uint64_t> g_enemies;
static std::mutex g_enemies_mtx;
static Matrix4x4 g_view_matrix = {};
static std::mutex g_view_mtx;
static std::atomic<bool> g_running{false};

static sigjmp_buf g_jmp;
static volatile bool g_jmp_active = false;
static void signal_handler(int sig) { if (g_jmp_active) siglongjmp(g_jmp, 1); }
static void install_signal_guard() {
    struct sigaction sa = {};
    sa.sa_handler = signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGBUS, &sa, nullptr);
}

// ─────────────────────────────────────────────
// Hook do TryShoot via MethodInfo (sem Dobby)
// ─────────────────────────────────────────────
typedef void (*TryShootFn)(void* self, Vector3 origin, Vector3 direction);
static TryShootFn g_tryShootOrig = nullptr;
static MethodInfo* g_tryShootMethod = nullptr;
static Vector3 g_silent_target = {};
static volatile bool g_silent_has_target = false;

static void tryShoot_hook(void* self, Vector3 origin, Vector3 direction) {
    if (g_cfg.silent_aim && g_silent_has_target) {
        Vector3 dir = { g_silent_target.x - origin.x, g_silent_target.y - origin.y, g_silent_target.z - origin.z };
        float len = sqrtf(dir.x*dir.x + dir.y*dir.y + dir.z*dir.z);
        if (len > 0.1f) {
            Vector3 want = { dir.x / len, dir.y / len, dir.z / len };
            float orig_len = sqrtf(direction.x*direction.x + direction.y*direction.y + direction.z*direction.z);
            if (orig_len > 0.001f) {
                Vector3 o = { direction.x/orig_len, direction.y/orig_len, direction.z/orig_len };
                float cos_angle = o.x*want.x + o.y*want.y + o.z*want.z;
                cos_angle = fmaxf(-1.0f, fminf(1.0f, cos_angle));
                float angle_deg = acosf(cos_angle) * 180.0f / 3.14159265358979f;
                if (angle_deg <= g_cfg.max_snap_deg) direction = want;
            }
        }
    }
    if (g_tryShootOrig) g_tryShootOrig(self, origin, direction);
}

static void install_tryshoot_hook() {
    Il2CppClass* klass = find_class_any("", "WeaponSync");
    if (!klass) { LOGI("WeaponSync nao encontrado"); return; }

    // TryShoot tem 2 parametros (origin, direction) + this implicito
    MethodInfo* m = (MethodInfo*)class_get_method(klass, "TryShoot", 2);
    if (!m) { LOGI("TryShoot nao encontrado"); return; }

    g_tryShootMethod = m;
    g_tryShootOrig = (TryShootFn)m->methodPointer;
    m->methodPointer = (Il2CppMethodPointer)tryShoot_hook;

    LOGI("TryShoot hookado via MethodInfo: %p -> %p", g_tryShootOrig, tryShoot_hook);
}

// ─────────────────────────────────────────────
// Offsets confirmados pelo dump
// CharacterLinker: baseCharacter 0x30, playerEventer 0x38, passiveMode 0x40,
//                  hpSync 0x68, playerSync 0x78, characterThirdPerson 0x90
// CharacterHpSync: hp 0x68, maxHp 0x6A, dead 0x6E
// ─────────────────────────────────────────────
static void* get_transform_position(void* transform) {
    if (!transform) return nullptr;
    void* native = *(void**)((char*)transform + 0x10);
    if (!native) return nullptr;
    void* td = *(void**)((char*)native + 0x18);
    if (!td) return nullptr;
    uint32_t idx = *(uint32_t*)((char*)native + 0x20);
    void* arr = *(void**)((char*)td + 0x08);
    if (!arr) return nullptr;
    return (void*)((char*)arr + 0x30 * idx + 0x10);
}

static Vector3 read_transform_pos(void* transform) {
    void* p = get_transform_position(transform);
    if (!p) return {0,0,0};
    return *(Vector3*)p;
}

static void* get_cl_field(uint64_t linker, size_t off) {
    if (!linker) return nullptr;
    return *(void**)((char*)linker + off);
}

static void read_health(uint64_t linker, int16_t& hp, int16_t& maxHp, bool& dead) {
    void* hpSync = get_cl_field(linker, 0x68);
    if (!hpSync) { hp = maxHp = 0; dead = true; return; }
    hp = *(int16_t*)((char*)hpSync + 0x68);
    maxHp = *(int16_t*)((char*)hpSync + 0x6A);  // corrigido: era 0x6C
    bool flagDead = *(bool*)((char*)hpSync + 0x6E);
    dead = flagDead || hp <= 0;
}

static bool is_passive(uint64_t linker) { return get_cl_field(linker, 0x40) != nullptr; }

static Vector3 get_head_pos(uint64_t linker) {
    void* ct = get_cl_field(linker, 0x90);
    if (!ct) return {0,0,0};
    void* head_tr = *(void**)((char*)ct + 0x88);
    if (!head_tr) return {0,0,0};
    return read_transform_pos(head_tr);
}

static Vector3 get_foot_pos(uint64_t linker) {
    void* ct = get_cl_field(linker, 0x90);
    if (!ct) return {0,0,0};
    void* foot_tr = *(void**)((char*)ct + 0x90);
    if (!foot_tr) return {0,0,0};
    return read_transform_pos(foot_tr);
}

static void read_view_matrix() {
    if (!g_local_player) return;
    void* base_char = get_cl_field(g_local_player, 0x30);
    if (!base_char) return;
    void* cam_target = *(void**)((char*)base_char + 0x20);
    if (!cam_target) return;
    void* native = *(void**)((char*)cam_target + 0x10);
    if (!native) return;
    Matrix4x4 m = *(Matrix4x4*)((char*)native + 0x1B4);
    std::lock_guard<std::mutex> lk(g_view_mtx);
    g_view_matrix = m;
}

// ─────────────────────────────────────────────
// Coleta de inimigos
// ─────────────────────────────────────────────
static MethodInfo* g_find_objects_m = nullptr;
static Il2CppClass* g_char_linker_klass = nullptr;

static std::vector<uint64_t> try_find_objects() {
    std::vector<uint64_t> out;
    if (!g_find_objects_m || !g_char_linker_klass) return out;
    Il2CppObject* type_obj = type_get_object(class_get_type(g_char_linker_klass));
    if (!type_obj) return out;
    void* args[1] = { type_obj };
    void* exception = nullptr;
    Il2CppObject* result = runtime_invoke(g_find_objects_m, nullptr, args, (Il2CppException**)&exception);
    if (exception || !result) return out;
    uint32_t len = *(uint32_t*)((char*)result + 0x18);
    if (len == 0 || len > 256) return out;
    void** elements = (void**)((char*)result + 0x20);
    for (uint32_t i = 0; i < len; i++) {
        uint64_t p = (uint64_t)elements[i];
        if (p) out.push_back(p);
    }
    return out;
}

static std::vector<uint64_t> try_player_sync() {
    std::vector<uint64_t> out;
    if (!g_local_player) return out;
    uint64_t psync = (uint64_t)get_cl_field(g_local_player, 0x78);
    if (!psync) return out;
    const uintptr_t offs[] = { 0x28, 0x30, 0x38, 0x40 };
    for (uintptr_t off : offs) {
        uint64_t list_ptr = *(uint64_t*)(psync + off);
        if (!list_ptr) continue;
        uint64_t items = *(uint64_t*)(list_ptr + 0x10);
        int32_t size = *(int32_t*)(list_ptr + 0x18);
        if (!items || size <= 0 || size > 256) continue;
        for (int32_t i = 0; i < size; i++) {
            uint64_t p = *(uint64_t*)(items + 0x20 + i * 8);
            if (p) out.push_back(p);
        }
        if (!out.empty()) break;
    }
    return out;
}

static void Cheat_Tick() {
    if (!g_cfg.enabled || !g_local_player) { g_silent_has_target = false; return; }
    Matrix4x4 vm;
    { std::lock_guard<std::mutex> lk(g_view_mtx); vm = g_view_matrix; }
    std::vector<uint64_t> enemies;
    { std::lock_guard<std::mutex> lk(g_enemies_mtx); enemies = g_enemies; }
    Vector3 eye = get_head_pos(g_local_player);
    if (eye.x == 0 && eye.y == 0 && eye.z == 0) eye = get_foot_pos(g_local_player);
    if (eye.x == 0 && eye.y == 0 && eye.z == 0) return;
    float cam_yaw = atan2f(-vm.m[2], -vm.m[10]) * 180.0f / 3.14159265f;
    float best_angle = g_cfg.fov_deg;
    Vector3 best_target = {};
    bool found = false;
    for (uint64_t p : enemies) {
        if (p == g_local_player) continue;
        Vector3 head = get_head_pos(p);
        if (head.x == 0 && head.y == 0 && head.z == 0) continue;
        float dx = head.x - eye.x;
        float dy = head.y - eye.y;
        float dz = head.z - eye.z;
        float dist = sqrtf(dx*dx + dy*dy + dz*dz);
        if (dist < 1.0f) continue;
        float target_yaw = atan2f(dx, dz) * 180.0f / 3.14159265f;
        float delta = target_yaw - cam_yaw;
        while (delta > 180.0f) delta -= 360.0f;
        while (delta < -180.0f) delta += 360.0f;
        float angle = fabsf(delta);
        if (angle > g_cfg.fov_deg) continue;
        if (angle < best_angle) { best_angle = angle; best_target = head; found = true; }
    }
    if (found) { g_silent_target = best_target; g_silent_has_target = true; }
    else g_silent_has_target = false;
}

static void cheat_thread() {
    LOGI("cheat thread iniciada");
    install_signal_guard();
    il2cpp_api::init();
    if (!il2cpp_api::domain_get) { LOGI("il2cpp falhou"); return; }

    g_char_linker_klass = find_class_any("", "CharacterLinker");
    if (!g_char_linker_klass) { LOGI("CharacterLinker nao encontrado"); return; }

    // Instance static: offset 0x0 (confirmado pelo dump)
    g_local_player = (uint64_t)get_static_field(g_char_linker_klass, "ADBAABCBCCDCCADAACDDADD");
    LOGI("Local Player: 0x%lx", g_local_player);

    install_tryshoot_hook();

    Il2CppClass* obj_klass = find_class_any("UnityEngine", "Object");
    if (obj_klass) {
        g_find_objects_m = (MethodInfo*)class_get_method(obj_klass, "FindObjectsOfType", 1);
        LOGI("FindObjectsOfType: %p", g_find_objects_m);
    }

    g_running = true;
    while (g_running) {
        std::vector<uint64_t> enemies;
        if (sigsetjmp(g_jmp, 1) == 0) {
            g_jmp_active = true;
            enemies = try_find_objects();
            if (enemies.empty()) enemies = try_player_sync();
            g_jmp_active = false;
        } else g_jmp_active = false;
        if (!enemies.empty()) { std::lock_guard<std::mutex> lk(g_enemies_mtx); g_enemies = enemies; }
        read_view_matrix();
        Cheat_Tick();
        usleep(16000);
    }
}

void Cheat_Init() { std::thread(cheat_thread).detach(); }

int Cheat_GetTargets(TargetData* out, int max) {
    std::lock_guard<std::mutex> lk(g_enemies_mtx);
    int count = 0;
    Vector3 local_pos = g_local_player ? get_foot_pos(g_local_player) : Vector3{0,0,0};
    for (uint64_t p : g_enemies) {
        if (count >= max) break;
        int16_t hp = 0, maxHp = 0; bool dead = false;
        read_health(p, hp, maxHp, dead);
        if (dead || maxHp <= 0) continue;
        if (is_passive(p)) continue;
        TargetData& t = out[count];
        t.pos = get_foot_pos(p);
        t.head = get_head_pos(p);
        if (t.pos.x == 0 && t.pos.y == 0 && t.pos.z == 0) continue;
        t.hp = hp; t.maxHp = maxHp;
        float dx = t.pos.x - local_pos.x;
        float dy = t.pos.y - local_pos.y;
        float dz = t.pos.z - local_pos.z;
        t.distance = sqrtf(dx*dx + dy*dy + dz*dz);
        t.name[0] = 0;
        t.valid = true;
        t.is_local = (p == g_local_player);
        if (t.is_local) continue;
        count++;
    }
    return count;
}

Matrix4x4 Cheat_GetViewMatrix() {
    std::lock_guard<std::mutex> lk(g_view_mtx);
    return g_view_matrix;
}