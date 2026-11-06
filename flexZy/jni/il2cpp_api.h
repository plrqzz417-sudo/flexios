#pragma once
#include <sys/types.h>
#include <dlfcn.h>
#include <link.h>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <android/log.h>

extern "C" {
    void* xdl_open(const char* filename, int flags);
    void* xdl_sym(void* handle, const char* symbol, size_t* size);
}
#define XDL_DEFAULT 0x00

namespace il2cpp_api {

    struct Il2CppObject;
    struct Il2CppClass;
    struct Il2CppImage;
    struct Il2CppAssembly;
    struct Il2CppDomain;
    struct FieldInfo;
    struct Il2CppType;
    struct Il2CppException;
    struct Il2CppArray;
    struct Il2CppString;

    typedef void (*Il2CppMethodPointer)();
    typedef void (*InvokerMethod)(Il2CppMethodPointer, const void*, void*, void**, void*);

    struct MethodInfo {
        Il2CppMethodPointer methodPointer;      // 0x0
        Il2CppMethodPointer virtualMethodPointer; // 0x8
        InvokerMethod invoker_method;           // 0x10
        const char* name;                       // 0x18
        Il2CppClass* klass;                     // 0x20
        const Il2CppType* return_type;          // 0x28
        const Il2CppType** parameters;          // 0x30
        union {
            const void* rgctx_data;
            const void* methodMetadataHandle;
        };                                      // 0x38
        union {
            const void* genericMethod;
            const void* genericContainerHandle;
        };                                      // 0x40
        uint32_t token;                         // 0x48
        uint16_t flags;                         // 0x4C
        uint16_t iflags;                        // 0x4E
        uint16_t slot;                          // 0x50
        uint8_t parameters_count;               // 0x52
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
    using runtime_invoke_fn        = Il2CppObject* (*)(const MethodInfo*, void*, void**, Il2CppException**);
    using object_new_fn            = Il2CppObject* (*)(Il2CppClass*);
    using array_length_fn          = uint32_t (*)(Il2CppArray*);
    using thread_attach_fn         = Il2CppDomain* (*)(Il2CppDomain*);
    using thread_detach_fn         = void (*)(Il2CppDomain*);

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
    inline runtime_invoke_fn        runtime_invoke        = nullptr;
    inline object_new_fn            object_new            = nullptr;
    inline array_length_fn          array_length          = nullptr;
    inline thread_attach_fn         thread_attach         = nullptr;
    inline thread_detach_fn         thread_detach         = nullptr;

    inline uintptr_t libil2cpp_base   = 0;
    inline void*     libil2cpp_handle = nullptr;

    inline void init() {
        if (libil2cpp_base && domain_get) return;

        FILE* f = fopen("/proc/self/maps", "r");
        if (f) {
            char line[512];
            while (fgets(line, sizeof(line), f)) {
                if (!strstr(line, "libil2cpp.so")) continue;
                uintptr_t start = strtoull(line, nullptr, 16);
                if (start && (!libil2cpp_base || start < libil2cpp_base)) libil2cpp_base = start;
            }
            fclose(f);
        }
        if (!libil2cpp_base) {
            __android_log_print(ANDROID_LOG_ERROR, "FlexCity", "il2cpp base nao encontrado");
            return;
        }

        void* h = xdl_open("libil2cpp.so", XDL_DEFAULT);
        if (!h) {
            __android_log_print(ANDROID_LOG_ERROR, "FlexCity", "xdl_open libil2cpp falhou");
            return;
        }
        libil2cpp_handle = h;

        #define SYM(var, sym) var = (decltype(var))xdl_sym(h, sym, nullptr)
        SYM(domain_get,            "il2cpp_domain_get");
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
        SYM(runtime_invoke,        "il2cpp_runtime_invoke");
        SYM(object_new,            "il2cpp_object_new");
        SYM(array_length,          "il2cpp_array_length");
        SYM(thread_attach,         "il2cpp_thread_attach");
        SYM(thread_detach,         "il2cpp_thread_detach");
        #undef SYM

        if (!domain_get) {
            __android_log_print(ANDROID_LOG_ERROR, "FlexCity", "dlsym il2cpp falhou");
            return;
        }
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

    inline Il2CppClass* find_class(const char* namespace_, const char* name) {
        Il2CppImage* img = get_game_image();
        if (!img) return nullptr;
        Il2CppClass* k = class_from_name(img, namespace_, name);
        if (!k && namespace_ && namespace_[0]) k = class_from_name(img, "", name);
        return k;
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

    inline void* get_static_field(Il2CppClass* klass, const char* field_name) {
        if (!klass || !class_get_static_data || !field_get_offset) return nullptr;
        void* static_data = class_get_static_data(klass);
        if (!static_data) return nullptr;
        if (class_get_field) {
            FieldInfo* fi = class_get_field(klass, field_name);
            if (fi) {
                size_t off = field_get_offset(fi);
                return *(void**)((char*)static_data + off);
            }
        }
        return nullptr;
    }

    inline void* rva(uintptr_t r) { return (void*)(libil2cpp_base + r); }
}