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

#endif /* !_MACHINE_PLTFM_H_ */
