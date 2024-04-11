#pragma once
#include "mqtt_client.h"

#include "MqttContext.h"

class MqttClient {
public:
    MqttClient(MqttContext* context, const char* brokerUri, const char* clientId, const char* username, const char* password);
    ~MqttClient();

    void start();
    void publish(const char* topic, const char* data);
    void subscribe(const char* topic);

private:
    esp_mqtt_client_handle_t client;
	MqttContext* context;
    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
};
