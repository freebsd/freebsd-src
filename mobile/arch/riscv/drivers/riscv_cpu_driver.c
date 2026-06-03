/*
 * RISC-V CPU Driver
 * Mobile OS Project
 *
 * Core CPU driver for RISC-V processors
 */

#include "riscv_cpu_driver.h"

static riscv_cpu_t g_cpu_info[8];

int riscv_cpu_driver_init(void) {
    riscv_hal_init();

    for (int i = 0; i < 8; i++) {
        g_cpu_info[i].hart_id = i;
        g_cpu_info[i].online = (i == 0);
        g_cpu_info[i].frequency_max = 2500;
        g_cpu_info[i].frequency_min = 400;
        g_cpu_info[i].frequency_current = 1800;
    }

    riscv_hal_get_cpu_features();
    return 0;
}

riscv_cpu_t* riscv_cpu_get_info(uint32_t hart_id) {
    if (hart_id >= 8) return NULL;
    return &g_cpu_info[hart_id];
}

int riscv_cpu_set_frequency(uint32_t hart_id, uint32_t freq_mhz) {
    if (hart_id >= 8) return -1;
    riscv_cpu_t* cpu = &g_cpu_info[hart_id];
    if (freq_mhz < cpu->frequency_min || freq_mhz > cpu->frequency_max)
        return -1;
    cpu->frequency_current = freq_mhz;
    return 0;
}

int riscv_cpu_set_online(uint32_t hart_id, bool online) {
    if (hart_id >= 8 || hart_id == 0) return -1;
    g_cpu_info[hart_id].online = online;
    return 0;
}

uint32_t riscv_cpu_get_temperature(uint32_t hart_id) {
    if (hart_id >= 8) return 0;
    return g_cpu_info[hart_id].temperature;
}

int riscv_cpu_idle_prepare(uint32_t hart_id) {
    if (hart_id >= 8) return -1;
    riscv_hal_wfi();
    return 0;
}
