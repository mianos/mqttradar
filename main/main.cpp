#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "mqtt_client.h"

#include "MqttClient.h"
#include "WifiManager.h"

static const char *TAG = "mqtt_main";

static SemaphoreHandle_t wifiSemaphore;

static void localEventHandler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_id == IP_EVENT_STA_GOT_IP) {
        ESP_LOGI(TAG, "got ip");
		auto client = static_cast<MqttClient *>(arg);
		client->start();
		client->publish("tele/radar3/init", "Hello MQTT");
	    xSemaphoreGive(wifiSemaphore);
	}
}

#if 1
extern "C" void app_main() {
	wifiSemaphore = xSemaphoreCreateBinary();
//    wifiManager.clearWiFiCredentials(); // TODO: put this as an argument to the wifiManager constructor
	// or put a pin to hold down to init it.
    MqttClient client("mqtt://mqtt2.mianos.com", "radar3", "rob", "secret");
	WiFiManager wifiManager(localEventHandler, &client);

//    client.subscribe("your/subscribe/topic");
    if (xSemaphoreTake(wifiSemaphore, portMAX_DELAY)) {
        ESP_LOGI(TAG, "Main task continues after WiFi connection.");
		while (true) {
			ESP_LOGI(TAG, "tick");
			vTaskDelay(pdMS_TO_TICKS(1000)); 
		}
    }
//	 vSemaphoreDelete(wifiSemaphore);
}
#endif

#if 0
//declare MQTT configstructure, will be initialised in mqtt_setup() function
static esp_mqtt_client_config_t mqtt_cfg;

// this will be called if MQTT is connected, here we have to subscribe to topics we need. Change it acccording to your needs
void onMqttConnect(esp_mqtt_client_handle_t client) {
  // subscribe to topics required
  char *topic=(char *)malloc(MAX_MQTT_TOPIC_LEN+4);
  if(topic!=nullptr) {
      for(int i=0;i<NUMBER_DATAPOINTS;i++) {
          strcpy(topic,settings.mqtt_topicprefix);
          strcat(topic,"/");
          strcat(topic,settings.param_mqtttopic[i]);
          strcat(topic,"/set");
          esp_mqtt_client_subscribe(client, topic, 0);
        }
  free(topic);
 }
}

// this is event handler that we will register in mqtt_setup(). You will need at least two events: MQTT_EVENT_CONNECTED and MQTT_EVENT_DATA. more possible events see ESP-IDF example
static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t mqttclient = event->client;
    int msg_id;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            onMqttConnect(mqttclient);
            break;
        case MQTT_EVENT_DATA:
            // Write your own onMqttMessage function that react on topics received
            onMqttMessage(event->topic, event->topic_len, (const unsigned char *) event->data, event->data_len, 0, 0, 0);
            break;
        default:
            break;
    }
    return ESP_OK;
}

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
