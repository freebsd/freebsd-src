#include <machine/asm.h>
#include <arm/at91/at91_rstreg.h>
#include <arm/at91/at91reg.h>
#include <arm/at91/at91sam9g20reg.h>
__FBSDID("$FreeBSD$");

#define SDRAM_TR  (AT91_BASE + \
	AT91SAM9G20_SDRAMC_BASE + AT91SAM9G20_SDRAMC_TR)
#define SDRAM_LPR (AT91_BASE + \
	AT91SAM9G20_SDRAMC_BASE + AT91SAM9G20_SDRAMC_LPR)
#define RSTC_RCR  (AT91_BASE + \
	AT91SAM9G20_RSTC_BASE + RST_CR)

/*
 * From AT91SAM9G20 Datasheet errata 44:3.5:
 *
 * When User Reset occurs during SDRAM read access, the SDRAM clock is turned
 * off while data are ready to be read on the data bus. The SDRAM maintains
 * the data until the clock restarts.
 *
 * If the User reset is programed to assert a general reset, the data
 * maintained by the SDRAM leads to a data bus conflict and adversly affects
 * the boot memories connected to the EBI:
 *  + NAND Flash boot functionality, if the system boots out of internal ROM.
 *  + NOR Flash boot, if the system boots on an external memory connected to
 *    the EBI CS0.
 *
 * Assembly code is mandatory for the following sequnce as ARM
 * instructions need to be piplined.
 *
 */

ENTRY(cpu_reset_sam9g20)

	/* Disable IRQs */
	mrs	r0, cpsr
	orr	r0, r0, #0x80
	msr	cpsr_c, r0

	/* Change Refresh to block all data access */
	ldr	r0, =SDRAM_TR
	ldr	r1, =1
	str	r1, [r0]

	/* Prepare power down command */
	ldr	r0, =SDRAM_LPR
	ldr	r1, =2

	/* Prepare proc_reset and periph reset */
	ldr	r2, =RSTC_RCR
	ldr	r3, =0xA5000005

	/* perform power down command */
	str 	r1, [r0]

	/* Perfom proc_reset and periph reset (in the ARM pipeline) */
	str	r3, [r2]
