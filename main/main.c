/* Evaluacion del consumo energetico del ESP32 con adquisicion ADC sin DMA (One-shot)
   y con DMA (Continuous). Un INA219 por I2C mide voltaje, corriente y potencia de la
   alimentacion; los resultados de cada prueba se envian por la consola serie. */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"
#include "esp_timer.h"
#include "esp_pm.h"
#include "config.h"
#include "ina219.h"
#include "adc_sin_dma.h"
#include "adc_con_dma.h"
#include "resultados.h"

/* Fases de la evaluacion */
typedef enum {
    FASE_ESTABILIZACION,
    FASE_SIN_DMA,
    FASE_PAUSA,
    FASE_CON_DMA,
    FASE_FIN
} fase_t;

/* Variables del programa */
static i2c_master_bus_handle_t bus_i2c;
static fase_t fase;
static resultados_t sin_dma;
static resultados_t con_dma;
static ina219_lectura_t lectura_ina;
static int valores[MAX_MUESTRAS_CICLO];
static uint32_t inicio_fase = 0;
static uint32_t ultima_ina = 0;
static uint32_t ultimo_resumen = 0;
static bool ina_valida = false;
static bool adc_valido = false;
static int muestra_adc = 0;

/* Prototipos de funciones */
static void PM_Init(void);
static void I2C_Init(void);
static uint32_t obtener_tiempo_ms(void);
static int64_t obtener_tiempo_us(void);

/* Programa principal */
void app_main(void)
{
    /* 1. Activar la gestion de energia: 240 MHz con trabajo, 80 MHz en reposo */
    PM_Init();
    /* 2. Crear el bus I2C del INA219 (SDA = GPIO 21, SCL = GPIO 22) */
    I2C_Init();
    /* 3. Configurar y calibrar el INA219 (0x40, shunt 0.1 ohm, Corriente_LSB 0.1 mA) */
    INA219_Init(bus_i2c);
    /* 4. Configurar ADC1 canal 6 (GPIO 34) en modo One-shot */
    ADC_SinDMA_Init();
    /* 5. Iniciar en la fase de estabilizacion */
    fase = FASE_ESTABILIZACION;
    inicio_fase = obtener_tiempo_ms();
    ultima_ina = inicio_fase;

    while (1)
    {
        /* 6. Obtener el tiempo del sistema */
        uint32_t tiempo_sistema = obtener_tiempo_ms();
        uint32_t muestras = 0;
        int64_t inicio_bloque_us = 0;

        /* 7. Ejecutar la fase actual */
        switch (fase) {
        case FASE_ESTABILIZACION:
            /* Cada 1 s leer el INA219 y una muestra del ADC para revisar que sean plausibles */
            if (tiempo_sistema - ultima_ina >= PERIODO_INA219_MS) {
                ultima_ina = tiempo_sistema;
                ina_valida = ina219_leer(&lectura_ina);
                adc_valido = adc_sin_dma_leer(&muestra_adc);

                if (ina_valida) {
                    printf("[ESTABILIZACION] t = %lu s | V = %.3f V | I = %.2f mA | P = %.2f mW | ADC = %d\n",
                           (unsigned long)((tiempo_sistema - inicio_fase) / 1000),
                           (double)lectura_ina.voltaje_v, (double)lectura_ina.corriente_ma,
                           (double)lectura_ina.potencia_mw, muestra_adc);
                } else {
                    printf("[ESTABILIZACION] Lectura invalida del INA219 | ADC = %d\n", muestra_adc);
                }
            }

            /* Al cumplir 10 s, iniciar la prueba sin DMA si las lecturas son plausibles */
            if (tiempo_sistema - inicio_fase >= TIEMPO_ESTABILIZACION_MS) {
                if (ina_valida && adc_valido &&
                    lectura_ina.voltaje_v >= VOLTAJE_MIN_V && lectura_ina.voltaje_v <= VOLTAJE_MAX_V &&
                    lectura_ina.corriente_ma >= CORRIENTE_MIN_MA &&
                    lectura_ina.corriente_ma <= CORRIENTE_MAX_MA) {
                    printf("\nLecturas plausibles. Inicia la prueba SIN DMA (%d s)\n\n",
                           DURACION_PRUEBA_MS / 1000);
                    resultados_reiniciar(&sin_dma, METODO_SIN_DMA);
                    fase = FASE_SIN_DMA;
                    inicio_fase = tiempo_sistema;
                    ultima_ina = tiempo_sistema;
                    ultimo_resumen = tiempo_sistema;
                    adc_sin_dma_iniciar();
                } else {
                    printf("Lecturas fuera de rango, se repite la estabilizacion\n");
                    inicio_fase = tiempo_sistema;
                }
            }
            break;

        case FASE_SIN_DMA:
            /* El CPU pide cada conversion durante el bloque y luego se procesan las muestras */
            inicio_bloque_us = obtener_tiempo_us();
            muestras = adc_sin_dma_leer_bloque(valores, MAX_MUESTRAS_CICLO, &sin_dma.errores_adc);
            resultados_procesar_bloque(&sin_dma, valores, muestras,
                                       obtener_tiempo_us() - inicio_bloque_us);
            break;

        case FASE_PAUSA:
            /* Reinicio controlado: liberar el ADC One-shot y preparar el ADC continuo.
               Este tiempo no se cuenta en ninguna de las dos pruebas */
            ADC_SinDMA_Deinit();
            ADC_ConDMA_Init();
            resultados_reiniciar(&con_dma, METODO_CON_DMA);

            /* Revisar que la alimentacion siga siendo la misma antes de la segunda prueba */
            if (ina219_leer(&lectura_ina)) {
                printf("Alimentacion antes de la prueba CON DMA: V = %.3f V | I = %.2f mA\n",
                       (double)lectura_ina.voltaje_v, (double)lectura_ina.corriente_ma);
            } else {
                printf("Lectura invalida del INA219 antes de la prueba CON DMA\n");
            }

            printf("\nInicia la prueba CON DMA (%d s)\n\n", DURACION_PRUEBA_MS / 1000);
            tiempo_sistema = obtener_tiempo_ms();
            fase = FASE_CON_DMA;
            inicio_fase = tiempo_sistema;
            ultima_ina = tiempo_sistema;
            ultimo_resumen = tiempo_sistema;
            adc_con_dma_iniciar();
            break;

        case FASE_CON_DMA:
            /* Procesar las muestras que el DMA guardo mientras el CPU descansaba */
            inicio_bloque_us = obtener_tiempo_us();
            muestras = adc_con_dma_leer_bloque(valores, MAX_MUESTRAS_CICLO, &con_dma.errores_adc);
            resultados_procesar_bloque(&con_dma, valores, muestras,
                                       obtener_tiempo_us() - inicio_bloque_us);
            break;

        case FASE_FIN:
            /* La evaluacion termino; solo se espera */
            break;

        default:
            break;
        }

        if (fase == FASE_SIN_DMA || fase == FASE_CON_DMA) {
            /* Actualizar el tiempo, porque el bloque sin DMA ocupa 99 ms */
            tiempo_sistema = obtener_tiempo_ms();

            /* 8. Leer el INA219 cada 1 s y acumular la energia con el tiempo real entre lecturas.
                  Si la lectura no es valida se cuenta el error y se reintenta en el siguiente bloque;
                  el intervalo se integra con la siguiente lectura valida */
            if (tiempo_sistema - ultima_ina >= PERIODO_INA219_MS) {
                float dt_s = (tiempo_sistema - ultima_ina) / 1000.0f;

                if (ina219_leer(&lectura_ina)) {
                    if (fase == FASE_SIN_DMA) {
                        resultados_registrar_ina(&sin_dma, &lectura_ina, dt_s);
                    } else {
                        resultados_registrar_ina(&con_dma, &lectura_ina, dt_s);
                    }
                    ultima_ina = tiempo_sistema;
                } else {
                    if (fase == FASE_SIN_DMA) {
                        sin_dma.errores_ina++;
                    } else {
                        con_dma.errores_ina++;
                    }
                }
            }

            /* 9. Imprimir el estado de la prueba cada 5 s */
            if (tiempo_sistema - ultimo_resumen >= PERIODO_RESUMEN_MS) {
                ultimo_resumen = tiempo_sistema;
                if (fase == FASE_SIN_DMA) {
                    resultados_imprimir_estado(&sin_dma, tiempo_sistema - inicio_fase, &lectura_ina);
                } else {
                    resultados_imprimir_estado(&con_dma, tiempo_sistema - inicio_fase, &lectura_ina);
                }
            }

            /* 10. Al cumplir 120 s cerrar la prueba. Antes se hace una ultima lectura del INA219
                   para integrar el tramo que falta desde la lectura anterior */
            if (tiempo_sistema - inicio_fase >= DURACION_PRUEBA_MS) {
                uint32_t duracion_ms = tiempo_sistema - inicio_fase;
                float dt_s = (tiempo_sistema - ultima_ina) / 1000.0f;

                if (fase == FASE_SIN_DMA) {
                    if (dt_s > 0.0f && ina219_leer(&lectura_ina)) {
                        resultados_registrar_ina(&sin_dma, &lectura_ina, dt_s);
                    }
                                        /* Procesar las muestras que el DMA guardo desde el ultimo bloque */
                    inicio_bloque_us = obtener_tiempo_us();
                    muestras = adc_con_dma_leer_bloque(valores, MAX_MUESTRAS_CICLO, &con_dma.errores_adc);
                    resultados_procesar_bloque(&con_dma, valores, muestras,
                                               obtener_tiempo_us() - inicio_bloque_us);
                    resultados_cerrar(&sin_dma, duracion_ms, 0);
                    resultados_imprimir_resumen(&sin_dma);
                    fase = FASE_PAUSA;
                } else {
                    if (dt_s > 0.0f && ina219_leer(&lectura_ina)) {
                        resultados_registrar_ina(&con_dma, &lectura_ina, dt_s);
                    }
                    resultados_cerrar(&con_dma, duracion_ms, adc_con_dma_desbordamientos());
                    ADC_ConDMA_Deinit();
                    resultados_imprimir_resumen(&con_dma);
                    resultados_imprimir_comparacion(&sin_dma, &con_dma);
                    printf("Evaluacion terminada. Presione EN para repetir el par de pruebas.\n");
                    fase = FASE_FIN;
                }
            }
        }

        /* 11. Descansar: con DMA el CPU espera el bloque completo mientras el DMA adquiere;
               sin DMA solo descansa 1 ms; al terminar, se revisa cada segundo */
        if (fase == FASE_CON_DMA) {
            vTaskDelay(pdMS_TO_TICKS(PERIODO_BLOQUE_MS));
        } else if (fase == FASE_FIN) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        } else {
            vTaskDelay(pdMS_TO_TICKS(DESCANSO_SIN_DMA_MS));
        }
    }
}

/* Escalado dinamico de frecuencia igual para las dos pruebas (sin light sleep,
   porque el ADC continuo no puede trabajar con el chip dormido) */
static void PM_Init(void)
{
    esp_pm_config_t energia = {
        .max_freq_mhz = CPU_FREQ_MAX_MHZ,
        .min_freq_mhz = CPU_FREQ_MIN_MHZ,
        .light_sleep_enable = false,
    };
    esp_pm_configure(&energia);
}

/* Bus I2C maestro con pull-ups internos (el modulo INA219 tambien trae pull-ups) */
static void I2C_Init(void)
{
    i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = I2C_SDA_GPIO,
        .scl_io_num = I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    i2c_new_master_bus(&bus_config, &bus_i2c);
}

/* Tiempo desde el arranque en milisegundos */
static uint32_t obtener_tiempo_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/* Tiempo desde el arranque en microsegundos (para medir el tiempo de CPU por bloque) */
static int64_t obtener_tiempo_us(void)
{
    return esp_timer_get_time();
}