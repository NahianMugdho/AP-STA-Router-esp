// led_config.c
#include "led_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TAG "LED_CONFIG"

static bool blinking = false;
static TaskHandle_t blink_task_handle = NULL;

// LED brightness set করার function
void led_set_brightness(uint8_t brightness) {
    ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, brightness);
    ledc_update_duty(LEDC_MODE, LEDC_CHANNEL);
}

// LED fully ON করার function
void led_on(void) {
    led_set_brightness(255);
}

// LED fully OFF করার function
void led_off(void) {
    led_set_brightness(0);
}

// Blink task যা background এ run করবে
static void blink_task(void *param) {
    while (blinking) {
        led_set_brightness(128); // Half brightness for blink
        vTaskDelay(pdMS_TO_TICKS(300));
        led_off();
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    vTaskDelete(NULL);
}

// Blinking শুরু করার function
void led_blink_start(void) {
    if (!blinking) {
        blinking = true;
        xTaskCreate(blink_task, "LED_Blink", 2048, NULL, 1, &blink_task_handle);
        ESP_LOGI(TAG, "LED Blinking started");
    }
}

// Blinking বন্ধ করার function
void led_blink_stop(void) {
    blinking = false;
    if (blink_task_handle != NULL) {
        vTaskDelay(pdMS_TO_TICKS(100)); // Task শেষ হওয়ার জন্য একটু wait
        blink_task_handle = NULL;
    }
    ESP_LOGI(TAG, "LED Blinking stopped");
}

// LED initialize করার function
void led_init(void) {
    // Timer config
    ledc_timer_config_t ledc_timer = {
        .speed_mode       = LEDC_MODE,
        .timer_num        = LEDC_TIMER,
        .duty_resolution  = LEDC_DUTY_RES,
        .freq_hz          = LEDC_FREQUENCY,
        .clk_cfg          = LEDC_AUTO_CLK
    };
    ledc_timer_config(&ledc_timer);
    
    // Channel config
    ledc_channel_config_t ledc_channel = {
        .gpio_num       = LED_GPIO,
        .speed_mode     = LEDC_MODE,
        .channel        = LEDC_CHANNEL,
        .intr_type      = LEDC_INTR_DISABLE,
        .timer_sel      = LEDC_TIMER,
        .duty           = 0, // শুরুতে OFF
        .hpoint         = 0
    };
    ledc_channel_config(&ledc_channel);
    
    ESP_LOGI(TAG, "LED initialized on GPIO %d", LED_GPIO);
}