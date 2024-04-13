#pragma once
#include <vector>

#include "esp_timer.h"

#include "Events.h"
#include "SettingsManager.h"
#include "MqttClient.h"

class RadarSensor {
  SettingsManager& settings;
public:
    RadarSensor(EventProc* ep,  SettingsManager& settings);
    virtual std::vector<std::unique_ptr<Value>> get_decoded_radar_data() = 0;
    void process(float minPower = 0.0);

protected:
    enum DetectionState {
        STATE_NOT_DETECTED,
        STATE_DETECTED_ONCE,
        STATE_DETECTED,
        STATE_CLEARED_ONCE
    };

    EventProc* ep;
    DetectionState currentState = STATE_NOT_DETECTED;
    uint64_t lastDetectionTime = 0;
};

class LocalEP : public EventProc {
  SettingsManager& settings;
  uint32_t lastTrackingUpdateTime = 0;
  uint32_t lastPresenceUpdateTime = 0;
  MqttClient& mqttClient;
public:
  LocalEP(SettingsManager& settings, MqttClient &mqtt_client) : settings(settings), mqttClient(mqtt_client) {
  }

  virtual void Detected(Value *vv) {
	ESP_LOGI("LEP", "detected");
    //mqtt->mqtt_update_presence(true, vv);
  }

  virtual void Cleared() {
	ESP_LOGI("LEP", "cleared");
//    mqtt->mqtt_update_presence(false);
  }

  virtual void PresenceUpdate(Value *vv) {
    uint32_t currentMillis = esp_timer_get_time() / 1000;
    if (settings.presence && (currentMillis - lastPresenceUpdateTime >= settings.presence)) {
//     ESP_LOGI("LEP", "update with flag etype %s", vv->etype());
		vv->print();
//      mqtt->mqtt_update_presence(vv->etype() != "no", vv);
      lastPresenceUpdateTime = currentMillis;
    }
  }

  virtual void TrackingUpdate(Value *vv) {
  //  if (!mqtt->client.connected()) {
//      return;
//    }
    uint32_t currentMillis = esp_timer_get_time() / 1000;
    if (settings.tracking && (currentMillis - lastTrackingUpdateTime >= settings.tracking)) {
//      mqtt->mqtt_track(vv);
     ESP_LOGI("LEP", "track");
	 vv->print();
      lastTrackingUpdateTime = currentMillis;
    }
  }
};

