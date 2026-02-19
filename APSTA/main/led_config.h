// led_config.h
#pragma once
#include "driver/ledc.h"

#define LED_GPIO        2   // তোমার LED pin নাম্বার
#define LEDC_CHANNEL    LEDC_CHANNEL_0
#define LEDC_TIMER      LEDC_TIMER_0
#define LEDC_MODE       LEDC_HIGH_SPEED_MODE
#define LEDC_DUTY_RES   LEDC_TIMER_8_BIT // 0-255 duty
#define LEDC_FREQUENCY  5000             // 5 KHz
#define LED_ACTIVE_LOW  0   // ছিল 0, এখন 1 করো
// LED control functions
void led_init(void);
void led_set_brightness(uint8_t brightness); // 0-255
void led_on(void);
void led_off(void);
void led_blink_start(void);
void led_blink_stop(void);