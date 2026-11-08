#include <stdint.h>
#include <stdbool.h>

#include "esp_adc/adc_oneshot.h"
#include "esp_timer.h"

#include "config.h"
#include "adc_sin_dma.h"

static adc_oneshot_unit_handle_t adc_handle;
static int64_t proxima_muestra_us = 0;

/* ADC1 de 12 bits con atenuacion de 12 dB (0 a 3.3 V aprox.) */
void ADC_SinDMA_Init(void)
{
    adc_oneshot_unit_init_cfg_t unidad = {
        .unit_id = ADC_UNIT_1,
    };
    adc_oneshot_new_unit(&unidad, &adc_handle);

    adc_oneshot_chan_cfg_t canal = {
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_12,
    };
    adc_oneshot_config_channel(adc_handle, POT_CANAL, &canal);
}

/* Marcar el instante de la primera conversion de la prueba */
void adc_sin_dma_iniciar(void)
{
    proxima_muestra_us = esp_timer_get_time();
}

/* Una conversion por software. Devuelve false si no hubo lectura valida */
bool adc_sin_dma_leer(int *valor)
{
    int lectura = -1;
    adc_oneshot_read(adc_handle, POT_CANAL, &lectura);
    *valor = lectura;

    if (lectura < 0 || lectura > ADC_MAXIMO) {
        return false;
    }
    return true;
}

/* El CPU solicita una conversion cada 100 us (10 kHz) durante el bloque.
   Entre conversiones revisa el tiempo hasta que toque la siguiente, por lo que
   permanece ocupado todo el bloque. Si va atrasado, hace las pendientes seguidas */
uint32_t adc_sin_dma_leer_bloque(int *valores, uint32_t max_valores, uint32_t *errores)
{
    int64_t inicio_bloque = esp_timer_get_time();
    int64_t duracion_us = (PERIODO_BLOQUE_MS - DESCANSO_SIN_DMA_MS) * 1000;
    uint32_t n = 0;

    while (esp_timer_get_time() - inicio_bloque < duracion_us && n < max_valores) {
        if (esp_timer_get_time() >= proxima_muestra_us) {
            proxima_muestra_us += PERIODO_MUESTRA_US;

            int valor;
            if (adc_sin_dma_leer(&valor)) {
                valores[n] = valor;
                n++;
            } else {
                (*errores)++;
            }
        }
    }
    return n;
}

/* Liberar la unidad ADC1 para que la pueda usar el modo continuo */
void ADC_SinDMA_Deinit(void)
{
    adc_oneshot_del_unit(adc_handle);
}
