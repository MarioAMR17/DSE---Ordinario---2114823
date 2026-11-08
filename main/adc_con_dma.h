#ifndef ADC_CON_DMA_H
#define ADC_CON_DMA_H

#include <stdint.h>

void ADC_ConDMA_Init(void);
void adc_con_dma_iniciar(void);
uint32_t adc_con_dma_leer_bloque(int *valores, uint32_t max_valores, uint32_t *errores);
uint32_t adc_con_dma_desbordamientos(void);
void ADC_ConDMA_Deinit(void);

#endif
