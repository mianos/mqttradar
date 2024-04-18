#pragma once
#include <vector>
#include "driver/uart.h"
#include "RadarSensor.h"


class LD2450 : public RadarSensor {
	uart_port_t uartPort = UART_NUM_1;
	int txPin = 21;	// esp32c3 xiao pins
	int rxPin = 20;

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
    LD2450(EventProc* ep, SettingsManager& settings) : RadarSensor(ep, settings) {
		uart_config_t uartConfig;
		uartConfig.baud_rate = 256000;
		uartConfig.data_bits = UART_DATA_8_BITS;
		uartConfig.parity = UART_PARITY_DISABLE;
		uartConfig.stop_bits = UART_STOP_BITS_1;
		uartConfig.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
		uartConfig.source_clk = UART_SCLK_APB;
		uart_param_config(uartPort, &uartConfig);
		uart_set_pin(uartPort, txPin, rxPin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
		uart_driver_install(uartPort, 1024 * 2, 0, 0, NULL, 0);
    }

    std::vector<std::unique_ptr<Value>>  get_decoded_radar_data() {
	  uint8_t data[1024]; // Buffer for reading data
	  int length = 0; // Variable to store the length of received data

      std::vector<std::unique_ptr<Value>> valuesList;
      while (true) {
        if ((length = uart_read_bytes(uartPort, data, sizeof (data), 2 / portTICK_PERIOD_MS)) == 0) {
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
					startSeqCount = 0;  // Reset if any byte doesn't match
				}

				if (startSeqCount == 4) {  // All 4 start bytes matched
					currentState = PROCESSING_DATA;
					dataByteCount = 0;
					startSeqCount = 0;  // Reset for the next packet
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
					currentState = SEARCH_FOR_START;  // Something went wrong, start over
					endSeqCount = 0;
				}

				if (endSeqCount == TOTAL_END_BYTES) {
					// Print decoded target data only if not all distances are zero
					if (!isAllDistancesZero()) {
						for (int i = 0; i < 3; i++) { // for each target
							int16_t x = decodeCoordinate(targetData[i*8], targetData[i*8 + 1]);
							int16_t y = decodeCoordinate(targetData[i*8 + 2], targetData[i*8 + 3]);
							int16_t speed = decodeSpeed(targetData[i*8 + 4], targetData[i*8 + 5]);
							//uint16_t resolution = (targetData[i*8 + 7] << 8) | targetData[i*8 + 6]; // TODO: add this

							//ESP_LOGI("LD2450", "Target %d - X: %d mm, Y: %d mm, Speed: %d cm/s, Resolution: %d mm", i+1, x, y, speed, resolution);
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
