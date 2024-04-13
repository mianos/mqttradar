#pragma once
#include <vector>
#include <deque>
#include <string>
#include <utility>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mqtt_client.h"

#include "MqttContext.h"

class MqttClient {
public:
    MqttClient(MqttContext* context, const char* brokerUri, const char* clientId, const char* username, const char* password);
    ~MqttClient();

    void start();
    void wait_for_connection();
    void publish(std::string topic, std::string data);
    void subscribe(std::string topic);
    MqttContext* context;

private:
    SemaphoreHandle_t connected_sem;
    esp_mqtt_client_handle_t client;
    std::vector<std::string> subscriptions;
    std::deque<std::pair<std::string, std::string>> messageQueue;

    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void resubscribe();
    void flushMessageQueue();

};
