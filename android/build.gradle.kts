// Applied into :app's own build script via apply(from = ...) - see
// android/app/build.gradle.kts's generic per-module loop, which does this
// for any enabled module that ships this exact file. This is NOT a
// separate Gradle subproject - it executes in :app's own Project context,
// so a plain dependencies{} block here behaves exactly as if it had been
// written directly in :app's build.gradle.kts, without the engine ever
// having to know this module - or Samsung IAP - exists. Configurations are
// added by string name ("implementation", not the typed implementation(...)
// function) - Gradle's type-safe accessors for a project's own
// configurations aren't generated for a script applied this way via
// apply(from = ...).
//
// Samsung IAP is not a Maven dependency at all - Samsung distributes it
// only as a downloadable .aar gated behind a Samsung Developer account
// login, so the developer places it under this module's own
// third_party/ (gitignored) themselves, the same "developer-provided,
// never committed" rule the desktop backends' vendored SDKs already
// follow. A missing .aar is a clear GradleException naming where to put
// it, not a silent skip or a confusing Kotlin "unresolved reference"
// error - the same bar CMakeLists.txt's own FATAL_ERROR already sets for a
// missing desktop SDK.
val nxEngineRoot = project.extra["nxEngineRoot"] as java.io.File
val galaxyStoreLibs = nxEngineRoot.resolve("modules/store_galaxy_store/third_party")
val galaxyStoreAars = galaxyStoreLibs.takeIf { it.isDirectory }
    ?.listFiles { file -> file.extension == "aar" } ?: emptyArray()
if (galaxyStoreAars.isEmpty()) {
    throw GradleException(
        "store_galaxy_store requires the Samsung IAP SDK .aar under " +
            "${galaxyStoreLibs.canonicalFile.invariantSeparatorsPath} - " +
            "download it from the Samsung Developer site (requires a " +
            "Samsung account) and place it there",
    )
}
dependencies {
    "implementation"(fileTree(galaxyStoreLibs) { include("*.aar") })
}
