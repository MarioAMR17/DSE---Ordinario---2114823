idf_component_register(
    SRCS "main.c"
         "ina219.c"
         "adc_sin_dma.c"
         "adc_con_dma.c"
         "resultados.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_driver_i2c esp_adc esp_timer esp_pm
)
