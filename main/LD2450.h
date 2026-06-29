#pragma once

#include <memory>
#include <vector>

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"

#include "RadarSensor.h"   // ldradar component: RadarSensor / EventProc / Value / Range

// HLK-LD2450 multi-target tracking radar driver.
//
// Unlike the LD1125 (ASCII "test mode" protocol at 115200), the LD2450 streams
// a fixed binary frame at 256000 baud: header AA FF 03 00, then 3 targets * 8
// bytes (x, y, speed, resolution, little-endian, sign in bit 15), then the
// trailer 55 CC. Each non-zero target becomes a Range value (metres, m/s).
//
// The application owns UART configuration and driver install (see
// setupRadarUart in main.cpp) — this class only parses the byte stream, to
// match the ldradar component's LD1125 convention.
class LD2450 : public RadarSensor {
    uart_port_t uartPort;

    enum DecoderState {
        SEARCH_FOR_START,
        PROCESSING_DATA,
        VERIFY_END
    };

    DecoderState currentState = SEARCH_FOR_START;
    int startSeqCount = 0;
    int dataByteCount = 0;
    int endSeqCount = 0;

    static const int TOTAL_TARGET_BYTES = 24;  // 8 bytes for each of 3 targets
    static const int TOTAL_END_BYTES = 2;

    uint8_t targetData[TOTAL_TARGET_BYTES];

    bool isAllDistancesZero() {
        for (int i = 0; i < 3; i++) {
            int16_t x = decodeCoordinate(targetData[i*8], targetData[i*8 + 1]);
            int16_t y = decodeCoordinate(targetData[i*8 + 2], targetData[i*8 + 3]);
            int16_t speed = decodeSpeed(targetData[i*8 + 4], targetData[i*8 + 5]);
            if (x != 0 || y != 0 || speed != 0) {
                return false;
            }
        }
        return true;
    }

    int16_t decodeSpeed(uint8_t lowByte, uint8_t highByte) {
        int16_t speed = (highByte & 0x7F) << 8 | lowByte;
        if ((highByte & 0x80) == 0) {
            speed = -speed;
        }
        return speed;
    }

    int16_t decodeCoordinate(uint8_t lowByte, uint8_t highByte) {
        int16_t coordinate = (highByte & 0x7F) << 8 | lowByte;
        if ((highByte & 0x80) == 0) {
            coordinate = -coordinate;
        }
        return coordinate;
    }

public:
    explicit LD2450(EventProc* event_processor, uart_port_t uart_port_in)
        : RadarSensor(event_processor), uartPort(uart_port_in) {}

    std::vector<std::unique_ptr<Value>> get_decoded_radar_data() override {
        uint8_t data[1024];
        int length = 0;

        std::vector<std::unique_ptr<Value>> valuesList;
        while (true) {
            if ((length = uart_read_bytes(uartPort, data, sizeof(data), 2 / portTICK_PERIOD_MS)) == 0) {
                break;
            }
            for (int i = 0; i < length; ++i) {
                uint8_t byteValue = data[i];
                switch (currentState) {
                case SEARCH_FOR_START:
                    if ((startSeqCount == 0 && byteValue == 0xAA) ||
                        (startSeqCount == 1 && byteValue == 0xFF) ||
                        (startSeqCount == 2 && byteValue == 0x03) ||
                        (startSeqCount == 3 && byteValue == 0x00)) {
                        startSeqCount++;
                    } else {
                        startSeqCount = 0;
                    }
                    if (startSeqCount == 4) {
                        currentState = PROCESSING_DATA;
                        dataByteCount = 0;
                        startSeqCount = 0;
                    }
                    break;

                case PROCESSING_DATA:
                    targetData[dataByteCount] = byteValue;
                    dataByteCount++;
                    if (dataByteCount == TOTAL_TARGET_BYTES) {
                        currentState = VERIFY_END;
                    }
                    break;

                case VERIFY_END:
                    if ((endSeqCount == 0 && byteValue == 0x55) ||
                        (endSeqCount == 1 && byteValue == 0xCC)) {
                        endSeqCount++;
                    } else {
                        currentState = SEARCH_FOR_START;
                        endSeqCount = 0;
                    }
                    if (endSeqCount == TOTAL_END_BYTES) {
                        if (!isAllDistancesZero()) {
                            for (int i = 0; i < 3; i++) {
                                int16_t x = decodeCoordinate(targetData[i*8], targetData[i*8 + 1]);
                                int16_t y = decodeCoordinate(targetData[i*8 + 2], targetData[i*8 + 3]);
                                int16_t speed = decodeSpeed(targetData[i*8 + 4], targetData[i*8 + 5]);
                                if (x) {
                                    valuesList.push_back(std::unique_ptr<Value>(
                                        new Range(static_cast<float>(x) / 1000.0,
                                                  static_cast<float>(y) / 1000.0,
                                                  static_cast<float>(speed) * 0.036,
                                                  i)));
                                }
                            }
                        }
                        currentState = SEARCH_FOR_START;
                        endSeqCount = 0;
                        return valuesList;
                    }
                    break;
                }
            }
        }
        return valuesList;
    }
};
