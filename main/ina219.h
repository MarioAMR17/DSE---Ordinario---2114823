#ifndef INA219_H
#define INA219_H

#include <stdbool.h>
#include "driver/i2c_master.h"

typedef struct {
    float voltaje_v;        /* voltaje de bus (lado de la carga) */
    float shunt_mv;         /* caida en la resistencia shunt */
    float corriente_ma;
    float potencia_mw;      /* P = V x I */
} ina219_lectura_t;

bool INA219_Init(i2c_master_bus_handle_t bus);
bool ina219_leer(ina219_lectura_t *lectura);

#endif
