idf_component_register(SRCS "src/WifiManager.cpp"
                      INCLUDE_DIRS "include"
                       REQUIRES esp_wifi
                       nvs_flash
                       wifi_provisioning
                       nvsstoragemanager)
