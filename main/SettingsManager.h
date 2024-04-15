#pragma once
#include <string>
#include <type_traits>
#include <vector>
#include <utility>

#include "nv.h"
#include "JsonWrapper.h"

class SettingsManager {
	NvsStorageManager nvs;
public:
	using ChangeList = std::vector<std::pair<std::string, std::string>>;

    SettingsManager(NvsStorageManager& nvs) : nvs(nvs) {
        loadSettings();
    }

    std::string mqttServer = "mqtt2.mianos.com";
    int mqttPort = 1883;
    std::string sensorName = "radar3";
    int tracking = 0;
    int presence = 1000;
    int detectionTimeout = 10000;
    std::string tz = "AEST-10AEDT,M10.1.0,M4.1.0/3";

	void loadSettings() {
        std::string value; // Temporary storage for the retrieved value

        nvs.retrieve("mqttServer", mqttServer);
        if (nvs.retrieve("mqttPort", value)) mqttPort = std::stoi(value);
        nvs.retrieve("sensorName", sensorName);
        if (nvs.retrieve("tracking", value)) tracking = std::stoi(value);
        if (nvs.retrieve("presence", value)) presence = std::stoi(value);
        if (nvs.retrieve("detectionTimeout", value)) detectionTimeout = std::stoi(value);
        nvs.retrieve("tz", tz);
    }


	std::string ToJson() const {
        JsonWrapper json;
        json.AddItem("mqttServer", mqttServer);
        json.AddItem("mqttPort", mqttPort);
        json.AddItem("sensorName", sensorName);
        json.AddItem("tracking", tracking);
        json.AddItem("presence", presence);
        json.AddItem("detectionTimeout", detectionTimeout);
        json.AddItem("tz", tz);
        return json.ToString();
    }


   ChangeList updateFromJson(const std::string& jsonString) {
        ChangeList changes;
        JsonWrapper json = JsonWrapper::Parse(jsonString);
        updateFieldIfChanged(json, "mqttServer", mqttServer, changes);
        updateFieldIfChanged(json, "mqttPort", mqttPort, changes);
        updateFieldIfChanged(json, "sensorName", sensorName, changes);
        updateFieldIfChanged(json, "tracking", tracking, changes);
        updateFieldIfChanged(json, "presence", presence, changes);
        updateFieldIfChanged(json, "detectionTimeout", detectionTimeout, changes);
        updateFieldIfChanged(json, "tz", tz, changes);

        // Save any changes to NVRAM
        for (const auto& [key, value] : changes) {
            nvs.store(key, value);
        }
        return changes;
    }
private:
	template <typename T>
	void updateFieldIfChanged(JsonWrapper& json,
			const std::string& key,
			T& field,
			SettingsManager::ChangeList& changes) {
		if (json.ContainsField(key)) {  // Only proceed if the key exists in the JSON
			T newValue;
			if (json.GetField(key, newValue)) {  // Try to get the new value
				if (newValue != field) {
					field = newValue;  // Update the field if the new value is different

					// Use type traits to handle conversion based on type
					if constexpr (std::is_same_v<T, std::string>) {
						changes.emplace_back(key, field);  // Directly use the string
					} else {
						changes.emplace_back(key, std::to_string(field));  // Convert numeric types to string
					}
				}
			}
		}
	}

};

