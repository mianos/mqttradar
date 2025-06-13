#include "RadarSensor.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string>

static const char* TAG_RS = "RadarSensor";

RadarSensor::RadarSensor(EventProc* ep, SettingsManager& settings)
    : settings(settings),
      ep(ep),
      currentState(STATE_NOT_DETECTED),
      lastDetectionTime(0) {}

void RadarSensor::process(float minDistance) {
    auto valuesList = get_decoded_radar_data();

#if 0
    // print only when debounced readings arrive
    if (!valuesList.empty()) {
        for (const auto& v : valuesList) {
            ESP_LOGI(TAG_RS,
                     "Emit: type=%s distance=%.2f m power=%.2f",
                     v->etype().c_str(),
                     v->get_main(),
                     v->get_power());
        }
    }
#endif
    bool noTargetFound = true;
    if (!valuesList.empty()) {
        for (const auto& v : valuesList) {
            if (v->get_main() >= minDistance) {
                noTargetFound = false;
                break;
            }
        }
    }

    uint64_t now = esp_timer_get_time() / 1000;
    switch (currentState) {
        case STATE_NOT_DETECTED:
            if (!noTargetFound) {
                float dist = valuesList.front()->get_main();
                ESP_LOGI(TAG_RS,
                         "Detected: distance=%.2f m",
                         dist);
                ep->Detected(valuesList.front().get());
                currentState = STATE_DETECTED_ONCE;
            }
            break;

        case STATE_DETECTED_ONCE:
            currentState = STATE_DETECTED;
            lastDetectionTime = now;
            break;

        case STATE_DETECTED:
            if (noTargetFound &&
                now - lastDetectionTime > settings.detectionTimeout) {
                ESP_LOGI(TAG_RS, "Cleared");
                ep->Cleared();
                currentState = STATE_CLEARED_ONCE;
            } else if (!noTargetFound) {
                lastDetectionTime = now;
            }
            break;

        case STATE_CLEARED_ONCE:
            currentState = STATE_NOT_DETECTED;
            break;
    }
}

