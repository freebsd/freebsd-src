/*-
 * Copyright (c) 1997, 1998 Nicolas Souchu
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: ppc.c,v 1.8 1998/09/13 20:57:06 nsouch Exp $
 *
 */
#include "ppc.h"

#if NPPC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/isa/isa_device.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppb_msq.h>

#include <i386/isa/ppcreg.h>

static int	ppcprobe(struct isa_device *);
static int	ppcattach(struct isa_device *);

struct isa_driver ppcdriver = {
	ppcprobe, ppcattach, "ppc"
};

static struct ppc_data *ppcdata[NPPC];
static int nppc = 0;

static char *ppc_types[] = {
	"SMC-like", "SMC FDC37C665GT", "SMC FDC37C666GT", "PC87332", "PC87306",
	"82091AA", "Generic", "W83877F", "W83877AF", "Winbond", 0
};

/* list of available modes */
static char *ppc_avms[] = {
	"COMPATIBLE", "NIBBLE-only", "PS2-only", "PS2/NIBBLE", "EPP-only",
	"EPP/NIBBLE", "EPP/PS2", "EPP/PS2/NIBBLE", "ECP-only",
	"ECP/NIBBLE", "ECP/PS2", "ECP/PS2/NIBBLE", "ECP/EPP",
	"ECP/EPP/NIBBLE", "ECP/EPP/PS2", "ECP/EPP/PS2/NIBBLE", 0
};

/* list of current executing modes
 * Note that few modes do not actually exist.
 */
static char *ppc_modes[] = {
	"COMPATIBLE", "NIBBLE", "PS/2", "PS/2", "EPP",
	"EPP", "EPP", "EPP", "ECP",
	"ECP", "ECP+PS2", "ECP+PS2", "ECP+EPP",
	"ECP+EPP", "ECP+EPP", "ECP+EPP", 0
};

static char *ppc_epp_protocol[] = { " (EPP 1.9)", " (EPP 1.7)", 0 };

/*
 * BIOS printer list - used by BIOS probe.
 */
#define	BIOS_PPC_PORTS	0x408
#define	BIOS_PORTS	(short *)(KERNBASE+BIOS_PPC_PORTS)
#define	BIOS_MAX_PPC	4

/*
 * All these functions are default actions for IN/OUT operations.
 * They may be redefined if needed.
 */
static void ppc_outsb_epp(int unit, char *addr, int cnt) {
	outsb(ppcdata[unit]->ppc_base + PPC_EPP_DATA, addr, cnt); }
static void ppc_outsw_epp(int unit, char *addr, int cnt) {
	outsw(ppcdata[unit]->ppc_base + PPC_EPP_DATA, addr, cnt); }
static void ppc_outsl_epp(int unit, char *addr, int cnt) {
	outsl(ppcdata[unit]->ppc_base + PPC_EPP_DATA, addr, cnt); }
static void ppc_insb_epp(int unit, char *addr, int cnt) {
	insb(ppcdata[unit]->ppc_base + PPC_EPP_DATA, addr, cnt); }
static void ppc_insw_epp(int unit, char *addr, int cnt) {
	insw(ppcdata[unit]->ppc_base + PPC_EPP_DATA, addr, cnt); }
static void ppc_insl_epp(int unit, char *addr, int cnt) {
	insl(ppcdata[unit]->ppc_base + PPC_EPP_DATA, addr, cnt); }

static char ppc_rdtr(int unit) { return r_dtr(ppcdata[unit]); }
static char ppc_rstr(int unit) { return r_str(ppcdata[unit]); }
static char ppc_rctr(int unit) { return r_ctr(ppcdata[unit]); }
static char ppc_repp(int unit) { return r_epp(ppcdata[unit]); }
static char ppc_recr(int unit) { return r_ecr(ppcdata[unit]); }
static char ppc_rfifo(int unit) { return r_fifo(ppcdata[unit]); }

static void ppc_wdtr(int unit, char byte) { w_dtr(ppcdata[unit], byte); }
static void ppc_wstr(int unit, char byte) { w_str(ppcdata[unit], byte); }
static void ppc_wctr(int unit, char byte) { w_ctr(ppcdata[unit], byte); }
static void ppc_wepp(int unit, char byte) { w_epp(ppcdata[unit], byte); }
static void ppc_wecr(int unit, char byte) { w_ecr(ppcdata[unit], byte); }
static void ppc_wfifo(int unit, char byte) { w_fifo(ppcdata[unit], byte); }

static void ppc_reset_epp_timeout(int);
static void ppc_ecp_sync(int);

static int ppc_exec_microseq(int, struct ppb_microseq **);
static int ppc_generic_setmode(int, int);
static int ppc_smclike_setmode(int, int);

static struct ppb_adapter ppc_smclike_adapter = {

	0,	/* no intr handler, filled by chipset dependent code */

	ppc_reset_epp_timeout, ppc_ecp_sync,

	ppc_exec_microseq,

	ppc_smclike_setmode,

	ppc_outsb_epp, ppc_outsw_epp, ppc_outsl_epp,
	ppc_insb_epp, ppc_insw_epp, ppc_insl_epp,

	ppc_rdtr, ppc_rstr, ppc_rctr, ppc_repp, ppc_recr, ppc_rfifo,
	ppc_wdtr, ppc_wstr, ppc_wctr, ppc_wepp, ppc_wecr, ppc_wfifo
};

static struct ppb_adapter ppc_generic_adapter = {

	0,	/* no intr handler, filled by chipset dependent code */

	ppc_reset_epp_timeout, ppc_ecp_sync,

	ppc_exec_microseq,

	ppc_generic_setmode,

	ppc_outsb_epp, ppc_outsw_epp, ppc_outsl_epp,
	ppc_insb_epp, ppc_insw_epp, ppc_insl_epp,

	ppc_rdtr, ppc_rstr, ppc_rctr, ppc_repp, ppc_recr, ppc_rfifo,
	ppc_wdtr, ppc_wstr, ppc_wctr, ppc_wepp, ppc_wecr, ppc_wfifo
};

/*
 * ppc_ecp_sync()		XXX
 */
static void
ppc_ecp_sync(int unit) {

	struct ppc_data *ppc = ppcdata[unit];
	int i, r;

	r = r_ecr(ppc);
	if ((r & 0xe0) != 0x80)
		return;

	for (i = 0; i < 100; i++) {
		r = r_ecr(ppc);
		if (r & 0x1)
			return;
		DELAY(100);
	}

	printf("ppc%d: ECP sync failed as data still " \
		"present in FIFO.\n", unit);

	return;
}

void
ppcintr(int unit)
{
	/* call directly upper code */
	ppb_intr(&ppcdata[unit]->ppc_link);

	return;
}

static int
ppc_detect_port(struct ppc_data *ppc)
{

	w_ctr(ppc, 0x0c);	/* To avoid missing PS2 ports */
	w_dtr(ppc, 0xaa);
	if (r_dtr(ppc) != (char) 0xaa)
		return (0);

	return (1);
}

/*
 * ppc_pc873xx_detect
 *
 * Probe for a Natsemi PC873xx-family part.
 *
 * References in this function are to the National Semiconductor
 * PC87332 datasheet TL/C/11930, May 1995 revision.
 */
static int pc873xx_basetab[] = {0x0398, 0x026e, 0x015c, 0x002e, 0};
static int pc873xx_porttab[] = {0x0378, 0x03bc, 0x0278, 0};

static int
ppc_pc873xx_detect(struct ppc_data *ppc, int chipset_mode)	/* XXX mode never forced */
{
    static int	index = 0;
    int		base, idport;
    int		val;
    
    while ((idport = pc873xx_basetab[index++])) {
	
	/* XXX should check first to see if this location is already claimed */

	/*
	 * Pull the 873xx through the power-on ID cycle (2.2,1.).  We can't use this
	 * to locate the chip as it may already have been used by the BIOS.
	 */
	(void)inb(idport); (void)inb(idport); (void)inb(idport); (void)inb(idport);

	/*
	 * Read the SID byte.  Possible values are :
	 *
	 * 0001xxxx	PC87332
	 * 01110xxx	PC87306
	 */
	outb(idport, PC873_SID);
	val = inb(idport + 1);
	if ((val & 0xf0) == 0x10) {
	    ppc->ppc_type = NS_PC87332;
	} else if ((val & 0xf8) == 0x70) {
	    ppc->ppc_type = NS_PC87306;
	} else {
	    if (bootverbose && (val != 0xff))
		printf("PC873xx probe at 0x%x got unknown ID 0x%x\n", idport, val);
	    continue ;		/* not recognised */
	}
	
	/*
	 * We think we have one.  Is it enabled and where we want it to be?	 
	 */
	outb(idport, PC873_FER);
	val = inb(idport + 1);
	if (!(val & PC873_PPENABLE)) {
	    if (bootverbose)
		printf("PC873xx parallel port disabled\n");
	    continue;
	}
	outb(idport, PC873_FAR);
	val = inb(idport + 1) & 0x3;
	/* XXX we should create a driver instance for every port found */
	if (pc873xx_porttab[val] != ppc->ppc_base) {
	    if (bootverbose)
		printf("PC873xx at 0x%x not for driver at port 0x%x\n",
		       pc873xx_porttab[val], ppc->ppc_base);
	    continue;
	}
	
	/* 
	 * This is the port we want.  Can we dink with it to improve
	 * our chances?
	 */
	outb(idport, PC873_PTR);
	val = inb(idport + 1);
	if (val & PC873_CFGLOCK) {
	    if (bootverbose)
		printf("PC873xx locked\n");

	    /* work out what mode we're in */
	    ppc->ppc_avm |= PPB_NIBBLE;		/* worst case */
	    
	    outb(idport, PC873_PCR);
	    val = inb(idport + 1);
	    if ((val & PC873_EPPEN) && (val & PC873_EPP19)) {
		outb(idport, PC873_PTR);
		val = inb(idport + 1);
		if (!(val & PC873_EPPRDIR)) {
		    ppc->ppc_avm |= PPB_EPP;	/* As we would have done it anwyay */
		}
	    } else if ((val & PC873_ECPEN) && (val & PC873_ECPCLK)) {
		ppc->ppc_avm |= PPB_PS2;	/* tolerable alternative */
	    }	    
	} else {
	    if (bootverbose)
		printf("PC873xx unlocked, ");

#if 0	/* broken */
	    /*
	     * Frob the zero-wait-state option if possible; it causes
	     * unreliable operation.
	     */
	    outb(idport, PC873_FCR);
	    val = inb(idport + 1);
	    if ((ppc->ppc_type == NS_PC87306) ||	/* we are a '306 */
		!(val & PC873_ZWSPWDN)) {		/* or pin _is_ ZWS */
		val &= ~PC873_ZWS;
		outb(idport + 1, val);			/* must disable ZWS */
		outb(idport + 1, val);
		
		if (bootverbose)
		    printf("ZWS %s, ", (val & PC873_ZWS) ? "enabled" : "disabled");
	    }

#endif
	    if (bootverbose)
		printf("reconfiguring for ");
	    
	    /* 
	     * if the chip is at 0x3bc, we can't use EPP as there's no room
	     * for the extra registers.
	     *
	     * XXX should we use ECP mode always and use the EPP submode?
	     */
	    if (ppc->ppc_base != 0x3bc) {
		if (bootverbose)
		    printf("EPP 1.9\n");
		
		/* configure for EPP 1.9 operation XXX should be configurable */
		outb(idport, PC873_PCR);
		val = inb(idport + 1);
		val &= ~(PC873_ECPEN | PC873_ECPCLK);	/* disable ECP */
		val |= (PC873_EPPEN | PC873_EPP19);	/* enable EPP */
		outb(idport + 1, val);
		outb(idport + 1, val);

		/* enable automatic direction turnover */
		outb(idport, PC873_PTR);
		val = inb(idport + 1);
		val &= ~PC873_EPPRDIR;			/* disable "regular" direction change */
		outb(idport + 1, val);
		outb(idport + 1, val);

		/* we are an EPP-32 port */
		ppc->ppc_avm |= PPB_EPP;
	    } else {
		if (bootverbose)
		    printf("ECP\n");
		
		/* configure as an ECP port to get bidirectional operation for now */
		outb(idport, PC873_PCR);
		outb(idport + 1, inb(idport + 1) | PC873_ECPEN | PC873_ECPCLK);

		/* we look like a PS/2 port */
		ppc->ppc_avm |= PPB_PS2;
	    }
	}

	return(chipset_mode);
    }
    return(-1);
}

static int
ppc_check_epp_timeout(struct ppc_data *ppc)
{
	ppc_reset_epp_timeout(ppc->ppc_unit);

	return (!(r_str(ppc) & TIMEOUT));
}

/*
 * ppc_smc37c66xgt_detect
 *
 * SMC FDC37C66xGT configuration.
 */
static int
ppc_smc37c66xgt_detect(struct ppc_data *ppc, int chipset_mode)
{
	int s, i;
	char r;
	int type = -1;
	int csr = SMC66x_CSR;	/* initial value is 0x3F0 */

	int port_address[] = { -1 /* disabled */ , 0x3bc, 0x378, 0x278 };


#define cio csr+1	/* config IO port is either 0x3F1 or 0x371 */

	/*
	 * Detection: enter configuration mode and read CRD register.
	 */
	 
	s = splhigh();
	outb(csr, SMC665_iCODE);
	outb(csr, SMC665_iCODE);
	splx(s);

	outb(csr, 0xd);
	if (inb(cio) == 0x65) {
		type = SMC_37C665GT;
		goto config;
	}

	for (i = 0; i < 2; i++) {
		s = splhigh();
		outb(csr, SMC666_iCODE);
		outb(csr, SMC666_iCODE);
		splx(s);

		outb(csr, 0xd);
		if (inb(cio) == 0x66) {
			type = SMC_37C666GT;
			break;
		}

		/* Another chance, CSR may be hard-configured to be at 0x370 */
		csr = SMC666_CSR;
	}

config:
	/*
	 * If chipset not found, do not continue.
	 */
	if (type == -1)
		return (-1);

	/* select CR1 */
	outb(csr, 0x1);

	/* read the port's address: bits 0 and 1 of CR1 */
	r = inb(cio) & SMC_CR1_ADDR;
	if (port_address[r] != ppc->ppc_base)
		return (-1);

	ppc->ppc_type = type;

	/*
	 * CR1 and CR4 registers bits 3 and 0/1 for mode configuration
	 * If SPP mode is detected, try to set ECP+EPP mode
	 */

	if (bootverbose) {
		outb(csr, 0x1);
		printf("ppc%d: SMC registers CR1=0x%x", ppc->ppc_unit,
			inb(cio) & 0xff);

		outb(csr, 0x4);
		printf(" CR4=0x%x", inb(cio) & 0xff);
	}

	/* select CR1 */
	outb(csr, 0x1);

	if (!chipset_mode) {
		/* autodetect mode */

		/* 666GT is ~certainly~ hardwired to an extended ECP+EPP mode */
		if (type == SMC_37C666GT) {
			ppc->ppc_avm |= PPB_ECP | PPB_EPP | PPB_SPP;
			if (bootverbose)
				printf(" configuration hardwired, supposing " \
					"ECP+EPP SPP");

		} else
		   if ((inb(cio) & SMC_CR1_MODE) == 0) {
			/* already in extended parallel port mode, read CR4 */
			outb(csr, 0x4);
			r = (inb(cio) & SMC_CR4_EMODE);

			switch (r) {
			case SMC_SPP:
				ppc->ppc_avm |= PPB_SPP;
				if (bootverbose)
					printf(" SPP");
				break;

			case SMC_EPPSPP:
				ppc->ppc_avm |= PPB_EPP | PPB_SPP;
				if (bootverbose)
					printf(" EPP SPP");
				break;

			case SMC_ECP:
				ppc->ppc_avm |= PPB_ECP | PPB_SPP;
				if (bootverbose)
					printf(" ECP SPP");
				break;

			case SMC_ECPEPP:
				ppc->ppc_avm |= PPB_ECP | PPB_EPP | PPB_SPP;
				if (bootverbose)
					printf(" ECP+EPP SPP");
				break;
			}
		   } else {
			/* not an extended port mode */
			ppc->ppc_avm |= PPB_SPP;
			if (bootverbose)
				printf(" SPP");
		   }

	} else {
		/* mode forced */
		ppc->ppc_avm = chipset_mode;

		/* 666GT is ~certainly~ hardwired to an extended ECP+EPP mode */
		if (type == SMC_37C666GT)
			goto end_detect;

		r = inb(cio);
		if ((chipset_mode & (PPB_ECP | PPB_EPP)) == 0) {
			/* do not use ECP when the mode is not forced to */
			outb(cio, r | SMC_CR1_MODE);
			if (bootverbose)
				printf(" SPP");
		} else {
			/* an extended mode is selected */
			outb(cio, r & ~SMC_CR1_MODE);

			/* read CR4 register and reset mode field */
			outb(csr, 0x4);
			r = inb(cio) & ~SMC_CR4_EMODE;

			if (chipset_mode & PPB_ECP) {
				if (chipset_mode & PPB_EPP) {
					outb(cio, r | SMC_ECPEPP);
					if (bootverbose)
						printf(" ECP+EPP");
				} else {
					outb(cio, r | SMC_ECP);
					if (bootverbose)
						printf(" ECP");
				}
			} else {
				/* PPB_EPP is set */
				outb(cio, r | SMC_EPPSPP);
				if (bootverbose)
					printf(" EPP SPP");
			}
		}
		ppc->ppc_avm = chipset_mode;
	}

end_detect:

	if (bootverbose)
		printf ("\n");

	if (ppc->ppc_avm & PPB_EPP) {
		/* select CR4 */
		outb(csr, 0x4);
		r = inb(cio);

		/*
		 * Set the EPP protocol...
		 * Low=EPP 1.9 (1284 standard) and High=EPP 1.7
		 */
		if (ppc->ppc_epp == EPP_1_9)
			outb(cio, (r & ~SMC_CR4_EPPTYPE));
		else
			outb(cio, (r | SMC_CR4_EPPTYPE));
	}

	/* end config mode */
	outb(csr, 0xaa);

	ppc->ppc_link.adapter = &ppc_smclike_adapter;
	ppc_smclike_setmode(ppc->ppc_unit, chipset_mode);

	return (chipset_mode);
}

/*
 * Winbond W83877F stuff
 *
 * EFER: extended function enable register
 * EFIR: extended function index register
 * EFDR: extended function data register
 */
#define efir ((efer == 0x250) ? 0x251 : 0x3f0)
#define efdr ((efer == 0x250) ? 0x252 : 0x3f1)

static int w83877f_efers[] = { 0x250, 0x3f0, 0x3f0, 0x250 };
static int w83877f_keys[] = { 0x89, 0x86, 0x87, 0x88 };	
static int w83877f_keyiter[] = { 1, 2, 2, 1 };
static int w83877f_hefs[] = { WINB_HEFERE, WINB_HEFRAS, WINB_HEFERE | WINB_HEFRAS, 0 };

static int
ppc_w83877f_detect(struct ppc_data *ppc, int chipset_mode)
{
	int i, j, efer, base;
	unsigned char r, hefere, hefras;

	for (i = 0; i < 4; i ++) {
		/* first try to enable configuration registers */
		efer = w83877f_efers[i];

		/* write the key to the EFER */
		for (j = 0; j < w83877f_keyiter[i]; j ++)
			outb (efer, w83877f_keys[i]);

		/* then check HEFERE and HEFRAS bits */
		outb (efir, 0x0c);
		hefere = inb(efdr) & WINB_HEFERE;

		outb (efir, 0x16);
		hefras = inb(efdr) & WINB_HEFRAS;

		/*
		 * HEFRAS	HEFERE
		 *   0		   1	write 89h to 250h (power-on default)
		 *   1		   0	write 86h twice to 3f0h
		 *   1		   1	write 87h twice to 3f0h
		 *   0		   0	write 88h to 250h
		 */
		if ((hefere | hefras) == w83877f_hefs[i])
			goto found;
	}

	return (-1);	/* failed */

found:
	/* check base port address - read from CR23 */
	outb(efir, 0x23);
	if (ppc->ppc_base != inb(efdr) * 4)		/* 4 bytes boundaries */
		return (-1);

	/* read CHIP ID from CR9/bits0-3 */
	outb(efir, 0x9);

	switch (inb(efdr) & WINB_CHIPID) {
		case WINB_W83877F_ID:
			ppc->ppc_type = WINB_W83877F;
			break;

		case WINB_W83877AF_ID:
			ppc->ppc_type = WINB_W83877AF;
			break;

		default:
			ppc->ppc_type = WINB_UNKNOWN;
	}

	if (bootverbose) {
		/* dump of registers */
		printf("ppc%d: 0x%x - ", ppc->ppc_unit, w83877f_keys[i]);
		for (i = 0; i <= 0xd; i ++) {
			outb(efir, i);
			printf("0x%x ", inb(efdr));
		}
		for (i = 0x10; i <= 0x17; i ++) {
			outb(efir, i);
			printf("0x%x ", inb(efdr));
		}
		outb(efir, 0x1e);
		printf("0x%x ", inb(efdr));
		for (i = 0x20; i <= 0x29; i ++) {
			outb(efir, i);
			printf("0x%x ", inb(efdr));
		}
		printf("\n");
		printf("ppc%d:", ppc->ppc_unit);
	}

	ppc->ppc_link.adapter = &ppc_generic_adapter;

	if (!chipset_mode) {
		/* autodetect mode */

		/* select CR0 */
		outb(efir, 0x0);
		r = inb(efdr) & (WINB_PRTMODS0 | WINB_PRTMODS1);

		/* select CR9 */
		outb(efir, 0x9);
		r |= (inb(efdr) & WINB_PRTMODS2);

		switch (r) {
		case WINB_W83757:
			if (bootverbose)
				printf("ppc%d: W83757 compatible mode\n",
					ppc->ppc_unit);
			return (-1);	/* generic or SMC-like */

		case WINB_EXTFDC:
		case WINB_EXTADP:
		case WINB_EXT2FDD:
		case WINB_JOYSTICK:
			if (bootverbose)
				printf(" not in parallel port mode\n");
			return (-1);

		case (WINB_PARALLEL | WINB_EPP_SPP):
			ppc->ppc_avm |= PPB_EPP | PPB_SPP;
			if (bootverbose)
				printf(" EPP SPP");
			break;

		case (WINB_PARALLEL | WINB_ECP):
			ppc->ppc_avm |= PPB_ECP | PPB_SPP;
			if (bootverbose)
				printf(" ECP SPP");
			break;

		case (WINB_PARALLEL | WINB_ECP_EPP):
			ppc->ppc_avm |= PPB_ECP | PPB_EPP | PPB_SPP;
			ppc->ppc_link.adapter = &ppc_smclike_adapter;

			if (bootverbose)
				printf(" ECP+EPP SPP");
			break;
		default:
			printf("%s: unknown case (0x%x)!\n", __FUNCTION__, r);
		}

	} else {
		/* mode forced */

		/* select CR9 and set PRTMODS2 bit */
		outb(efir, 0x9);
		outb(efdr, inb(efdr) & ~WINB_PRTMODS2);

		/* select CR0 and reset PRTMODSx bits */
		outb(efir, 0x0);
		outb(efdr, inb(efdr) & ~(WINB_PRTMODS0 | WINB_PRTMODS1));

		if (chipset_mode & PPB_ECP) {
			if (chipset_mode & PPB_EPP) {
				outb(efdr, inb(efdr) | WINB_ECP_EPP);
				if (bootverbose)
					printf(" ECP+EPP");

				ppc->ppc_link.adapter = &ppc_smclike_adapter;

			} else {
				outb(efdr, inb(efdr) | WINB_ECP);
				if (bootverbose)
					printf(" ECP");
			}
		} else {
			/* select EPP_SPP otherwise */
			outb(efdr, inb(efdr) | WINB_EPP_SPP);
			if (bootverbose)
				printf(" EPP SPP");
		}
		ppc->ppc_avm = chipset_mode;
	}

	if (bootverbose)
		printf("\n");
	
	/* exit configuration mode */
	outb(efer, 0xaa);

	ppc->ppc_link.adapter->setmode(ppc->ppc_unit, chipset_mode);

	return (chipset_mode);
}

/*
 * ppc_generic_detect
 */
static int
ppc_generic_detect(struct ppc_data *ppc, int chipset_mode)
{
	char save_control;

	/* default to generic */
	ppc->ppc_link.adapter = &ppc_generic_adapter;

	if (bootverbose)
		printf("ppc%d:", ppc->ppc_unit);

	if (!chipset_mode) {
		/* first, check for ECP */
		w_ecr(ppc, 0x20);
		if ((r_ecr(ppc) & 0xe0) == 0x20) {
			ppc->ppc_avm |= PPB_ECP | PPB_SPP;
			if (bootverbose)
				printf(" ECP SPP");

			/* search for SMC style ECP+EPP mode */
			w_ecr(ppc, 0x80);
		}

		/* try to reset EPP timeout bit */
		if (ppc_check_epp_timeout(ppc)) {
			ppc->ppc_avm |= PPB_EPP;

			if (ppc->ppc_avm & PPB_ECP) {
				/* SMC like chipset found */
				ppc->ppc_type = SMC_LIKE;
				ppc->ppc_link.adapter = &ppc_smclike_adapter;

				if (bootverbose)
					printf(" ECP+EPP");
			} else {
				if (bootverbose)
					printf(" EPP");
			}
		} else {
			/* restore to standard mode */
			w_ecr(ppc, 0x0);
		}

		/* XXX try to detect NIBBLE and PS2 modes */
		ppc->ppc_avm |= PPB_NIBBLE;

		if (bootverbose)
			printf(" SPP");

	} else {
		ppc->ppc_avm = chipset_mode;
	}

	if (bootverbose)
		printf("\n");

	ppc->ppc_link.adapter->setmode(ppc->ppc_unit, chipset_mode);

	return (chipset_mode);
}

/*
 * ppc_detect()
 *
 * mode is the mode suggested at boot
 */
static int
ppc_detect(struct ppc_data *ppc, int chipset_mode) {

	int i, mode;

	/* list of supported chipsets */
	int (*chipset_detect[])(struct ppc_data *, int) = {
		ppc_pc873xx_detect,
		ppc_smc37c66xgt_detect,
		ppc_w83877f_detect,
		ppc_generic_detect,
		NULL
	};

	/* if can't find the port and mode not forced return error */
	if (!ppc_detect_port(ppc) && chipset_mode == 0)
		return (EIO);			/* failed, port not present */

	/* assume centronics compatible mode is supported */
	ppc->ppc_avm = PPB_COMPATIBLE;

	/* we have to differenciate available chipset modes,
	 * chipset running modes and IEEE-1284 operating modes
	 *
	 * after detection, the port must support running in compatible mode
	 */
	for (i=0; chipset_detect[i] != NULL; i++) {
		if ((mode = chipset_detect[i](ppc, chipset_mode)) != -1) {
			ppc->ppc_mode = mode;
			break;
		}
	}

	return (0);
}

/*
 * ppc_exec_microseq()
 *
 * Execute a microsequence.
 * Microsequence mechanism is supposed to handle fast I/O operations.
 */
static int
ppc_exec_microseq(int unit, struct ppb_microseq **p_msq)
{
	struct ppc_data	*ppc = ppcdata[unit];
	struct ppb_microseq *mi;
	char cc, *p;
	int i, iter, len;
	int error;

	register int reg;
	register char mask;
	register int accum = 0;
	register char *ptr = 0;

	struct ppb_microseq *stack = 0;

/* microsequence registers are equivalent to PC-like port registers */
#define r_reg(register,ppc) ((char)inb((ppc)->ppc_base + register))
#define w_reg(register,ppc,byte) outb((ppc)->ppc_base + register, byte)

#define INCR_PC (mi ++)		/* increment program counter */

	mi = *p_msq;
	for (;;) {
		switch (mi->opcode) {                                           
		case MS_OP_RSET:
			cc = r_reg(mi->arg[0].i, ppc);
			cc &= (char)mi->arg[2].i;	/* clear mask */
			cc |= (char)mi->arg[1].i;	/* assert mask */
                        w_reg(mi->arg[0].i, ppc, cc);
			INCR_PC;
                        break;

		case MS_OP_RASSERT_P:
			reg = mi->arg[1].i;
			ptr = ppc->ppc_ptr;

			if ((len = mi->arg[0].i) == MS_ACCUM) {
				accum = ppc->ppc_accum;
				for (; accum; accum--)
					w_reg(reg, ppc, *ptr++);
				ppc->ppc_accum = accum;
			} else
				for (i=0; i<len; i++)
					w_reg(reg, ppc, *ptr++);
			ppc->ppc_ptr = ptr;

			INCR_PC;
			break;

                case MS_OP_RFETCH_P:
			reg = mi->arg[1].i;
			mask = (char)mi->arg[2].i;
			ptr = ppc->ppc_ptr;

			if ((len = mi->arg[0].i) == MS_ACCUM) {
				accum = ppc->ppc_accum;
				for (; accum; accum--)
					*ptr++ = r_reg(reg, ppc) & mask;
				ppc->ppc_accum = accum;
			} else
				for (i=0; i<len; i++)
					*ptr++ = r_reg(reg, ppc) & mask;
			ppc->ppc_ptr = ptr;

			INCR_PC;
                        break;                                        

                case MS_OP_RFETCH:
			*((char *) mi->arg[2].p) = r_reg(mi->arg[0].i, ppc) &
							(char)mi->arg[1].i;
			INCR_PC;
                        break;                                        

		case MS_OP_RASSERT:
                case MS_OP_DELAY:
		
		/* let's suppose the next instr. is the same */
		prefetch:
			for (;mi->opcode == MS_OP_RASSERT; INCR_PC)
				w_reg(mi->arg[0].i, ppc, (char)mi->arg[1].i);

			if (mi->opcode == MS_OP_DELAY) {
				DELAY(mi->arg[0].i);
				INCR_PC;
				goto prefetch;
			}
			break;

		case MS_OP_ADELAY:
			if (mi->arg[0].i)
				tsleep(NULL, PPBPRI, "ppbdelay",
						mi->arg[0].i * (hz/1000));
			INCR_PC;
			break;

		case MS_OP_TRIG:
			reg = mi->arg[0].i;
			iter = mi->arg[1].i;
			p = (char *)mi->arg[2].p;

			/* XXX delay limited to 255 us */
			for (i=0; i<iter; i++) {
				w_reg(reg, ppc, *p++);
				DELAY((unsigned char)*p++);
			}
			INCR_PC;
			break;

                case MS_OP_SET:
                        ppc->ppc_accum = mi->arg[0].i;
			INCR_PC;
                        break;                                         

                case MS_OP_DBRA:
                        if (--ppc->ppc_accum > 0)
                                mi += mi->arg[0].i;
			else
				INCR_PC;
                        break;                                        

                case MS_OP_BRSET:
                        cc = r_str(ppc);
                        if ((cc & (char)mi->arg[0].i) == (char)mi->arg[0].i) 
                                mi += mi->arg[1].i;                      
			else
				INCR_PC;
                        break;

                case MS_OP_BRCLEAR:
                        cc = r_str(ppc);
                        if ((cc & (char)mi->arg[0].i) == 0)    
                                mi += mi->arg[1].i;                             
			else
				INCR_PC;
                        break;                                

		case MS_OP_BRSTAT:
			cc = r_str(ppc);
			if ((cc & ((char)mi->arg[0].i | (char)mi->arg[1].i)) ==
							(char)mi->arg[0].i)
				mi += mi->arg[2].i;
			else
				INCR_PC;
			break;

		case MS_OP_C_CALL:
			/*
			 * If the C call returns !0 then end the microseq.
			 * The current state of ptr is passed to the C function
			 */
			if ((error = mi->arg[0].f(mi->arg[1].p, ppc->ppc_ptr)))
				return (error);

			INCR_PC;
			break;

		case MS_OP_PTR:
			ppc->ppc_ptr = (char *)mi->arg[0].p;
			INCR_PC;
			break;

		case MS_OP_CALL:
			if (stack)
				panic("%s: too much calls", __FUNCTION__);

			if (mi->arg[0].p) {
				/* store the state of the actual
				 * microsequence
				 */
				stack = mi;

				/* jump to the new microsequence */
				mi = (struct ppb_microseq *)mi->arg[0].p;
			} else
				INCR_PC;

			break;

		case MS_OP_SUBRET:
			/* retrieve microseq and pc state before the call */
			mi = stack;

			/* reset the stack */
			stack = 0;

			/* XXX return code */

			INCR_PC;
			break;

                case MS_OP_PUT:
                case MS_OP_GET:
                case MS_OP_RET:
			/* can't return to ppb level during the execution
			 * of a submicrosequence */
			if (stack)
				panic("%s: can't return to ppb level",
								__FUNCTION__);

			/* update pc for ppb level of execution */
			*p_msq = mi;

			/* return to ppb level of execution */
			return (0);

                default:                         
                        panic("%s: unknown microsequence opcode 0x%x",
                                __FUNCTION__, mi->opcode);        
                }
	}

	/* unreached */
}

/*
 * Configure current operating mode
 */
static int
ppc_generic_setmode(int unit, int mode)
{
	struct ppc_data *ppc = ppcdata[unit];

	/* back to compatible mode, XXX don't know yet what to do here */
	if (mode == 0) {
		ppc->ppc_mode = PPB_COMPATIBLE;
		return (0);
	}

	/* check if mode is available */
	if (!(ppc->ppc_avm & mode))
		return (EOPNOTSUPP);

	/* if ECP mode, configure ecr register */
	if (ppc->ppc_avm & PPB_ECP) {

		/* XXX disable DMA, enable interrupts */
		if (mode & PPB_EPP)
			return (EOPNOTSUPP);
		else if (mode & PPB_PS2)
			/* select PS2 mode with ECP */
			w_ecr(ppc, 0x20);
		else if (mode & PPB_ECP)
			/* select ECP mode */
			w_ecr(ppc, 0x60);
		else
			/* select standard parallel port mode */
			w_ecr(ppc, 0x00);
	}

	ppc->ppc_mode = mode;

	return (0);
}

int
ppc_smclike_setmode(int unit, int mode)
{
	struct ppc_data *ppc = ppcdata[unit];

	/* back to compatible mode, XXX don't know yet what to do here */
	if (mode == 0) {
		ppc->ppc_mode = PPB_COMPATIBLE;
		return (0);
	}

	/* check if mode is available */
	if (!(ppc->ppc_avm & mode))
		return (EOPNOTSUPP);

	/* if ECP mode, configure ecr register */
	if (ppc->ppc_avm & PPB_ECP) {

		/* XXX disable DMA, enable interrupts */
		if (mode & PPB_EPP)
			/* select EPP mode */
			w_ecr(ppc, 0x80);
		else if (mode & PPB_PS2)
			/* select PS2 mode with ECP */
			w_ecr(ppc, 0x20);
		else if (mode & PPB_ECP)
			/* select ECP mode */
			w_ecr(ppc, 0x60);
		else
			/* select standard parallel port mode */
			w_ecr(ppc, 0x00);
	}

	ppc->ppc_mode = mode;


	return (0);
}

/*
 * EPP timeout, according to the PC87332 manual
 * Semantics of clearing EPP timeout bit.
 * PC87332	- reading SPP_STR does it...
 * SMC		- write 1 to EPP timeout bit			XXX
 * Others	- (???) write 0 to EPP timeout bit
 */
static void
ppc_reset_epp_timeout(int unit)
{
	struct ppc_data *ppc = ppcdata[unit];
	register char r;

	r = r_str(ppc);
	w_str(ppc, r | 0x1);
	w_str(ppc, r & 0xfe);

	return;
}

static int
ppcprobe(struct isa_device *dvp)
{
	static short next_bios_ppc = 0;
	struct ppc_data *ppc;
	int error;

	/*
	 * If port not specified, use bios list.
	 */
	if(dvp->id_iobase < 0) {
		if((next_bios_ppc < BIOS_MAX_PPC) &&
				(*(BIOS_PORTS+next_bios_ppc) != 0) ) {
			dvp->id_iobase = *(BIOS_PORTS+next_bios_ppc++);
			printf("ppc: parallel port found at 0x%x\n",
							dvp->id_iobase);
		} else
			return (0);
	}

	/*
	 * Port was explicitly specified.
	 * This allows probing of ports unknown to the BIOS.
	 */

	/*
	 * Allocate the ppc_data structure.
	 */
	ppc = malloc(sizeof(struct ppc_data), M_DEVBUF, M_NOWAIT);
	if (!ppc) {
		printf("ppc: cannot malloc!\n");
		goto error;
	}
	bzero(ppc, sizeof(struct ppc_data));

	ppc->ppc_base = dvp->id_iobase;
	ppc->ppc_unit = dvp->id_unit;
	ppc->ppc_type = GENERIC;

	ppc->ppc_mode = PPB_COMPATIBLE;
	ppc->ppc_epp = (dvp->id_flags & 0x10) >> 4;

	/*
	 * XXX Try and detect if interrupts are working
	 */
	if (!(dvp->id_flags & 0x20))
		ppc->ppc_irq = (dvp->id_irq);

	ppcdata[ppc->ppc_unit] = ppc;
	nppc ++;

	/*
	 * Link the Parallel Port Chipset (adapter) to
	 * the future ppbus. Default to a generic chipset
	 */
	ppc->ppc_link.adapter_unit = ppc->ppc_unit;
	ppc->ppc_link.adapter = &ppc_generic_adapter;

	/*
	 * Try to detect the chipset and its mode.
	 */
	if (ppc_detect(ppc, dvp->id_flags & 0xf))
		goto error;

end_probe:

	return (1);

error:
	return (0);
}

static int
ppcattach(struct isa_device *isdp)
{
	struct ppc_data *ppc = ppcdata[isdp->id_unit];
	struct ppb_data *ppbus;
	char * mode;

	printf("ppc%d: %s chipset (%s) in %s mode%s\n", ppc->ppc_unit,
		ppc_types[ppc->ppc_type], ppc_avms[ppc->ppc_avm],
		ppc_modes[ppc->ppc_mode], (PPB_IS_EPP(ppc->ppc_mode)) ?
			ppc_epp_protocol[ppc->ppc_epp] : "");

	/*
	 * Prepare ppbus data area for upper level code.
	 */
	ppbus = ppb_alloc_bus();

	if (!ppbus)
		return (0);

	ppc->ppc_link.ppbus = ppbus;
	ppbus->ppb_link = &ppc->ppc_link;

	/*
	 * Probe the ppbus and attach devices found.
	 */
	ppb_attachdevs(ppbus);

	return (1);
}
#endif
