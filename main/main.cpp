#include <cstdlib>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include "esp_sntp.h"
#include "esp_netif.h"

#include "esp_heap_caps.h"

#include "MqttClient.h"
#include "WifiManager.h"

#include "Events.h"
#include "ld2450.h"
#include "SettingsManager.h"
#include "web.h"

static const char *TAG = "mqtt_main";

static SemaphoreHandle_t wifiSemaphore;

void initialize_sntp() {
	setenv("TZ", "AEST-10AEDT,M10.1.0,M4.1.0/3", 1);	// TODO: config
	tzset();
    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "ntp.mianos.com");	// TODO: config
    esp_sntp_init();
    ESP_LOGI(TAG, "SNTP service initialized");
    int max_retry = 50;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && max_retry--) {
        vTaskDelay(100 / portTICK_PERIOD_MS); 
    }
    if (max_retry <= 0) {
        ESP_LOGE(TAG, "Failed to synchronize NTP time");
        return; // Exit if unable to sync
    }
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    ESP_LOGI("TimeTest", "Current local time and date: %d-%d-%d %d:%d:%d",
             1900 + timeinfo.tm_year, 1 + timeinfo.tm_mon, timeinfo.tm_mday,
             timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
}



void PublishMqttInit(MqttClient& client, SettingsManager& settings) {
    JsonWrapper doc;

    doc.AddItem("version", 4);
    doc.AddItem("name", settings.sensorName);
	doc.AddTime();
	doc.AddTime(false, "gmt");

    esp_netif_t* netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (!netif) {
        ESP_LOGE("NET_INFO", "Network interface for STA not found");
        return;
    }
    // Get hostname
    const char* hostname;
    esp_netif_get_hostname(netif, &hostname);
	doc.AddItem("hostname", std::string(hostname));

    // Get IP Address
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
        char ip_str[16]; // Buffer to hold the IP address string
        esp_ip4addr_ntoa(&ip_info.ip, ip_str, sizeof(ip_str));
		doc.AddItem("ip", std::string(ip_str));
    } else {
        ESP_LOGE("NET_INFO", "Failed to get IP information");
    }
	doc.AddItem("settings", "cmnd/" + settings.sensorName + "/settings");

    std::string status_topic = std::string("tele/") + settings.sensorName + "/init";
    std::string output = doc.ToString();
    client.publish(status_topic, output);
}


static void localEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
	    xSemaphoreGive(wifiSemaphore);
	}
}

extern "C" void app_main() {
	wifiSemaphore = xSemaphoreCreateBinary();
	NvsStorageManager nv;
	SettingsManager settings(nv);
	ESP_LOGI(TAG, "Settings %s", settings.ToJson().c_str());
    MqttClient client(settings, "mqtt://mqtt2.mianos.com", "radar3", "rob", "secret");
	// TODO:  add 'clear=true' to clear credentials. Use a button on start.
	WiFiManager wifiManager(nv, localEventHandler, nullptr);
	LocalEP ep(settings, client);
	LD2450 rsense(&ep, settings);
	WebContext wc{settings};
	WebServer webServer(wc); // Specify the web server port

    if (xSemaphoreTake(wifiSemaphore, portMAX_DELAY) ) {
		initialize_sntp();
		client.start();
		PublishMqttInit(client, settings);
        ESP_LOGI(TAG, "Main task continues after WiFi connection.");
		while (true) {
//			size_t freeMem = esp_get_free_heap_size();
//			ESP_LOGI(TAG, "Free memory: %u bytes", freeMem);
			rsense.process();
			vTaskDelay(pdMS_TO_TICKS(10)); 
		}
    }
//	 vSemaphoreDelete(wifiSemaphore);
}


#if 0

void mqtt_setup(){
 // init config structure
    mqtt_cfg.event_handle = mqtt_event_handler;
    mqtt_cfg.host=(const char *)settings.mqtt_server_ip;
    char *uri_line=(char *)malloc(100);
    sprintf(uri_line,"mqtt://%s:%u",settings.mqtt_server_ip,settings.mqtt_port);
    mqtt_cfg.uri=uri_line;
    mqtt_cfg.port=settings.mqtt_port;
    mqtt_cfg.username=(const char *)settings.mqtt_user;
    mqtt_cfg.password=(const char *)settings.mqtt_pwd;
 // setup MQTT
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
}
#endif
