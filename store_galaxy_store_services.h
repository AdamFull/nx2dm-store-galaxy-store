#pragma once

#include "store_galaxy_store/store_galaxy_store_platform.h"

#include "store/store_service.h"

#include <jni.h>

namespace nxm::store_galaxy_store {

/// Two of the five neutral services (store_service.h), backed by Samsung
/// IAP - store.achievements/store.cloud_saves/store.presence are never
/// registered: Samsung IAP has none of those subsystems (Samsung Game
/// Services is a separate, unrelated product for that), the same "simply
/// doesn't provide it" shape `store_google_play_services.h`/
/// `store_app_gallery_services.h` already established.
///
/// Every call here is asynchronous via the Java shim's callbacks (see
/// store_galaxy_store_platform.h for the JNI mechanics) - both classes
/// therefore hold a small cache populated by their own query's JNI-exported
/// callback, with the neutral interface's synchronous methods reading
/// whatever is cached so far, the same eventually-consistent shape the
/// other two mobile backends already established.

class GalaxyStoreCore final : public store::StoreCore {
public:
  explicit GalaxyStoreCore(GalaxyStorePlatform &platform) noexcept;
  ~GalaxyStoreCore() override;

  /// Samsung IAP has no "own the base game" concept at all - Galaxy Store
  /// already gates who can install/run the APK, so a ready connection
  /// implies base ownership. A non-empty @p dlc_id checks the cache
  /// refresh_ownership() populates instead.
  [[nodiscard]] bool is_owned(nx::string_view dlc_id = {}) const override;
  [[nodiscard]] nx::vector<nx::string> owned_dlc_ids() const override {
    return m_owned_dlc_ids;
  }
  [[nodiscard]] nx::string_view store_name() const noexcept override {
    return "galaxy_store";
  }

  /// Fires `NxSamsungIap.obtainOwnedProducts()`, refreshing owned_dlc_ids()
  /// with every product id the signed-in Samsung account currently owns.
  /// @p dlc_id is ignored, same as store::StoreCore::refresh_ownership()
  /// documents for any bulk-capable backend.
  void refresh_ownership(nx::string_view dlc_id = {}) override;

  static void dispatch_owned_products_queried(jboolean success,
                                              jobjectArray product_ids);

private:
  void on_owned_products_queried(jboolean success, jobjectArray product_ids);

  GalaxyStorePlatform &m_platform;
  nx::vector<nx::string> m_owned_dlc_ids;

  static GalaxyStoreCore *s_instance;
};

class GalaxyStoreIap final : public store::StoreIap {
public:
  explicit GalaxyStoreIap(GalaxyStorePlatform &platform) noexcept;
  ~GalaxyStoreIap() override;

  [[nodiscard]] nx::vector<store::StoreProduct> products() const override {
    return m_products;
  }
  bool purchase(nx::string_view product_id) override;
  [[nodiscard]] bool purchase_pending() const override { return m_purchase_pending; }
  [[nodiscard]] nx::string_view purchase_error() const override {
    return m_purchase_error.view();
  }

  /// Fires `NxSamsungIap.obtainProductDetails()` for exactly the ids given -
  /// like the other two mobile backends, Samsung IAP has no "list
  /// everything" query, the game must know its own product ids up front.
  void refresh_products(const nx::vector<nx::string> &product_ids) override;

  static void dispatch_product_details_response(jboolean success,
                                                 jobjectArray product_ids,
                                                 jobjectArray titles,
                                                 jobjectArray formatted_prices);
  static void dispatch_purchase_result(jboolean success, jstring product_id);

private:
  void on_product_details_response(jboolean success, jobjectArray product_ids,
                                   jobjectArray titles,
                                   jobjectArray formatted_prices);
  void on_purchase_result(jboolean success, jstring product_id);

  GalaxyStorePlatform &m_platform;
  nx::vector<store::StoreProduct> m_products;
  bool m_purchase_pending = false;
  nx::string m_purchase_error;

  static GalaxyStoreIap *s_instance;
};

}
