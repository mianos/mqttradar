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

#include "JsonWrapper.h"

using HandlerFunc = std::function<esp_err_t(class MqttClient*, const std::string&, const JsonWrapper&, void*)>;

struct HandlerBinding {
    std::string subscriptionTopic;
    std::regex matchPattern;
    HandlerFunc handler;
    void* context;  // Use void* for context
};

class MqttClient {
public:
	std::string sensorName;

	MqttClient(esp_mqtt_client_config_t& mqtt_cfg, std::string sensorName);
    ~MqttClient();

    void start();
    void wait_for_connection();
    void publish(std::string topic, std::string data);
    void subscribe(std::string topic);
	
	void registerHandler(const std::string topic, const std::regex pattern, HandlerFunc handler, void* context);

private:
    SemaphoreHandle_t connected_sem;
    esp_mqtt_client_handle_t client;
    std::vector<std::string> subscriptions;
    std::deque<std::pair<std::string, std::string>> messageQueue;

    static void mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
    void resubscribe();
    void flushMessageQueue();

	std::vector<HandlerBinding> bindings;
	static void dispatchEvent(MqttClient* client, const std::string& topic, const JsonWrapper& data);
};
