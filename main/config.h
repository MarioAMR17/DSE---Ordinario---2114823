#ifndef CONFIG_H
#define CONFIG_H

#include "hal/adc_types.h"

/* Bus I2C del INA219 */
#define I2C_SDA_GPIO        21
#define I2C_SCL_GPIO        22
#define INA219_DIRECCION    0x40        /* A0 y A1 sin puentear */

/* Potenciometro en GPIO 34 = ADC1 canal 6 */
#define POT_CANAL           ADC_CHANNEL_6
#define ADC_MAXIMO          4095

/* Frecuencia efectiva procesada en las dos pruebas */
#define FRECUENCIA_ADC_HZ   10000

/* El ADC continuo del ESP32 no acepta menos de 20 kHz:
   se convierte a 20 kHz y se procesa 1 de cada 2 muestras */
#define FRECUENCIA_DMA_HZ   20000
#define FACTOR_DMA          (FRECUENCIA_DMA_HZ / FRECUENCIA_ADC_HZ)

/* Periodo entre muestras a 10 kHz (us) */
#define PERIODO_MUESTRA_US  (1000000 / FRECUENCIA_ADC_HZ)

/* Tiempos de la evaluacion (ms) */
#define TIEMPO_ESTABILIZACION_MS   10000
#define DURACION_PRUEBA_MS         120000
#define PERIODO_INA219_MS          1000
#define PERIODO_RESUMEN_MS         5000

/* Bloque de procesamiento: 100 ms (unas 1000 muestras) en los dos metodos.
   Sin DMA el CPU pide las conversiones durante 99 ms y descansa 1 ms;
   con DMA el CPU descansa 100 ms y luego procesa el bloque completo */
#define PERIODO_BLOQUE_MS   100
#define DESCANSO_SIN_DMA_MS 1

/* Maximo de muestras que se procesan en un bloque (120 ms a 10 kHz) */
#define MAX_MUESTRAS_CICLO  1200

/* Gestion de energia (igual en las dos pruebas) */
#define CPU_FREQ_MAX_MHZ    240
#define CPU_FREQ_MIN_MHZ    80

/* Limites para considerar plausible la alimentacion durante la estabilizacion */
#define VOLTAJE_MIN_V       4.5f
#define VOLTAJE_MAX_V       5.5f
#define CORRIENTE_MIN_MA    10.0f
#define CORRIENTE_MAX_MA    500.0f

#endif
