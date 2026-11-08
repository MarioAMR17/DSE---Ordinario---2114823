#include <stdint.h>
#include <stdbool.h>   
#include <stddef.h>

#include "esp_adc/adc_continuous.h"
#include "esp_attr.h"

#include "config.h"
#include "adc_con_dma.h"

/* Cada trama de DMA guarda 200 conversiones (10 ms a 20 kHz, 2 bytes cada una) */
#define TAMANO_TRAMA_BYTES   400
/* Memoria interna del driver: unos 200 ms de muestras (el CPU la vacia cada 100 ms) */
#define TAMANO_POOL_BYTES    8000

static adc_continuous_handle_t dma_handle = NULL;
static adc_continuous_data_t muestras_dma[MAX_MUESTRAS_CICLO * FACTOR_DMA];
static volatile uint32_t desbordamientos = 0;
static uint32_t contador_decimacion = 0;

/* Prototipos de funciones internas */
static bool dma_desborde_isr(adc_continuous_handle_t handle,
                                       const adc_continuous_evt_data_t *edata,
                                       void *user_data);

/* ADC1 canal 6 en modo continuo a 20 kHz con transferencia por DMA */
void ADC_ConDMA_Init(void)
{
    adc_continuous_handle_cfg_t memoria = {
        .max_store_buf_size = TAMANO_POOL_BYTES,
        .conv_frame_size = TAMANO_TRAMA_BYTES,
    };
    adc_continuous_new_handle(&memoria, &dma_handle);

    adc_digi_pattern_config_t patron = {
        .atten = ADC_ATTEN_DB_12,
        .channel = POT_CANAL,
        .unit = ADC_UNIT_1,
        .bit_width = ADC_BITWIDTH_12,
    };

    adc_continuous_config_t config = {
        .pattern_num = 1,
        .adc_pattern = &patron,
        .sample_freq_hz = FRECUENCIA_DMA_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE1,
    };
    adc_continuous_config(dma_handle, &config);

    adc_continuous_evt_cbs_t eventos = {
        .on_pool_ovf = dma_desborde_isr,
    };
    adc_continuous_register_event_callbacks(dma_handle, &eventos, NULL);

    desbordamientos = 0;
    contador_decimacion = 0;
}

/* Arrancar las conversiones */
void adc_con_dma_iniciar(void)
{
    adc_continuous_start(dma_handle);
}

/* Copiar las muestras que ya entrego el DMA, sin esperar (timeout 0).
   Se conserva 1 de cada FACTOR_DMA muestras para procesar 10 kHz */
uint32_t adc_con_dma_leer_bloque(int *valores, uint32_t max_valores, uint32_t *errores)
{
    uint32_t recibidas = 0;
    adc_continuous_read_parse(dma_handle, muestras_dma, max_valores * FACTOR_DMA, &recibidas, 0);

    uint32_t n = 0;
    for (uint32_t i = 0; i < recibidas; i++) {
        if (muestras_dma[i].valid && muestras_dma[i].channel == POT_CANAL) {
            if (contador_decimacion % FACTOR_DMA == 0) {
                valores[n] = (int)muestras_dma[i].raw_data;
                n++;
            }
            contador_decimacion++;
        } else {
            (*errores)++;
        }
    }
    return n;
}

/* Veces que se lleno la memoria del driver y se descartaron muestras */
uint32_t adc_con_dma_desbordamientos(void)
{
    return desbordamientos;
}

/* Detener las conversiones y liberar el driver */
void ADC_ConDMA_Deinit(void)
{
    adc_continuous_stop(dma_handle);
    adc_continuous_deinit(dma_handle);
}

/* ISR breve: solo cuenta el desbordamiento */
static bool IRAM_ATTR dma_desborde_isr(adc_continuous_handle_t handle,
                                       const adc_continuous_evt_data_t *edata,
                                       void *user_data)
{
    desbordamientos++;
    return false;
}
