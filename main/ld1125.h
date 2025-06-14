#pragma once

#include <memory>
#include <string>
#include <vector>

#include "driver/uart.h"
#include "esp_timer.h"
#include "sdkconfig.h"
#include "RadarSensor.h"

class LD1125 : public RadarSensor {
	static constexpr const char* TAG = "LD1125";
    public:
        LD1125(EventProc* ep, SettingsManager& settings)
            : RadarSensor(ep, settings),
              uartPort(UART_NUM_1),
              txPin(CONFIG_LD1125_TX_PIN),
              rxPin(CONFIG_LD1125_RX_PIN) {
            uart_config_t config{};
            config.baud_rate    = 115200;
            config.data_bits    = UART_DATA_8_BITS;
            config.parity       = UART_PARITY_DISABLE;
            config.stop_bits    = UART_STOP_BITS_1;
            config.flow_ctrl    = UART_HW_FLOWCTRL_DISABLE;
            config.source_clk   = UART_SCLK_DEFAULT;
            uart_param_config(uartPort, &config);
            uart_set_pin(
                uartPort,
                txPin,
                rxPin,
                UART_PIN_NO_CHANGE,
                UART_PIN_NO_CHANGE
            );
            uart_driver_install(
                uartPort,
                2 * 1024,
                0,
                0,
                nullptr,
                0
            );
			verifyTestMode();
        }


    void verifyTestMode() {
        static constexpr char testModeCmd[] = "test_mode=1\r\n";
        static constexpr char saveCmd[] = "save\r\n";

		ESP_LOGI(TAG, "test mode called =========================================== ");
        enum class ModeState {
            IDLE,
            SENT,
            ACKED,
            TIMEOUT
        };

        ModeState state = ModeState::IDLE;
        std::string buffer;
        int64_t startTime = esp_timer_get_time() / 1000;

        uart_flush_input(uartPort);
        uart_write_bytes(uartPort, testModeCmd, sizeof(testModeCmd) - 1);
        state = ModeState::SENT;

        while ((esp_timer_get_time() / 1000 - startTime) < 1000) {
            uint8_t byte;
            if (uart_read_bytes(uartPort, &byte, 1, 10 / portTICK_PERIOD_MS) > 0) {
                buffer.push_back(static_cast<char>(byte));
                if (buffer.find("str=") != std::string::npos) {
                    state = ModeState::ACKED;
                    break;
                }
            }
        }

        if (state == ModeState::ACKED) {
            uart_write_bytes(uartPort, saveCmd, sizeof(saveCmd) - 1);
            ESP_LOGI(TAG, "Test mode enabled and configuration saved.");
        } else {
            state = ModeState::TIMEOUT;
            ESP_LOGI(TAG, "Failed to enable test mode: timeout.");
        }
    }


        std::vector<std::unique_ptr<Value>> get_decoded_radar_data() override {
            std::vector<std::unique_ptr<Value>> valuesList;

            enum class State { WAIT, OCC_MOV, DIS, STR };
            static State state = State::WAIT;

            const int64_t timeoutMs     = 5000;
            int64_t startTimeMs         = esp_timer_get_time() / 1000;

            std::string distance;
            std::string strength;
            std::string rtype;
            uint8_t     byte;

            while (true) {
                if ((esp_timer_get_time() / 1000 - startTimeMs) >= timeoutMs) {
                    break;
                }

                int len = uart_read_bytes(
                    uartPort,
                    &byte,
                    1,
                    10 / portTICK_PERIOD_MS
                );
                if (len <= 0) {
                    break;
                }

                char c = static_cast<char>(byte);
				//putchar(c);
                switch (state) {
                    case State::WAIT:
                        if (c == 'o' || c == 'm') {
                            state = State::OCC_MOV;
                            rtype.push_back(c);
                        }
                        break;

                    case State::OCC_MOV:
                        rtype.push_back(c);
                        if (rtype == "occ," || rtype == "mov,") {
                            state = State::DIS;
                        } else if (rtype.length() > 4) {
                            rtype.clear();
                            state = State::WAIT;
                        }
                        break;

                    case State::DIS:
                        if (c == ',') {
                            state = State::STR;
                        } else if (c != ' ' &&
                                   c != 'd' &&
                                   c != 'i' &&
                                   c != 's' &&
                                   c != '=') {
                            distance.push_back(c);
                        }
                        break;

                    case State::STR:
                        if (c == '\n') {
                            std::string retType =
                                rtype.substr(0, rtype.length() - 1);
                            rtype.clear();

                            float power = 0.0f;
                            if (!strength.empty()) {
                                power = std::stof(strength) / 10.0f;
                                strength.clear();
                            }

                            float distanceVal = std::stof(distance);
                            distance.clear();

                            if (retType == "occ") {
                                valuesList.push_back(
                                    std::make_unique<Occupancy>(
                                        distanceVal,
                                        power
                                    )
                                );
                            } else {
                                valuesList.push_back(
                                    std::make_unique<Movement>(
                                        distanceVal,
                                        power
                                    )
                                );
                            }

                            state = State::WAIT;
                            return valuesList;
                        } else if (c != ' ' &&
                                   c != 's' &&
                                   c != 't' &&
                                   c != 'r' &&
                                   c != '=') {
                            strength.push_back(c);
                        }
                        break;
                }
            }

            return valuesList;
        }

    private:
        const uart_port_t uartPort;
        const int         txPin;
        const int         rxPin;
};

