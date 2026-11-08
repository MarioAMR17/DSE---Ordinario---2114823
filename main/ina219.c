#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>

#include "driver/i2c_master.h"

#include "config.h"
#include "ina219.h"

#define INA219_REG_CONFIG       0x00
#define INA219_REG_SHUNT        0x01
#define INA219_REG_BUS          0x02
#define INA219_REG_CORRIENTE    0x04
#define INA219_REG_CALIBRACION  0x05

/* Configuracion 0x0FFF:
   BRNG = 0     rango de bus de 16 V
   PG   = 01    ganancia /2, shunt de +-80 mV (hasta 0.8 A con 0.1 ohm)
   BADC = 1111  bus a 12 bits con promedio de 128 conversiones (68.1 ms)
   SADC = 1111  shunt a 12 bits con promedio de 128 conversiones (68.1 ms)
   MODE = 111   shunt y bus en modo continuo */
#define INA219_CONFIG           0x0FFF

/* Shunt del modulo (R100) = 0.1 ohm y Corriente_LSB = 0.1 mA
   CAL = 0.04096 / (0.0001 A x 0.1 ohm) = 4096 */
#define INA219_CALIBRACION      4096
#define CORRIENTE_LSB_MA        0.1f
#define SHUNT_LSB_MV            0.01f
#define BUS_LSB_MV              4.0f

static i2c_master_dev_handle_t ina_handle;

/* Prototipos de funciones internas */
static uint16_t leer_registro(uint8_t reg);
static void escribir_registro(uint8_t reg, uint16_t valor);

/* Registrar el INA219, escribir configuracion y calibracion y comprobar que responde */
bool INA219_Init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dispositivo = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = INA219_DIRECCION,
        .scl_speed_hz = 400000,
    };
    i2c_master_bus_add_device(bus, &dispositivo, &ina_handle);

    escribir_registro(INA219_REG_CONFIG, INA219_CONFIG);
    escribir_registro(INA219_REG_CALIBRACION, INA219_CALIBRACION);

    if (leer_registro(INA219_REG_CONFIG) == INA219_CONFIG) {
        printf("INA219 detectado en 0x%02X (CAL = %d)\n", INA219_DIRECCION, INA219_CALIBRACION);
        return true;
    }

    printf("INA219 no responde en 0x%02X\n", INA219_DIRECCION);
    return false;
}

/* Leer voltaje, corriente y potencia. Devuelve false si la lectura no es valida */
bool ina219_leer(ina219_lectura_t *lectura)
{
    /* Si la configuracion no coincide, el sensor no esta respondiendo */
    if (leer_registro(INA219_REG_CONFIG) != INA219_CONFIG) {
        return false;
    }

    uint16_t bus = leer_registro(INA219_REG_BUS);

    /* Bit 0 del registro de bus (OVF): el calculo de corriente se desbordo */
    if (bus % 2 == 1) {
        return false;
    }

    int16_t shunt = (int16_t)leer_registro(INA219_REG_SHUNT);
    int16_t corriente = (int16_t)leer_registro(INA219_REG_CORRIENTE);

    /* El voltaje ocupa los bits 15 a 3 del registro de bus */
    lectura->voltaje_v = (bus / 8) * BUS_LSB_MV / 1000.0f;
    lectura->shunt_mv = shunt * SHUNT_LSB_MV;
    lectura->corriente_ma = corriente * CORRIENTE_LSB_MA;
    lectura->potencia_mw = lectura->voltaje_v * lectura->corriente_ma;

    return true;
}

/* Leer un registro de 16 bits (primero el byte alto) */
static uint16_t leer_registro(uint8_t reg)
{
    uint8_t datos[2] = {0, 0};
    i2c_master_transmit_receive(ina_handle, &reg, 1, datos, 2, 100);
    return (uint16_t)(datos[0] * 256 + datos[1]);
}

/* Escribir un registro de 16 bits (primero el byte alto) */
static void escribir_registro(uint8_t reg, uint16_t valor)
{
    uint8_t datos[3] = {reg, (uint8_t)(valor / 256), (uint8_t)(valor % 256)};
    i2c_master_transmit(ina_handle, datos, sizeof(datos), 100);
}
