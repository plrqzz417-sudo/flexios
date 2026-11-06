// C++ / il2cpp_api.h / iOS ARM64 IL2CPP
// Adicionados: class_get_methods, type_get_name.

#pragma once
#include <sys/types.h>
#include <mutex>
#include <dlfcn.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <android/log.h>
#include "Platform.h"
#include "ApkTrace.h"
#ifdef JM_APK
#include "ApkRuntimeReady.h"
#include <sys/uio.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

namespace il2cpp_api {

    struct Il2CppObject;
    struct Il2CppClass;
    struct Il2CppImage;
    struct Il2CppAssembly;
    struct Il2CppDomain;
    struct Il2CppThread;
    struct FieldInfo;
    struct Il2CppType;
    struct Il2CppException;
    struct Il2CppArray;
    struct Il2CppString;
    // This game's runtime uses pointer-sized GC handles. Its get/free exports
    // mask the complete x0 address to locate the handle table (RVA 0x18e0d9c).
    // A uint32_t handle truncates the table address on ARM64 at target acquisition.
    using GCHandle = uintptr_t;
    static_assert(sizeof(GCHandle)==sizeof(void*), "GC handles must retain every address bit");
    using gchandle_new_fn = GCHandle (*)(Il2CppObject*, bool);
    using gchandle_free_fn = void (*)(GCHandle);
    using gchandle_get_target_fn = Il2CppObject* (*)(GCHandle);
    inline gchandle_new_fn gchandle_new = nullptr;
    inline gchandle_free_fn gchandle_free = nullptr;
    inline gchandle_get_target_fn gchandle_get_target = nullptr;
    using string_length_fn = int32_t (*)(Il2CppString*);
    using string_chars_fn = const uint16_t* (*)(Il2CppString*);
    inline string_length_fn string_length = nullptr;
    inline string_chars_fn string_chars = nullptr;

    using method_get_param_fn = const Il2CppType* (*)(const struct MethodInfo*, uint32_t);
    inline method_get_param_fn method_get_param = nullptr;
    using method_get_name_fn = const char* (*)(const struct MethodInfo*);
    using method_get_param_count_fn = uint32_t (*)(const struct MethodInfo*);
    inline method_get_name_fn method_get_name = nullptr;
    inline method_get_param_count_fn method_get_param_count = nullptr;

    typedef void (*Il2CppMethodPointer)();
    typedef void (*InvokerMethod)(Il2CppMethodPointer, const void*, void*, void**, void*);

    struct MethodInfo {
        Il2CppMethodPointer methodPointer;
        Il2CppMethodPointer virtualMethodPointer;
        InvokerMethod invoker_method;
        const char* name;
        Il2CppClass* klass;
        const Il2CppType* return_type;
        const Il2CppType** parameters;
        union { const void* rgctx_data; const void* methodMetadataHandle; };
        union { const void* genericMethod; const void* genericContainerHandle; };
        uint32_t token;
        uint16_t flags;
        uint16_t iflags;
        uint16_t slot;
        uint8_t parameters_count;
        uint8_t is_generic : 1;
        uint8_t is_inflated : 1;
        uint8_t wrapper_type : 1;
        uint8_t has_full_generic_sharing_signature : 1;
        uint8_t indirect_call_via_invokers : 1;
    };

    using domain_get_fn            = Il2CppDomain* (*)();
    using domain_get_assemblies_fn = const Il2CppAssembly** (*)(const Il2CppDomain*, size_t*);
    using assembly_get_image_fn    = Il2CppImage* (*)(const Il2CppAssembly*);
    using image_get_name_fn        = const char* (*)(const Il2CppImage*);
    using image_get_class_count_fn = size_t (*)(const Il2CppImage*);
    using image_get_class_fn       = Il2CppClass* (*)(const Il2CppImage*, size_t);
    using class_from_name_fn       = Il2CppClass* (*)(const Il2CppImage*, const char*, const char*);
    using class_get_type_fn        = const Il2CppType* (*)(Il2CppClass*);
    using class_get_name_fn        = const char* (*)(Il2CppClass*);
    using class_get_namespace_fn   = const char* (*)(Il2CppClass*);
    using class_get_parent_fn      = Il2CppClass* (*)(Il2CppClass*);
    using class_get_static_data_fn = void* (*)(Il2CppClass*);
    using class_get_method_fn      = MethodInfo* (*)(Il2CppClass*, const char*, int);
    using class_get_methods_fn     = const MethodInfo* (*)(Il2CppClass*, void**);
    using class_get_fields_fn      = FieldInfo* (*)(Il2CppClass*, void**);
    using class_get_field_fn       = FieldInfo* (*)(Il2CppClass*, const char*);
    using field_get_offset_fn      = size_t (*)(FieldInfo*);
    using field_get_name_fn        = const char* (*)(FieldInfo*);
    using field_get_type_fn        = const Il2CppType* (*)(FieldInfo*);
    using field_get_value_fn       = void (*)(Il2CppObject*, FieldInfo*, void*);
    using field_set_value_fn       = void (*)(Il2CppObject*, FieldInfo*, void*);
    using field_static_get_fn      = void (*)(FieldInfo*, void*);
    using field_static_set_fn      = void (*)(FieldInfo*, void*);
    using type_get_object_fn       = Il2CppObject* (*)(const Il2CppType*);
    using type_get_name_fn         = const char* (*)(const Il2CppType*);
    using runtime_invoke_fn        = Il2CppObject* (*)(const MethodInfo*, void*, void**, Il2CppException**);
    using object_new_fn            = Il2CppObject* (*)(Il2CppClass*);
    using object_unbox_fn          = void* (*)(Il2CppObject*);
    using array_length_fn          = uint32_t (*)(Il2CppArray*);
    using thread_current_fn        = Il2CppThread* (*)();
    using thread_attach_fn         = Il2CppThread* (*)(Il2CppDomain*);
    using thread_detach_fn         = void (*)(Il2CppThread*);
    using gc_foreach_heap_fn       = void (*)(void(*func)(void*, void*), void* userData);
    using gc_heap_usage_fn         = int64_t (*)();
    using gc_heap_foreach_alt_fn   = void (*)(void(*func)(void*, void*), void* userData);

    inline domain_get_fn            domain_get            = nullptr;
    inline domain_get_assemblies_fn domain_get_assemblies = nullptr;
    inline assembly_get_image_fn    assembly_get_image    = nullptr;
    inline image_get_name_fn        image_get_name        = nullptr;
    inline image_get_class_count_fn image_get_class_count = nullptr;
    inline image_get_class_fn       image_get_class       = nullptr;
    inline class_from_name_fn       class_from_name       = nullptr;
    inline class_get_type_fn        class_get_type        = nullptr;
    inline class_get_name_fn        class_get_name        = nullptr;
    inline class_get_namespace_fn   class_get_namespace   = nullptr;
    inline class_get_parent_fn      class_get_parent      = nullptr;
    inline class_get_static_data_fn class_get_static_data = nullptr;
    inline class_get_method_fn      class_get_method      = nullptr;
    inline class_get_methods_fn     class_get_methods     = nullptr;
    inline class_get_fields_fn      class_get_fields      = nullptr;
    inline class_get_field_fn       class_get_field       = nullptr;
    inline field_get_offset_fn      field_get_offset      = nullptr;
    inline field_get_name_fn        field_get_name        = nullptr;
    inline field_get_type_fn        field_get_type        = nullptr;
    inline field_get_value_fn       field_get_value       = nullptr;
    inline field_set_value_fn       field_set_value       = nullptr;
    inline field_static_get_fn      field_static_get      = nullptr;
    inline field_static_set_fn      field_static_set      = nullptr;
    inline type_get_object_fn       type_get_object       = nullptr;
    inline type_get_name_fn         type_get_name         = nullptr;
    inline runtime_invoke_fn        runtime_invoke        = nullptr;
    inline object_new_fn            object_new            = nullptr;
    inline object_unbox_fn          object_unbox          = nullptr;
    inline array_length_fn          array_length          = nullptr;
    inline thread_current_fn        thread_current        = nullptr;
    inline thread_attach_fn         thread_attach         = nullptr;
    inline thread_detach_fn         thread_detach         = nullptr;
    inline gc_foreach_heap_fn       gc_foreach_heap       = nullptr;
    inline gc_heap_usage_fn         gc_heap_usage         = nullptr;
    inline gc_heap_foreach_alt_fn   gc_heap_foreach_alt   = nullptr;

    inline void* libil2cpp_handle = nullptr;
    inline Il2CppDomain* attached_domain = nullptr;

    inline std::mutex init_mutex;

    inline void init() {
        std::lock_guard<std::mutex> lock(init_mutex);
        if (domain_get && attached_domain) {
            apk_trace::record("il2cpp.thread_attach.cached.begin");
            if (thread_attach) thread_attach(attached_domain);
            apk_trace::record("il2cpp.thread_attach.cached.ok");
            return;
        }

        if (!libil2cpp_handle) {
            apk_trace::record("il2cpp.dlopen_noload.begin");
            libil2cpp_handle = jm_ios_unity_handle();
            apk_trace::record(libil2cpp_handle?"il2cpp.dlopen_noload.found":"il2cpp.dlopen_noload.absent");
            if (!libil2cpp_handle) return;

            #define SYM(var, sym) var = (decltype(var))dlsym(libil2cpp_handle, sym)
            SYM(gchandle_new, "il2cpp_gchandle_new");
            SYM(gchandle_free, "il2cpp_gchandle_free");
            SYM(gchandle_get_target, "il2cpp_gchandle_get_target");
            SYM(string_length, "il2cpp_string_length");
            SYM(string_chars, "il2cpp_string_chars");
            SYM(domain_get,            "il2cpp_domain_get");
            SYM(method_get_name, "il2cpp_method_get_name");
            SYM(method_get_param_count, "il2cpp_method_get_param_count");
            SYM(method_get_param,      "il2cpp_method_get_param");
            SYM(domain_get_assemblies, "il2cpp_domain_get_assemblies");
            SYM(assembly_get_image,    "il2cpp_assembly_get_image");
            SYM(image_get_name,        "il2cpp_image_get_name");
            SYM(image_get_class_count, "il2cpp_image_get_class_count");
            SYM(image_get_class,       "il2cpp_image_get_class");
            SYM(class_from_name,       "il2cpp_class_from_name");
            SYM(class_get_type,        "il2cpp_class_get_type");
            SYM(class_get_name,        "il2cpp_class_get_name");
            SYM(class_get_namespace,   "il2cpp_class_get_namespace");
            SYM(class_get_parent,      "il2cpp_class_get_parent");
            SYM(class_get_static_data, "il2cpp_class_get_static_field_data");
            SYM(class_get_method,      "il2cpp_class_get_method_from_name");
            SYM(class_get_methods,     "il2cpp_class_get_methods");
            SYM(class_get_fields,      "il2cpp_class_get_fields");
            SYM(class_get_field,       "il2cpp_class_get_field_from_name");
            SYM(field_get_offset,      "il2cpp_field_get_offset");
            SYM(field_get_name,        "il2cpp_field_get_name");
            SYM(field_get_type,        "il2cpp_field_get_type");
            SYM(field_get_value,       "il2cpp_field_get_value");
            SYM(field_set_value,       "il2cpp_field_set_value");
            SYM(field_static_get,      "il2cpp_field_static_get_value");
            SYM(field_static_set,      "il2cpp_field_static_set_value");
            SYM(type_get_object,       "il2cpp_type_get_object");
            SYM(type_get_name,         "il2cpp_type_get_name");
            SYM(runtime_invoke,        "il2cpp_runtime_invoke");
            SYM(object_new,            "il2cpp_object_new");
            SYM(object_unbox,          "il2cpp_object_unbox");
            SYM(array_length,          "il2cpp_array_length");
            SYM(thread_current,        "il2cpp_thread_current");
            SYM(thread_attach,         "il2cpp_thread_attach");
            SYM(thread_detach,         "il2cpp_thread_detach");
            SYM(gc_foreach_heap,       "il2cpp_gc_foreach_heap");
            if (!thread_current) {
                __android_log_print(ANDROID_LOG_ERROR, "FlexCity", "il2cpp_thread_current unavailable; render IL2CPP calls disabled");
            }
            #undef SYM

            gc_heap_usage = (gc_heap_usage_fn)dlsym(libil2cpp_handle, "il2cpp_gc_get_heap_size");
            if (!gc_heap_usage) gc_heap_usage = (gc_heap_usage_fn)dlsym(libil2cpp_handle, "il2cpp_gc_get_used_size");

            gc_heap_foreach_alt = (gc_heap_foreach_alt_fn)dlsym(libil2cpp_handle, "il2cpp_gc_heap_foreach");

            __android_log_print(ANDROID_LOG_INFO, "FlexCity",
                "symbols: gc_foreach_heap=%p gc_heap_usage=%p gc_heap_foreach_alt=%p "
                "class_get_methods=%p type_get_name=%p",
                (void*)gc_foreach_heap, (void*)gc_heap_usage, (void*)gc_heap_foreach_alt,
                (void*)class_get_methods, (void*)type_get_name);

            if (!domain_get) {
                __android_log_print(ANDROID_LOG_ERROR, "FlexCity", "dlsym il2cpp falhou");
                dlclose(libil2cpp_handle);
                libil2cpp_handle = nullptr;
                return;
            }
            __android_log_print(ANDROID_LOG_INFO, "FlexCity", "il2cpp OK (handle=%p)", libil2cpp_handle);
        }

        if (!attached_domain) {
#ifdef JM_APK
            // domain_get is a lazy initializer, not a safe readiness probe.
            // Wait for Unity's verified late-init flag and existing domain.
            Dl_info image{};
            if(!dladdr((void*)domain_get,&image)||!image.dli_fbase)return;
            auto readiness=apk_runtime::check((uintptr_t)image.dli_fbase,(uintptr_t)domain_get,
                [](unsigned long long address,auto& value) {
                    iovec local{&value,sizeof(value)},remote{(void*)(uintptr_t)address,sizeof(value)};
                    return syscall(SYS_process_vm_readv,getpid(),&local,1,&remote,1,0)==ssize_t(sizeof(value));
                });
            if(readiness!=apk_runtime::State::Ready) {
                apk_trace::record(readiness==apk_runtime::State::Waiting?"il2cpp.runtime.waiting_for_unity":
                    readiness==apk_runtime::State::ReadFailed?"il2cpp.runtime.probe_unreadable":"il2cpp.runtime.signature_rejected");
                return;
            }
            apk_trace::record("il2cpp.runtime.ready");
#endif
            apk_trace::record("il2cpp.domain_get.begin");
            Il2CppDomain* d = domain_get ? domain_get() : nullptr;
            apk_trace::record(d?"il2cpp.domain_get.present":"il2cpp.domain_get.empty");
            if (!d) return;
            apk_trace::record("il2cpp.thread_attach.first.begin");
            if (thread_attach) thread_attach(d);
            apk_trace::record("il2cpp.thread_attach.first.ok");
            attached_domain = d;
            __android_log_print(ANDROID_LOG_INFO, "FlexCity", "thread anexada ao il2cpp");
        }
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

    inline Il2CppClass* find_class_by_iter(const char* name) {
        Il2CppImage* img = get_game_image();
        if (!img) return nullptr;
        if (!image_get_class_count || !image_get_class || !class_get_name) return nullptr;
        size_t count = image_get_class_count(img);
        for (size_t i = 0; i < count; i++) {
            Il2CppClass* k = image_get_class(img, i);
            if (!k) continue;
            const char* cname = class_get_name(k);
            if (cname && strcmp(cname, name) == 0) return k;
        }
        return nullptr;
    }

    inline Il2CppClass* find_class(const char* namespace_, const char* name) {
        Il2CppImage* img = get_game_image();
        if (img) {
            Il2CppClass* k = class_from_name(img, namespace_, name);
            if (k) return k;
            if (namespace_ && namespace_[0]) {
                k = class_from_name(img, "", name);
                if (k) return k;
            }
        }
        return find_class_by_iter(name);
    }

    inline Il2CppClass* find_class_any(const char* namespace_, const char* name) {
        if (!domain_get || !domain_get_assemblies) return nullptr;
        Il2CppDomain* d = domain_get();
        if (!d) return nullptr;
        size_t n = 0;
        const Il2CppAssembly** asms = domain_get_assemblies(d, &n);
        for (size_t i = 0; i < n; i++) {
            Il2CppImage* img = assembly_get_image(asms[i]);
            if (!img) continue;
            Il2CppClass* k = class_from_name(img, namespace_, name);
            if (k) return k;
            if (namespace_ && namespace_[0]) {
                k = class_from_name(img, "", name);
                if (k) return k;
            }
        }
        return nullptr;
    }
}
