set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

idf_component_register(SRCS "main.cpp" "RadarWebServer.cpp"
                       INCLUDE_DIRS "."
                       REQUIRES
                         nvsstoragemanager
                         wifimanager
                         settings
                         mqttwrapper
                         webserver
                         ldradar
                         jsonwrapper
                         espressif__mqtt
                         esp_netif
                         esp_event
                         nvs_flash
                         app_update
                         esp_timer
                         esp_driver_uart
                         esp_driver_gpio)
