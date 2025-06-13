
#pragma once

#include <vector>
#include <memory>
#include <string>
#include <cmath>
#include <cstdio>

#include "driver/uart.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "esp_timer.h"
#include "RadarSensor.h"

class LD1125 : public RadarSensor {
 public:
    LD1125(EventProc* ep, SettingsManager& settings)
        : RadarSensor(ep, settings),
          uartPort(UART_NUM_1),
          txPin(CONFIG_LD1125_TX_PIN),
          rxPin(CONFIG_LD1125_RX_PIN) {
        uart_config_t cfg{};
        cfg.baud_rate    = 115200;
        cfg.data_bits    = UART_DATA_8_BITS;
        cfg.parity       = UART_PARITY_DISABLE;
        cfg.stop_bits    = UART_STOP_BITS_1;
        cfg.flow_ctrl    = UART_HW_FLOWCTRL_DISABLE;
        cfg.source_clk   = UART_SCLK_DEFAULT;
        uart_param_config(uartPort, &cfg);
        uart_set_pin(uartPort, txPin, rxPin,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
        uart_driver_install(uartPort, 2048, 0, 0, nullptr, 0);
        verifyTestMode();
    }

    std::vector<std::unique_ptr<Value>> get_decoded_radar_data() override {
        std::vector<std::unique_ptr<Value>> values;
        std::string line;
        const int64_t timeoutMs = 5000;
        int64_t start = esp_timer_get_time() / 1000;
        uint8_t b;
        while (esp_timer_get_time() / 1000 - start < timeoutMs) {
            if (uart_read_bytes(uartPort, &b, 1,
                                10 / portTICK_PERIOD_MS) <= 0) break;
            char c = static_cast<char>(b);
            if (c == '\r') continue;
            line.push_back(c);
            if (c == '\n') {
                parseLine(line, values);
                line.clear();
                return values;
            }
        }
        if (!lastType.empty()) parseLine(lastType + ",dis=" +
            std::to_string(lastDistance) + ",str=" +
            std::to_string(lastPower) + "\n", values);
        return values;
    }

    void debugRaw() {
        constexpr char cmd[] = "test_mode=1\r\n";
        uart_flush_input(uartPort);
        uart_write_bytes(uartPort, cmd, sizeof(cmd) - 1);
        vTaskDelay(pdMS_TO_TICKS(100));
        uint8_t b;
        while (true) if (uart_read_bytes(uartPort, &b, 1,
                        pdMS_TO_TICKS(100)) > 0) putchar(static_cast<char>(b));
    }

 private:
    enum class ModeState { IDLE, SENT, ACKED, TIMEOUT };
    const uart_port_t uartPort;
    const int txPin;
    const int rxPin;
    float lastDistance = -1.0f;
    float lastPower    = 0.0f;
    std::string lastType;

    void verifyTestMode() {
        constexpr char cmd[] = "test_mode=1\r\n";
        ModeState state = ModeState::IDLE;
        int64_t start = esp_timer_get_time() / 1000;
        std::string buf;
        uart_flush_input(uartPort);
        uart_write_bytes(uartPort, cmd, sizeof(cmd) - 1);
        state = ModeState::SENT;
        while (esp_timer_get_time() / 1000 - start < 1000) {
            uint8_t b;
            if (uart_read_bytes(uartPort, &b, 1,
                10 / portTICK_PERIOD_MS) > 0) {
                buf.push_back(static_cast<char>(b));
                if (buf.find("OK") != std::string::npos) {
                    state = ModeState::ACKED;
                    break;
                }
            }
        }
        if (state != ModeState::ACKED) state = ModeState::TIMEOUT;
        putchar(state == ModeState::ACKED ? 'T' : 'F');
    }

    void parseLine(const std::string& line,
                   std::vector<std::unique_ptr<Value>>& values) {
        size_t c = line.find(',');
        std::string type = c == std::string::npos
            ? line.substr(0, line.size() - 1)
            : line.substr(0, c);
        float d = 0.0f;
        float p = 0.0f;
        size_t pos = line.find("dis=");
        if (pos != std::string::npos) {
            size_t start = pos + 4;
            size_t end = line.find(',', start);
            d = std::stof(line.substr(start, end - start));
        }
        pos = line.find("str=");
        if (pos != std::string::npos) {
            size_t start = pos + 4;
            size_t end = line.find(',', start);
            p = std::stof(line.substr(start, end - start));
        }
        if (!(type == lastType &&
              std::fabs(d - lastDistance) < 1e-3f &&
              std::fabs(p - lastPower)    < 1e-3f)) {
            lastType     = type;
            lastDistance = d;
            lastPower    = p;
        }
        if (lastType == "occ") values.push_back(
            std::make_unique<Occupancy>(lastDistance, lastPower));
        else if (lastType == "mov") values.push_back(
            std::make_unique<Movement>(lastDistance, lastPower));
    }
};

