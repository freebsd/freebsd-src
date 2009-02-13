/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * This software is derived from software provide by Kwikbyte who specifically
 * disclaimed copyright on the code.
 *
 * $FreeBSD$
 */

#include "at91rm9200.h"
#include "at91rm9200_lowlevel.h"

extern int __bss_start__[];
extern int __bss_end__[];

#define BAUD	115200
#define AT91C_US_ASYNC_MODE (AT91C_US_USMODE_NORMAL | AT91C_US_NBSTOP_1_BIT | \
		AT91C_US_PAR_NONE | AT91C_US_CHRL_8_BITS | AT91C_US_CLKS_CLOCK)

/*
 * void DefaultSystemInit(void)
 *  Load the system with sane values based on how the system is configured.
 *  at91rm9200_lowlevel.h is expected to define the necessary parameters.
 */
void
_init(void)
{
	int *i;

	AT91PS_USART pUSART = (AT91PS_USART)AT91C_BASE_DBGU;
	AT91PS_PDC pPDC = (AT91PS_PDC)&(pUSART->US_RPR);

	register unsigned	value;
	volatile sdram_size_t *p = (sdram_size_t *)SDRAM_BASE;

	AT91C_BASE_ST->ST_RTMR = 1;
#ifdef BOOT_TSC
	// For the TSC board, we turn ON the one LED we have while
	// early in boot.
	AT91C_BASE_PIOC->PIO_PER = AT91C_PIO_PC10;
	AT91C_BASE_PIOC->PIO_OER = AT91C_PIO_PC10;
	AT91C_BASE_PIOC->PIO_CODR = AT91C_PIO_PC10;
#endif

#if defined(BOOT_KB920X)
	AT91C_BASE_PIOC->PIO_PER = AT91C_PIO_PC18 | AT91C_PIO_PC19 |
	  AT91C_PIO_PC20;
	AT91C_BASE_PIOC->PIO_OER = AT91C_PIO_PC18 | AT91C_PIO_PC19 |
	  AT91C_PIO_PC20;
	AT91C_BASE_PIOC->PIO_SODR = AT91C_PIO_PC18 | AT91C_PIO_PC19 |
	  AT91C_PIO_PC20;
	AT91C_BASE_PIOC->PIO_CODR = AT91C_PIO_PC18;
#endif

	// configure clocks
	// assume:
	//    main osc = 10Mhz
	//    PLLB configured for 96MHz (48MHz after div)
	//    CSS = PLLB
	// set PLLA = 180MHz
	// assume main osc = 10Mhz
	// div = 5 , out = 2 (150MHz = 240MHz)
	value = AT91C_BASE_CKGR->CKGR_PLLAR;
	value &= ~(AT91C_CKGR_DIVA | AT91C_CKGR_OUTA | AT91C_CKGR_MULA);
	value |= OSC_MAIN_FREQ_DIV | AT91C_CKGR_OUTA_2 | AT91C_CKGR_SRCA |
	    ((OSC_MAIN_MULT - 1) << 16);
	AT91C_BASE_CKGR->CKGR_PLLAR = value;

	// wait for lock
	while (!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_LOCKA))
		continue;

	// change divider = 3, pres = 1
	value = AT91C_BASE_PMC->PMC_MCKR;
	value &= ~(AT91C_PMC_MDIV | AT91C_PMC_PRES);
	value |= AT91C_PMC_MDIV_3 | AT91C_PMC_PRES_CLK;
	AT91C_BASE_PMC->PMC_MCKR = value;

	// wait for update
	while (!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY))
		continue;

	// change CSS = PLLA
	value &= ~AT91C_PMC_CSS;
	value |= AT91C_PMC_CSS_PLLA_CLK;
	AT91C_BASE_PMC->PMC_MCKR = value;

	// wait for update
	while (!(AT91C_BASE_PMC->PMC_SR & AT91C_PMC_MCKRDY))
		continue;

#ifdef BOOT_KB920X
	// setup flash access (allow ample margin)
	// 9 wait states, 1 setup, 1 hold, 1 float for 8-bit device
	((AT91PS_SMC2)AT91C_BASE_SMC2)->SMC2_CSR[0] =
		AT91C_SMC2_WSEN |
		(9 & AT91C_SMC2_NWS) |
		((1 << 8) & AT91C_SMC2_TDF) |
		AT91C_SMC2_DBW_8 |
		((1 << 24) & AT91C_SMC2_RWSETUP) |
		((1 << 29) & AT91C_SMC2_RWHOLD);
#endif

	// setup SDRAM access
	// EBI chip-select register (CS1 = SDRAM controller)
	// 9 col, 13row, 4 bank, CAS2
	// write recovery = 2 (Twr)
	// row cycle = 5 (Trc)
	// precharge delay = 2 (Trp)
	// row to col delay 2 (Trcd)
	// active to precharge = 4 (Tras)
	// exit self refresh to active = 6 (Txsr)
	value = ((AT91PS_EBI)AT91C_BASE_EBI)->EBI_CSA;
	value &= ~AT91C_EBI_CS1A;
	value |= AT91C_EBI_CS1A_SDRAMC;
	AT91C_BASE_EBI->EBI_CSA = value;

	AT91C_BASE_SDRC->SDRC_CR =
#ifdef KB9202_B
	    AT91C_SDRC_NC_10 |
#else
	    AT91C_SDRC_NC_9 |
#endif
	    AT91C_SDRC_NR_13 |
	    AT91C_SDRC_NB_4_BANKS |
	    AT91C_SDRC_CAS_2 |
	    ((2 << 7) & AT91C_SDRC_TWR) |
	    ((5 << 11) & AT91C_SDRC_TRC) |
	    ((2 << 15) & AT91C_SDRC_TRP) |
	    ((2 << 19) & AT91C_SDRC_TRCD) |
	    ((4 << 23) & AT91C_SDRC_TRAS) |
	    ((6 << 27) & AT91C_SDRC_TXSR);

	// Step 1: We assume 200us of idle time.
	// Step 2: Issue an all banks precharge command
	AT91C_BASE_SDRC->SDRC_MR = SDRAM_WIDTH | AT91C_SDRC_MODE_PRCGALL_CMD;
	*p = 0;

	// Step 3: Issue 8 Auto-refresh (CBR) cycles
	AT91C_BASE_SDRC->SDRC_MR = SDRAM_WIDTH | AT91C_SDRC_MODE_RFSH_CMD;
	*p = 0;
	*p = 0;
	*p = 0;
	*p = 0;
	*p = 0;
	*p = 0;
	*p = 0;
	*p = 0;

	// Step 4: Issue an Mode Set Register (MRS) cycle to program in
	// the parameters that we setup in the SDRC_CR register above.
	AT91C_BASE_SDRC->SDRC_MR = SDRAM_WIDTH | AT91C_SDRC_MODE_LMR_CMD;
	*p = 0;

	// Step 5: set the refresh timer and access memory to start it
	// running.  We have to wait 3 clocks after the LMR_CMD above,
	// and this fits the bill nicely.
	AT91C_BASE_SDRC->SDRC_TR = 7 * AT91C_MASTER_CLOCK / 1000000;
	*p = 0;

	// Step 6: Set normal mode.
	AT91C_BASE_SDRC->SDRC_MR = SDRAM_WIDTH | AT91C_SDRC_MODE_NORMAL_CMD;
	*p = 0;

#if	SDRAM_WIDTH == AT91C_SDRC_DBW_32_BITS
	// Turn on the upper 16 bits on the SDRAM bus.
	AT91C_BASE_PIOC->PIO_ASR = 0xffff0000;
	AT91C_BASE_PIOC->PIO_PDR = 0xffff0000;
#endif
	// Configure DBGU -use local routine optimized for space
	AT91C_BASE_PIOA->PIO_ASR = AT91C_PA31_DTXD | AT91C_PA30_DRXD;
	AT91C_BASE_PIOA->PIO_PDR = AT91C_PA31_DTXD | AT91C_PA30_DRXD;
	pUSART->US_IDR = (unsigned int) -1;
	pUSART->US_CR =
	    AT91C_US_RSTRX | AT91C_US_RSTTX | AT91C_US_RXDIS | AT91C_US_TXDIS;
	pUSART->US_BRGR = ((((AT91C_MASTER_CLOCK*10)/(BAUD*16))+5)/10);
	pUSART->US_TTGR = 0;
	pPDC->PDC_PTCR = AT91C_PDC_RXTDIS;
	pPDC->PDC_PTCR = AT91C_PDC_TXTDIS;
	pPDC->PDC_TNPR = 0;
	pPDC->PDC_TNCR = 0;

	pPDC->PDC_RNPR = 0;
	pPDC->PDC_RNCR = 0;

	pPDC->PDC_TPR = 0;
	pPDC->PDC_TCR = 0;

	pPDC->PDC_RPR = 0;
	pPDC->PDC_RCR = 0;

	pPDC->PDC_PTCR = AT91C_PDC_RXTEN;
	pPDC->PDC_PTCR = AT91C_PDC_TXTEN;

	pUSART->US_MR = AT91C_US_ASYNC_MODE;
	pUSART->US_CR = AT91C_US_TXEN;
	pUSART->US_CR = AT91C_US_RXEN;

	/* Zero BSS now that we have memory setup */
	i = (int *)__bss_start__;
	while (i < (int *)__bss_end__)
		*i++ = 0;
}
