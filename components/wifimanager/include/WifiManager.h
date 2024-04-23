#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_smartconfig.h"

#include "NvsStorageManager.h"

class WiFiManager {
public:
    WiFiManager(NvsStorageManager& storageManager,
				esp_event_handler_t eventHandler=nullptr,
				void* eventHandlerArg = nullptr,
				bool clear_settings=false);
    ~WiFiManager();
	void clear();
private:
    static void localEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
    static void smartConfigTask(void* param);

    NvsStorageManager& storageManager; 
    static EventGroupHandle_t wifi_event_group;
    static constexpr int CONNECTED_BIT = BIT0;
    static constexpr int ESPTOUCH_DONE_BIT = BIT1;
    static const char* TAG;
};
