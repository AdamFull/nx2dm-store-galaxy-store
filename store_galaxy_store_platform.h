#pragma once

#include "core/foundation/core/foundation.h"
#include "core/foundation/strings/utf8_string.h"

#include <jni.h>

namespace nxm::store_galaxy_store {

/// Looks up `com.nx2d.runtime.NxSamsungIap` - shared by the platform and
/// both services, each of which calls a different one of its static
/// methods. Returns a local ref (caller's to `DeleteLocalRef`), or nullptr
/// with any pending exception already cleared.
[[nodiscard]] jclass find_iap_shim_class(JNIEnv *env);

/// Owns the JNI handle to the Java-side static facade
/// `com.nx2d.runtime.NxSamsungIap` (contributed by this module's own
/// `modules/store_galaxy_store/android/java` tree - see
/// `android/app/build.gradle.kts`'s per-module Java source-dir loop; never
/// referenced from `NxActivity.java`).
///
/// Samsung IAP, like Google Play Billing and HMS IAP Kit, has **no
/// native/NDK API** - this class (and `store_galaxy_store_services.h`) is
/// pure JNI plumbing: call into `NxSamsungIap`'s static methods via
/// `nx::android::JniScope`, and receive results back through a handful of
/// JNI-exported C++ functions Java calls directly
/// (`Java_com_nx2d_runtime_NxSamsungIap_nativeOnXxx`, resolved by the JVM's
/// own symbol-name convention against the already-loaded `libnx2d.so`), the
/// same shape `store_google_play_platform.h`/`store_app_gallery_platform.h`
/// already established. Unlike HMS IAP Kit, `startPayment()` takes no
/// Activity/request-code pair at all and resolves purely through a
/// listener callback - this backend needs no
/// `NxActivity.ActivityResultHandler` registration.
///
/// Exactly one `GalaxyStorePlatform` (and one `GalaxyStoreCore`/
/// `GalaxyStoreIap`) is ever alive in a process, the same invariant
/// `order_modules()` already enforces for "only one store backend active" -
/// each JNI export function below dispatches through a static "current
/// instance" pointer, mirroring the other two mobile backends' own pattern.
class GalaxyStorePlatform {
public:
  ~GalaxyStorePlatform();

  bool initialize();
  void shutdown();

  [[nodiscard]] bool ready() const noexcept { return m_ready; }

  [[nodiscard]] JavaVM *vm() const noexcept { return m_vm; }
  /// A global ref on the Android `Activity` SDL created this process with -
  /// `IapHelper.getInstance()` only needs the application `Context` half of
  /// it, but holding the `Activity` costs nothing extra and matches the
  /// other two mobile backends' shape.
  [[nodiscard]] jobject activity() const noexcept { return m_activity; }

  /// Dispatch target for this module's one `nativeOnXxx` JNI export owned
  /// by the platform itself - public because a plain `extern "C"`
  /// function, not a member, is what the JVM actually calls.
  static void dispatch_connected();

private:
  void on_connected();

  JavaVM *m_vm = nullptr;
  jobject m_activity = nullptr;
  bool m_ready = false;

  static GalaxyStorePlatform *s_instance;
};

}
