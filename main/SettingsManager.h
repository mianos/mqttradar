#pragma once
#include <string>
#include "JsonWrapper.h"  // Ensure this file includes the necessary cJSON headers.

class SettingsManager {
public:
    std::string mqttServer = "mqtt2.mianos.com";
    int mqttPort = 1883;
    std::string sensorName = "radar3";
    int tracking = 0;
    int presence = 1000;
    int detectionTimeout = 10000;
    std::string tz = "AEST-10AEDT,M10.1.0,M4.1.0/3";

    // Method to update settings from a JSON string
    bool updateFromJson(const std::string& jsonString) {
        JsonWrapper json = JsonWrapper::Parse(jsonString);
        if (json.Empty()) {
            return false;
        }
        json.GetField("mqttServer", mqttServer);
        json.GetField("mqttPort", mqttPort);
        json.GetField("sensorName", sensorName);
        json.GetField("tracking", tracking);
        json.GetField("presence", presence);
        json.GetField("detectionTimeout", detectionTimeout);
        json.GetField("tz", tz);
        return true;
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
};

