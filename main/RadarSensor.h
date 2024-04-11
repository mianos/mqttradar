#pragma once
#include <vector>
#include <memory>

#include "Events.h"
#include "SettingsManager.h"

class RadarSensor {
  std::shared_ptr<SettingsManager> settings;
public:
    RadarSensor(EventProc* ep,  std::shared_ptr<SettingsManager> settings);
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

