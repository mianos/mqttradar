#include <cstring>
#include <string>
#include <memory>

#include "esp_log.h"

#include "MqttClient.h"

static const char* TAG = "MqttClient";

MqttClient::MqttClient(esp_mqtt_client_config_t& mqtt_cfg, std::string sensorName)
        : sensorName(sensorName) {
    connected_sem = xSemaphoreCreateBinary();

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client,
								static_cast<esp_mqtt_event_id_t>(ESP_EVENT_ANY_ID),
								mqtt_event_handler,
								this);
}

MqttClient::~MqttClient() {
    esp_mqtt_client_stop(client);
    esp_mqtt_client_destroy(client);
}

void MqttClient::start() {
    esp_mqtt_client_start(client);
}

void MqttClient::publish(std::string topic, std::string data) {
    if (xSemaphoreTake(connected_sem, 0) == pdTRUE) {
        esp_mqtt_client_publish(client, topic.c_str(), data.c_str(), 0, 1, 0);
        xSemaphoreGive(connected_sem);
		// ESP_LOGI(TAG, "published %s data %s", topic.c_str(), data.c_str());
    } else {
        messageQueue.emplace_back(std::make_pair(topic, data));
	}
}


void MqttClient::flushMessageQueue() {
    while (!messageQueue.empty()) {
        const auto& msg = messageQueue.front();
        esp_mqtt_client_publish(client, msg.first.c_str(), msg.second.c_str(), 0, 1, 0);
        messageQueue.pop_front();  // Remove the message from the queue after publishing
    }
}

void MqttClient::subscribe(std::string topic) {
    // Check if the topic is already in the vector to avoid duplicates
    if (std::find(subscriptions.begin(), subscriptions.end(), topic) == subscriptions.end()) {
        subscriptions.push_back(topic);  // Add to subscriptions if not already present
    }
    // Perform subscription only if already connected
    if (xSemaphoreTake(connected_sem, 0) == pdTRUE) {  // Non-blocking take
        esp_mqtt_client_subscribe(client, topic.c_str(), 0);
        xSemaphoreGive(connected_sem);  // Release the semaphore immediately after checking
    }
}

void MqttClient::resubscribe() {
    for (const auto& topic : subscriptions) {
        esp_mqtt_client_subscribe(client, topic.c_str(), 0);
    }
}

void MqttClient::registerHandler(const std::string topic,
							     const std::regex pattern,
								 HandlerFunc handler, void* context) {
	subscribe(topic);
	bindings.push_back({topic, pattern, handler, context});
}


void MqttClient::dispatchEvent(MqttClient* client, const std::string& topic, const JsonWrapper& data) {
	//ESP_LOGI(TAG, "topic in '%s'", topic.c_str());
    for (const auto& binding : client->bindings) {
        if (std::regex_match(topic, binding.matchPattern)) {
            if (binding.handler) {
                binding.handler(client, topic, data, binding.context);
                return; // Assuming only one handler per topic pattern
            }
        }
    }
    ESP_LOGW(TAG, "Unhandled topic: %s", topic.c_str());
}


void MqttClient::mqtt_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    auto* clientInstance = static_cast<MqttClient*>(handler_args); // Cast to MqttClient*
    auto* event = static_cast<esp_mqtt_event_handle_t>(event_data); // Cast event_data to esp_mqtt_event_handle_t

    // Check the type of MQTT event
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        xSemaphoreGive(clientInstance->connected_sem);
        clientInstance->resubscribe();
		clientInstance->flushMessageQueue();
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        //ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
       //  ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA: {
			std::string topicStr(event->topic, event->topic_len);
			std::string payloadStr(event->data, event->data_len);

			auto jsonPayload = JsonWrapper::Parse(payloadStr);
			if (!jsonPayload.Empty()) {
				dispatchEvent(clientInstance, topicStr, jsonPayload);
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

void MqttClient::wait_for_connection() {
    if (xSemaphoreTake(connected_sem, portMAX_DELAY)) {
        ESP_LOGI(TAG, "Connected, semaphore released");
    } else {
        ESP_LOGE(TAG, "Something went wrong with semaphore");
    }
}
