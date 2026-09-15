#include "store_galaxy_store/store_galaxy_store_platform.h"
#include "store_galaxy_store/store_galaxy_store_services.h"

#include "store/store_service.h"

#include "app/engine.h"
#include "app/module_system/module.h"
#include "app/module_system/module_context.h"

#include "core/foundation/diagnostics/log.h"

namespace nxm::store_galaxy_store {
namespace {

const nx::log::Category log_store_galaxy_store =
    nx::log::category("store_galaxy_store");

// store.achievements/store.cloud_saves/store.presence are deliberately
// absent - Samsung IAP has none of those subsystems (Samsung Game Services
// is a separate, unrelated product for that), the same "simply doesn't
// provide it" shape `store_google_play_module.cpp`/
// `store_app_gallery_module.cpp` already established.
constexpr nxe::ModuleService PROVIDED_SERVICES[] = {
    {.id = store::kCoreService, .version = {1, 0, 0}},
    {.id = store::kIapService, .version = {1, 0, 0}},
};

class StoreGalaxyStoreModule final : public nxe::Module {
public:
  StoreGalaxyStoreModule() : m_core(m_platform), m_iap(m_platform) {}

  [[nodiscard]] nxe::ModuleDescriptor descriptor() const noexcept override {
    nxe::ModuleDescriptor out{};
    out.id = "store_galaxy_store";
    out.version = {1, 0, 0};
    out.provided_services = PROVIDED_SERVICES;
    out.platforms = nxe::ModulePlatform::Android;
    return out;
  }

  bool on_register(nxe::ModuleContext &ctx) override {
    nxe::ServiceRegistrar registrar = ctx.service_registrar();
    store::StoreCore &core = m_core;
    store::StoreIap &iap = m_iap;
    return registrar.provide(store::kCoreService, PROVIDED_SERVICES[0].version, core) &&
           registrar.provide(store::kIapService, PROVIDED_SERVICES[1].version, iap);
  }

  bool on_attach(nxe::ModuleContext &) override {
    // No pump system registered here, unlike the desktop backends - every
    // Samsung IAP call resolves through the Java shim's own callbacks,
    // dispatched by the Android runtime itself (see
    // store_galaxy_store_platform.h), not from anything this module needs
    // to poll each frame. There's also no per-project config file: IapHelper
    // takes no developer-supplied credentials at runtime at all - it
    // resolves everything from the process's own package identity and the
    // signed-in Samsung account, matching store_google_play's own shape.
    if (m_platform.initialize())
      nx::logi(log_store_galaxy_store, "attached, connecting to Samsung IAP");
    else
      nx::logi(log_store_galaxy_store, "no Android activity available; staying idle");
    return true;
  }

  void on_detach(nxe::ModuleContext &) override { m_platform.shutdown(); }

private:
  GalaxyStorePlatform m_platform;
  GalaxyStoreCore m_core;
  GalaxyStoreIap m_iap;
};

} // namespace
} // namespace nxm::store_galaxy_store

NX_DECLARE_MODULE(store_galaxy_store, nxm::store_galaxy_store::StoreGalaxyStoreModule)
