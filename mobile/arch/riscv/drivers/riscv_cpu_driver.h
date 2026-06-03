/*
 * RISC-V CPU Driver Header
 * Mobile OS Project
 */

#ifndef _RISCV_CPU_DRIVER_H_
#define _RISCV_CPU_DRIVER_H_

#include "riscv_hal.h"

typedef struct {
    uint32_t hart_id;
    cpu_features_t features;
    uint32_t frequency_current;
    uint32_t frequency_max;
    uint32_t frequency_min;
    uint32_t voltage_current;
    uint32_t temperature;
    bool online;
} riscv_cpu_t;

int riscv_cpu_driver_init(void);
riscv_cpu_t* riscv_cpu_get_info(uint32_t hart_id);
int riscv_cpu_set_frequency(uint32_t hart_id, uint32_t freq_mhz);
int riscv_cpu_set_online(uint32_t hart_id, bool online);
uint32_t riscv_cpu_get_temperature(uint32_t hart_id);
int riscv_cpu_idle_prepare(uint32_t hart_id);

#endif /* _RISCV_CPU_DRIVER_H_ */
