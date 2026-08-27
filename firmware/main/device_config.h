#pragma once

#include <cstddef>

namespace self_involving {

struct DeviceConfig {
    char wifi_ssid[33] = {};
    char wifi_password[65] = {};
    char gateway_uri[192] = {};
    char device_token[160] = {};
    bool complete = false;
};

DeviceConfig LoadDeviceConfig();

}  // namespace self_involving
