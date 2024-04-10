#include <cstring>

#include "MqttClient.h"

MqttClient::MqttClient(const char* brokerUri, const char* clientId, const char* username, const char* password) {
    esp_mqtt_client_config_t mqtt_cfg = {};

    // Set broker URI
    mqtt_cfg.broker.address.uri = brokerUri;

    // Set client credentials
    mqtt_cfg.credentials.username = username;
    mqtt_cfg.credentials.client_id = clientId;
    mqtt_cfg.credentials.authentication.password = password;

    // Initialize the MQTT client with the configuration
    client = esp_mqtt_client_init(&mqtt_cfg);
//    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, client);
	esp_mqtt_client_register_event(client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqtt_event_handler, client);

}

MqttClient::~MqttClient() {
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
}

void MqttClient::start() {
    esp_mqtt_client_start(client);
}

void MqttClient::publish(const char* topic, const char* data) {
    esp_mqtt_client_publish(client, topic, data, 0, 1, 0);
}

void MqttClient::subscribe(const char* topic) {
    esp_mqtt_client_subscribe(client, topic, 0);
}

void MqttClient::mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    // Handle MQTT events here
}
