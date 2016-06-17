/*
 * CPCI-405 board specific definitions
 *
 * Copyright (c) 2001 Stefan Roese (stefan.roese@esd-electronics.com)
 */

#ifdef __KERNEL__
#ifndef __ASM_CPCI405_H__
#define __ASM_CPCI405_H__

#include <linux/config.h>

/* We have a 405GP core */
#include <platforms/ibm405gp.h>

#include <asm/ppcboot.h>

#ifndef __ASSEMBLY__
/* Some 4xx parts use a different timebase frequency from the internal clock.
*/
#define bi_tbfreq bi_intfreq

/* Map for the NVRAM space */
#define CPCI405_NVRAM_PADDR	((uint)0xf0200000)
#define CPCI405_NVRAM_SIZE	((uint)32*1024)

#if defined(CONFIG_UART0_TTYS0)
#define ACTING_UART0_IO_BASE	UART0_IO_BASE
#define ACTING_UART1_IO_BASE	UART1_IO_BASE
#define ACTING_UART0_INT	UART0_INT
#define ACTING_UART1_INT	UART1_INT
#else
#define ACTING_UART0_IO_BASE	UART1_IO_BASE
#define ACTING_UART1_IO_BASE	UART0_IO_BASE
#define ACTING_UART0_INT	UART1_INT
#define ACTING_UART1_INT	UART0_INT
#endif

/* The UART clock is based off an internal clock -
 * define BASE_BAUD based on the internal clock and divider(s).
 * Since BASE_BAUD must be a constant, we will initialize it
 * using clock/divider values which U-Boot initializes
 * for typical configurations at various CPU speeds.
 * The base baud is calculated as (FWDA / EXT UART DIV / 16)
 */
#define BASE_BAUD       0

#define PPC4xx_MACHINE_NAME "esd CPCI-405"

#endif /* !__ASSEMBLY__ */
#endif	/* __ASM_CPCI405_H__ */
#endif /* __KERNEL__ */
