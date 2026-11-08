#include <stdio.h>
#include <stdint.h>

#include "esp_timer.h"

#include "config.h"
#include "resultados.h"

/* Prototipos de funciones internas */
static const char *nombre_metodo(metodo_t metodo);
static float promedio(float suma, uint32_t n);
static float voltaje_adc_mv(const resultados_t *r);
static float ms_por_bloque(uint64_t tiempo_us, uint32_t bloques);

/* Poner en cero contadores y acumuladores antes de una prueba */
void resultados_reiniciar(resultados_t *r, metodo_t metodo)
{
    r->metodo = metodo;
    r->duracion_ms = 0;
    r->muestras_validas = 0;
    r->muestras_perdidas = 0;
    r->errores_adc = 0;
    r->desbordamientos = 0;
    r->suma_adc = 0;
    r->minimo_adc = ADC_MAXIMO;
    r->maximo_adc = 0;
    r->bloques = 0;
    r->tiempo_adquisicion_us = 0;
    r->tiempo_procesamiento_us = 0;
    r->lecturas_ina = 0;
    r->errores_ina = 0;
    r->suma_voltaje_v = 0.0f;
    r->suma_corriente_ma = 0.0f;
    r->suma_potencia_mw = 0.0f;
    r->corriente_min_ma = 100000.0f;
    r->corriente_max_ma = 0.0f;
    r->energia_j = 0.0f;
}

/* Procesamiento identico para los dos metodos: suma, minimo y maximo de cada muestra */
void resultados_procesar_bloque(resultados_t *r, const int *valores, uint32_t n,
                                int64_t tiempo_adquisicion_us)
{
    int64_t inicio = esp_timer_get_time();

    for (uint32_t i = 0; i < n; i++) {
        r->suma_adc += valores[i];
        if (valores[i] < r->minimo_adc) {
            r->minimo_adc = valores[i];
        }
        if (valores[i] > r->maximo_adc) {
            r->maximo_adc = valores[i];
        }
    }
    r->muestras_validas += n;

    r->tiempo_procesamiento_us += esp_timer_get_time() - inicio;
    r->tiempo_adquisicion_us += tiempo_adquisicion_us;
    r->bloques++;
}

/* Acumular una lectura del INA219. Energia: E = E + P x dt (P en W, dt en s) */
void resultados_registrar_ina(resultados_t *r, const ina219_lectura_t *lectura, float dt_s)
{
    r->suma_voltaje_v += lectura->voltaje_v;
    r->suma_corriente_ma += lectura->corriente_ma;
    r->suma_potencia_mw += lectura->potencia_mw;
    r->energia_j += (lectura->potencia_mw / 1000.0f) * dt_s;
    r->lecturas_ina++;

    /* Minimo y maximo para ver que la corriente se mantiene estable en la prueba */
    if (lectura->corriente_ma < r->corriente_min_ma) {
        r->corriente_min_ma = lectura->corriente_ma;
    }
    if (lectura->corriente_ma > r->corriente_max_ma) {
        r->corriente_max_ma = lectura->corriente_ma;
    }
}

/* Cerrar la prueba: las perdidas son las muestras esperadas a 10 kHz que no se procesaron */
void resultados_cerrar(resultados_t *r, uint32_t duracion_ms, uint32_t desbordamientos)
{
    uint32_t esperadas = (uint32_t)((uint64_t)duracion_ms * FRECUENCIA_ADC_HZ / 1000);

    r->duracion_ms = duracion_ms;
    r->desbordamientos = desbordamientos;

    if (esperadas > r->muestras_validas) {
        r->muestras_perdidas = esperadas - r->muestras_validas;
    } else {
        r->muestras_perdidas = 0;
    }
}

/* Linea de estado (cada 5 s) */
void resultados_imprimir_estado(const resultados_t *r, uint32_t tiempo_ms,
                                const ina219_lectura_t *lectura)
{
    printf("[%s] t = %5.1f s | V = %.3f V | I = %6.2f mA | P = %7.2f mW | E = %7.3f J | "
           "muestras = %lu | err ADC = %lu | err INA = %lu\n",
           nombre_metodo(r->metodo), tiempo_ms / 1000.0f,
           (double)lectura->voltaje_v, (double)lectura->corriente_ma,
           (double)lectura->potencia_mw, (double)r->energia_j,
           (unsigned long)r->muestras_validas, (unsigned long)r->errores_adc,
           (unsigned long)r->errores_ina);
}

/* Resumen al terminar una prueba */
void resultados_imprimir_resumen(const resultados_t *r)
{
    float duracion_s = r->duracion_ms / 1000.0f;

    printf("\n===== Resumen %s =====\n", nombre_metodo(r->metodo));
    printf("Duracion efectiva      : %.1f s\n", (double)duracion_s);
    printf("Frecuencia efectiva    : %.1f Hz\n", (double)(r->muestras_validas / duracion_s));
    printf("Muestras validas       : %lu\n", (unsigned long)r->muestras_validas);
    printf("Muestras perdidas      : %lu\n", (unsigned long)r->muestras_perdidas);
    printf("Errores ADC            : %lu\n", (unsigned long)r->errores_adc);
    printf("Desbordamientos DMA    : %lu\n", (unsigned long)r->desbordamientos);
    printf("ADC promedio           : %.1f mV (crudo min = %d, max = %d)\n",
           (double)voltaje_adc_mv(r), r->minimo_adc, r->maximo_adc);
    printf("Adquisicion por bloque : %.3f ms\n",
           (double)ms_por_bloque(r->tiempo_adquisicion_us, r->bloques));
    printf("Procesamiento por bloque: %.3f ms\n",
           (double)ms_por_bloque(r->tiempo_procesamiento_us, r->bloques));
    printf("Lecturas INA219        : %lu (errores: %lu)\n",
           (unsigned long)r->lecturas_ina, (unsigned long)r->errores_ina);
    printf("Voltaje promedio       : %.3f V\n",
           (double)promedio(r->suma_voltaje_v, r->lecturas_ina));
    printf("Corriente promedio     : %.2f mA\n",
           (double)promedio(r->suma_corriente_ma, r->lecturas_ina));
    printf("Corriente min / max    : %.2f / %.2f mA\n",
           (double)r->corriente_min_ma, (double)r->corriente_max_ma);
    printf("Potencia promedio      : %.2f mW\n",
           (double)promedio(r->suma_potencia_mw, r->lecturas_ina));
    printf("Energia acumulada      : %.3f J\n\n", (double)r->energia_j);
}

/* Tabla comparativa final. Referencia del porcentaje: la prueba sin DMA */
void resultados_imprimir_comparacion(const resultados_t *sin_dma, const resultados_t *con_dma)
{
    float reduccion = 0.0f;
    if (sin_dma->energia_j > 0.0f) {
        reduccion = (sin_dma->energia_j - con_dma->energia_j) / sin_dma->energia_j * 100.0f;
    }

    printf("\n================ COMPARACION FINAL ================\n");
    printf("%-28s %12s %12s\n", "Metrica", "Sin DMA", "Con DMA");
    printf("%-28s %12.1f %12.1f\n", "Duracion efectiva (s)",
           (double)(sin_dma->duracion_ms / 1000.0f), (double)(con_dma->duracion_ms / 1000.0f));
    printf("%-28s %12.3f %12.3f\n", "Voltaje promedio (V)",
           (double)promedio(sin_dma->suma_voltaje_v, sin_dma->lecturas_ina),
           (double)promedio(con_dma->suma_voltaje_v, con_dma->lecturas_ina));
    printf("%-28s %12.2f %12.2f\n", "Corriente promedio (mA)",
           (double)promedio(sin_dma->suma_corriente_ma, sin_dma->lecturas_ina),
           (double)promedio(con_dma->suma_corriente_ma, con_dma->lecturas_ina));
    printf("%-28s %12.2f %12.2f\n", "Corriente minima (mA)",
           (double)sin_dma->corriente_min_ma, (double)con_dma->corriente_min_ma);
    printf("%-28s %12.2f %12.2f\n", "Corriente maxima (mA)",
           (double)sin_dma->corriente_max_ma, (double)con_dma->corriente_max_ma);
    printf("%-28s %12.2f %12.2f\n", "Potencia promedio (mW)",
           (double)promedio(sin_dma->suma_potencia_mw, sin_dma->lecturas_ina),
           (double)promedio(con_dma->suma_potencia_mw, con_dma->lecturas_ina));
    printf("%-28s %12.3f %12.3f\n", "Energia acumulada (J)",
           (double)sin_dma->energia_j, (double)con_dma->energia_j);
    printf("%-28s %12lu %12lu\n", "Muestras validas",
           (unsigned long)sin_dma->muestras_validas, (unsigned long)con_dma->muestras_validas);
    printf("%-28s %12lu %12lu\n", "Muestras perdidas",
           (unsigned long)sin_dma->muestras_perdidas, (unsigned long)con_dma->muestras_perdidas);
    printf("%-28s %12lu %12lu\n", "Errores ADC",
           (unsigned long)sin_dma->errores_adc, (unsigned long)con_dma->errores_adc);
    printf("%-28s %12s %12lu\n", "Desbordamientos DMA", "N/A",
           (unsigned long)con_dma->desbordamientos);
    printf("%-28s %12.3f %12.3f\n", "Adquisicion por bloque (ms)",
           (double)ms_por_bloque(sin_dma->tiempo_adquisicion_us, sin_dma->bloques),
           (double)ms_por_bloque(con_dma->tiempo_adquisicion_us, con_dma->bloques));
    printf("%-28s %12.3f %12.3f\n", "Procesamiento/bloque (ms)",
           (double)ms_por_bloque(sin_dma->tiempo_procesamiento_us, sin_dma->bloques),
           (double)ms_por_bloque(con_dma->tiempo_procesamiento_us, con_dma->bloques));
    printf("Reduccion de energia respecto a la prueba sin DMA: %.2f %%\n", (double)reduccion);
    printf("(un valor negativo indica que la prueba con DMA consumio mas energia)\n");
    printf("===================================================\n\n");
}

static const char *nombre_metodo(metodo_t metodo)
{
    if (metodo == METODO_CON_DMA) {
        return "CON DMA";
    }
    return "SIN DMA";
}

static float promedio(float suma, uint32_t n)
{
    if (n == 0) {
        return 0.0f;
    }
    return suma / n;
}

/* Conversion lineal aproximada (sin calibracion) del promedio del ADC a mV */
static float voltaje_adc_mv(const resultados_t *r)
{
    if (r->muestras_validas == 0) {
        return 0.0f;
    }
    float promedio_crudo = (float)r->suma_adc / r->muestras_validas;
    return promedio_crudo * 3300.0f / ADC_MAXIMO;
}

static float ms_por_bloque(uint64_t tiempo_us, uint32_t bloques)
{
    if (bloques == 0) {
        return 0.0f;
    }
    return (float)tiempo_us / bloques / 1000.0f;
}
