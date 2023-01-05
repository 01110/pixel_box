#include "wifi_manager.hpp"

//wifimanager_lite parameters
#define USE_DYNAMIC_PARAMETERS      false 
#define REQUIRE_ONE_SET_SSID_PW     true
#define SCAN_WIFI_NETWORKS          true
#define USE_LITTLEFS                true
#define USE_SPIFFS                  false
#define TO_LOAD_DEFAULT_CONFIG_DATA false
// #define CONFIG_TIMEOUT              60s

#include <ESPAsync_WiFiManager_Lite.h>

bool LOAD_DEFAULT_CONFIG_DATA = false;
ESP_WM_LITE_Configuration defaultConfig;

namespace pixelbox
{
  namespace wifi_manager
  {
    ESPAsync_WiFiManager_Lite wifi_manager;

    void setup()
    {
      wifi_manager.begin();
    }

    void loop()
    {
      wifi_manager.run();
    }
  }
}
