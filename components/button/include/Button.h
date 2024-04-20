#pragma once
#include "driver/gpio.h"
#include "esp_timer.h"

class Button {
public:
    explicit Button(gpio_num_t pin = GPIO_NUM_9) : buttonPin(pin), state(OPEN) {
        gpio_reset_pin(buttonPin);  // Reset the pin to ensure it's not used by other configs
        gpio_set_direction(buttonPin, GPIO_MODE_INPUT);
        gpio_pullup_en(buttonPin);  // Enable pull-up since button goes to GND when pressed
        lastStateChangeTime = esp_timer_get_time();
    }

    bool longPressed() {
        switch(state) {
            case OPEN:
                if (gpio_get_level(buttonPin) == 0) { // Button is pressed (active low)
                    state = CLOSED;
                    lastStateChangeTime = esp_timer_get_time();
                }
                break;

            case CLOSED:
                if (gpio_get_level(buttonPin) == 0) { // Still pressed
                    if (esp_timer_get_time() - lastStateChangeTime > LONG_PRESS_TIME_MS * 1000) {
                        state = TRIGGERED;
                        return true; // Trigger the long press action immediately
                    }
                } else {
                    state = OPEN;
                }
                break;

            case TRIGGERED:
                if (gpio_get_level(buttonPin) == 1) { // Check if the button is released
                    state = OPEN;
                }
                break;

            default:
                state = OPEN;
                break;
        }
        return false;
    }

private:
    gpio_num_t buttonPin;
    int64_t lastStateChangeTime;
    enum State { OPEN, CLOSED, TRIGGERED };
    State state;
    static constexpr int LONG_PRESS_TIME_MS = 5000; // Long press time in milliseconds
};

