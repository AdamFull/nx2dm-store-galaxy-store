#include "store_galaxy_store/store_galaxy_store_services.h"

#include "core/foundation/platform/android_jni.h"

#include "core/foundation/diagnostics/log.h"
#include "core/foundation/strings/format.h"

#include <utility>

namespace nxm::store_galaxy_store {
namespace {

const nx::log::Category log_store_galaxy_store =
    nx::log::category("store_galaxy_store");

/// Looks up and calls a static void method on the Java shim by name/
/// signature, forwarding whatever jvalue args the caller already built -
/// the same helper `store_google_play_services.cpp`/
/// `store_app_gallery_services.cpp` already established, so it's
/// centralized once here rather than repeating the FindClass/
/// GetStaticMethodID/exception-clear dance per call site.
void call_shim_static_void(JNIEnv *const env, const char *const name,
                           const char *const signature, jvalue *const args) {
  const jclass shim = find_iap_shim_class(env);
  if (shim == nullptr)
    return;
  const jmethodID method = nx::android::static_method(env, shim, name, signature);
  if (method != nullptr) {
    env->CallStaticVoidMethodA(shim, method, args);
    if (env->ExceptionCheck())
      env->ExceptionClear();
  }
  env->DeleteLocalRef(shim);
}

} // namespace

// -- GalaxyStoreCore --------------------------------------------------------

GalaxyStoreCore *GalaxyStoreCore::s_instance = nullptr;

GalaxyStoreCore::GalaxyStoreCore(GalaxyStorePlatform &platform) noexcept
    : m_platform(platform) {
  s_instance = this;
}

GalaxyStoreCore::~GalaxyStoreCore() {
  if (s_instance == this)
    s_instance = nullptr;
}

bool GalaxyStoreCore::is_owned(const nx::string_view dlc_id) const {
  if (!m_platform.ready())
    return false;
  if (dlc_id.empty())
    return true;
  for (const nx::string &id : m_owned_dlc_ids)
    if (id.view() == dlc_id)
      return true;
  return false;
}

void GalaxyStoreCore::refresh_ownership() {
  if (!m_platform.ready())
    return;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return;
  call_shim_static_void(env.get(), "obtainOwnedProducts", "()V", nullptr);
}

void GalaxyStoreCore::on_owned_products_queried(const jboolean success,
                                                const jobjectArray product_ids) {
  if (success != JNI_TRUE)
    return;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return;
  m_owned_dlc_ids = nx::android::to_nx_string_vector(env.get(), product_ids);
}

void GalaxyStoreCore::dispatch_owned_products_queried(const jboolean success,
                                                      const jobjectArray product_ids) {
  if (s_instance != nullptr)
    s_instance->on_owned_products_queried(success, product_ids);
}

// -- GalaxyStoreIap -----------------------------------------------------------

GalaxyStoreIap *GalaxyStoreIap::s_instance = nullptr;

GalaxyStoreIap::GalaxyStoreIap(GalaxyStorePlatform &platform) noexcept
    : m_platform(platform) {
  s_instance = this;
}

GalaxyStoreIap::~GalaxyStoreIap() {
  if (s_instance == this)
    s_instance = nullptr;
}

bool GalaxyStoreIap::purchase(const nx::string_view product_id) {
  if (!m_platform.ready())
    return false;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return false;
  const jstring id = nx::android::to_jstring(env.get(), product_id);
  if (id == nullptr)
    return false;

  m_purchase_pending = true;
  m_purchase_error = nx::string{};

  jvalue args[1];
  args[0].l = id;
  call_shim_static_void(env.get(), "purchase", "(Ljava/lang/String;)V", args);
  env->DeleteLocalRef(id);
  return true;
}

void GalaxyStoreIap::refresh_products(const nx::vector<nx::string> &product_ids) {
  if (!m_platform.ready())
    return;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return;
  const jobjectArray ids = nx::android::to_jstring_array(env.get(), product_ids);
  if (ids == nullptr)
    return;
  jvalue args[1];
  args[0].l = ids;
  call_shim_static_void(env.get(), "obtainProductDetails", "([Ljava/lang/String;)V",
                        args);
  env->DeleteLocalRef(ids);
}

void GalaxyStoreIap::on_product_details_response(
    const jboolean success, const jobjectArray product_ids,
    const jobjectArray titles, const jobjectArray formatted_prices) {
  if (success != JNI_TRUE)
    return;
  const nx::android::JniScope env(m_platform.vm());
  if (!env)
    return;
  const nx::vector<nx::string> ids = nx::android::to_nx_string_vector(env.get(), product_ids);
  const nx::vector<nx::string> names = nx::android::to_nx_string_vector(env.get(), titles);
  const nx::vector<nx::string> prices =
      nx::android::to_nx_string_vector(env.get(), formatted_prices);

  nx::vector<store::StoreProduct> products;
  products.reserve(ids.size());
  for (usize i = 0; i < ids.size(); ++i) {
    store::StoreProduct product;
    product.id = ids[i];
    product.title = i < names.size() ? names[i] : nx::string{};
    product.price_display = i < prices.size() ? prices[i] : nx::string{};
    products.push_back(std::move(product));
  }
  m_products = std::move(products);
}

void GalaxyStoreIap::on_purchase_result(const jboolean success, const jstring) {
  m_purchase_pending = false;
  if (success == JNI_TRUE)
    m_purchase_error = nx::string{};
  else
    m_purchase_error = nx::string("Samsung IAP purchase failed");
}

void GalaxyStoreIap::dispatch_product_details_response(
    const jboolean success, const jobjectArray product_ids,
    const jobjectArray titles, const jobjectArray formatted_prices) {
  if (s_instance != nullptr)
    s_instance->on_product_details_response(success, product_ids, titles,
                                            formatted_prices);
}

void GalaxyStoreIap::dispatch_purchase_result(const jboolean success,
                                              const jstring product_id) {
  if (s_instance != nullptr)
    s_instance->on_purchase_result(success, product_id);
}

}

extern "C" JNIEXPORT void JNICALL
Java_com_nx2d_runtime_NxSamsungIap_nativeOnOwnedProductsQueried(
    JNIEnv *, jclass, const jboolean success, const jobjectArray product_ids) {
  nxm::store_galaxy_store::GalaxyStoreCore::dispatch_owned_products_queried(
      success, product_ids);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nx2d_runtime_NxSamsungIap_nativeOnProductDetailsResponse(
    JNIEnv *, jclass, const jboolean success, const jobjectArray product_ids,
    const jobjectArray titles, const jobjectArray formatted_prices) {
  nxm::store_galaxy_store::GalaxyStoreIap::dispatch_product_details_response(
      success, product_ids, titles, formatted_prices);
}

extern "C" JNIEXPORT void JNICALL
Java_com_nx2d_runtime_NxSamsungIap_nativeOnPurchaseResult(
    JNIEnv *, jclass, const jboolean success, const jstring product_id) {
  nxm::store_galaxy_store::GalaxyStoreIap::dispatch_purchase_result(success,
                                                                    product_id);
}
