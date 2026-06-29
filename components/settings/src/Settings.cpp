#include "Settings.h"

#include "esp_log.h"

static const char* TAG = "settings";

Settings::Settings(NvsStorageManager& nvs, const std::string& key)
    : nvs_(nvs), key_(key) {
    std::string raw;
    if (nvs_.retrieve(key_, raw) && !raw.empty()) {
        JsonWrapper doc = JsonWrapper::Parse(raw);
        if (!doc.Empty()) {
            loadFromJson(doc);
        } else {
            ESP_LOGW(TAG, "stored config unparseable, using defaults");
        }
    } else {
        ESP_LOGI(TAG, "no stored config, using defaults");
    }
}

namespace {
// Apply a field if present; record (key, newValue) when it actually changes.
template <typename T>
void apply(const JsonWrapper& doc, const std::string& key, T& field,
           Settings::ChangeList& changes) {
    if (!doc.ContainsField(key)) return;
    T value{};
    if (!doc.GetField(key, value)) {
        ESP_LOGW(TAG, "field %s present but wrong type, ignored", key.c_str());
        return;
    }
    if (value == field) return;
    field = value;
    if constexpr (std::is_same_v<T, std::string>) {
        changes.emplace_back(key, field);
    } else {
        changes.emplace_back(key, std::to_string(field));
    }
}
}  // namespace

Settings::ChangeList Settings::loadFromJson(const JsonWrapper& doc) {
    ChangeList changes;
    apply(doc, "mqttBrokerUri",     mqttBrokerUri,     changes);
    apply(doc, "mqttUserName",      mqttUserName,      changes);
    apply(doc, "mqttPassword",      mqttUserPassword,  changes);
    apply(doc, "sensorName",        sensorName,        changes);
    apply(doc, "tz",                tz,                changes);
    apply(doc, "ntpServer",         ntpServer,         changes);
    apply(doc, "presencePeriodSec", presencePeriodSec, changes);
    return changes;
}

JsonWrapper Settings::toJson() const {
    JsonWrapper d;
    d.AddItem("mqttBrokerUri",     mqttBrokerUri);
    d.AddItem("mqttUserName",      mqttUserName);
    d.AddItem("mqttPassword",      mqttUserPassword);
    d.AddItem("sensorName",        sensorName);
    d.AddItem("tz",                tz);
    d.AddItem("ntpServer",         ntpServer);
    d.AddItem("presencePeriodSec", presencePeriodSec);
    return d;
}

void Settings::save() const {
    if (!nvs_.store(key_, toJson().ToString())) {
        ESP_LOGE(TAG, "failed to persist settings");
    }
}

void Settings::resetToDefaults() {
    // Construct a throwaway Settings against an NVS key that is never written,
    // so it keeps the compiled-in member defaults (single source of truth in
    // Settings.h) instead of duplicating them here.
    Settings defaults(nvs_, "__defaults_probe__");
    mqttBrokerUri     = defaults.mqttBrokerUri;
    mqttUserName      = defaults.mqttUserName;
    mqttUserPassword  = defaults.mqttUserPassword;
    sensorName        = defaults.sensorName;
    tz                = defaults.tz;
    ntpServer         = defaults.ntpServer;
    presencePeriodSec = defaults.presencePeriodSec;
}

void Settings::log() const {
    ESP_LOGI(TAG, "broker=%s user=%s name=%s tz=%s ntp=%s presencePeriod=%d",
             mqttBrokerUri.c_str(), mqttUserName.c_str(), sensorName.c_str(),
             tz.c_str(), ntpServer.c_str(), presencePeriodSec);
}

std::string Settings::changesToJson(const ChangeList& changes) {
    JsonWrapper d;
    for (const auto& kv : changes) {
        d.AddItem(kv.first, kv.second);
    }
    return d.ToString();
}
