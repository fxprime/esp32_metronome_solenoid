#pragma once

// Pin definitions
#define ENCODER_A 17
#define ENCODER_B 18
#define ENCODER_BTN 16
#define BTN_START 26
#define BTN_STOP 33
#define SOLENOID_PIN 13
#define DAC_PIN 25    // ESP32 DAC1 pin (GPIO25)

// Display I2C pins
#define DISPLAY_SDA 21
#define DISPLAY_SCL 22

// Timing constants
#define MIN_BPM 20
#define MAX_BPM 500
#define DEFAULT_BPM 120
#define MAX_BEATS 16
#define SOLENOID_PULSE_MS 5
#define ACCENT_PULSE_MS 7
#define SOUND_DURATION_MS 25  // Duration of sound on each beat (in ms)

// Audio settings
#define AUDIO_FREQ_CH1 220     // Frequency for channel 1 in Hz
#define AUDIO_FREQ_CH2 330     // Frequency for channel 2 in Hz
#define AUDIO_MIXER_INTERVAL_MS 2 // Interval for audio mixer in milliseconds

// Multiplier values (in quarters)
#define MULTIPLIER_COUNT 5
#define MULTIPLIERS {0.25, 0.5, 1.0, 2.0, 4.0}
#define MULTIPLIER_NAMES {"1/4", "1/2", "1", "2", "4"}

// Display dimensions
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// UI Constants
#define FLASH_DURATION_MS 50
