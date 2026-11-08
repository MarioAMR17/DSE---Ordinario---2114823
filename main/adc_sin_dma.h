#ifndef ADC_SIN_DMA_H
#define ADC_SIN_DMA_H

#include <stdint.h>
#include <stdbool.h>

void ADC_SinDMA_Init(void);
void adc_sin_dma_iniciar(void);
bool adc_sin_dma_leer(int *valor);
uint32_t adc_sin_dma_leer_bloque(int *valores, uint32_t max_valores, uint32_t *errores);
void ADC_SinDMA_Deinit(void);

#endif
