/*-
 *	JNPR: pltfm.h,v 1.5.2.1 2007/09/10 05:56:11 girish
 * $FreeBSD$
 */

#ifndef _MACHINE_PLTFM_H_
#define	_MACHINE_PLTFM_H_

/*
 * This files contains platform-specific definitions.
 */
#define	SDRAM_ADDR_START	0 /* SDRAM addr space */
#define	SDRAM_ADDR_END		(SDRAM_ADDR_START + (1024*0x100000))
#define	SDRAM_MEM_SIZE		(SDRAM_ADDR_END - SDRAM_ADDR_START)

#define	UART_ADDR_START		0x1ef14000	/* UART */
#define	UART_ADDR_END		0x1ef14fff
#define	UART_MEM_SIZE		(UART_ADDR_END-UART_ADDR_START)

/*
 * NS16550 UART address
 */
#ifdef ADDR_NS16550_UART1
#undef ADDR_NS16550_UART1
#endif
#define	ADDR_NS16550_UART1	0x1ef14000	/* UART */
#define	VADDR_NS16550_UART1	0xbef14000	/* UART */

#endif /* !_MACHINE_PLTFM_H_ */
