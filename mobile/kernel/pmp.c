/*
 * Physical Memory Protection Implementation
 * uOS(m) - User OS Mobile
 */

#include "pmp.h"

extern void uart_puts(const char *s);

/* Write to PMP configuration register */
static void pmp_write_cfg(uint64_t reg, uint64_t value) {
    switch (reg) {
        case PMP_CFG0:
            asm volatile("csrw pmpcfg0, %0" :: "r"(value));
            break;
        case PMP_CFG1:
            asm volatile("csrw pmpcfg1, %0" :: "r"(value));
            break;
        case PMP_CFG2:
            asm volatile("csrw pmpcfg2, %0" :: "r"(value));
            break;
        case PMP_CFG3:
            asm volatile("csrw pmpcfg3, %0" :: "r"(value));
            break;
    }
}

/* Write to PMP address register */
static void pmp_write_addr(uint64_t reg, uint64_t value) {
    switch (reg) {
        case PMP_ADDR0:
            asm volatile("csrw pmpaddr0, %0" :: "r"(value));
            break;
        case PMP_ADDR1:
            asm volatile("csrw pmpaddr1, %0" :: "r"(value));
            break;
        case PMP_ADDR2:
            asm volatile("csrw pmpaddr2, %0" :: "r"(value));
            break;
        case PMP_ADDR3:
            asm volatile("csrw pmpaddr3, %0" :: "r"(value));
            break;
        case PMP_ADDR4:
            asm volatile("csrw pmpaddr4, %0" :: "r"(value));
            break;
        case PMP_ADDR5:
            asm volatile("csrw pmpaddr5, %0" :: "r"(value));
            break;
        case PMP_ADDR6:
            asm volatile("csrw pmpaddr6, %0" :: "r"(value));
            break;
        case PMP_ADDR7:
            asm volatile("csrw pmpaddr7, %0" :: "r"(value));
            break;
        case PMP_ADDR8:
            asm volatile("csrw pmpaddr8, %0" :: "r"(value));
            break;
        case PMP_ADDR9:
            asm volatile("csrw pmpaddr9, %0" :: "r"(value));
            break;
        case PMP_ADDR10:
            asm volatile("csrw pmpaddr10, %0" :: "r"(value));
            break;
        case PMP_ADDR11:
            asm volatile("csrw pmpaddr11, %0" :: "r"(value));
            break;
        case PMP_ADDR12:
            asm volatile("csrw pmpaddr12, %0" :: "r"(value));
            break;
        case PMP_ADDR13:
            asm volatile("csrw pmpaddr13, %0" :: "r"(value));
            break;
        case PMP_ADDR14:
            asm volatile("csrw pmpaddr14, %0" :: "r"(value));
            break;
        case PMP_ADDR15:
            asm volatile("csrw pmpaddr15, %0" :: "r"(value));
            break;
    }
}

/* Read from PMP configuration register */
static uint64_t pmp_read_cfg(uint64_t reg) {
    uint64_t value = 0;
    switch (reg) {
        case PMP_CFG0:
            asm volatile("csrr %0, pmpcfg0" : "=r"(value));
            break;
        case PMP_CFG1:
            asm volatile("csrr %0, pmpcfg1" : "=r"(value));
            break;
        case PMP_CFG2:
            asm volatile("csrr %0, pmpcfg2" : "=r"(value));
            break;
        case PMP_CFG3:
            asm volatile("csrr %0, pmpcfg3" : "=r"(value));
            break;
    }
    return value;
}

/* Initialize PMP for kernel protection */
void pmp_init(void) {
    uart_puts("Initializing PMP (Physical Memory Protection)...\n");

    /* Configure PMP region 0: Kernel code and data (0x80000000 - 0x90000000) */
    /* NAPOT encoding: (end - start) / 2 - 1 */
    uint64_t kernel_start = 0x80000000ULL;
    uint64_t kernel_end = 0x90000000ULL;
    uint64_t napot_addr = ((kernel_end - 1) >> 2) | ((kernel_start >> 2) & 0x3FFFFFFFULL);

    pmp_write_addr(PMP_ADDR0, napot_addr);
    pmp_write_cfg(PMP_CFG0, PMP_A_NAPOT | PMP_R | PMP_W | PMP_X | PMP_L);

    /* Configure PMP region 1: UART device (0x10000000 - 0x10001000) */
    uint64_t uart_start = 0x10000000ULL;
    uint64_t uart_end = 0x10001000ULL;
    napot_addr = ((uart_end - 1) >> 2) | ((uart_start >> 2) & 0x3FFFFFFFULL);

    pmp_write_addr(PMP_ADDR1, napot_addr);
    pmp_write_cfg(PMP_CFG0, (pmp_read_cfg(PMP_CFG0) & 0x00FFFFFFULL) |
                  ((uint64_t)(PMP_A_NAPOT | PMP_R | PMP_W) << 24));

    /* Configure PMP region 2: PLIC (0x0c000000 - 0x10000000) */
    uint64_t plic_start = 0x0c000000ULL;
    uint64_t plic_end = 0x10000000ULL;
    napot_addr = ((plic_end - 1) >> 2) | ((plic_start >> 2) & 0x3FFFFFFFULL);

    pmp_write_addr(PMP_ADDR2, napot_addr);
    pmp_write_cfg(PMP_CFG0, (pmp_read_cfg(PMP_CFG0) & 0x0000FFFFULL) |
                  ((uint64_t)(PMP_A_NAPOT | PMP_R | PMP_W) << 48));

    uart_puts("PMP protection enabled\n");
}

/* Configure PMP region */
void pmp_config_region(int region, uint64_t start, uint64_t end, uint8_t perms) {
    if (region < 0 || region > 15) return;

    /* Calculate NAPOT address */
    uint64_t napot_addr = ((end - 1) >> 2) | ((start >> 2) & 0x3FFFFFFFULL);

    pmp_write_addr(PMP_ADDR0 + region, napot_addr);

    /* Update configuration register */
    uint64_t cfg_reg = PMP_CFG0 + (region / 4);
    uint64_t cfg_value = pmp_read_cfg(cfg_reg);
    uint8_t shift = (region % 4) * 8;

    cfg_value &= ~(0xFFULL << shift);
    cfg_value |= ((uint64_t)perms << shift);

    pmp_write_cfg(cfg_reg, cfg_value);
}