#include "math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "metrics.h"


#define WIFI_CONNECTED_BIT      BIT0
#define WIFI_FAIL_BIT           BIT1

#define HOSTNAME  (CONFIG_METRICS_HOSTNAME)
#define WIFI_SSID (CONFIG_METRICS_WIFI_SSID)
#define WIFI_PSK  (CONFIG_METRICS_WIFI_PSK)
#define HTTP_PATH (CONFIG_METRICS_HTTP_PATH)

static const char* TAG       = "metrics";

static int wifi_retry_num    = 0;
static nvs_handle_t nvs_esp  = 0;
static httpd_handle_t httpd  = NULL;
static metric_list_t metrics = {};

void init_nvs() {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Open
    ESP_ERROR_CHECK(nvs_open(TAG, NVS_READWRITE, &nvs_esp));
}

#define buf_printf(...) {\
        int n = snprintf(&buf[buf_pos], buf_size - buf_pos, __VA_ARGS__);\
        if (n < 0) {\
            xSemaphoreGive(metrics.semphr);\
            free(buf);\
            ESP_LOGE(TAG, "Failed to write buffer: %d", n);\
            return ESP_FAIL;\
        } else {\
            buf_pos += n;\
        }\
    }

static esp_err_t http_request_handler(httpd_req_t *req) {
    size_t buf_size = 8;
    size_t buf_pos = 0;
    char* buf = NULL;
    int64_t now = esp_timer_get_time() / 1000;
    uint8_t mac[6];
    char mac_str[18];

    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, sizeof(mac_str), "%02x%02x%02x%02x%02x%02x",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    httpd_resp_set_type(req, "application/openmetrics-text");
    xSemaphoreTake(metrics.semphr, portMAX_DELAY);

    for (size_t i = 0; i < metrics.len; i++)
        if (now < metrics.meta[i].exipred_at)
            buf_size += metrics.meta[i].buf_size;
    buf = malloc(buf_size);
    if (buf == NULL) {
        ESP_LOGE(TAG, "Out of memory");
        xSemaphoreGive(metrics.semphr);
        return ESP_FAIL;
    }

    for (size_t i = 0; i < metrics.len; i++) {
        metric_meta_t meta = metrics.meta[i];
        metric_t m = metrics.items[i];
        if (now >= meta.exipred_at) {
            ESP_LOGD(TAG, "Metric %s expired (%lli >= %lli)", m.name, now, meta.exipred_at);
            continue;
        }
        ESP_LOGD(TAG, "Print metric %s", m.name);
        if (m.help != NULL) buf_printf("# HELP %s %s\n", m.name, m.help);
        if (m.unit != NULL) buf_printf("# UNIT %s %s\n", m.name, m.unit);
        buf_printf("# TYPE %s %s\n", m.name, m.type);
        buf_printf("%s{host=\"%s\", mac=\"%s\"} %.*f\n\n", m.name, HOSTNAME, mac_str, m.precision, m.value);
    }
    buf_printf("# EOF\n");
    httpd_resp_send(req, buf, buf_pos);
    xSemaphoreGive(metrics.semphr);
    free(buf);
    return ESP_OK;
}

static const httpd_uri_t endpoint_root_get = {
    .uri       = HTTP_PATH,
    .method    = HTTP_GET,
    .handler   = http_request_handler
};

static httpd_handle_t start_webserver(void) {
    httpd_handle_t server = NULL;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server");
    httpd_config_t conf = HTTPD_DEFAULT_CONFIG();
    esp_err_t ret = httpd_start(&server, &conf);
    if (ESP_OK != ret) {
        ESP_LOGE(TAG, "Error starting http server!");
        return NULL;
    }

    // Set URI handlers
    ESP_LOGI(TAG, "Registering URI handlers");
    httpd_register_uri_handler(server, &endpoint_root_get);
    return server;
}


static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (httpd) {
            httpd_stop(httpd);
            httpd = NULL;
        }
        wifi_retry_num++;
        if (wifi_retry_num > 14) wifi_retry_num = 14;
        int delay_ms = powf(2.0f, wifi_retry_num);
        ESP_LOGI(TAG, "Failed to connect to AP %s, retry in %dms", WIFI_SSID, delay_ms);
        vTaskDelay(delay_ms / portTICK_RATE_MS);
        esp_wifi_connect();
        ESP_LOGI(TAG, "Retry to connect to the AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "Got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        if (httpd == NULL) httpd = start_webserver();
    }
}


void init_wifi() {
    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t* netif = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_netif_set_hostname(netif, HOSTNAME));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PSK,
            .scan_method = WIFI_ALL_CHANNEL_SCAN,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}


void metrics_init() {
    init_nvs();
    init_wifi();
    metrics_list_init(&metrics);
}

void metrics_list_init(metric_list_t *list) {
    list->len = 0;
    list->semphr = xSemaphoreCreateMutex();
    assert(list->semphr != NULL);
}

void metrics_list_update_at(metric_list_t *list, size_t idx, metric_t *item, uint32_t exipred_at) {
    list->items[idx] = *item;
    list->meta[idx].exipred_at = exipred_at;
    list->meta[idx].buf_size = 80 + strlen(item->name) * 4 + sizeof(HOSTNAME);
    if (item->help != NULL) list->meta[idx].buf_size += strlen(item->help);
}

#define put_into_then_return(i) {\
    size_t idx = (i);\
    ESP_LOGD(TAG, "Put %s to pos %d", metric->name, idx);\
    metrics_list_update_at(&metrics, idx, metric, now + expire_in_mllis);\
    xSemaphoreGive(metrics.semphr);\
    return;\
}

void metrics_put(metric_t* metric, uint32_t expire_in_mllis) {
    int64_t now = esp_timer_get_time() / 1000;
    xSemaphoreTake(metrics.semphr, portMAX_DELAY);

    // Update existing item
    for (size_t i = 0; i < metrics.len; i++)
        if (metrics.items[i].name == metric->name)
            put_into_then_return(i);

    // Replace expired item
    for (size_t i = 0; i < metrics.len; i++)
        if (now >= metrics.meta[i].exipred_at)
            put_into_then_return(i);

    // Append to end
    if (metrics.len >= METRICS_MAX_NUM) {
        ESP_LOGE(TAG, "Maximum metrics number reached, ignore %s", metric->name);
        xSemaphoreGive(metrics.semphr);
        return;
    }
    put_into_then_return(metrics.len++);
}
