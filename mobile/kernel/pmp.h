/*
 * Physical Memory Protection (PMP) for RISC-V
 * uOS(m) - User OS Mobile
 */

#ifndef _PMP_H_
#define _PMP_H_

#include <stdint.h>

/* PMP configuration register indices */
#define PMP_CFG0    0x3a0
#define PMP_CFG1    0x3a1
#define PMP_CFG2    0x3a2
#define PMP_CFG3    0x3a3

/* PMP address registers */
#define PMP_ADDR0   0x3b0
#define PMP_ADDR1   0x3b1
#define PMP_ADDR2   0x3b2
#define PMP_ADDR3   0x3b3
#define PMP_ADDR4   0x3b4
#define PMP_ADDR5   0x3b5
#define PMP_ADDR6   0x3b6
#define PMP_ADDR7   0x3b7
#define PMP_ADDR8   0x3b8
#define PMP_ADDR9   0x3b9
#define PMP_ADDR10  0x3ba
#define PMP_ADDR11  0x3bb
#define PMP_ADDR12  0x3bc
#define PMP_ADDR13  0x3bd
#define PMP_ADDR14  0x3be
#define PMP_ADDR15  0x3bf

/* PMP configuration flags */
#define PMP_R       (1 << 0)    /* Read permission */
#define PMP_W       (1 << 1)    /* Write permission */
#define PMP_X       (1 << 2)    /* Execute permission */
#define PMP_A_TOR   (1 << 3)    /* Top-of-range addressing */
#define PMP_A_NA4   (2 << 3)    /* Naturally aligned 4-byte */
#define PMP_A_NAPOT (3 << 3)    /* Naturally aligned power-of-two */
#define PMP_L       (1 << 7)    /* Lock bit */

/* Initialize PMP for kernel protection */
void pmp_init(void);

/* Configure PMP region */
void pmp_config_region(int region, uint64_t start, uint64_t end, uint8_t perms);

#endif /* _PMP_H_ */