#include "store_galaxy_store/store_galaxy_store_platform.h"

#include "core/foundation/platform/android_jni.h"

#include "core/foundation/diagnostics/log.h"

#include <SDL3/SDL_system.h>

namespace nxm::store_galaxy_store {
namespace {

const nx::log::Category log_store_galaxy_store =
    nx::log::category("store_galaxy_store");

constexpr const char *SHIM_CLASS = "com/nx2d/runtime/NxSamsungIap";

} // namespace

jclass find_iap_shim_class(JNIEnv *const env) {
  return nx::android::find_class(env, SHIM_CLASS);
}

GalaxyStorePlatform *GalaxyStorePlatform::s_instance = nullptr;

GalaxyStorePlatform::~GalaxyStorePlatform() { shutdown(); }

bool GalaxyStorePlatform::initialize() {
  JNIEnv *const env = static_cast<JNIEnv *>(SDL_GetAndroidJNIEnv());
  jobject const activity =
      env != nullptr ? static_cast<jobject>(SDL_GetAndroidActivity()) : nullptr;
  if (env == nullptr || activity == nullptr) {
    nx::logw(log_store_galaxy_store, "no Android activity available");
    return false;
  }
  if (env->GetJavaVM(&m_vm) != JNI_OK || m_vm == nullptr) {
    nx::logw(log_store_galaxy_store, "JNI_GetJavaVM failed");
    return false;
  }
  m_activity = env->NewGlobalRef(activity);
  env->DeleteLocalRef(activity);
  if (m_activity == nullptr) {
    nx::logw(log_store_galaxy_store, "failed to hold a global ref on the activity");
    return false;
  }

  const jclass shim = find_iap_shim_class(env);
  if (shim == nullptr) {
    nx::logw(log_store_galaxy_store,
              "NxSamsungIap.class not found - was the module enabled when "
              "the APK was built?");
    return false;
  }
  const jmethodID connect = nx::android::static_method(
      env, shim, "connect", "(Landroid/app/Activity;)V");
  if (connect == nullptr) {
    env->DeleteLocalRef(shim);
    return false;
  }

  s_instance = this;
  env->CallStaticVoidMethod(shim, connect, m_activity);
  if (env->ExceptionCheck())
    env->ExceptionClear();
  env->DeleteLocalRef(shim);
  return true;
}

void GalaxyStorePlatform::shutdown() {
  if (m_vm == nullptr)
    return;
  const nx::android::JniScope env(m_vm);
  if (env && m_activity != nullptr) {
    const jclass shim = find_iap_shim_class(env.get());
    if (shim != nullptr) {
      const jmethodID disconnect =
          nx::android::static_method(env.get(), shim, "disconnect", "()V");
      if (disconnect != nullptr)
        env->CallStaticVoidMethod(shim, disconnect);
      if (env->ExceptionCheck())
        env->ExceptionClear();
      env->DeleteLocalRef(shim);
    }
    env->DeleteGlobalRef(m_activity);
  }
  m_activity = nullptr;
  m_vm = nullptr;
  m_ready = false;
  if (s_instance == this)
    s_instance = nullptr;
}

void GalaxyStorePlatform::on_connected() {
  m_ready = true;
  nx::logi(log_store_galaxy_store, "Samsung IAP helper connected");
}

void GalaxyStorePlatform::dispatch_connected() {
  if (s_instance != nullptr)
    s_instance->on_connected();
}

}

extern "C" JNIEXPORT void JNICALL
Java_com_nx2d_runtime_NxSamsungIap_nativeOnConnected(JNIEnv *, jclass) {
  nxm::store_galaxy_store::GalaxyStorePlatform::dispatch_connected();
}
