#include "device_config.h"

#include <cstring>

#include "nvs.h"
#include "sdkconfig.h"

namespace self_involving {
namespace {

template <std::size_t N>
void Copy(char (&destination)[N], const char* source) {
    if (source == nullptr) return;
    std::strncpy(destination, source, N - 1);
    destination[N - 1] = '\0';
}

template <std::size_t N>
void ReadString(nvs_handle_t handle, const char* key, char (&value)[N]) {
    std::size_t length = N;
    if (nvs_get_str(handle, key, value, &length) != ESP_OK) {
        value[0] = '\0';
    }
}

}  // namespace

DeviceConfig LoadDeviceConfig() {
    DeviceConfig config;
    Copy(config.wifi_ssid, CONFIG_M3_WIFI_SSID);
    Copy(config.wifi_password, CONFIG_M3_WIFI_PASSWORD);
    Copy(config.gateway_uri, CONFIG_M3_GATEWAY_URI);
    Copy(config.device_token, CONFIG_M3_DEVICE_TOKEN);

    nvs_handle_t handle = 0;
    if (nvs_open("m3_config", NVS_READONLY, &handle) == ESP_OK) {
        char value[192] = {};
        ReadString(handle, "wifi_ssid", value);
        if (value[0] != '\0') Copy(config.wifi_ssid, value);
        ReadString(handle, "wifi_pass", value);
        if (value[0] != '\0') Copy(config.wifi_password, value);
        ReadString(handle, "gateway_uri", value);
        if (value[0] != '\0') Copy(config.gateway_uri, value);
        ReadString(handle, "device_token", value);
        if (value[0] != '\0') Copy(config.device_token, value);
        std::memset(value, 0, sizeof(value));
        nvs_close(handle);
    }
    config.complete = config.wifi_ssid[0] != '\0' &&
                      config.gateway_uri[0] != '\0' &&
                      config.device_token[0] != '\0';
    return config;
}

}  // namespace self_involving
