/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (c) 2001-2003 Silicon Graphics, Inc.  All rights reserved.
 */


#ifndef _ASM_IA64_SN_SN2_SHUB_H
#define _ASM_IA64_SN_SN2_SHUB_H

#include <asm/sn/sn2/shub_mmr.h>		/* shub mmr addresses and formats */
#include <asm/sn/sn2/shub_md.h>	
#include <asm/sn/sn2/shubio.h>	
#ifndef __ASSEMBLY__
#include <asm/sn/sn2/shub_mmr_t.h>		/* shub mmr struct defines */
#endif

/*
 * Junk Bus Address Space
 *   The junk bus is used to access the PROM, LED's, and UART. It's 
 *   accessed through the local block MMR space. The data path is
 *   16 bits wide. This space requires address bits 31-27 to be set, and
 *   is further divided by address bits 26:15.
 *   The LED addresses are write-only. To read the LEDs, you need to use
 *   SH_JUNK_BUS_LED0-3, defined in shub_mmr.h
 *		
 */
#define SH_REAL_JUNK_BUS_LED0           0x7fed00000
#define SH_REAL_JUNK_BUS_LED1           0x7fed10000
#define SH_REAL_JUNK_BUS_LED2           0x7fed20000
#define SH_REAL_JUNK_BUS_LED3           0x7fed30000
#define SH_JUNK_BUS_UART0               0x7fed40000
#define SH_JUNK_BUS_UART1               0x7fed40008
#define SH_JUNK_BUS_UART2               0x7fed40010
#define SH_JUNK_BUS_UART3               0x7fed40018
#define SH_JUNK_BUS_UART4               0x7fed40020
#define SH_JUNK_BUS_UART5               0x7fed40028
#define SH_JUNK_BUS_UART6               0x7fed40030
#define SH_JUNK_BUS_UART7               0x7fed40038

#endif /* _ASM_IA64_SN_SN2_SHUB_H */
