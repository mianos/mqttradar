#pragma once

#include <string>
#include <utility>
#include <vector>

#include "NvsStorageManager.h"
#include "JsonWrapper.h"

// Persistent device configuration for the mqttradar presence sensor. Mirrors
// the doorbell3 appsettings pattern: the whole config is stored as a single
// JSON blob in NVS (via NvsStorageManager) under one key. Missing/garbage
// config falls back to the compiled-in defaults below (fail-fast + log).
class Settings {
public:
    // MQTT field/key names match the topic payloads mqttradar has always used,
    // so existing cmnd/<name>/settings publishers keep working.
    std::string mqttBrokerUri    = "mqtt://mqtt2.mianos.com";
    std::string mqttUserName     = "";
    std::string mqttUserPassword = "";
    std::string sensorName       = "radar3";
    std::string tz               = "AEST-10AEDT,M10.1.0,M4.1.0/3";
    std::string ntpServer        = "time.google.com";
    int         presencePeriodSec = 1;  // <=0 disables presence publishing

    // (key, new-value) pairs for every field that changed in the last apply.
    using ChangeList = std::vector<std::pair<std::string, std::string>>;

    explicit Settings(NvsStorageManager& nvs, const std::string& key = "config");

    // Apply fields present in `doc`; returns the changed (key,value) pairs.
    ChangeList loadFromJson(const JsonWrapper& doc);

    JsonWrapper toJson() const;                                  // full settings as JSON
    void        save() const;                                    // persist to NVS
    void        resetToDefaults();                               // restore compiled-in defaults
    void        log() const;
    static std::string changesToJson(const ChangeList& changes); // ack payload

private:
    NvsStorageManager& nvs_;
    std::string        key_;
};
