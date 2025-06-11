#pragma once
#include <vector>
#include <deque>
#include <string>
#include <utility>
#include <regex>
#include <functional>

#include <cJSON.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "mqtt_client.h"

#include "SettingsManager.h"

using HandlerFunc = std::function<void(class MqttClient *, const std::string&, cJSON*)>;

struct HandlerBinding {
    std::string subscriptionTopic;  // Topic pattern used for MQTT subscription
    std::regex matchPattern;        // Regex pattern used to match incoming topics
    HandlerFunc handler;            // Function to handle the incoming data
};

class MqttClient {
public:
    MqttClient(SettingsManager& settings);
    ~MqttClient();

    void start();
    void wait_for_connection();
    void publish(std::string topic, std::string data);
    void subscribe(std::string topic);
	void registerHandlers();

private:
	SettingsManager& settings;
    SemaphoreHandle_t connected_sem;
    esp_mqtt_client_handle_t client;
    std::vector<std::string> subscriptions;
    std::deque<std::pair<std::string, std::string>> messageQueue;

    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void resubscribe();
    void flushMessageQueue();

	std::vector<HandlerBinding> bindings;
	static void dispatchEvent(MqttClient* client, const std::string& topic, cJSON* data);
};
