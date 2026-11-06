# JM FlexMod iOS source port

Target: Flex City 1.11.31 (`com.jarvigames.viceonline`) from the supplied decrypted IPA. This tree is isolated from the Android `jni/` release. The iOS edition is labeled 1.0; no Android build/version is changed.

The port retains the internal gameplay core and ImGui menu. `Platform.mm` maps logging, self-memory reads, thread identity and UnityFramework discovery to Darwin. `InlineHook.h` adapts the existing hook calls to Dobby and rejects targets outside UnityFramework. `Auth.mm` uses Foundation networking with the existing signed KeyAuth response protocol. `Hooks.mm` renders ImGui and ESP through MetalKit and forwards touches only inside the menu or floating aim button. iOS does not have the Android stream-overlay capture exclusion, so that control is unavailable.

The iOS target uses `ios/Cheat.cpp`, `ios/Menu.cpp`, `ios/il2cpp_api.h`, the adapters above, and shared platform-independent modules from `jni/`. Do not compile `jni/Main.cpp`, `jni/Hooks.cpp`, `jni/Auth.cpp`, `jni/InlineHook.h` or the Android ImGui renderer for this target.

## Build inputs

- iPhoneOS SDK, Apple C/C++/Objective-C++ toolchain and iOS arm64 target.
- The Dear ImGui **v1.90.4** Metal backend and Dobby commit `5dfc8546954ce3b3198132ab13fddb89ee92cdd7` are included in `ios/vendor/`. CMake builds and links `dobby_static` from this source.

On a Mac with Xcode and CMake, run from the package root:

```sh
cmake -S ios -B build-ios -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_SYSTEM_PROCESSOR=arm64 \
  -DCMAKE_OSX_SYSROOT=iphoneos -DCMAKE_OSX_ARCHITECTURES=arm64 \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 -DCMAKE_BUILD_TYPE=Release
cmake --build build-ios --config Release --target jmflexmod_ios
find build-ios -name 'JMFlexMod.dylib' -print
```

The resulting `JMFlexMod.dylib` is intended for IPA injection and re-signing with Sideloadly. No dylib or IPA has been produced or tested yet. If you only have Windows, run these commands on a macOS build runner with Xcode; Sideloadly injects and signs an already built dylib, it does not compile one.

On Windows without a Mac, create a GitHub repository from this package, keeping `.github/workflows/build-ios.yml` at the repository root. In **Actions > Build JM FlexMod iOS > Run workflow**, start the macOS runner. Download the `JMFlexMod-iOS-arm64` artifact when the job succeeds. Open the decrypted 1.11.31 IPA in Sideloadly, then use **Advanced Options > Inject dylibs/frameworks** to add `JMFlexMod.dylib` and sign/install the IPA with your certificate. The workflow builds only the dylib; do not upload a license key or the IPA to the repository.

## Current validation boundary

The supplied IPA has ARM64 Mach-O UnityFramework and IL2CPP metadata v39. The source-side method-name inventory finds 113 of 114 names; `Renderer.get_forceRenderingOff` is optional. The iOS exports use pointer-sized GC handles (the get/free bodies mask full x0). `ios/Cheat.cpp` and `ios/Menu.cpp` pass ARM64 C++ syntax checks with the local Clang toolchain. These checks do not verify IL2CPP field offsets, method calling conventions, Metal/UI runtime behavior, server acceptance, or iOS code signing. A device test is required before claiming feature parity or stability.
