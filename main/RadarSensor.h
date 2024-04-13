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

  void mqtt_update_presence(bool entry, const Value *vv=nullptr) {
		JsonWrapper doc;
		doc.AddItem("entry", entry);
		if (entry && vv) {
			vv->toJson(doc);
		}
		std::string status_topic = "tele/" + settings.sensorName + "/presence";
		// ESP_LOGI("LocalEP", "sending '%s' to '%s'\n", doc.ToString().c_str(), status_topic.c_str());
		mqttClient.publish(status_topic, doc.ToString());
	}

	void mqtt_track(const Value *vv) {
#if 0
	  JsonDocument doc;
	  doc["time"] = DateTime.toISOString();
	  vv->toJson(doc);
	  String status_topic = "tele/" + settings->sensorName + "/tracking";
	  String output;
	  serializeJson(doc, output);
	  ESP_LOGI("LocalEP", ("sending '%s' to '%s'\n", output.c_str(), status_topic.c_str());
	  mqttClient.publish(status_topic, output);
#endif
	}

public:
  LocalEP(SettingsManager& settings, MqttClient &mqtt_client) : settings(settings), mqttClient(mqtt_client) {
  }

  virtual void Detected(Value *vv) {
	// ESP_LOGI("LEP", "detected");
    mqtt_update_presence(true, vv);
  }

  virtual void Cleared() {
	//ESP_LOGI("LEP", "cleared");
    mqtt_update_presence(false);
  }

  virtual void PresenceUpdate(Value *vv) {
    uint32_t currentMillis = esp_timer_get_time() / 1000;
    if (settings.presence && (currentMillis - lastPresenceUpdateTime >= settings.presence)) {
		// vv->print();
		mqtt_update_presence(vv->etype() != "no", vv);
     	lastPresenceUpdateTime = currentMillis;
    }
  }

  virtual void TrackingUpdate(Value *vv) {
    uint32_t currentMillis = esp_timer_get_time() / 1000;
    if (settings.tracking && (currentMillis - lastTrackingUpdateTime >= settings.tracking)) {
//      mqtt->mqtt_track(vv);
     ESP_LOGI("LEP", "track");
	  vv->print();
      lastTrackingUpdateTime = currentMillis;
    }
  }
};

