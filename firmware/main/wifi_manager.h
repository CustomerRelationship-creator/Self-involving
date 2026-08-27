#pragma once

#include "device_config.h"
#include "esp_err.h"

namespace self_involving {

class WifiManager {
public:
    esp_err_t Connect(const DeviceConfig& config, int timeout_ms);
    bool connected() const { return connected_; }

private:
    bool connected_ = false;
};

}  // namespace self_involving
