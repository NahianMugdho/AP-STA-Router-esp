# idf_component_register(SRCS "factory_reset.c" "wifi_manager.c" "app_main.c" "led_config.c"
#        "wifi_manager.c"      
#         "factory_reset.c"     
#                        PRIV_REQUIRES spi_flash nvs_flash
#                        INCLUDE_DIRS ""
#                        REQUIRES driver esp_wifi esp_netif freertos lwip esp_event log mqtt  esp_http_server  
#                        )

idf_component_register(
    SRCS 
        "factory_reset.c" 
        "wifi_manager.c" 
        "app_main.c" 
        "led_config.c"
       
       
       
       
       
    
    PRIV_REQUIRES 
        spi_flash 
        nvs_flash
    
    INCLUDE_DIRS 
        ""
    
    REQUIRES 
        driver 
        esp_wifi 
        esp_netif 
        freertos 
        lwip 
        esp_event 
        log 
        mqtt 
        esp_http_server
        esp_timer
        esp_adc        # Changed from esp_adc_cal (for ESP-IDF v5.x)
        dht            # DHT sensor library from esp-idf-lib
        ds18x20        # DS18B20 library from esp-idf-lib
)