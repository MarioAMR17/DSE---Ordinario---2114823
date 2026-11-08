#ifndef RESULTADOS_H
#define RESULTADOS_H

#include <stdint.h>
#include "ina219.h"

typedef enum {
    METODO_SIN_DMA,
    METODO_CON_DMA
} metodo_t;

typedef struct {
    metodo_t metodo;
    uint32_t duracion_ms;

    /* Adquisicion ADC */
    uint32_t muestras_validas;
    uint32_t muestras_perdidas;
    uint32_t errores_adc;
    uint32_t desbordamientos;
    uint64_t suma_adc;
    int minimo_adc;
    int maximo_adc;

    /* Tiempos de CPU */
    uint32_t bloques;
    uint64_t tiempo_adquisicion_us;
    uint64_t tiempo_procesamiento_us;

    /* INA219 */
    uint32_t lecturas_ina;
    uint32_t errores_ina;
    float suma_voltaje_v;
    float suma_corriente_ma;
    float suma_potencia_mw;
    float corriente_min_ma;
    float corriente_max_ma;
    float energia_j;
} resultados_t;

void resultados_reiniciar(resultados_t *r, metodo_t metodo);
void resultados_procesar_bloque(resultados_t *r, const int *valores, uint32_t n,
                                int64_t tiempo_adquisicion_us);
void resultados_registrar_ina(resultados_t *r, const ina219_lectura_t *lectura, float dt_s);
void resultados_cerrar(resultados_t *r, uint32_t duracion_ms, uint32_t desbordamientos);
void resultados_imprimir_estado(const resultados_t *r, uint32_t tiempo_ms,
                                const ina219_lectura_t *lectura);
void resultados_imprimir_resumen(const resultados_t *r);
void resultados_imprimir_comparacion(const resultados_t *sin_dma, const resultados_t *con_dma);

#endif
