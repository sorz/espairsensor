#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#include "lywsd02.h"

#define MAC_ADDR (CONFIG_LYWSD02_MAC_ADDR)

static const char* TAG = "lywsd02";
static ble_addr_t peer_addr;

void ble_scan();

int ble_on_gap_event(struct ble_gap_event *event, void *arg) {
    if (event->type == BLE_GAP_EVENT_CONNECT) {
        if (event->connect.status == 0) {
            ESP_LOGI(TAG, "BLE connected");
        } else {
            ESP_LOGW(TAG, "BLE connect fail with %d", event->connect.status);
            ble_scan();
        }
    } else if (event->type == BLE_GAP_EVENT_DISCONNECT) {
        ESP_LOGI(TAG, "BLE disconnected (%d)", event->disconnect.reason);
    } else if (event->type == BLE_GAP_EVENT_DISC) {
        ESP_LOGI(TAG, "Device discovered: RSSI=%d ADDR=%d/%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
            event->disc.rssi, event->disc.addr.type,
            event->disc.addr.val[5], event->disc.addr.val[4], event->disc.addr.val[3],
            event->disc.addr.val[2], event->disc.addr.val[1], event->disc.addr.val[0]);

        ESP_ERROR_CHECK(ble_gap_disc_cancel());

        uint8_t own_addr_type;
        ESP_ERROR_CHECK(ble_hs_id_infer_auto(0, &own_addr_type));

        int ret = ble_gap_connect(own_addr_type, &event->disc.addr, 30000, NULL, ble_on_gap_event, NULL);
        if (ret) ESP_LOGE(TAG, "ble_gap_connect fail: %d", ret);
    }
    return ESP_OK;
}

static void ble_on_reset(int reason) {
    MODLOG_DFLT(ERROR, "Resetting state; reason=%d\n", reason);
}

static void ble_on_sync() {
    ESP_ERROR_CHECK(ble_hs_util_ensure_addr(0));
    ESP_LOGI(TAG, "Scanning for [%s]...", MAC_ADDR);

    ble_scan();
}

void ble_scan() {
    uint8_t own_addr_type;
    ESP_ERROR_CHECK(ble_hs_id_infer_auto(true, &own_addr_type));

    struct ble_gap_disc_params disc_params = {
        .filter_policy = BLE_HCI_SCAN_FILT_USE_WL,
        .passive = true,
        .filter_duplicates = true,
    };

    ESP_ERROR_CHECK(ble_gap_wl_set(&peer_addr, 1));
    ESP_ERROR_CHECK(ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &disc_params, ble_on_gap_event, NULL));
}

void blecent_host_task(void *param) {
    ESP_LOGI(TAG, "BLE Host Task Started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void lywsd02_init() {
    sscanf(MAC_ADDR, "%hhx:%hhx:%hhx:%hhx:%hhx:%hhx",
        &peer_addr.val[5], &peer_addr.val[4], &peer_addr.val[3],
        &peer_addr.val[2], &peer_addr.val[1], &peer_addr.val[0]);

    ESP_ERROR_CHECK(esp_nimble_hci_and_controller_init());
    nimble_port_init();
    ble_hs_cfg.reset_cb = ble_on_reset;
    ble_hs_cfg.sync_cb = ble_on_sync;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ESP_ERROR_CHECK(ble_svc_gap_device_name_set("espair"));
    nimble_port_freertos_init(blecent_host_task);
}