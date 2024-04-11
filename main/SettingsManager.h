#pragma once
#include <string>

struct SettingsManager {
  std::string mqttServer = "mqtt2.mianos.com";
  int mqttPort = 1883;
  std::string sensorName = "radar3";
  int tracking = 0;
  int presence = 10000;
  int detectionTimeout = 60000;
  std::string tz = "AEST-10AEDT,M10.1.0,M4.1.0/3";

  SettingsManager(); // Constructor declaration

  enum class SettingChange {
    None = 0,
    //VolumeChanged
  };
};
