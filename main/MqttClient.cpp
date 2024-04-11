#include <cstring>
#include <string>
#include <regex>
#include <memory>
#include <functional>

#include "esp_log.h"

#include "JsonWrapper.h"
#include "MqttClient.h"

static const char* TAG = "MqttClient";

MqttClient::MqttClient(MqttContext* context, const char* brokerUri, const char* clientId, const char* username, const char* password)
    : context(context) {
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
	esp_mqtt_client_register_event(client, static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID), mqtt_event_handler, this);

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

using HandlerFunc = std::function<void(MqttClient&, MqttContext&, const std::string&, cJSON*)>;

struct HandlerBinding {
    std::regex pattern;
    HandlerFunc handler;
};


void handleConfig(MqttClient& client, MqttContext& context, const std::string& topic, cJSON* data) {
    JsonWrapper jsonData(data);
    auto jsonString = jsonData.ToString();

    if (jsonString.empty()) {
        ESP_LOGE(TAG, "Failed to serialize JSON data");
        return;
    }

    ESP_LOGI(TAG, "Config handler - Topic: %s, Data: %s", topic.c_str(), jsonString.c_str());
}

constexpr const char* device = "radar3";

std::vector<HandlerBinding> handlers = {
    {std::regex(std::string("cmnd/") + device + "/config"), handleConfig},
    // Add more bindings as needed...
};


void dispatchEvent(MqttClient& client, MqttContext& context, const std::string& topic, cJSON* data) {
    for (const auto& binding : handlers) {
        if (std::regex_match(topic, binding.pattern)) {
            binding.handler(client, context, topic, data);
            return; // Assuming only one handler per topic pattern
        }
    }
    // Optionally, handle the case where no pattern matches
}

void MqttClient::mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    auto* clientInstance = static_cast<MqttClient*>(handler_args); // Cast to MqttClient*
    auto* context = clientInstance->context; // Access the MqttContext from the clientInstance
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data); // Cast event_data to esp_mqtt_event_handle_t

    // Check the type of MQTT event
    switch (event->event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		break;
	case MQTT_EVENT_DISCONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
		break;
	case MQTT_EVENT_SUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_UNSUBSCRIBED:
		ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_PUBLISHED:
		ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
		break;
	case MQTT_EVENT_DATA: {
			ESP_LOGI(TAG, "MQTT_EVENT_DATA");
			std::string topicStr(event->topic, event->topic_len);
			std::string payloadStr(event->data, event->data_len);

			auto jsonPayload = JsonWrapper::Parse(payloadStr);
			if (!jsonPayload.Empty()) {
				dispatchEvent(*clientInstance, *context, topicStr, jsonPayload.Release());
			} else {
				const char* error_ptr = cJSON_GetErrorPtr();
				if (error_ptr != nullptr) {
					ESP_LOGE(TAG, "Error parsing JSON: %s", error_ptr);
				}
			}
		}
		break;
	case MQTT_EVENT_ERROR:
		ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
		break;
	default:
		ESP_LOGI(TAG, "Other MQTT Event ID: %d", event->event_id);
		break;
    }
}

