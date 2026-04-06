/*
 * RISC-V CPU Driver
 * Mobile OS Project
 * 
 * Core CPU driver for RISC-V processors
 */

#ifndef _RISCV_CPU_DRIVER_H_
#define _RISCV_CPU_DRIVER_H_

#include "riscv_hal.h"

/* ============================================================================
 * CPU Driver Interface
 * ============================================================================ */

typedef struct {
    uint32_t hart_id;                    /* Hardware thread ID */
    cpu_features_t features;             /* CPU capabilities */
    uint32_t frequency_current;          /* Current frequency (MHz) */
    uint32_t frequency_max;              /* Maximum frequency (MHz) */
    uint32_t frequency_min;              /* Minimum frequency (MHz) */
    uint32_t voltage_current;            /* Current voltage (mV) */
    uint32_t temperature;                /* Current temperature (°C) */
    bool online;                         /* CPU online status */
} riscv_cpu_t;

/* ============================================================================
 * CPU Driver Functions
 * ============================================================================ */

/* Initialize CPU driver */
int riscv_cpu_driver_init(void);

/* Get CPU information */
riscv_cpu_t* riscv_cpu_get_info(uint32_t hart_id);

/* CPU frequency scaling */
int riscv_cpu_set_frequency(uint32_t hart_id, uint32_t freq_mhz);

/* CPU offline/online */
int riscv_cpu_set_online(uint32_t hart_id, bool online);

/* CPU temperature monitoring */
uint32_t riscv_cpu_get_temperature(uint32_t hart_id);

/* CPU power management */
int riscv_cpu_idle_prepare(uint32_t hart_id);

#endif /* _RISCV_CPU_DRIVER_H_ */


/* ============================================================================
 * CPU Driver Implementation (Simplified)
 * ============================================================================ */

#include <stdio.h>

static riscv_cpu_t g_cpu_info[8];  /* Support up to 8 HARTs */

int riscv_cpu_driver_init(void) {
    printf("[RISCV] Initializing CPU driver...\n");
    
    riscv_hal_init();
    
    /* Initialize CPU info for each HART */
    for (int i = 0; i < 8; i++) {
        g_cpu_info[i].hart_id = i;
        g_cpu_info[i].online = (i == 0);  /* Only hart 0 online initially */
        g_cpu_info[i].frequency_max = 2500;  /* 2.5 GHz */
        g_cpu_info[i].frequency_min = 400;   /* 400 MHz */
        g_cpu_info[i].frequency_current = 1800;  /* 1.8 GHz startup */
    }
    
    riscv_hal_get_cpu_features();
    printf("[RISCV] CPU driver initialized\n");
    
    return 0;
}

riscv_cpu_t* riscv_cpu_get_info(uint32_t hart_id) {
    if (hart_id >= 8) return NULL;
    return &g_cpu_info[hart_id];
}

int riscv_cpu_set_frequency(uint32_t hart_id, uint32_t freq_mhz) {
    if (hart_id >= 8) return -1;
    
    riscv_cpu_t* cpu = &g_cpu_info[hart_id];
    
    if (freq_mhz < cpu->frequency_min || freq_mhz > cpu->frequency_max) {
        printf("[RISCV] Invalid frequency: %u MHz\n", freq_mhz);
        return -1;
    }
    
    cpu->frequency_current = freq_mhz;
    printf("[RISCV] CPU %u frequency set to %u MHz\n", hart_id, freq_mhz);
    
    return 0;
}

int riscv_cpu_set_online(uint32_t hart_id, bool online) {
    if (hart_id >= 8 || hart_id == 0) return -1;  /* Can't offline hart 0 */
    
    g_cpu_info[hart_id].online = online;
    printf("[RISCV] CPU %u is now %s\n", hart_id, online ? "online" : "offline");
    
    return 0;
}

uint32_t riscv_cpu_get_temperature(uint32_t hart_id) {
    if (hart_id >= 8) return 0;
    return g_cpu_info[hart_id].temperature;
}

int riscv_cpu_idle_prepare(uint32_t hart_id) {
    if (hart_id >= 8) return -1;
    
    /* Prepare CPU for idle state */
    riscv_hal_wfi();  /* Wait for interrupt */
    
    return 0;
}
