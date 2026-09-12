package com.nx2d.runtime

import android.app.Activity
import com.samsung.android.sdk.iap.lib.helper.HelperDefine
import com.samsung.android.sdk.iap.lib.helper.IapHelper
import com.samsung.android.sdk.iap.lib.listener.OnGetOwnedListListener
import com.samsung.android.sdk.iap.lib.listener.OnGetProductsDetailsListener
import com.samsung.android.sdk.iap.lib.listener.OnPaymentListener
import com.samsung.android.sdk.iap.lib.vo.ErrorVo
import com.samsung.android.sdk.iap.lib.vo.OwnedProductVo
import com.samsung.android.sdk.iap.lib.vo.ProductVo
import com.samsung.android.sdk.iap.lib.vo.PurchaseVo

// Deliberately NOT referenced anywhere in NxActivity.java (unlike NxHaptics)
// - this object only exists in the compiled app when store_galaxy_store is
// enabled (see android/app/build.gradle.kts's per-module Kotlin/Java
// source-dir loop and its conditional Samsung IAP .aar wiring), the same
// contract NxGooglePlayBilling.kt/NxHuaweiIap.kt already established.
// Unlike Huawei's IAP Kit, Samsung's startPayment()
// takes no Activity/request-code pair and resolves purely through
// OnPaymentListener - IapHelper manages launching and binding to the
// Galaxy Store checkout UI internally, so this backend needs no
// NxActivity.registerActivityResultHandler() at all, the same
// pure-listener shape Google Play Billing has.
object NxSamsungIap {
    private var iapHelper: IapHelper? = null

    // Every product here is treated as a durable, non-consumable entitlement
    // (store.core's DLC-ownership model), the same honest scope limit
    // NxGooglePlayBilling.kt/NxHuaweiIap.kt already document for their own
    // backends - a game that needs consumable currency-style products isn't
    // served by this backend's generic purchase() call, so
    // consumePurchasedItems() is never called.

    @JvmStatic
    fun connect(activity: Activity) {
        val helper = IapHelper.getInstance(activity.applicationContext).also { iapHelper = it }
        // OPERATION_MODE_PRODUCTION talks to the real Galaxy Store backend -
        // switching to OPERATION_MODE_TEST is a Seller Portal-side developer
        // choice (per-device test accounts), not something this generic
        // backend should hardcode.
        helper.setOperationMode(HelperDefine.OperationMode.OPERATION_MODE_PRODUCTION)
        nativeOnConnected()
    }

    @JvmStatic
    fun disconnect() {
        // IapHelper has no documented teardown call - it is a
        // process-lifetime singleton bound to the application context
        // passed to getInstance(), the same shape Huawei's IapClient has.
        iapHelper = null
    }

    // The re-check every game should run at startup and after a purchase
    // completes - getOwnedList() only lists entries the signed-in Samsung
    // account actually owns, the IAP Kit/Play Billing equivalent of
    // filtering to unconsumed/PURCHASED state.
    @JvmStatic
    fun obtainOwnedProducts() {
        val helper = iapHelper ?: return
        helper.getOwnedList(
            IapHelper.PRODUCT_TYPE_ALL,
            OnGetOwnedListListener { errorVo: ErrorVo, ownedList: ArrayList<OwnedProductVo>? ->
                if (errorVo.errorCode != IapHelper.IAP_ERROR_NONE) {
                    nativeOnOwnedProductsQueried(false, emptyArray())
                    return@OnGetOwnedListListener
                }
                for (owned in ownedList.orEmpty()) {
                    acknowledgeIfNeeded(helper, owned)
                }
                val ids = ownedList.orEmpty().map { it.itemId }.toTypedArray()
                nativeOnOwnedProductsQueried(true, ids)
            },
        )
    }

    @JvmStatic
    fun obtainProductDetails(productIds: Array<String>) {
        val helper = iapHelper ?: return
        helper.getProductsDetails(
            productIds.joinToString(","),
            OnGetProductsDetailsListener { errorVo: ErrorVo, productList: ArrayList<ProductVo>? ->
                if (errorVo.errorCode != IapHelper.IAP_ERROR_NONE) {
                    nativeOnProductDetailsResponse(false, emptyArray(), emptyArray(), emptyArray())
                    return@OnGetProductsDetailsListener
                }
                val ids = mutableListOf<String>()
                val titles = mutableListOf<String>()
                val prices = mutableListOf<String>()
                for (product in productList.orEmpty()) {
                    ids.add(product.itemId)
                    titles.add(product.itemName)
                    prices.add(product.itemPriceString)
                }
                nativeOnProductDetailsResponse(
                    true,
                    ids.toTypedArray(),
                    titles.toTypedArray(),
                    prices.toTypedArray(),
                )
            },
        )
    }

    @JvmStatic
    fun purchase(productId: String) {
        val helper = iapHelper
        if (helper == null) {
            nativeOnPurchaseResult(false, "")
            return
        }
        helper.startPayment(
            productId,
            /* obfuscatedAccountId = */ "",
            /* obfuscatedProfileId = */ "",
            OnPaymentListener { errorVo: ErrorVo, purchaseVo: PurchaseVo? ->
                if (errorVo.errorCode != IapHelper.IAP_ERROR_NONE || purchaseVo == null) {
                    nativeOnPurchaseResult(false, "")
                    return@OnPaymentListener
                }
                acknowledgeIfNeeded(helper, purchaseVo.purchaseId)
                nativeOnPurchaseResult(true, purchaseVo.itemId)
            },
        )
    }

    private fun acknowledgeIfNeeded(helper: IapHelper, owned: OwnedProductVo) {
        if (owned.acknowledgedStatus != HelperDefine.AcknowledgedStatus.NOT_ACKNOWLEDGED) {
            return
        }
        acknowledgeIfNeeded(helper, owned.purchaseId)
    }

    // Galaxy Store auto-refunds any purchase left unacknowledged, the same
    // "must happen for every durable purchase" step NxGooglePlayBilling.kt's
    // own acknowledgeIfNeeded() documents - called both right after a fresh
    // purchase and for every unacknowledged entry a re-query turns up.
    private fun acknowledgeIfNeeded(helper: IapHelper, purchaseId: String) {
        helper.acknowledgePurchases(purchaseId) { _, _ -> }
    }

    // Declared external (Kotlin's `native`), implemented in C++
    // (store_galaxy_store_platform.cpp / store_galaxy_store_services.cpp)
    // and resolved by the JVM's own symbol-name convention against
    // libnx2d.so, the same Java-calls-C++ direction
    // NxGooglePlayBilling.kt/NxHuaweiIap.kt already established. @JvmStatic
    // is what makes these compile to real static methods rather than
    // instance methods on Kotlin's synthesized object singleton.
    @JvmStatic
    private external fun nativeOnConnected()

    @JvmStatic
    private external fun nativeOnOwnedProductsQueried(success: Boolean, productIds: Array<String>)

    @JvmStatic
    private external fun nativeOnProductDetailsResponse(
        success: Boolean,
        productIds: Array<String>,
        titles: Array<String>,
        formattedPrices: Array<String>,
    )

    @JvmStatic
    private external fun nativeOnPurchaseResult(success: Boolean, productId: String)
}
