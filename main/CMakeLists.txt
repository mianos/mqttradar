# Set the C++ standard to C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Include the ESP-IDF components
idf_component_register(SRCS main.cpp 
                    WifiManager.cpp
                    nv.cpp
                    MqttClient.cpp
                INCLUDE_DIRS "."
                REQUIRES mqtt 
                    esp_wifi
                    nvs_flash
					json)
