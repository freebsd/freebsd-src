/*-
 * Copyright (c) 1997 Nicolas Souchu
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
 *	$Id: ppc.c,v 1.1 1997/08/14 14:01:35 msmith Exp $
 *
 */
#include "ppc.h"

#if NPPC > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/malloc.h>

#include <machine/clock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>

#include <i386/isa/isa_device.h>

#include <dev/ppbus/ppbconf.h>
#include <i386/isa/ppcreg.h>

static int	ppcprobe(struct isa_device *);
static int	ppcattach(struct isa_device *);

struct isa_driver ppcdriver = {
	ppcprobe, ppcattach, "ppc"
};

static struct ppc_data *ppcdata[NPPC];
static int nppc = 0;

static char *ppc_types[] = {
	"SMC", "SMC FDC37C665GT", "SMC FDC37C666GT",
	"NatSemi", "PC87332", "PC87306",
	"Intel 82091AA", "Generic", 0
};

static char *ppc_modes[] = {
	"AUTODETECT", "NIBBLE", "PS/2", "EPP", "ECP+EPP", "ECP+PS/2", "ECP",
	"UNKNOWN", 0
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

static struct ppb_adapter ppc_adapter = {

	0,	/* no intr handler, filled by chipset dependent code */

	ppc_reset_epp_timeout, ppc_ecp_sync,

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

	printf("ppc: ECP sync failed as data still " \
		"present in FIFO.\n");

	return;
}

void
ppcintr(int unit)
{
	/* call directly upper code */
	ppb_intr(&ppcdata[unit]->ppc_link);

	return;
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
ppc_pc873xx_detect(struct ppc_data *ppc)
{
    static int	index = 0;
    int		base, idport;
    int		val, mode;
    
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
	    mode = PPB_NIBBLE;		/* worst case */
	    
	    outb(idport, PC873_PCR);
	    val = inb(idport + 1);
	    if ((val & PC873_EPPEN) && (val & PC873_EPP19)) {
		outb(idport, PC873_PTR);
		val = inb(idport + 1);
		if (!(val & PC873_EPPRDIR)) {
		    mode = PPB_EPP;	/* As we would have done it anwyay */
		}
	    } else if ((val & PC873_ECPEN) && (val & PC873_ECPCLK)) {
		mode = PPB_PS2;		/* tolerable alternative */
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
		mode = PPB_EPP;
	    } else {
		if (bootverbose)
		    printf("ECP\n");
		
		/* configure as an ECP port to get bidirectional operation for now */
		outb(idport, PC873_PCR);
		outb(idport + 1, inb(idport + 1) | PC873_ECPEN | PC873_ECPCLK);

		/* we look like a PS/2 port */
		mode = PPB_PS2;
	    }
	}
	return(mode);
    }
    return(0);
}

static int
ppc_detect_ps2(struct ppc_data *ppc)
{
	char save_control, r;

	save_control = r_ctr(ppc);

	/* Try PS/2 mode */
	w_ctr(ppc, 0xec);
	w_dtr(ppc, 0x55);

	/* needed if in ECP mode */
	if (ppc->ppc_mode == PPB_ECP)
		w_ctr(ppc, PCD | 0xec);
	r = r_dtr(ppc);

	if (r != (char) 0xff) {
		if (r != (char) 0x55)
			return 0;

		w_dtr(ppc, 0xaa);
		r = r_dtr(ppc);
		if (r != (char) 0xaa)
			return 0;

		return (PPB_NIBBLE);
	} else
		w_ctr(ppc, save_control);

	return (PPB_PS2);
}

/*
 * ppc_smc37c66xgt_detect
 *
 * SMC FDC37C66xGT configuration.
 */
static int
ppc_smc37c66xgt_detect(struct ppc_data *ppc, int mode)
{
	int s, i;
	char r;
	int retry = 0;		/* boolean */
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
		return (0);

	/* select CR1 */
	outb(csr, 0x1);

	/* read the port's address: bits 0 and 1 of CR1 */
	r = inb(cio) & SMC_CR1_ADDR;
	if (port_address[r] != ppc->ppc_base)
		return (0);

	ppc->ppc_type = type;

	/*
	 * CR1 and CR4 registers bits 3 and 0/1 for mode configuration
	 * If SPP mode is detected, try to set ECP+EPP mode end retry
	 * detection to verify.
	 */

retry:
	/* select CR1 register */
	outb(csr, 0x1);

	if (!mode) {
		/* autodetect mode */

		/* 666GT chipset is hardwired to an extended mode */
		if (type == SMC_37C666GT)
			mode = PPB_ECP_EPP;

		else if ((inb(cio) & SMC_CR1_MODE) == 0) {
			/* already in extended parallel port mode, read CR4 */
			outb(csr, 0x4);
			r = (inb(cio) & SMC_CR4_EMODE);

			switch (r) {
			case SMC_SPP:
				/* let's detect NIBBLE or PS/2 later */
				break;

			case SMC_EPPSPP:
				mode = PPB_EPP;
				break;

			case SMC_ECP:
				/*
				 * Yet, don't know what to do with it! 	XXX
				 * So, consider ECP mode as PS/2.
				 * (see configuration later).
				 */
				mode = PPB_ECP;
				break;

			case SMC_ECPEPP:
				mode = PPB_ECP_EPP;
				break;
			}
		}
	} else {
		/* mode forced */

		/* 666GT chipset is hardwired to an extended mode */
		if (type == SMC_37C666GT)
			goto end_detect;

		r = inb(cio);
		if (mode == PPB_NIBBLE || mode == PPB_PS2) {
			/* do not use ECP when the mode is forced to SPP */
			outb(cio, r | SMC_CR1_MODE);
		} else {
			/* an extended mode is selected */
			outb(cio, r & ~SMC_CR1_MODE);

			/* read CR4 register and reset mode field */
			outb(csr, 0x4);
			r = inb(cio) & ~SMC_CR4_EMODE;

			switch (mode) {
			case PPB_EPP:
				outb(cio, r | SMC_EPPSPP);
				break;

			case PPB_ECP:
			case PPB_ECP_PS2:
				outb(cio, r | SMC_ECP);
				break;

			case PPB_ECP_EPP:
				outb(cio, r | SMC_ECPEPP);
				break;

			default:
				printf("ppc: unknown mode (%d)\n",
					mode);
				return (0);
			}
		}
	}

end_detect:
	if (PPB_IS_EPP(mode)) {
		/* select CR4 */
		outb(csr, 0x4);
		r = inb(cio);

		/*
		 * Set the EPP protocol...
		 * Low=EPP 1.9 (1284 standard) and High=EPP 1.7
		 * ...then check the result.
		 */
		if (ppc->ppc_epp == EPP_1_9)
			outb(cio, (r & ~SMC_CR4_EPPTYPE));

		else
			outb(cio, (r | SMC_CR4_EPPTYPE));
	}

	/* end config mode */
	outb(csr, 0xaa);

	/*
	 * Write 100 to the mode bits and disable DMA, enable intr.
	 */
	if (mode == PPB_ECP_EPP)
		w_ecr(ppc, 0x80);

	/*
	 * Write 001 to the mode bits and disable DMA, enable intr.
	 */
	if (mode == PPB_ECP)
		w_ecr(ppc, 0x20);

	if (PPB_IS_EPP(mode)) {
		/*
		 * Try to reset EPP timeout bit.
		 * If it fails, try PS/2 and NIBBLE modes.
		 */
		ppc_reset_epp_timeout(ppc->ppc_unit);

		r = r_str(ppc);
		if (!(r & TIMEOUT))
			return (mode);
	} else {
		if (mode)
			return (mode);
	}

	/* detect PS/2 or NIBBLE mode */
	return (ppc_detect_ps2(ppc));
}

static int
ppc_check_ecpepp_timeout(struct ppc_data *ppc)
{
	char r;

	ppc_reset_epp_timeout(ppc->ppc_unit);

	r = r_str(ppc);
	if (!(r & TIMEOUT)) {
		return (PPB_ECP_EPP);
	}

	/* If EPP timeout bit is not reset, DON'T use EPP */
	w_ecr(ppc, 0x20);

	return (PPB_ECP_PS2);
}

/*
 * ppc_generic_detect
 */
static int
ppc_generic_detect(struct ppc_data *ppc, int mode)
{
	char save_control, r;

	/* don't know what to do here */
	if (mode)
		return (mode);

	/* try to reset EPP timeout bit */
	ppc_reset_epp_timeout(ppc->ppc_unit);

	r = r_str(ppc);
	if (!(r & TIMEOUT)) {
		return (PPB_EPP);
	}

	/* Now check for ECP */
	w_ecr(ppc, 0x20);
	r = r_ecr(ppc);
	if ((r & 0xe0) == 0x20) {
		/* Search for SMC style EPP+ECP mode */
		w_ecr(ppc, 0x80);

		return (ppc_check_ecpepp_timeout(ppc));
	}

	return (ppc_detect_ps2(ppc));
}

/*
 * ppc_detect()
 *
 * mode is the mode suggested at boot
 */
static int
ppc_detect(struct ppc_data *ppc, int mode) {

	if (!ppc->ppc_mode && (ppc->ppc_mode = ppc_pc873xx_detect(ppc)))
		goto end_detect;

	if (!ppc->ppc_mode && (ppc->ppc_mode =
				ppc_smc37c66xgt_detect(ppc, mode)))
		goto end_detect;

	if (!ppc->ppc_mode && (ppc->ppc_mode = ppc_generic_detect(ppc, mode)))
		goto end_detect;

	printf("ppc: port not present at 0x%x.\n", ppc->ppc_base);
	return (PPC_ENOPORT);

end_detect:

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

	/* PPB_AUTODETECT is default to allow chipset detection even if
	 * mode is forced by dvp->id_flags (see later, ppc_detect() call) */
	ppc->ppc_mode = PPB_AUTODETECT;
	ppc->ppc_epp = (dvp->id_flags & 0x8) >> 3;

	/*
	 * XXX
	 * Try and detect if interrupts are working.
	 */
	if (!(dvp->id_flags & 0x10))
		ppc->ppc_irq = (dvp->id_irq);

	ppcdata[ppc->ppc_unit] = ppc;
	nppc ++;

	/*
	 * Try to detect the chipset and it's mode.
	 */
	if (ppc_detect(ppc, dvp->id_flags & 0x7))
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

	/*
	 * Link the Parallel Port Chipset (adapter) to
	 * the future ppbus.
	 */
	ppc->ppc_link.adapter_unit = ppc->ppc_unit;
	ppc->ppc_link.adapter = &ppc_adapter;

	printf("ppc%d: %s chipset in %s mode%s\n", ppc->ppc_unit,
		ppc_types[ppc->ppc_type], ppc_modes[ppc->ppc_mode],
		(PPB_IS_EPP(ppc->ppc_mode)) ?
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
