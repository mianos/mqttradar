#include <cstring>
#include "esp_netif.h"
#include "esp_system.h"

#include <wifi_provisioning/manager.h>

#include "wifimanager.h"

EventGroupHandle_t WiFiManager::wifi_event_group;
const char* WiFiManager::TAG = "WiFiManager";

WiFiManager::WiFiManager(NvsStorageManager& storageManager,
						 esp_event_handler_t eventHandler,
						 void* eventHandlerArg, 
						 bool clear_settings) : storageManager(storageManager) {
	if (clear_settings) {
		clear();
	}
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &localEventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &localEventHandler, this));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &localEventHandler, this));
	if (eventHandler != nullptr) {
		ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, eventHandler, eventHandlerArg));
	}
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

WiFiManager::~WiFiManager() {
    vEventGroupDelete(wifi_event_group);
    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &localEventHandler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &localEventHandler);
    esp_event_handler_unregister(SC_EVENT, ESP_EVENT_ANY_ID, &localEventHandler);
    esp_wifi_stop();
    esp_wifi_deinit();
}

void WiFiManager::localEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
	auto *wev = static_cast<WiFiManager *>(arg);
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		ESP_LOGI(TAG, "INTO WIFI START EVENT");
		bool provisioned = false;
		ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
		if (!provisioned) {
			ESP_LOGI(TAG, "Not provisioned");
			xTaskCreate(smartConfigTask, "smartConfigTask", 4096, NULL, 3, NULL);
		} else {
			ESP_LOGI(TAG, "already provisioned");
			esp_wifi_connect();
		}
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE) {
        ESP_LOGI(TAG, "Scan done");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL) {
        ESP_LOGI(TAG, "Found channel");
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD) {
        ESP_LOGI(TAG, "Got SSID and password");
        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        memset(&wifi_config, 0, sizeof(wifi_config));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
        esp_wifi_connect();
    } else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE) {
        xEventGroupSetBits(wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

void WiFiManager::smartConfigTask(void* param) {
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH_V2));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));

    while (1) {
        EventBits_t uxBits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, pdTRUE, pdFALSE, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi Connected to AP");
        }
        if (uxBits & ESPTOUCH_DONE_BIT) {
            ESP_LOGI(TAG, "SmartConfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
}

void WiFiManager::clear() {
	wifi_prov_mgr_reset_provisioning();
	ESP_LOGI(TAG, "WiFi credentials cleared.");
	esp_restart();
}
