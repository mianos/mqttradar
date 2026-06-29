idf_component_register(SRCS "src/MqttClient.cpp"
                        INCLUDE_DIRS "include"
                       REQUIRES espressif__mqtt
                       nvsstoragemanager
                       espressif__cjson
                       jsonwrapper)
