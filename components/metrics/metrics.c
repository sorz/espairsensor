#include "math.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
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

static esp_err_t http_request_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/openmetrics-text");
    xSemaphoreTake(metrics.semphr, portMAX_DELAY);

    for (size_t i = 0; i < metrics.len; i++) {
        metric_t m = metrics.items[i];
        if (m.help != NULL) {
            httpd_resp_sendstr_chunk(req, "# HELP ");
            httpd_resp_sendstr_chunk(req, m.name);
            httpd_resp_sendstr_chunk(req, " ");
            httpd_resp_sendstr_chunk(req, m.help);
            httpd_resp_sendstr_chunk(req, "\n");
        }
        httpd_resp_sendstr_chunk(req, "# TYPE ");
        httpd_resp_sendstr_chunk(req, m.name);
        httpd_resp_sendstr_chunk(req, " ");
        httpd_resp_sendstr_chunk(req, m.type);
        httpd_resp_sendstr_chunk(req, "\n");
        if (m.unit != NULL) {
            httpd_resp_sendstr_chunk(req, "# UNIT ");
            httpd_resp_sendstr_chunk(req, m.name);
            httpd_resp_sendstr_chunk(req, " ");
            httpd_resp_sendstr_chunk(req, m.unit);
            httpd_resp_sendstr_chunk(req, "\n");
        }
        httpd_resp_sendstr_chunk(req, m.name);
        httpd_resp_sendstr_chunk(req, "{host=");
        httpd_resp_sendstr_chunk(req, HOSTNAME);
        httpd_resp_sendstr_chunk(req, "} ");
        httpd_resp_sendstr_chunk(req, m.value);
        httpd_resp_sendstr_chunk(req, "\n\n");
    }

    httpd_resp_sendstr(req, "# EOF");
    xSemaphoreGive(metrics.semphr);
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
        if (wifi_retry_num > 10) wifi_retry_num = 10;
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
	        .threshold.authmode = WIFI_AUTH_WPA2_WPA3_PSK,
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
    list->semphr = xSemaphoreCreateBinary();
    assert(list->semphr != NULL);
}

void metrics_put(metric_t* metric) {
    xSemaphoreTake(metrics.semphr, portMAX_DELAY);

    for (size_t i = 0; i < metrics.len; i++) {
        if (metrics.items[i].name == metric->name) {
            metrics.items[i] = *metric;
            xSemaphoreGive(metrics.semphr);
            return;
        }
    }
    if (metrics.len >= METRICS_MAX_NUM) {
        ESP_LOGE(TAG, "Maximum metrics number reached, ignore %s", metric->name);
        xSemaphoreGive(metrics.semphr);
        return;
    }
    metrics.items[metrics.len] = *metric;
    metrics.len ++;
    xSemaphoreGive(metrics.semphr);
}
