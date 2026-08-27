#include "wifi_manager.h"

#include <cstring>

#include "esp_check.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

namespace self_involving {
namespace {
constexpr char kTag[] = "m3_wifi";
constexpr EventBits_t kConnected = BIT0;
constexpr EventBits_t kFailed = BIT1;
EventGroupHandle_t s_events = nullptr;
int s_retries = 0;

void EventHandler(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (++s_retries <= 5) esp_wifi_connect();
        else xEventGroupSetBits(s_events, kFailed);
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto* event = static_cast<ip_event_got_ip_t*>(data);
        ESP_LOGI(kTag, "IPv4 acquired: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retries = 0;
        xEventGroupSetBits(s_events, kConnected);
    }
}
}  // namespace

esp_err_t WifiManager::Connect(const DeviceConfig& config, int timeout_ms) {
    ESP_RETURN_ON_ERROR(esp_netif_init(), kTag, "initialize network stack");
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) return err;
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    ESP_RETURN_ON_ERROR(esp_wifi_init(&init), kTag, "initialize Wi-Fi");
    s_events = xEventGroupCreate();
    if (s_events == nullptr) return ESP_ERR_NO_MEM;
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                   EventHandler, nullptr),
                        kTag, "register Wi-Fi event handler");
    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                   EventHandler, nullptr),
                        kTag, "register IP event handler");
    wifi_config_t wifi = {};
    std::strncpy(reinterpret_cast<char*>(wifi.sta.ssid), config.wifi_ssid,
                 sizeof(wifi.sta.ssid) - 1);
    std::strncpy(reinterpret_cast<char*>(wifi.sta.password), config.wifi_password,
                 sizeof(wifi.sta.password) - 1);
    wifi.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi.sta.pmf_cfg.capable = true;
    wifi.sta.pmf_cfg.required = false;
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), kTag, "set station mode");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi), kTag,
                        "set Wi-Fi configuration");
    std::memset(wifi.sta.password, 0, sizeof(wifi.sta.password));
    ESP_RETURN_ON_ERROR(esp_wifi_start(), kTag, "start Wi-Fi");
    const EventBits_t bits = xEventGroupWaitBits(
        s_events, kConnected | kFailed, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(timeout_ms));
    connected_ = (bits & kConnected) != 0;
    return connected_ ? ESP_OK : ESP_ERR_TIMEOUT;
}

}  // namespace self_involving
