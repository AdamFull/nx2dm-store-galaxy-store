
option(NX_MODULE_STORE_GALAXY_STORE "Build the Samsung Galaxy Store (Samsung IAP) storefront backend" OFF)

if (NOT NX_MODULE_STORE_GALAXY_STORE)
    return()
endif ()

# nx_module()'s own PLATFORMS gate runs too late to help here - a project
# that also builds for Windows/Linux/macOS/iOS still passes
# -DNX_MODULE_STORE_GALAXY_STORE=ON for this platform-irrelevant module (its
# module list isn't per-platform), so this must skip itself before ever
# reaching that gate, the same "skipped on <platform>" outcome
# _nx_module_gate would otherwise report.
if (NOT ANDROID)
    _nx_current_platform(_here)
    _nx_module_skip_platform(store_galaxy_store "${_here}")
    return()
endif ()

# No vendor SDK to resolve here on the CMake side - Samsung IAP has no
# native/NDK API at all, only a hand-written Kotlin JNI shim
# (modules/store_galaxy_store/android/java/com/nx2d/runtime/
# NxSamsungIap.kt, picked up by android/app/build.gradle.kts's per-module
# source-dir loop) against a developer-provided .aar
# (modules/store_galaxy_store/third_party/, gitignored - unlike Google
# Play/HMS IAP Kit, Samsung's SDK is not a plain Maven dependency, so
# android/app/build.gradle.kts resolves it as a local fileTree dependency
# instead). This CMakeLists.txt only builds the C++ side of the JNI
# boundary.
nx_module(store_galaxy_store
        DISPLAY "Samsung Galaxy Store (Samsung IAP) storefront backend"
        SOURCES store_galaxy_store_module.cpp store_galaxy_store_platform.cpp
                store_galaxy_store_services.cpp
        DEPENDS nx_module_store nx::sdl
        PLATFORMS android)
