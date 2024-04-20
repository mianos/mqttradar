#pragma once
#include <string>
#include <type_traits>
#include <vector>
#include <utility>

#include "esp_log.h"
#include "NvsStorageManager.h"
#include "JsonWrapper.h"


class SettingsManager {
	NvsStorageManager nvs;
public:
	using ChangeList = std::vector<std::pair<std::string, std::string>>;

    SettingsManager(NvsStorageManager& nvs) : nvs(nvs) {
        loadSettings();
    }

    std::string mqttBrokerUri = "mqtt://mqtt2.mianos.com";
	std::string mqttUserName = "";
	std::string mqttUserPassword = "";
    std::string sensorName = "radar3";
    int tracking = 0;
    int presence = 1000;
    int detectionTimeout = 10000;
    std::string tz = "AEST-10AEDT,M10.1.0,M4.1.0/3";
    std::string ntpServer = "time.google.com";


	std::string convertChangesToJson(const SettingsManager::ChangeList& changes) {
		cJSON *root = cJSON_CreateObject();
		for (const auto& [key, value] : changes) {
			cJSON_AddStringToObject(root, key.c_str(), value.c_str());
		}
		char *rawJson = cJSON_Print(root);
		std::string jsonResponse(rawJson);
		cJSON_Delete(root);
		free(rawJson); // cJSON_Print allocates memory that must be freed
		return jsonResponse;
	}

	void loadSettings() {
        std::string value;

        nvs.retrieve("mqttBrokerUri", mqttBrokerUri);
        nvs.retrieve("mqttUserName", mqttUserName);
        nvs.retrieve("mqttUserPassword", mqttUserPassword);
        nvs.retrieve("sensorName", sensorName);
        if (nvs.retrieve("tracking", value)) tracking = std::stoi(value);
        if (nvs.retrieve("presence", value)) presence = std::stoi(value);
        if (nvs.retrieve("detectionTimeout", value)) detectionTimeout = std::stoi(value);
        nvs.retrieve("tz", tz);
        nvs.retrieve("ntpServer", ntpServer);
    }


	std::string toJson() const {
        JsonWrapper json;
        json.AddItem("mqttBrokerUri", mqttBrokerUri);
        json.AddItem("mqttUserName", mqttUserName);
        json.AddItem("mqttUserPassword", mqttUserPassword);
        json.AddItem("sensorName", sensorName);
        json.AddItem("tracking", tracking);
        json.AddItem("presence", presence);
        json.AddItem("detectionTimeout", detectionTimeout);
        json.AddItem("tz", tz);
        json.AddItem("ntpServer", ntpServer);
        return json.ToString();
    }


   ChangeList updateFromJson(const std::string& jsonString) {
        ChangeList changes;
        JsonWrapper json = JsonWrapper::Parse(jsonString);
        updateFieldIfChanged(json, "mqttBrokerUri", mqttBrokerUri, changes);
        updateFieldIfChanged(json, "mqttUserName", mqttUserName, changes);
        updateFieldIfChanged(json, "mqttUserPassword", mqttUserPassword, changes);
        updateFieldIfChanged(json, "sensorName", sensorName, changes);
        updateFieldIfChanged(json, "tracking", tracking, changes);
        updateFieldIfChanged(json, "presence", presence, changes);
        updateFieldIfChanged(json, "detectionTimeout", detectionTimeout, changes);
        updateFieldIfChanged(json, "tz", tz, changes);
        updateFieldIfChanged(json, "ntpServer", ntpServer, changes);

        // Save any changes to NVRAM
        for (const auto& [key, value] : changes) {
            nvs.store(key, value);
        }
        return changes;
    }
private:
	template <typename T>
	void updateFieldIfChanged(JsonWrapper& json, const std::string& key, T& field, SettingsManager::ChangeList& changes) {
		if (json.ContainsField(key)) {  // Only proceed if the key exists in the JSON
			T newValue;
			if (json.GetField(key, newValue)) {  // Successfully retrieved new value
				if (newValue != field) {
					field = newValue;

					// Log the change for response
					if constexpr (std::is_same_v<T, std::string>) {
						changes.emplace_back(key, field);
					} else {
						changes.emplace_back(key, std::to_string(field));
					}
				}
			} else {
				ESP_LOGE("SettingsUpdate", "Failed to retrieve new value for %s", key.c_str());
			}
		}
	}

};

