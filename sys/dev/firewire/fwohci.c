/*
 * Copyright (c) 1998-2002 Katsushi Kobayashi and Hidetoshi Shimokawa
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the acknowledgement as bellow:
 *
 *    This product includes software developed by K. Kobayashi and H. Shimokawa
 *
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD$
 *
 */

#define ATRQ_CH 0
#define ATRS_CH 1
#define ARRQ_CH 2
#define ARRS_CH 3
#define ITX_CH 4
#define IRX_CH 0x24

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/mbuf.h>
#include <sys/mman.h> 
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/signalvar.h> 
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/sockio.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/conf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/cpufunc.h>            /* for rdtsc proto for clock.h below */
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <vm/vm.h>
#include <vm/vm_extern.h> 
#include <vm/pmap.h>            /* for vtophys proto */

#include <dev/firewire/firewire.h>
#include <dev/firewire/firewirereg.h>
#include <dev/firewire/fwohcireg.h>
#include <dev/firewire/fwohcivar.h>
#include <dev/firewire/firewire_phy.h>

#include <dev/firewire/iec68113.h>

#undef OHCI_DEBUG

static char dbcode[16][0x10]={"OUTM", "OUTL","INPM","INPL",
		"STOR","LOAD","NOP ","STOP",};
static char dbkey[8][0x10]={"ST0", "ST1","ST2","ST3",
		"UNDEF","REG","SYS","DEV"};
char fwohcicode[32][0x20]={
	"No stat","Undef","long","miss Ack err",
	"underrun","overrun","desc err", "data read err",
	"data write err","bus reset","timeout","tcode err",
	"Undef","Undef","unknown event","flushed",
	"Undef","ack complete","ack pend","Undef",
	"ack busy_X","ack busy_A","ack busy_B","Undef",
	"Undef","Undef","Undef","ack tardy",
	"Undef","ack data_err","ack type_err",""};
#define MAX_SPEED 2
extern char linkspeed[MAX_SPEED+1][0x10];
static char dbcond[4][0x10]={"NEV","C=1", "C=0", "ALL"};
u_int32_t tagbit[4] = { 1 << 28, 1 << 29, 1 << 30, 1 << 31};

static struct tcode_info tinfo[] = {
/*		hdr_len block 	flag*/
/* 0 WREQQ  */ {16,	FWTI_REQ | FWTI_TLABEL},
/* 1 WREQB  */ {16,	FWTI_REQ | FWTI_TLABEL | FWTI_BLOCK_ASY},
/* 2 WRES   */ {12,	FWTI_RES},
/* 3 XXX    */ { 0,	0},
/* 4 RREQQ  */ {12,	FWTI_REQ | FWTI_TLABEL},
/* 5 RREQB  */ {16,	FWTI_REQ | FWTI_TLABEL},
/* 6 RRESQ  */ {16,	FWTI_RES},
/* 7 RRESB  */ {16,	FWTI_RES | FWTI_BLOCK_ASY},
/* 8 CYCS   */ { 0,	0},
/* 9 LREQ   */ {16,	FWTI_REQ | FWTI_TLABEL | FWTI_BLOCK_ASY},
/* a STREAM */ { 4,	FWTI_REQ | FWTI_BLOCK_STR},
/* b LRES   */ {16,	FWTI_RES | FWTI_BLOCK_ASY},
/* c XXX    */ { 0,	0},
/* d XXX    */ { 0, 	0},
/* e PHY    */ {12,	FWTI_REQ},
/* f XXX    */ { 0,	0}
};

#define OHCI_WRITE_SIGMASK 0xffff0000
#define OHCI_READ_SIGMASK 0xffff0000

#define OWRITE(sc, r, x) bus_space_write_4((sc)->bst, (sc)->bsh, (r), (x))
#define OREAD(sc, r) bus_space_read_4((sc)->bst, (sc)->bsh, (r))

static void fwohci_ibr __P((struct firewire_comm *));
static void fwohci_db_init __P((struct fwohci_dbch *));
static void fwohci_db_free __P((struct fwohci_dbch *));
static void fwohci_arcv __P((struct fwohci_softc *, struct fwohci_dbch *, int));
static void fwohci_ircv __P((struct fwohci_softc *, struct fwohci_dbch *, int));
static void fwohci_txd __P((struct fwohci_softc *, struct fwohci_dbch *));
static void fwohci_start_atq __P((struct firewire_comm *));
static void fwohci_start_ats __P((struct firewire_comm *));
static void fwohci_start __P((struct fwohci_softc *, struct fwohci_dbch *));
static void fwohci_drain_atq __P((struct firewire_comm *, struct fw_xfer *));
static void fwohci_drain_ats __P((struct firewire_comm *, struct fw_xfer *));
static void fwohci_drain __P((struct firewire_comm *, struct fw_xfer *, struct fwohci_dbch *));
static u_int32_t fwphy_wrdata __P(( struct fwohci_softc *, u_int32_t, u_int32_t));
static u_int32_t fwphy_rddata __P(( struct fwohci_softc *, u_int32_t));
static int fwohci_rx_enable __P((struct fwohci_softc *, struct fwohci_dbch *));
static int fwohci_tx_enable __P((struct fwohci_softc *, struct fwohci_dbch *));
static int fwohci_irx_enable __P((struct firewire_comm *, int));
static int fwohci_irxpp_enable __P((struct firewire_comm *, int));
static int fwohci_irxbuf_enable __P((struct firewire_comm *, int));
static int fwohci_irx_disable __P((struct firewire_comm *, int));
static void fwohci_irx_post __P((struct firewire_comm *, u_int32_t *));
static int fwohci_itxbuf_enable __P((struct firewire_comm *, int));
static int fwohci_itx_disable __P((struct firewire_comm *, int));
static void fwohci_timeout __P((void *));
static void fwohci_poll __P((struct firewire_comm *, int, int));
static void fwohci_set_intr __P((struct firewire_comm *, int));
static int fwohci_add_rx_buf __P((struct fwohcidb_tr *, unsigned short, int, void *, void *));
static int fwohci_add_tx_buf __P((struct fwohcidb_tr *, unsigned short, int, void *));
static void	dump_db __P((struct fwohci_softc *, u_int32_t));
static void 	print_db __P((volatile struct fwohcidb *, u_int32_t , u_int32_t));
static void	dump_dma __P((struct fwohci_softc *, u_int32_t));
static u_int32_t fwohci_cyctimer __P((struct firewire_comm *));
static void fwohci_rbuf_update __P((struct fwohci_softc *, int));
static void fwohci_tbuf_update __P((struct fwohci_softc *, int));
void fwohci_txbufdb __P((struct fwohci_softc *, int , struct fw_bulkxfer *));

/*
 * memory allocated for DMA programs
 */
#define DMA_PROG_ALLOC		(8 * PAGE_SIZE)

/* #define NDB 1024 */
#define NDB FWMAXQUEUE
#define NDVDB (DVBUF * NDB)

#define	OHCI_VERSION		0x00
#define	OHCI_CROMHDR		0x18
#define	OHCI_BUS_OPT		0x20
#define	OHCI_BUSIRMC		(1 << 31)
#define	OHCI_BUSCMC		(1 << 30)
#define	OHCI_BUSISC		(1 << 29)
#define	OHCI_BUSBMC		(1 << 28)
#define	OHCI_BUSPMC		(1 << 27)
#define OHCI_BUSFNC		OHCI_BUSIRMC | OHCI_BUSCMC | OHCI_BUSISC |\
				OHCI_BUSBMC | OHCI_BUSPMC

#define	OHCI_EUID_HI		0x24
#define	OHCI_EUID_LO		0x28

#define	OHCI_CROMPTR		0x34
#define	OHCI_HCCCTL		0x50
#define	OHCI_HCCCTLCLR		0x54
#define	OHCI_AREQHI		0x100
#define	OHCI_AREQHICLR		0x104
#define	OHCI_AREQLO		0x108
#define	OHCI_AREQLOCLR		0x10c
#define	OHCI_PREQHI		0x110
#define	OHCI_PREQHICLR		0x114
#define	OHCI_PREQLO		0x118
#define	OHCI_PREQLOCLR		0x11c
#define	OHCI_PREQUPPER		0x120

#define	OHCI_SID_BUF		0x64
#define	OHCI_SID_CNT		0x68
#define OHCI_SID_CNT_MASK	0xffc

#define	OHCI_IT_STAT		0x90
#define	OHCI_IT_STATCLR		0x94
#define	OHCI_IT_MASK		0x98
#define	OHCI_IT_MASKCLR		0x9c

#define	OHCI_IR_STAT		0xa0
#define	OHCI_IR_STATCLR		0xa4
#define	OHCI_IR_MASK		0xa8
#define	OHCI_IR_MASKCLR		0xac

#define	OHCI_LNKCTL		0xe0
#define	OHCI_LNKCTLCLR		0xe4

#define	OHCI_PHYACCESS		0xec
#define	OHCI_CYCLETIMER		0xf0

#define	OHCI_DMACTL(off)	(off)
#define	OHCI_DMACTLCLR(off)	(off + 4)
#define	OHCI_DMACMD(off)	(off + 0xc)
#define	OHCI_DMAMATCH(off)	(off + 0x10)

#define OHCI_ATQOFF		0x180
#define OHCI_ATQCTL		OHCI_ATQOFF
#define OHCI_ATQCTLCLR		(OHCI_ATQOFF + 4)
#define OHCI_ATQCMD		(OHCI_ATQOFF + 0xc)
#define OHCI_ATQMATCH		(OHCI_ATQOFF + 0x10)

#define OHCI_ATSOFF		0x1a0
#define OHCI_ATSCTL		OHCI_ATSOFF
#define OHCI_ATSCTLCLR		(OHCI_ATSOFF + 4)
#define OHCI_ATSCMD		(OHCI_ATSOFF + 0xc)
#define OHCI_ATSMATCH		(OHCI_ATSOFF + 0x10)

#define OHCI_ARQOFF		0x1c0
#define OHCI_ARQCTL		OHCI_ARQOFF
#define OHCI_ARQCTLCLR		(OHCI_ARQOFF + 4)
#define OHCI_ARQCMD		(OHCI_ARQOFF + 0xc)
#define OHCI_ARQMATCH		(OHCI_ARQOFF + 0x10)

#define OHCI_ARSOFF		0x1e0
#define OHCI_ARSCTL		OHCI_ARSOFF
#define OHCI_ARSCTLCLR		(OHCI_ARSOFF + 4)
#define OHCI_ARSCMD		(OHCI_ARSOFF + 0xc)
#define OHCI_ARSMATCH		(OHCI_ARSOFF + 0x10)

#define OHCI_ITOFF(CH)		(0x200 + 0x10 * (CH))
#define OHCI_ITCTL(CH)		(OHCI_ITOFF(CH))
#define OHCI_ITCTLCLR(CH)	(OHCI_ITOFF(CH) + 4)
#define OHCI_ITCMD(CH)		(OHCI_ITOFF(CH) + 0xc)

#define OHCI_IROFF(CH)		(0x400 + 0x20 * (CH))
#define OHCI_IRCTL(CH)		(OHCI_IROFF(CH))
#define OHCI_IRCTLCLR(CH)	(OHCI_IROFF(CH) + 4)
#define OHCI_IRCMD(CH)		(OHCI_IROFF(CH) + 0xc)
#define OHCI_IRMATCH(CH)	(OHCI_IROFF(CH) + 0x10)

d_ioctl_t fwohci_ioctl;

/*
 * Communication with PHY device
 */
static u_int32_t
fwphy_wrdata( struct fwohci_softc *sc, u_int32_t addr, u_int32_t data)
{
	u_int32_t fun;

	addr &= 0xf;
	data &= 0xff;

	fun = (PHYDEV_WRCMD | (addr << PHYDEV_REGADDR) | (data << PHYDEV_WRDATA));
	OWRITE(sc, OHCI_PHYACCESS, fun);
	DELAY(100);

	return(fwphy_rddata( sc, addr));
}

static u_int32_t
fwohci_set_bus_manager(struct firewire_comm *fc, u_int node)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int i;
	u_int32_t bm;

#define OHCI_CSR_DATA	0x0c
#define OHCI_CSR_COMP	0x10
#define OHCI_CSR_CONT	0x14
#define OHCI_BUS_MANAGER_ID	0

	OWRITE(sc, OHCI_CSR_DATA, node);
	OWRITE(sc, OHCI_CSR_COMP, 0x3f);
	OWRITE(sc, OHCI_CSR_CONT, OHCI_BUS_MANAGER_ID);
 	for (i = 0; !(OREAD(sc, OHCI_CSR_CONT) & (1<<31)) && (i < 1000); i++)
		DELAY(10);
	bm = OREAD(sc, OHCI_CSR_DATA);
	if((bm & 0x3f) == 0x3f)
		bm = node;
	if (bootverbose)
		device_printf(sc->fc.dev,
			"fw_set_bus_manager: %d->%d (loop=%d)\n", bm, node, i);

	return(bm);
}

static u_int32_t
fwphy_rddata(struct fwohci_softc *sc,  u_int addr)
{
	u_int32_t fun, stat;
	u_int i, retry = 0;

	addr &= 0xf;
#define MAX_RETRY 100
again:
	OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_REG_FAIL);
	fun = PHYDEV_RDCMD | (addr << PHYDEV_REGADDR);
	OWRITE(sc, OHCI_PHYACCESS, fun);
	for ( i = 0 ; i < MAX_RETRY ; i ++ ){
		fun = OREAD(sc, OHCI_PHYACCESS);
		if ((fun & PHYDEV_RDCMD) == 0 && (fun & PHYDEV_RDDONE) != 0)
			break;
		DELAY(100);
	}
	if(i >= MAX_RETRY) {
		if (bootverbose)
			device_printf(sc->fc.dev, "phy read failed(1).\n");
		if (++retry < MAX_RETRY) {
			DELAY(100);
			goto again;
		}
	}
	/* Make sure that SCLK is started */
	stat = OREAD(sc, FWOHCI_INTSTAT);
	if ((stat & OHCI_INT_REG_FAIL) != 0 ||
			((fun >> PHYDEV_REGADDR) & 0xf) != addr) {
		if (bootverbose)
			device_printf(sc->fc.dev, "phy read failed(2).\n");
		if (++retry < MAX_RETRY) {
			DELAY(100);
			goto again;
		}
	}
	if (bootverbose || retry >= MAX_RETRY)
		device_printf(sc->fc.dev, 
			"fwphy_rddata: loop=%d, retry=%d\n", i, retry);
#undef MAX_RETRY
	return((fun >> PHYDEV_RDDATA )& 0xff);
}
/* Device specific ioctl. */
int
fwohci_ioctl (dev_t dev, u_long cmd, caddr_t data, int flag, fw_proc *td)
{
	struct firewire_softc *sc;
	struct fwohci_softc *fc;
	int unit = DEV2UNIT(dev);
	int err = 0;
	struct fw_reg_req_t *reg  = (struct fw_reg_req_t *) data;
	u_int32_t *dmach = (u_int32_t *) data;

	sc = devclass_get_softc(firewire_devclass, unit);
	if(sc == NULL){
		return(EINVAL);
	}
	fc = (struct fwohci_softc *)sc->fc;

	if (!data)
		return(EINVAL);

	switch (cmd) {
	case FWOHCI_WRREG:
#define OHCI_MAX_REG 0x800
		if(reg->addr <= OHCI_MAX_REG){
			OWRITE(fc, reg->addr, reg->data);
			reg->data = OREAD(fc, reg->addr);
		}else{
			err = EINVAL;
		}
		break;
	case FWOHCI_RDREG:
		if(reg->addr <= OHCI_MAX_REG){
			reg->data = OREAD(fc, reg->addr);
		}else{
			err = EINVAL;
		}
		break;
/* Read DMA descriptors for debug  */
	case DUMPDMA:
		if(*dmach <= OHCI_MAX_DMA_CH ){
			dump_dma(fc, *dmach);
			dump_db(fc, *dmach);
		}else{
			err = EINVAL;
		}
		break;
	default:
		break;
	}
	return err;
}

static int
fwohci_probe_phy(struct fwohci_softc *sc, device_t dev)
{
	u_int32_t reg, reg2;
	int e1394a = 1;
/*
 * probe PHY parameters
 * 0. to prove PHY version, whether compliance of 1394a.
 * 1. to probe maximum speed supported by the PHY and 
 *    number of port supported by core-logic.
 *    It is not actually available port on your PC .
 */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LPS);
#if 0
	/* XXX wait for SCLK. */
	DELAY(100000);
#endif
	reg = fwphy_rddata(sc, FW_PHY_SPD_REG);

	if((reg >> 5) != 7 ){
		sc->fc.mode &= ~FWPHYASYST;
		sc->fc.nport = reg & FW_PHY_NP;
		sc->fc.speed = reg & FW_PHY_SPD >> 6;
		if (sc->fc.speed > MAX_SPEED) {
			device_printf(dev, "invalid speed %d (fixed to %d).\n",
				sc->fc.speed, MAX_SPEED);
			sc->fc.speed = MAX_SPEED;
		}
		device_printf(dev,
			"Phy 1394 only %s, %d ports.\n",
			linkspeed[sc->fc.speed], sc->fc.nport);
	}else{
		reg2 = fwphy_rddata(sc, FW_PHY_ESPD_REG);
		sc->fc.mode |= FWPHYASYST;
		sc->fc.nport = reg & FW_PHY_NP;
		sc->fc.speed = (reg2 & FW_PHY_ESPD) >> 5;
		if (sc->fc.speed > MAX_SPEED) {
			device_printf(dev, "invalid speed %d (fixed to %d).\n",
				sc->fc.speed, MAX_SPEED);
			sc->fc.speed = MAX_SPEED;
		}
		device_printf(dev,
			"Phy 1394a available %s, %d ports.\n",
			linkspeed[sc->fc.speed], sc->fc.nport);

		/* check programPhyEnable */
		reg2 = fwphy_rddata(sc, 5);
#if 0
		if (e1394a && (OREAD(sc, OHCI_HCCCTL) & OHCI_HCC_PRPHY)) {
#else	/* XXX force to enable 1394a */
		if (e1394a) {
#endif
			if (bootverbose)
				device_printf(dev,
					"Enable 1394a Enhancements\n");
			/* enable EAA EMC */
			reg2 |= 0x03;
			/* set aPhyEnhanceEnable */
			OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_PHYEN);
			OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_PRPHY);
		} else {
			/* for safe */
			reg2 &= ~0x83;
		}
		reg2 = fwphy_wrdata(sc, 5, reg2);
	}

	reg = fwphy_rddata(sc, FW_PHY_SPD_REG);
	if((reg >> 5) == 7 ){
		reg = fwphy_rddata(sc, 4);
		reg |= 1 << 6;
		fwphy_wrdata(sc, 4, reg);
		reg = fwphy_rddata(sc, 4);
	}
	return 0;
}


void
fwohci_reset(struct fwohci_softc *sc, device_t dev)
{
	int i, max_rec, speed;
	u_int32_t reg, reg2;
	struct fwohcidb_tr *db_tr;

	/* Disable interrupt */ 
	OWRITE(sc, FWOHCI_INTMASKCLR, ~0);

	/* Now stopping all DMA channel */
	OWRITE(sc,  OHCI_ARQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ARSCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);

	OWRITE(sc,  OHCI_IR_MASKCLR, ~0);
	for( i = 0 ; i < sc->fc.nisodma ; i ++ ){
		OWRITE(sc,  OHCI_IRCTLCLR(i), OHCI_CNTL_DMA_RUN);
		OWRITE(sc,  OHCI_ITCTLCLR(i), OHCI_CNTL_DMA_RUN);
	}

	/* FLUSH FIFO and reset Transmitter/Reciever */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_RESET);
	if (bootverbose)
		device_printf(dev, "resetting OHCI...");
	i = 0;
	while(OREAD(sc, OHCI_HCCCTL) & OHCI_HCC_RESET) {
		if (i++ > 100) break;
		DELAY(1000);
	}
	if (bootverbose)
		printf("done (loop=%d)\n", i);

	/* Probe phy */
	fwohci_probe_phy(sc, dev);

	/* Probe link */
	reg = OREAD(sc,  OHCI_BUS_OPT);
	reg2 = reg | OHCI_BUSFNC;
	max_rec = (reg & 0x0000f000) >> 12;
	speed = (reg & 0x00000007);
	device_printf(dev, "Link %s, max_rec %d bytes.\n",
			linkspeed[speed], MAXREC(max_rec));
	/* XXX fix max_rec */
	sc->fc.maxrec = sc->fc.speed + 8;
	if (max_rec != sc->fc.maxrec) {
		reg2 = (reg2 & 0xffff0fff) | (sc->fc.maxrec << 12);
		device_printf(dev, "max_rec %d -> %d\n",
				MAXREC(max_rec), MAXREC(sc->fc.maxrec));
	}
	if (bootverbose)
		device_printf(dev, "BUS_OPT 0x%x -> 0x%x\n", reg, reg2);
	OWRITE(sc,  OHCI_BUS_OPT, reg2);

	/* Initialize registers */
	OWRITE(sc, OHCI_CROMHDR, sc->fc.config_rom[0]);
	OWRITE(sc, OHCI_CROMPTR, vtophys(&sc->fc.config_rom[0]));
	OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_BIGEND);
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_POSTWR);
	OWRITE(sc, OHCI_SID_BUF, vtophys(sc->fc.sid_buf));
	OWRITE(sc, OHCI_LNKCTL, OHCI_CNTL_SID);
	fw_busreset(&sc->fc);

	/* Enable link */
	OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LINKEN);

	/* Force to start async RX DMA */
	sc->arrq.xferq.flag &= ~FWXFERQ_RUNNING;
	sc->arrs.xferq.flag &= ~FWXFERQ_RUNNING;
	fwohci_rx_enable(sc, &sc->arrq);
	fwohci_rx_enable(sc, &sc->arrs);

	/* Initialize async TX */
	OWRITE(sc, OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN | OHCI_CNTL_DMA_DEAD);
	OWRITE(sc, OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN | OHCI_CNTL_DMA_DEAD);
	/* AT Retries */
	OWRITE(sc, FWOHCI_RETRY,
		/* CycleLimit   PhyRespRetries ATRespRetries ATReqRetries */
		(0xffff << 16 ) | (0x0f << 8) | (0x0f << 4) | 0x0f) ;
	for( i = 0, db_tr = sc->atrq.top; i < sc->atrq.ndb ;
				i ++, db_tr = STAILQ_NEXT(db_tr, link)){
		db_tr->xfer = NULL;
	}
	for( i = 0, db_tr = sc->atrs.top; i < sc->atrs.ndb ;
				i ++, db_tr = STAILQ_NEXT(db_tr, link)){
		db_tr->xfer = NULL;
	}


	/* Enable interrupt */
	OWRITE(sc, FWOHCI_INTMASK,
			OHCI_INT_ERR  | OHCI_INT_PHY_SID 
			| OHCI_INT_DMA_ATRQ | OHCI_INT_DMA_ATRS 
			| OHCI_INT_DMA_PRRQ | OHCI_INT_DMA_PRRS
			| OHCI_INT_PHY_BUS_R | OHCI_INT_PW_ERR);
	fwohci_set_intr(&sc->fc, 1);

}

int
fwohci_init(struct fwohci_softc *sc, device_t dev)
{
	int i;
	u_int32_t reg;

	reg = OREAD(sc, OHCI_VERSION);
	device_printf(dev, "OHCI version %x.%x (ROM=%d)\n",
			(reg>>16) & 0xff, reg & 0xff, (reg>>24) & 1);

/* XXX: Available Isochrounous DMA channel probe */
	for( i = 0 ; i < 0x20 ; i ++ ){
		OWRITE(sc,  OHCI_IRCTL(i), OHCI_CNTL_DMA_RUN);
		reg = OREAD(sc, OHCI_IRCTL(i));
		if(!(reg & OHCI_CNTL_DMA_RUN)) break;
		OWRITE(sc,  OHCI_ITCTL(i), OHCI_CNTL_DMA_RUN);
		reg = OREAD(sc, OHCI_ITCTL(i));
		if(!(reg & OHCI_CNTL_DMA_RUN)) break;
	}
	sc->fc.nisodma = i;
	device_printf(dev, "No. of Isochronous channel is %d.\n", i);

	sc->fc.arq = &sc->arrq.xferq;
	sc->fc.ars = &sc->arrs.xferq;
	sc->fc.atq = &sc->atrq.xferq;
	sc->fc.ats = &sc->atrs.xferq;

	sc->arrq.xferq.start = NULL;
	sc->arrs.xferq.start = NULL;
	sc->atrq.xferq.start = fwohci_start_atq;
	sc->atrs.xferq.start = fwohci_start_ats;

	sc->arrq.xferq.drain = NULL;
	sc->arrs.xferq.drain = NULL;
	sc->atrq.xferq.drain = fwohci_drain_atq;
	sc->atrs.xferq.drain = fwohci_drain_ats;

	sc->arrq.ndesc = 1;
	sc->arrs.ndesc = 1;
	sc->atrq.ndesc = 6;	/* equal to maximum of mbuf chains */
	sc->atrs.ndesc = 6 / 2;

	sc->arrq.ndb = NDB;
	sc->arrs.ndb = NDB / 2;
	sc->atrq.ndb = NDB;
	sc->atrs.ndb = NDB / 2;

	sc->arrq.dummy = NULL;
	sc->arrs.dummy = NULL;
	sc->atrq.dummy = NULL;
	sc->atrs.dummy = NULL;
	for( i = 0 ; i < sc->fc.nisodma ; i ++ ){
		sc->fc.it[i] = &sc->it[i].xferq;
		sc->fc.ir[i] = &sc->ir[i].xferq;
		sc->it[i].ndb = 0;
		sc->ir[i].ndb = 0;
	}

	sc->fc.tcode = tinfo;

	sc->cromptr = (u_int32_t *)
		contigmalloc(CROMSIZE * 2, M_DEVBUF, M_NOWAIT, 0, ~0, 1<<10, 0);

	if(sc->cromptr == NULL){
		device_printf(dev, "cromptr alloc failed.");
		return ENOMEM;
	}
	sc->fc.dev = dev;
	sc->fc.config_rom = &(sc->cromptr[CROMSIZE/4]);

	sc->fc.config_rom[1] = 0x31333934;
	sc->fc.config_rom[2] = 0xf000a002;
	sc->fc.config_rom[3] = OREAD(sc, OHCI_EUID_HI);
	sc->fc.config_rom[4] = OREAD(sc, OHCI_EUID_LO);
	sc->fc.config_rom[5] = 0;
	sc->fc.config_rom[0] = (4 << 24) | (5 << 16);

	sc->fc.config_rom[0] |= fw_crc16(&sc->fc.config_rom[1], 5*4);


/* SID recieve buffer must allign 2^11 */
#define	OHCI_SIDSIZE	(1 << 11)
	sc->fc.sid_buf = (u_int32_t *) vm_page_alloc_contig( OHCI_SIDSIZE,
					0x10000, 0xffffffff, OHCI_SIDSIZE);
	if (sc->fc.sid_buf == NULL) {
		device_printf(dev, "sid_buf alloc failed.\n");
		return ENOMEM;
	}

	
	fwohci_db_init(&sc->arrq);
	if ((sc->arrq.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(&sc->arrs);
	if ((sc->arrs.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(&sc->atrq);
	if ((sc->atrq.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	fwohci_db_init(&sc->atrs);
	if ((sc->atrs.flags & FWOHCI_DBCH_INIT) == 0)
		return ENOMEM;

	reg = OREAD(sc, FWOHCIGUID_H);
	for( i = 0 ; i < 4 ; i ++){
		sc->fc.eui[3 - i] = reg & 0xff;
		reg = reg >> 8;
	}
	reg = OREAD(sc, FWOHCIGUID_L);
	for( i = 0 ; i < 4 ; i ++){
		sc->fc.eui[7 - i] = reg & 0xff;
		reg = reg >> 8;
	}
	device_printf(dev, "EUI64 %02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x\n",
		sc->fc.eui[0], sc->fc.eui[1], sc->fc.eui[2], sc->fc.eui[3],
		sc->fc.eui[4], sc->fc.eui[5], sc->fc.eui[6], sc->fc.eui[7]);
	sc->fc.ioctl = fwohci_ioctl;
	sc->fc.cyctimer = fwohci_cyctimer;
	sc->fc.set_bmr = fwohci_set_bus_manager;
	sc->fc.ibr = fwohci_ibr;
	sc->fc.irx_enable = fwohci_irx_enable;
	sc->fc.irx_disable = fwohci_irx_disable;

	sc->fc.itx_enable = fwohci_itxbuf_enable;
	sc->fc.itx_disable = fwohci_itx_disable;
	sc->fc.irx_post = fwohci_irx_post;
	sc->fc.itx_post = NULL;
	sc->fc.timeout = fwohci_timeout;
	sc->fc.poll = fwohci_poll;
	sc->fc.set_intr = fwohci_set_intr;

	fw_init(&sc->fc);
	fwohci_reset(sc, dev);

	return 0;
}

void
fwohci_timeout(void *arg)
{
	struct fwohci_softc *sc;

	sc = (struct fwohci_softc *)arg;
	sc->fc.timeouthandle = timeout(fwohci_timeout,
				(void *)sc, FW_XFERTIMEOUT * hz * 10);
}

u_int32_t
fwohci_cyctimer(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	return(OREAD(sc, OHCI_CYCLETIMER));
}

int
fwohci_detach(struct fwohci_softc *sc, device_t dev)
{
	int i;

	if (sc->fc.sid_buf != NULL)
		contigfree((void *)(uintptr_t)sc->fc.sid_buf,
					OHCI_SIDSIZE, M_DEVBUF);
	if (sc->cromptr != NULL)
		contigfree((void *)sc->cromptr, CROMSIZE * 2, M_DEVBUF);

	fwohci_db_free(&sc->arrq);
	fwohci_db_free(&sc->arrs);

	fwohci_db_free(&sc->atrq);
	fwohci_db_free(&sc->atrs);

	for( i = 0 ; i < sc->fc.nisodma ; i ++ ){
		fwohci_db_free(&sc->it[i]);
		fwohci_db_free(&sc->ir[i]);
	}

	return 0;
}

#define LAST_DB(dbtr, db) do {						\
	struct fwohcidb_tr *_dbtr = (dbtr);				\
	int _cnt = _dbtr->dbcnt;					\
	db = &_dbtr->db[ (_cnt > 2) ? (_cnt -1) : 0];			\
} while (0)
	
static void
fwohci_start(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int i, s;
	int tcode, hdr_len, hdr_off, len;
	int fsegment = -1;
	u_int32_t off;
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	volatile struct fwohci_txpkthdr *ohcifp;
	struct fwohcidb_tr *db_tr;
	volatile struct fwohcidb *db;
	struct mbuf *m;
	struct tcode_info *info;
	static int maxdesc=0;

	if(&sc->atrq == dbch){
		off = OHCI_ATQOFF;
	}else if(&sc->atrs == dbch){
		off = OHCI_ATSOFF;
	}else{
		return;
	}

	if (dbch->flags & FWOHCI_DBCH_FULL)
		return;

	s = splfw();
	db_tr = dbch->top;
txloop:
	xfer = STAILQ_FIRST(&dbch->xferq.q);
	if(xfer == NULL){
		goto kick;
	}
	if(dbch->xferq.queued == 0 ){
		device_printf(sc->fc.dev, "TX queue empty\n");
	}
	STAILQ_REMOVE_HEAD(&dbch->xferq.q, link);
	db_tr->xfer = xfer;
	xfer->state = FWXF_START;
	dbch->xferq.packets++;

	fp = (struct fw_pkt *)(xfer->send.buf + xfer->send.off);
	tcode = fp->mode.common.tcode;

	ohcifp = (volatile struct fwohci_txpkthdr *) db_tr->db[1].db.immed;
	info = &tinfo[tcode];
	hdr_len = hdr_off = info->hdr_len;
	/* fw_asyreq must pass valid send.len */
	len = xfer->send.len;
	for( i = 0 ; i < hdr_off ; i+= 4){
		ohcifp->mode.ld[i/4] = ntohl(fp->mode.ld[i/4]);
	}
	ohcifp->mode.common.spd = xfer->spd;
	if (tcode == FWTCODE_STREAM ){
		hdr_len = 8;
		ohcifp->mode.stream.len = ntohs(fp->mode.stream.len);
	} else if (tcode == FWTCODE_PHY) {
		hdr_len = 12;
		ohcifp->mode.ld[1] = ntohl(fp->mode.ld[1]);
		ohcifp->mode.ld[2] = ntohl(fp->mode.ld[2]);
		ohcifp->mode.common.spd = 0;
		ohcifp->mode.common.tcode = FWOHCITCODE_PHY;
	} else {
		ohcifp->mode.asycomm.dst = ntohs(fp->mode.hdr.dst);
		ohcifp->mode.asycomm.srcbus = OHCI_ASYSRCBUS;
		ohcifp->mode.asycomm.tlrt |= FWRETRY_X;
	}
	db = &db_tr->db[0];
 	db->db.desc.cmd = OHCI_OUTPUT_MORE | OHCI_KEY_ST2 | hdr_len;
 	db->db.desc.status = 0;
/* Specify bound timer of asy. responce */
	if(&sc->atrs == dbch){
 		db->db.desc.count
			 = (OREAD(sc, OHCI_CYCLETIMER) >> 12) + (1 << 13);
	}

	db_tr->dbcnt = 2;
	db = &db_tr->db[db_tr->dbcnt];
	if(len > hdr_off){
		if (xfer->mbuf == NULL) {
			db->db.desc.addr
				= vtophys(xfer->send.buf + xfer->send.off) + hdr_off;
			db->db.desc.cmd
				= OHCI_OUTPUT_MORE | ((len - hdr_off) & 0xffff);
 			db->db.desc.status = 0;

			db_tr->dbcnt++;
		} else {
			/* XXX we assume mbuf chain is shorter than ndesc */
			for (m = xfer->mbuf; m != NULL; m = m->m_next) {
				if (m->m_len == 0)
					/* unrecoverable error could ocurre. */
					continue;
				if (db_tr->dbcnt >= dbch->ndesc) {
					device_printf(sc->fc.dev,
						"dbch->ndesc is too small"
						", trancated.\n");
					break;
				}
				db->db.desc.addr
					= vtophys(mtod(m, caddr_t));
				db->db.desc.cmd = OHCI_OUTPUT_MORE | m->m_len;
 				db->db.desc.status = 0;
				db++;
				db_tr->dbcnt++;
			}
		}
	}
	if (maxdesc < db_tr->dbcnt) {
		maxdesc = db_tr->dbcnt;
		if (bootverbose)
			device_printf(sc->fc.dev, "maxdesc: %d\n", maxdesc);
	}
	/* last db */
	LAST_DB(db_tr, db);
 	db->db.desc.cmd |= OHCI_OUTPUT_LAST
			| OHCI_INTERRUPT_ALWAYS
			| OHCI_BRANCH_ALWAYS;
 	db->db.desc.depend = vtophys(STAILQ_NEXT(db_tr, link)->db);

	if(fsegment == -1 )
		fsegment = db_tr->dbcnt;
	if (dbch->pdb_tr != NULL) {
		LAST_DB(dbch->pdb_tr, db);
 		db->db.desc.depend |= db_tr->dbcnt;
	}
	dbch->pdb_tr = db_tr;
	db_tr = STAILQ_NEXT(db_tr, link);
	if(db_tr != dbch->bottom){
		goto txloop;
	} else {
		device_printf(sc->fc.dev, "fwohci_start: lack of db_trq\n");
		dbch->flags |= FWOHCI_DBCH_FULL;
	}
kick:
	if (firewire_debug) printf("kick\n");
	/* kick asy q */

	if(dbch->xferq.flag & FWXFERQ_RUNNING) {
		OWRITE(sc, OHCI_DMACTL(off), OHCI_CNTL_DMA_WAKE);
	} else {
		if (bootverbose)
			device_printf(sc->fc.dev, "start AT DMA status=%x\n",
					OREAD(sc, OHCI_DMACTL(off)));
		OWRITE(sc, OHCI_DMACMD(off), vtophys(dbch->top->db) | fsegment);
		OWRITE(sc, OHCI_DMACTL(off), OHCI_CNTL_DMA_RUN);
		dbch->xferq.flag |= FWXFERQ_RUNNING;
	}

	dbch->top = db_tr;
	splx(s);
	return;
}

static void
fwohci_drain_atq(struct firewire_comm *fc, struct fw_xfer *xfer)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	fwohci_drain(&sc->fc, xfer, &(sc->atrq));
	return;
}

static void
fwohci_drain_ats(struct firewire_comm *fc, struct fw_xfer *xfer)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	fwohci_drain(&sc->fc, xfer, &(sc->atrs));
	return;
}

static void
fwohci_start_atq(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	fwohci_start( sc, &(sc->atrq));
	return;
}

static void
fwohci_start_ats(struct firewire_comm *fc)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	fwohci_start( sc, &(sc->atrs));
	return;
}

void
fwohci_txd(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int s, err = 0;
	struct fwohcidb_tr *tr;
	volatile struct fwohcidb *db;
	struct fw_xfer *xfer;
	u_int32_t off;
	u_int stat;
	int	packets;
	struct firewire_comm *fc = (struct firewire_comm *)sc;
	if(&sc->atrq == dbch){
		off = OHCI_ATQOFF;
	}else if(&sc->atrs == dbch){
		off = OHCI_ATSOFF;
	}else{
		return;
	}
	s = splfw();
	tr = dbch->bottom;
	packets = 0;
	while(dbch->xferq.queued > 0){
		LAST_DB(tr, db);
		if(!(db->db.desc.status & OHCI_CNTL_DMA_ACTIVE)){
			if (fc->status != FWBUSRESET) 
				/* maybe out of order?? */
				goto out;
		}
		if(db->db.desc.status & OHCI_CNTL_DMA_DEAD) {
#ifdef OHCI_DEBUG
			dump_dma(sc, ch);
			dump_db(sc, ch);
#endif
/* Stop DMA */
			OWRITE(sc, OHCI_DMACTLCLR(off), OHCI_CNTL_DMA_RUN);
			device_printf(sc->fc.dev, "force reset AT FIFO\n");
			OWRITE(sc, OHCI_HCCCTLCLR, OHCI_HCC_LINKEN);
			OWRITE(sc, OHCI_HCCCTL, OHCI_HCC_LPS | OHCI_HCC_LINKEN);
			OWRITE(sc, OHCI_DMACTLCLR(off), OHCI_CNTL_DMA_RUN);
		}
		stat = db->db.desc.status & FWOHCIEV_MASK;
		switch(stat){
		case FWOHCIEV_ACKCOMPL:
		case FWOHCIEV_ACKPEND:
			err = 0;
			break;
		case FWOHCIEV_ACKBSA:
		case FWOHCIEV_ACKBSB:
			device_printf(sc->fc.dev, "txd err=%2x %s\n", stat, fwohcicode[stat]);
		case FWOHCIEV_ACKBSX:
			err = EBUSY;
			break;
		case FWOHCIEV_FLUSHED:
		case FWOHCIEV_ACKTARD:
			device_printf(sc->fc.dev, "txd err=%2x %s\n", stat, fwohcicode[stat]);
			err = EAGAIN;
			break;
		case FWOHCIEV_MISSACK:
		case FWOHCIEV_UNDRRUN:
		case FWOHCIEV_OVRRUN:
		case FWOHCIEV_DESCERR:
		case FWOHCIEV_DTRDERR:
		case FWOHCIEV_TIMEOUT:
		case FWOHCIEV_TCODERR:
		case FWOHCIEV_UNKNOWN:
		case FWOHCIEV_ACKDERR:
		case FWOHCIEV_ACKTERR:
		default:
			device_printf(sc->fc.dev, "txd err=%2x %s\n",
							stat, fwohcicode[stat]);
			err = EINVAL;
			break;
		}
		if(tr->xfer != NULL){
			xfer = tr->xfer;
			xfer->state = FWXF_SENT;
			if(err == EBUSY && fc->status != FWBUSRESET){
				xfer->state = FWXF_BUSY;
				switch(xfer->act_type){
				case FWACT_XFER:
					xfer->resp = err;
					if(xfer->retry_req != NULL){
						xfer->retry_req(xfer);
					}
					break;
				default:
					break;
				}
			} else if( stat != FWOHCIEV_ACKPEND){
				if (stat != FWOHCIEV_ACKCOMPL)
					xfer->state = FWXF_SENTERR;
				xfer->resp = err;
				switch(xfer->act_type){
				case FWACT_XFER:
					fw_xfer_done(xfer);
					break;
				default:
					break;
				}
			}
			dbch->xferq.queued --;
		}
		tr->xfer = NULL;

		packets ++;
		tr = STAILQ_NEXT(tr, link);
		dbch->bottom = tr;
	}
out:
	if ((dbch->flags & FWOHCI_DBCH_FULL) && packets > 0) {
		printf("make free slot\n");
		dbch->flags &= ~FWOHCI_DBCH_FULL;
		fwohci_start(sc, dbch);
	}
	splx(s);
}

static void
fwohci_drain(struct firewire_comm *fc, struct fw_xfer *xfer, struct fwohci_dbch *dbch)
{
	int i, s;
	struct fwohcidb_tr *tr;

	if(xfer->state != FWXF_START) return;

	s = splfw();
	tr = dbch->bottom;
	for( i = 0 ; i <= dbch->xferq.queued  ; i ++){
		if(tr->xfer == xfer){
			s = splfw();
			tr->xfer = NULL;
			dbch->xferq.queued --;
#if 1
			/* XXX */
			if (tr == dbch->bottom)
				dbch->bottom = STAILQ_NEXT(tr, link);
#endif
			if (dbch->flags & FWOHCI_DBCH_FULL) {
				printf("fwohci_drain: make slot\n");
				dbch->flags &= ~FWOHCI_DBCH_FULL;
				fwohci_start((struct fwohci_softc *)fc, dbch);
			}
				
			splx(s);
			break;
		}
		tr = STAILQ_NEXT(tr, link);
	}
	splx(s);
	return;
}

static void
fwohci_db_free(struct fwohci_dbch *dbch)
{
	struct fwohcidb_tr *db_tr;
	int idb;

	if ((dbch->flags & FWOHCI_DBCH_INIT) == 0)
		return;

	if(!(dbch->xferq.flag & FWXFERQ_EXTBUF)){
		for(db_tr = STAILQ_FIRST(&dbch->db_trq), idb = 0;
			idb < dbch->ndb;
			db_tr = STAILQ_NEXT(db_tr, link), idb++){
			if (db_tr->buf != NULL) {
				free(db_tr->buf, M_DEVBUF);
				db_tr->buf = NULL;
			}
		}
	}
	dbch->ndb = 0;
	db_tr = STAILQ_FIRST(&dbch->db_trq);
	contigfree((void *)(uintptr_t)(volatile void *)db_tr->db,
		sizeof(struct fwohcidb) * dbch->ndesc * dbch->ndb, M_DEVBUF);
	free(db_tr, M_DEVBUF);
	STAILQ_INIT(&dbch->db_trq);
	dbch->flags &= ~FWOHCI_DBCH_INIT;
}

static void
fwohci_db_init(struct fwohci_dbch *dbch)
{
	int	idb;
	struct fwohcidb *db;
	struct fwohcidb_tr *db_tr;


	if ((dbch->flags & FWOHCI_DBCH_INIT) != 0)
		goto out;

	/* allocate DB entries and attach one to each DMA channels */
	/* DB entry must start at 16 bytes bounary. */
	STAILQ_INIT(&dbch->db_trq);
	db_tr = (struct fwohcidb_tr *)
		malloc(sizeof(struct fwohcidb_tr) * dbch->ndb,
		M_DEVBUF, M_DONTWAIT | M_ZERO);
	if(db_tr == NULL){
		printf("fwohci_db_init: malloc failed\n");
		return;
	}
	db = (struct fwohcidb *)
		contigmalloc(sizeof (struct fwohcidb) * dbch->ndesc * dbch->ndb,
		M_DEVBUF, M_DONTWAIT, 0x10000, 0xffffffff, PAGE_SIZE, 0ul);
	if(db == NULL){
		printf("fwohci_db_init: contigmalloc failed\n");
		free(db_tr, M_DEVBUF);
		return;
	}
	bzero(db, sizeof (struct fwohcidb) * dbch->ndesc * dbch->ndb);
	/* Attach DB to DMA ch. */
	for(idb = 0 ; idb < dbch->ndb ; idb++){
		db_tr->dbcnt = 0;
		db_tr->db = &db[idb * dbch->ndesc];
		STAILQ_INSERT_TAIL(&dbch->db_trq, db_tr, link);
		if (!(dbch->xferq.flag & FWXFERQ_PACKET) &&
					dbch->xferq.bnpacket != 0) {
			if (idb % dbch->xferq.bnpacket == 0)
				dbch->xferq.bulkxfer[idb / dbch->xferq.bnpacket
						].start = (caddr_t)db_tr;
			if ((idb + 1) % dbch->xferq.bnpacket == 0)
				dbch->xferq.bulkxfer[idb / dbch->xferq.bnpacket
						].end = (caddr_t)db_tr;
		}
		db_tr++;
	}
	STAILQ_LAST(&dbch->db_trq, fwohcidb_tr,link)->link.stqe_next
			= STAILQ_FIRST(&dbch->db_trq);
out:
	dbch->frag.buf = NULL;
	dbch->frag.len = 0;
	dbch->frag.plen = 0;
	dbch->xferq.queued = 0;
	dbch->pdb_tr = NULL;
	dbch->top = STAILQ_FIRST(&dbch->db_trq);
	dbch->bottom = dbch->top;
	dbch->flags = FWOHCI_DBCH_INIT;
}

static int
fwohci_itx_disable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	OWRITE(sc, OHCI_ITCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_IT_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IT_STATCLR, 1 << dmach);
	fwohci_db_free(&sc->it[dmach]);
	sc->it[dmach].xferq.flag &= ~FWXFERQ_RUNNING;
	return 0;
}

static int
fwohci_irx_disable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;

	OWRITE(sc, OHCI_IRCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
	OWRITE(sc, OHCI_IR_MASKCLR, 1 << dmach);
	OWRITE(sc, OHCI_IR_STATCLR, 1 << dmach);
	if(sc->ir[dmach].dummy != NULL){
		free(sc->ir[dmach].dummy, M_DEVBUF);
	}
	sc->ir[dmach].dummy = NULL;
	fwohci_db_free(&sc->ir[dmach]);
	sc->ir[dmach].xferq.flag &= ~FWXFERQ_RUNNING;
	return 0;
}

static void
fwohci_irx_post (struct firewire_comm *fc , u_int32_t *qld)
{
	qld[0] = ntohl(qld[0]);
	return;
}

static int
fwohci_irxpp_enable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int err = 0;
	unsigned short tag, ich;

	tag = (sc->ir[dmach].xferq.flag >> 6) & 3;
	ich = sc->ir[dmach].xferq.flag & 0x3f;

#if 0
	if(STAILQ_FIRST(&fc->ir[dmach]->q) != NULL){
		wakeup(fc->ir[dmach]);
		return err;
	}
#endif

	OWRITE(sc, OHCI_IRMATCH(dmach), tagbit[tag] | ich);
	if(!(sc->ir[dmach].xferq.flag & FWXFERQ_RUNNING)){
		sc->ir[dmach].xferq.queued = 0;
		sc->ir[dmach].ndb = NDB;
		sc->ir[dmach].xferq.psize = FWPMAX_S400;
		sc->ir[dmach].ndesc = 1;
		fwohci_db_init(&sc->ir[dmach]);
		if ((sc->ir[dmach].flags & FWOHCI_DBCH_INIT) == 0)
			return ENOMEM;
		err = fwohci_rx_enable(sc, &sc->ir[dmach]);
	}
	if(err){
		device_printf(sc->fc.dev, "err in IRX setting\n");
		return err;
	}
	if(!(OREAD(sc, OHCI_IRCTL(dmach)) & OHCI_CNTL_DMA_ACTIVE)){
		OWRITE(sc, OHCI_IRCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
		OWRITE(sc, OHCI_IR_MASKCLR, 1 << dmach);
		OWRITE(sc, OHCI_IR_STATCLR, 1 << dmach);
		OWRITE(sc, OHCI_IR_MASK, 1 << dmach);
		OWRITE(sc, OHCI_IRCTLCLR(dmach), 0xf8000000);
		OWRITE(sc, OHCI_IRCTL(dmach), OHCI_CNTL_ISOHDR);
		OWRITE(sc, OHCI_IRCMD(dmach),
			vtophys(sc->ir[dmach].top->db) | 1);
		OWRITE(sc, OHCI_IRCTL(dmach), OHCI_CNTL_DMA_RUN);
		OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_DMA_IR);
	}
	return err;
}

static int
fwohci_tx_enable(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int err = 0;
	int idb, z, i, dmach = 0;
	u_int32_t off = NULL;
	struct fwohcidb_tr *db_tr;

	if(!(dbch->xferq.flag & FWXFERQ_EXTBUF)){
		err = EINVAL;
		return err;
	}
	z = dbch->ndesc;
	for(dmach = 0 ; dmach < sc->fc.nisodma ; dmach++){
		if( &sc->it[dmach] == dbch){
			off = OHCI_ITOFF(dmach);
			break;
		}
	}
	if(off == NULL){
		err = EINVAL;
		return err;
	}
	if(dbch->xferq.flag & FWXFERQ_RUNNING)
		return err;
	dbch->xferq.flag |= FWXFERQ_RUNNING;
	for( i = 0, dbch->bottom = dbch->top; i < (dbch->ndb - 1); i++){
		dbch->bottom = STAILQ_NEXT(dbch->bottom, link);
	}
	db_tr = dbch->top;
	for( idb = 0 ; idb < dbch->ndb ; idb ++){
		fwohci_add_tx_buf(db_tr,
			dbch->xferq.psize, dbch->xferq.flag,
			dbch->xferq.buf + dbch->xferq.psize * idb);
		if(STAILQ_NEXT(db_tr, link) == NULL){
			break;
		}
		db_tr->db[0].db.desc.depend
			= vtophys(STAILQ_NEXT(db_tr, link)->db) | z;
		db_tr->db[db_tr->dbcnt - 1].db.desc.depend
			= vtophys(STAILQ_NEXT(db_tr, link)->db) | z;
		if(dbch->xferq.flag & FWXFERQ_EXTBUF){
			if(((idb + 1 ) % dbch->xferq.bnpacket) == 0){
				db_tr->db[db_tr->dbcnt - 1].db.desc.cmd
					|= OHCI_INTERRUPT_ALWAYS;
				db_tr->db[0].db.desc.depend &= ~0xf;
				db_tr->db[db_tr->dbcnt - 1].db.desc.depend &=
						~0xf;
				/* OHCI 1.1 and above */
				db_tr->db[0].db.desc.cmd
					|= OHCI_INTERRUPT_ALWAYS;
			}
		}
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	dbch->bottom->db[db_tr->dbcnt - 1].db.desc.depend &= 0xfffffff0;
	return err;
}

static int
fwohci_rx_enable(struct fwohci_softc *sc, struct fwohci_dbch *dbch)
{
	int err = 0;
	int idb, z, i, dmach = 0;
	u_int32_t off = NULL;
	struct fwohcidb_tr *db_tr;

	z = dbch->ndesc;
	if(&sc->arrq == dbch){
		off = OHCI_ARQOFF;
	}else if(&sc->arrs == dbch){
		off = OHCI_ARSOFF;
	}else{
		for(dmach = 0 ; dmach < sc->fc.nisodma ; dmach++){
			if( &sc->ir[dmach] == dbch){
				off = OHCI_IROFF(dmach);
				break;
			}
		}
	}
	if(off == NULL){
		err = EINVAL;
		return err;
	}
	if(dbch->xferq.flag & FWXFERQ_STREAM){
		if(dbch->xferq.flag & FWXFERQ_RUNNING)
			return err;
	}else{
		if(dbch->xferq.flag & FWXFERQ_RUNNING){
			err = EBUSY;
			return err;
		}
	}
	dbch->xferq.flag |= FWXFERQ_RUNNING;
	dbch->top = STAILQ_FIRST(&dbch->db_trq);
	for( i = 0, dbch->bottom = dbch->top; i < (dbch->ndb - 1); i++){
		dbch->bottom = STAILQ_NEXT(dbch->bottom, link);
	}
	db_tr = dbch->top;
	for( idb = 0 ; idb < dbch->ndb ; idb ++){
		if(!(dbch->xferq.flag & FWXFERQ_EXTBUF)){
			fwohci_add_rx_buf(db_tr,
				dbch->xferq.psize, dbch->xferq.flag, 0, NULL);
		}else{
			fwohci_add_rx_buf(db_tr,
				dbch->xferq.psize, dbch->xferq.flag,
				dbch->xferq.buf + dbch->xferq.psize * idb,
				dbch->dummy + sizeof(u_int32_t) * idb);
		}
		if(STAILQ_NEXT(db_tr, link) == NULL){
			break;
		}
		db_tr->db[db_tr->dbcnt - 1].db.desc.depend
			= vtophys(STAILQ_NEXT(db_tr, link)->db) | z;
		if(dbch->xferq.flag & FWXFERQ_EXTBUF){
			if(((idb + 1 ) % dbch->xferq.bnpacket) == 0){
				db_tr->db[db_tr->dbcnt - 1].db.desc.cmd
					|= OHCI_INTERRUPT_ALWAYS;
				db_tr->db[db_tr->dbcnt - 1].db.desc.depend &=
						~0xf;
			}
		}
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	dbch->bottom->db[db_tr->dbcnt - 1].db.desc.depend &= 0xfffffff0;
	dbch->buf_offset = 0;
	if(dbch->xferq.flag & FWXFERQ_STREAM){
		return err;
	}else{
		OWRITE(sc, OHCI_DMACMD(off), vtophys(dbch->top->db) | z);
	}
	OWRITE(sc, OHCI_DMACTL(off), OHCI_CNTL_DMA_RUN);
	return err;
}

static int
fwohci_itxbuf_enable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int err = 0;
	unsigned short tag, ich;
	struct fwohci_dbch *dbch;
	struct fw_pkt *fp;
	struct fwohcidb_tr *db_tr;
	int cycle_now, sec, cycle, cycle_match;
	u_int32_t stat;

	tag = (sc->it[dmach].xferq.flag >> 6) & 3;
	ich = sc->it[dmach].xferq.flag & 0x3f;
	dbch = &sc->it[dmach];
	if ((dbch->flags & FWOHCI_DBCH_INIT) == 0) {
		dbch->xferq.queued = 0;
		dbch->ndb = dbch->xferq.bnpacket * dbch->xferq.bnchunk;
		dbch->ndesc = 3;
		fwohci_db_init(dbch);
		if ((dbch->flags & FWOHCI_DBCH_INIT) == 0)
			return ENOMEM;
		err = fwohci_tx_enable(sc, dbch);
	}
	if(err)
		return err;
	stat = OREAD(sc, OHCI_ITCTL(dmach));
	if (stat & OHCI_CNTL_DMA_ACTIVE) {
		if(dbch->xferq.stdma2 != NULL){
			fwohci_txbufdb(sc, dmach, dbch->xferq.stdma2);
			((struct fwohcidb_tr *)
		(dbch->xferq.stdma->end))->db[dbch->ndesc - 1].db.desc.cmd
			|= OHCI_BRANCH_ALWAYS;
			((struct fwohcidb_tr *)
		(dbch->xferq.stdma->end))->db[dbch->ndesc - 1].db.desc.depend =
	    vtophys(((struct fwohcidb_tr *)(dbch->xferq.stdma2->start))->db) | dbch->ndesc;
			((struct fwohcidb_tr *)(dbch->xferq.stdma->end))->db[0].db.desc.depend =
	    vtophys(((struct fwohcidb_tr *)(dbch->xferq.stdma2->start))->db) | dbch->ndesc;
			((struct fwohcidb_tr *)(dbch->xferq.stdma2->end))->db[dbch->ndesc - 1].db.desc.depend &= ~0xf;
			((struct fwohcidb_tr *)(dbch->xferq.stdma2->end))->db[0].db.desc.depend &= ~0xf;
		}
	} else if(!(stat & OHCI_CNTL_DMA_RUN)) {
		if (firewire_debug)
			printf("fwohci_itxbuf_enable: kick 0x%08x\n",
				OREAD(sc, OHCI_ITCTL(dmach)));
		fw_tbuf_update(&sc->fc, dmach, 0);
		if(dbch->xferq.stdma == NULL){
			return err;
		}
#if 0
		OWRITE(sc, OHCI_ITCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
#endif
		OWRITE(sc, OHCI_IT_MASKCLR, 1 << dmach);
		OWRITE(sc, OHCI_IT_STATCLR, 1 << dmach);
		OWRITE(sc, OHCI_IT_MASK, 1 << dmach);
		fwohci_txbufdb(sc, dmach, dbch->xferq.stdma);
		if(dbch->xferq.stdma2 != NULL){
			fwohci_txbufdb(sc, dmach, dbch->xferq.stdma2);
			((struct fwohcidb_tr *)
		(dbch->xferq.stdma->end))->db[dbch->ndesc - 1].db.desc.cmd
			|= OHCI_BRANCH_ALWAYS;
			((struct fwohcidb_tr *)(dbch->xferq.stdma->end))->db[dbch->ndesc - 1].db.desc.depend =
		    vtophys(((struct fwohcidb_tr *)(dbch->xferq.stdma2->start))->db) | dbch->ndesc;
			((struct fwohcidb_tr *)(dbch->xferq.stdma->end))->db[0].db.desc.depend =
		    vtophys(((struct fwohcidb_tr *)(dbch->xferq.stdma2->start))->db) | dbch->ndesc;
			((struct fwohcidb_tr *)(dbch->xferq.stdma2->end))->db[dbch->ndesc - 1].db.desc.depend &= ~0xf;
			((struct fwohcidb_tr *) (dbch->xferq.stdma2->end))->db[0].db.desc.depend &= ~0xf;
		}else{
			((struct fwohcidb_tr *) (dbch->xferq.stdma->end))->db[dbch->ndesc - 1].db.desc.depend &= ~0xf;
			((struct fwohcidb_tr *) (dbch->xferq.stdma->end))->db[0].db.desc.depend &= ~0xf;
		}
		OWRITE(sc, OHCI_ITCMD(dmach),
			vtophys(((struct fwohcidb_tr *)
				(dbch->xferq.stdma->start))->db) | dbch->ndesc);
#define CYCLE_OFFSET	1
		if(dbch->xferq.flag & FWXFERQ_DV){
			db_tr = (struct fwohcidb_tr *)dbch->xferq.stdma->start;
			fp = (struct fw_pkt *)db_tr->buf;
			dbch->xferq.dvoffset = CYCLE_OFFSET;
			fp->mode.ld[2] |= htonl(dbch->xferq.dvoffset << 12);
		}
		/* 2bit second + 13bit cycle */
		cycle_now = (fc->cyctimer(fc) >> 12) & 0x7fff;
		cycle = cycle_now & 0x1fff;
		sec = cycle_now >> 13;
#define CYCLE_MOD	0x10
#define CYCLE_DELAY	8	/* min delay to start DMA */
		cycle = cycle + CYCLE_DELAY;
		if (cycle >= 8000) {
			sec ++;
			cycle -= 8000;
		}
		cycle = ((cycle + CYCLE_MOD - 1) / CYCLE_MOD) * CYCLE_MOD;
		if (cycle >= 8000) {
			sec ++;
			if (cycle == 8000)
				cycle = 0;
			else
				cycle = CYCLE_MOD;
		}
		cycle_match = ((sec << 13) | cycle) & 0x7ffff;
		if (firewire_debug)
			printf("cycle_match: 0x%04x->0x%04x\n",
						cycle_now, cycle_match);
		/* Clear cycle match counter bits */
		OWRITE(sc, OHCI_ITCTLCLR(dmach), 0xffff0000);
		OWRITE(sc, OHCI_ITCTL(dmach),
				OHCI_CNTL_CYCMATCH_S | (cycle_match << 16)
				| OHCI_CNTL_DMA_RUN);
		OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_DMA_IT);
	} else {
		OWRITE(sc, OHCI_ITCTL(dmach), OHCI_CNTL_DMA_WAKE);
	}
	return err;
}

static int
fwohci_irxbuf_enable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int err = 0;
	unsigned short tag, ich;

	if(!(sc->ir[dmach].xferq.flag & FWXFERQ_RUNNING)){
		tag = (sc->ir[dmach].xferq.flag >> 6) & 3;
		ich = sc->ir[dmach].xferq.flag & 0x3f;
		OWRITE(sc, OHCI_IRMATCH(dmach), tagbit[tag] | ich);

		sc->ir[dmach].xferq.queued = 0;
		sc->ir[dmach].ndb = sc->ir[dmach].xferq.bnpacket *
				sc->ir[dmach].xferq.bnchunk;
		sc->ir[dmach].dummy =
			malloc(sizeof(u_int32_t) * sc->ir[dmach].ndb, 
			   M_DEVBUF, M_DONTWAIT);
		if(sc->ir[dmach].dummy == NULL){
			err = ENOMEM;
			return err;
		}
		sc->ir[dmach].ndesc = 2;
		fwohci_db_init(&sc->ir[dmach]);
		if ((sc->ir[dmach].flags & FWOHCI_DBCH_INIT) == 0)
			return ENOMEM;
		err = fwohci_rx_enable(sc, &sc->ir[dmach]);
	}
	if(err)
		return err;

	if(OREAD(sc, OHCI_IRCTL(dmach)) & OHCI_CNTL_DMA_ACTIVE){
		if(sc->ir[dmach].xferq.stdma2 != NULL){
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma->end))->db[sc->ir[dmach].ndesc - 1].db.desc.depend =
	    vtophys(((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma2->start))->db) | sc->ir[dmach].ndesc;
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma->end))->db[0].db.desc.depend =
	    vtophys(((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma2->start))->db);
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma2->end))->db[sc->ir[dmach].ndesc - 1].db.desc.depend &= ~0xf;
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma2->end))->db[0].db.desc.depend &= ~0xf;
		}
	}else if(!(OREAD(sc, OHCI_IRCTL(dmach)) & OHCI_CNTL_DMA_ACTIVE)
		&& !(sc->ir[dmach].xferq.flag & FWXFERQ_PACKET)){
		fw_rbuf_update(&sc->fc, dmach, 0);

		OWRITE(sc, OHCI_IRCTLCLR(dmach), OHCI_CNTL_DMA_RUN);
		OWRITE(sc, OHCI_IR_MASKCLR, 1 << dmach);
		OWRITE(sc, OHCI_IR_STATCLR, 1 << dmach);
		OWRITE(sc, OHCI_IR_MASK, 1 << dmach);
		OWRITE(sc, OHCI_IRCTLCLR(dmach), 0xf0000000);
		OWRITE(sc, OHCI_IRCTL(dmach), OHCI_CNTL_ISOHDR);
		if(sc->ir[dmach].xferq.stdma2 != NULL){
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma->end))->db[sc->ir[dmach].ndesc - 1].db.desc.depend =
		    vtophys(((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma2->start))->db) | sc->ir[dmach].ndesc;
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma->end))->db[0].db.desc.depend =
		    vtophys(((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma2->start))->db);
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma2->end))->db[sc->ir[dmach].ndesc - 1].db.desc.depend &= ~0xf;
		}else{
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma->end))->db[sc->ir[dmach].ndesc - 1].db.desc.depend &= ~0xf;
			((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma->end))->db[0].db.desc.depend &= ~0xf;
		}
		OWRITE(sc, OHCI_IRCMD(dmach),
			vtophys(((struct fwohcidb_tr *)(sc->ir[dmach].xferq.stdma->start))->db) | sc->ir[dmach].ndesc);
		OWRITE(sc, OHCI_IRCTL(dmach), OHCI_CNTL_DMA_RUN);
		OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_DMA_IR);
	}
	return err;
}

static int
fwohci_irx_enable(struct firewire_comm *fc, int dmach)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)fc;
	int err = 0;

	if(sc->ir[dmach].xferq.flag & FWXFERQ_PACKET){
		err = fwohci_irxpp_enable(fc, dmach);
		return err;
	}else{
		err = fwohci_irxbuf_enable(fc, dmach);
		return err;
	}
}

int
fwohci_shutdown(struct fwohci_softc *sc, device_t dev)
{
	u_int i;

/* Now stopping all DMA channel */
	OWRITE(sc,  OHCI_ARQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ARSCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
	OWRITE(sc,  OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);

	for( i = 0 ; i < sc->fc.nisodma ; i ++ ){
		OWRITE(sc,  OHCI_IRCTLCLR(i), OHCI_CNTL_DMA_RUN);
		OWRITE(sc,  OHCI_ITCTLCLR(i), OHCI_CNTL_DMA_RUN);
	}

/* FLUSH FIFO and reset Transmitter/Reciever */
	OWRITE(sc,  OHCI_HCCCTL, OHCI_HCC_RESET);

/* Stop interrupt */
	OWRITE(sc, FWOHCI_INTMASKCLR,
			OHCI_INT_EN | OHCI_INT_ERR | OHCI_INT_PHY_SID
			| OHCI_INT_PHY_INT
			| OHCI_INT_DMA_ATRQ | OHCI_INT_DMA_ATRS 
			| OHCI_INT_DMA_PRRQ | OHCI_INT_DMA_PRRS
			| OHCI_INT_DMA_ARRQ | OHCI_INT_DMA_ARRS 
			| OHCI_INT_PHY_BUS_R);
/* XXX Link down?  Bus reset? */
	return 0;
}

int
fwohci_resume(struct fwohci_softc *sc, device_t dev)
{
	int i;

	fwohci_reset(sc, dev);
	/* XXX resume isochronus receive automatically. (how about TX?) */
	for(i = 0; i < sc->fc.nisodma; i ++) {
		if((sc->ir[i].xferq.flag & FWXFERQ_RUNNING) != 0) {
			device_printf(sc->fc.dev,
				"resume iso receive ch: %d\n", i);
			sc->ir[i].xferq.flag &= ~FWXFERQ_RUNNING;
			sc->fc.irx_enable(&sc->fc, i);
		}
	}

	bus_generic_resume(dev);
	sc->fc.ibr(&sc->fc);
	return 0;
}

#define ACK_ALL
static void
fwohci_intr_body(struct fwohci_softc *sc, u_int32_t stat, int count)
{
	u_int32_t irstat, itstat;
	u_int i;
	struct firewire_comm *fc = (struct firewire_comm *)sc;

#ifdef OHCI_DEBUG
	if(stat & OREAD(sc, FWOHCI_INTMASK))
		device_printf(fc->dev, "INTERRUPT < %s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s> 0x%08x, 0x%08x\n",
			stat & OHCI_INT_EN ? "DMA_EN ":"",
			stat & OHCI_INT_PHY_REG ? "PHY_REG ":"",
			stat & OHCI_INT_CYC_LONG ? "CYC_LONG ":"",
			stat & OHCI_INT_ERR ? "INT_ERR ":"",
			stat & OHCI_INT_CYC_ERR ? "CYC_ERR ":"",
			stat & OHCI_INT_CYC_LOST ? "CYC_LOST ":"",
			stat & OHCI_INT_CYC_64SECOND ? "CYC_64SECOND ":"",
			stat & OHCI_INT_CYC_START ? "CYC_START ":"",
			stat & OHCI_INT_PHY_INT ? "PHY_INT ":"",
			stat & OHCI_INT_PHY_BUS_R ? "BUS_RESET ":"",
			stat & OHCI_INT_PHY_SID ? "SID ":"",
			stat & OHCI_INT_LR_ERR ? "DMA_LR_ERR ":"",
			stat & OHCI_INT_PW_ERR ? "DMA_PW_ERR ":"",
			stat & OHCI_INT_DMA_IR ? "DMA_IR ":"",
			stat & OHCI_INT_DMA_IT  ? "DMA_IT " :"",
			stat & OHCI_INT_DMA_PRRS  ? "DMA_PRRS " :"",
			stat & OHCI_INT_DMA_PRRQ  ? "DMA_PRRQ " :"",
			stat & OHCI_INT_DMA_ARRS  ? "DMA_ARRS " :"",
			stat & OHCI_INT_DMA_ARRQ  ? "DMA_ARRQ " :"",
			stat & OHCI_INT_DMA_ATRS  ? "DMA_ATRS " :"",
			stat & OHCI_INT_DMA_ATRQ  ? "DMA_ATRQ " :"",
			stat, OREAD(sc, FWOHCI_INTMASK) 
		);
#endif
/* Bus reset */
	if(stat & OHCI_INT_PHY_BUS_R ){
		device_printf(fc->dev, "BUS reset\n");
		OWRITE(sc, FWOHCI_INTMASKCLR,  OHCI_INT_CYC_LOST);
		OWRITE(sc, OHCI_LNKCTLCLR, OHCI_CNTL_CYCSRC);

		OWRITE(sc,  OHCI_ATQCTLCLR, OHCI_CNTL_DMA_RUN);
		sc->atrq.xferq.flag &= ~FWXFERQ_RUNNING;
		OWRITE(sc,  OHCI_ATSCTLCLR, OHCI_CNTL_DMA_RUN);
		sc->atrs.xferq.flag &= ~FWXFERQ_RUNNING;

#if 0
		for( i = 0 ; i < fc->nisodma ; i ++ ){
			OWRITE(sc,  OHCI_IRCTLCLR(i), OHCI_CNTL_DMA_RUN);
			OWRITE(sc,  OHCI_ITCTLCLR(i), OHCI_CNTL_DMA_RUN);
		}

#endif
		fw_busreset(fc);

		/* XXX need to wait DMA to stop */
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PHY_BUS_R);
#endif
#if 1
		/* pending all pre-bus_reset packets */
		fwohci_txd(sc, &sc->atrq);
		fwohci_txd(sc, &sc->atrs);
		fwohci_arcv(sc, &sc->arrs, -1);
		fwohci_arcv(sc, &sc->arrq, -1);
#endif


		OWRITE(sc, OHCI_AREQHI, 1 << 31);
		/* XXX insecure ?? */
		OWRITE(sc, OHCI_PREQHI, 0x7fffffff);
		OWRITE(sc, OHCI_PREQLO, 0xffffffff);
		OWRITE(sc, OHCI_PREQUPPER, 0x10000);

	}
	if((stat & OHCI_INT_DMA_IR )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_IR);
#endif
		irstat = OREAD(sc, OHCI_IR_STAT);
		OWRITE(sc, OHCI_IR_STATCLR, irstat);
		for(i = 0; i < fc->nisodma ; i++){
			if((irstat & (1 << i)) != 0){
				if(sc->ir[i].xferq.flag & FWXFERQ_PACKET){
					fwohci_ircv(sc, &sc->ir[i], count);
				}else{
					fwohci_rbuf_update(sc, i);
				}
			}
		}
	}
	if((stat & OHCI_INT_DMA_IT )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_IT);
#endif
		itstat = OREAD(sc, OHCI_IT_STAT);
		OWRITE(sc, OHCI_IT_STATCLR, itstat);
		for(i = 0; i < fc->nisodma ; i++){
			if((itstat & (1 << i)) != 0){
				fwohci_tbuf_update(sc, i);
			}
		}
	}
	if((stat & OHCI_INT_DMA_PRRS )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_PRRS);
#endif
#if 0
		dump_dma(sc, ARRS_CH);
		dump_db(sc, ARRS_CH);
#endif
		fwohci_arcv(sc, &sc->arrs, count);
	}
	if((stat & OHCI_INT_DMA_PRRQ )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_PRRQ);
#endif
#if 0
		dump_dma(sc, ARRQ_CH);
		dump_db(sc, ARRQ_CH);
#endif
		fwohci_arcv(sc, &sc->arrq, count);
	}
	if(stat & OHCI_INT_PHY_SID){
		caddr_t buf;
		int plen;

#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PHY_SID);
#endif
/*
** Checking whether the node is root or not. If root, turn on 
** cycle master.
*/
		device_printf(fc->dev, "node_id = 0x%08x, ", OREAD(sc, FWOHCI_NODEID));
		if(!(OREAD(sc, FWOHCI_NODEID) & OHCI_NODE_VALID)){
			printf("Bus reset failure\n");
			goto sidout;
		}
		if( OREAD(sc, FWOHCI_NODEID) & OHCI_NODE_ROOT ){
			printf("CYCLEMASTER mode\n");
			OWRITE(sc, OHCI_LNKCTL,
				OHCI_CNTL_CYCMTR | OHCI_CNTL_CYCTIMER);
		}else{
			printf("non CYCLEMASTER mode\n");
			OWRITE(sc, OHCI_LNKCTLCLR, OHCI_CNTL_CYCMTR);
			OWRITE(sc, OHCI_LNKCTL, OHCI_CNTL_CYCTIMER);
		}
		fc->nodeid = OREAD(sc, FWOHCI_NODEID) & 0x3f;

		plen = OREAD(sc, OHCI_SID_CNT) & OHCI_SID_CNT_MASK;
		plen -= 4; /* chop control info */
		buf = malloc( FWPMAX_S400, M_DEVBUF, M_NOWAIT);
		if(buf == NULL) goto sidout;
		bcopy((void *)(uintptr_t)(volatile void *)(fc->sid_buf + 1),
								buf, plen);
		fw_sidrcv(fc, buf, plen, 0);
	}
sidout:
	if((stat & OHCI_INT_DMA_ATRQ )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_ATRQ);
#endif
		fwohci_txd(sc, &(sc->atrq));
	}
	if((stat & OHCI_INT_DMA_ATRS )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_DMA_ATRS);
#endif
		fwohci_txd(sc, &(sc->atrs));
	}
	if((stat & OHCI_INT_PW_ERR )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PW_ERR);
#endif
		device_printf(fc->dev, "posted write error\n");
	}
	if((stat & OHCI_INT_ERR )){
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_ERR);
#endif
		device_printf(fc->dev, "unrecoverable error\n");
	}
	if((stat & OHCI_INT_PHY_INT)) {
#ifndef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, OHCI_INT_PHY_INT);
#endif
		device_printf(fc->dev, "phy int\n");
	}

	return;
}

void
fwohci_intr(void *arg)
{
	struct fwohci_softc *sc = (struct fwohci_softc *)arg;
	u_int32_t stat;

	if (!(sc->intmask & OHCI_INT_EN)) {
		/* polling mode */
		return;
	}

	while ((stat = OREAD(sc, FWOHCI_INTSTAT)) != 0) {
		if (stat == 0xffffffff) {
			device_printf(sc->fc.dev, 
				"device physically ejected?\n");
			return;
		}
#ifdef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, stat);
#endif
		fwohci_intr_body(sc, stat, -1);
	}
}

static void
fwohci_poll(struct firewire_comm *fc, int quick, int count)
{
	int s;
	u_int32_t stat;
	struct fwohci_softc *sc;


	sc = (struct fwohci_softc *)fc;
	stat = OHCI_INT_DMA_IR | OHCI_INT_DMA_IT |
		OHCI_INT_DMA_PRRS | OHCI_INT_DMA_PRRQ |
		OHCI_INT_DMA_ATRQ | OHCI_INT_DMA_ATRS;
#if 0
	if (!quick) {
#else
	if (1) {
#endif
		stat = OREAD(sc, FWOHCI_INTSTAT);
		if (stat == 0)
			return;
		if (stat == 0xffffffff) {
			device_printf(sc->fc.dev, 
				"device physically ejected?\n");
			return;
		}
#ifdef ACK_ALL
		OWRITE(sc, FWOHCI_INTSTATCLR, stat);
#endif
	}
	s = splfw();
	fwohci_intr_body(sc, stat, count);
	splx(s);
}

static void
fwohci_set_intr(struct firewire_comm *fc, int enable)
{
	struct fwohci_softc *sc;

	sc = (struct fwohci_softc *)fc;
	if (bootverbose)
		device_printf(sc->fc.dev, "fwohci_set_intr: %d\n", enable);
	if (enable) {
		sc->intmask |= OHCI_INT_EN;
		OWRITE(sc, FWOHCI_INTMASK, OHCI_INT_EN);
	} else {
		sc->intmask &= ~OHCI_INT_EN;
		OWRITE(sc, FWOHCI_INTMASKCLR, OHCI_INT_EN);
	}
}

static void
fwohci_tbuf_update(struct fwohci_softc *sc, int dmach)
{
	int stat;
	struct firewire_comm *fc = &sc->fc;
	struct fw_pkt *fp;
	struct fwohci_dbch *dbch;
	struct fwohcidb_tr *db_tr;

	dbch = &sc->it[dmach];
#if 0	/* XXX OHCI interrupt before the last packet is really on the wire */
	if((dbch->xferq.flag & FWXFERQ_DV) && (dbch->xferq.stdma2 != NULL)){
		db_tr = (struct fwohcidb_tr *)dbch->xferq.stdma2->start;
/*
 * Overwrite highest significant 4 bits timestamp information
 */
		fp = (struct fw_pkt *)db_tr->buf;
		fp->mode.ld[2] &= htonl(0xffff0fff);
		fp->mode.ld[2] |= htonl((fc->cyctimer(fc) + 0x4000) & 0xf000);
	}
#endif
	stat = OREAD(sc, OHCI_ITCTL(dmach)) & 0x1f;
	switch(stat){
	case FWOHCIEV_ACKCOMPL:
#if 1
	if (dbch->xferq.flag & FWXFERQ_DV) {
		struct ciphdr *ciph;
		int timer, timestamp, cycl, diff;
		static int last_timer=0;

		timer = (fc->cyctimer(fc) >> 12) & 0xffff;
		db_tr = (struct fwohcidb_tr *)dbch->xferq.stdma->start;
		fp = (struct fw_pkt *)db_tr->buf;
		ciph = (struct ciphdr *) &fp->mode.ld[1];
		timestamp = db_tr->db[2].db.desc.count & 0xffff;
		cycl = ntohs(ciph->fdf.dv.cyc) >> 12; 
		diff = cycl - (timestamp & 0xf) - CYCLE_OFFSET;
		if (diff < 0)
			diff += 16;
		if (diff > 8)
			diff -= 16;
		if (firewire_debug || diff != 0)
			printf("dbc: %3d timer: 0x%04x packet: 0x%04x"
				" cyc: 0x%x diff: %+1d\n",
				ciph->dbc, last_timer, timestamp, cycl, diff);
		last_timer = timer;
		/* XXX adjust dbch->xferq.dvoffset if diff != 0 or 1 */
	}
#endif
		fw_tbuf_update(fc, dmach, 1);
		break;
	default:
		device_printf(fc->dev, "Isochronous transmit err %02x\n", stat);
		fw_tbuf_update(fc, dmach, 0);
		break;
	}
	fwohci_itxbuf_enable(fc, dmach);
}

static void
fwohci_rbuf_update(struct fwohci_softc *sc, int dmach)
{
	struct firewire_comm *fc = &sc->fc;
	int stat;

	stat = OREAD(sc, OHCI_IRCTL(dmach)) & 0x1f;
	switch(stat){
	case FWOHCIEV_ACKCOMPL:
		fw_rbuf_update(fc, dmach, 1);
		wakeup(fc->ir[dmach]);
		fwohci_irx_enable(fc, dmach);
		break;
	default:
		device_printf(fc->dev, "Isochronous receive err %02x\n",
									stat);
		break;
	}
}

void
dump_dma(struct fwohci_softc *sc, u_int32_t ch)
{
	u_int32_t off, cntl, stat, cmd, match;

	if(ch == 0){
		off = OHCI_ATQOFF;
	}else if(ch == 1){
		off = OHCI_ATSOFF;
	}else if(ch == 2){
		off = OHCI_ARQOFF;
	}else if(ch == 3){
		off = OHCI_ARSOFF;
	}else if(ch < IRX_CH){
		off = OHCI_ITCTL(ch - ITX_CH);
	}else{
		off = OHCI_IRCTL(ch - IRX_CH);
	}
	cntl = stat = OREAD(sc, off);
	cmd = OREAD(sc, off + 0xc);
	match = OREAD(sc, off + 0x10);

	device_printf(sc->fc.dev, "dma ch %1x:dma regs 0x%08x 0x%08x 0x%08x 0x%08x \n",
		ch,
		cntl, 
		stat, 
		cmd, 
		match);
	stat &= 0xffff ;
	if(stat & 0xff00){
		device_printf(sc->fc.dev, "dma %d ch:%s%s%s%s%s%s %s(%x)\n",
			ch,
			stat & OHCI_CNTL_DMA_RUN ? "RUN," : "",
			stat & OHCI_CNTL_DMA_WAKE ? "WAKE," : "",
			stat & OHCI_CNTL_DMA_DEAD ? "DEAD," : "",
			stat & OHCI_CNTL_DMA_ACTIVE ? "ACTIVE," : "",
			stat & OHCI_CNTL_DMA_BT ? "BRANCH," : "",
			stat & OHCI_CNTL_DMA_BAD ? "BADDMA," : "",
			fwohcicode[stat & 0x1f],
			stat & 0x1f
		);
	}else{
		device_printf(sc->fc.dev, "dma %d ch: Nostat\n", ch);
	}
}

void
dump_db(struct fwohci_softc *sc, u_int32_t ch)
{
	struct fwohci_dbch *dbch;
	struct fwohcidb_tr *cp = NULL, *pp, *np;
	volatile struct fwohcidb *curr = NULL, *prev, *next = NULL;
	int idb, jdb;
	u_int32_t cmd, off;
	if(ch == 0){
		off = OHCI_ATQOFF;
		dbch = &sc->atrq;
	}else if(ch == 1){
		off = OHCI_ATSOFF;
		dbch = &sc->atrs;
	}else if(ch == 2){
		off = OHCI_ARQOFF;
		dbch = &sc->arrq;
	}else if(ch == 3){
		off = OHCI_ARSOFF;
		dbch = &sc->arrs;
	}else if(ch < IRX_CH){
		off = OHCI_ITCTL(ch - ITX_CH);
		dbch = &sc->it[ch - ITX_CH];
	}else {
		off = OHCI_IRCTL(ch - IRX_CH);
		dbch = &sc->ir[ch - IRX_CH];
	}
	cmd = OREAD(sc, off + 0xc);

	if( dbch->ndb == 0 ){
		device_printf(sc->fc.dev, "No DB is attached ch=%d\n", ch);
		return;
	}
	pp = dbch->top;
	prev = pp->db;
	for(idb = 0 ; idb < dbch->ndb ; idb ++ ){
		if(pp == NULL){
			curr = NULL;
			goto outdb;
		}
		cp = STAILQ_NEXT(pp, link);
		if(cp == NULL){
			curr = NULL;
			goto outdb;
		}
		np = STAILQ_NEXT(cp, link);
		if(cp == NULL) break;
		for(jdb = 0 ; jdb < dbch->ndesc ; jdb ++ ){
			if((cmd  & 0xfffffff0)
				== vtophys(&(cp->db[jdb]))){
				curr = cp->db;
				if(np != NULL){
					next = np->db;
				}else{
					next = NULL;
				}
				goto outdb;
			}
		}
		pp = STAILQ_NEXT(pp, link);
		prev = pp->db;
	}
outdb:
	if( curr != NULL){
		printf("Prev DB %d\n", ch);
		print_db(prev, ch, dbch->ndesc);
		printf("Current DB %d\n", ch);
		print_db(curr, ch, dbch->ndesc);
		printf("Next DB %d\n", ch);
		print_db(next, ch, dbch->ndesc);
	}else{
		printf("dbdump err ch = %d cmd = 0x%08x\n", ch, cmd);
	}
	return;
}

void
print_db(volatile struct fwohcidb *db, u_int32_t ch, u_int32_t max)
{
	fwohcireg_t stat;
	int i, key;

	if(db == NULL){
		printf("No Descriptor is found\n");
		return;
	}

	printf("ch = %d\n%8s %s %s %s %s %4s %8s %8s %4s:%4s\n",
		ch,
		"Current",
		"OP  ",
		"KEY",
		"INT",
		"BR ",
		"len",
		"Addr",
		"Depend",
		"Stat",
		"Cnt");
	for( i = 0 ; i <= max ; i ++){
		key = db[i].db.desc.cmd & OHCI_KEY_MASK;
#if __FreeBSD_version >= 500000
		printf("%08tx %s %s %s %s %5d %08x %08x %04x:%04x",
#else
		printf("%08x %s %s %s %s %5d %08x %08x %04x:%04x",
#endif
				vtophys(&db[i]),
				dbcode[(db[i].db.desc.cmd >> 28) & 0xf],
				dbkey[(db[i].db.desc.cmd >> 24) & 0x7],
				dbcond[(db[i].db.desc.cmd >> 20) & 0x3],
				dbcond[(db[i].db.desc.cmd >> 18) & 0x3],
				db[i].db.desc.cmd & 0xffff,
				db[i].db.desc.addr,
				db[i].db.desc.depend,
				db[i].db.desc.status, 
				db[i].db.desc.count);
		stat = db[i].db.desc.status;
		if(stat & 0xff00){
			printf(" %s%s%s%s%s%s %s(%x)\n",
				stat & OHCI_CNTL_DMA_RUN ? "RUN," : "",
				stat & OHCI_CNTL_DMA_WAKE ? "WAKE," : "",
				stat & OHCI_CNTL_DMA_DEAD ? "DEAD," : "",
				stat & OHCI_CNTL_DMA_ACTIVE ? "ACTIVE," : "",
				stat & OHCI_CNTL_DMA_BT ? "BRANCH," : "",
				stat & OHCI_CNTL_DMA_BAD ? "BADDMA," : "",
				fwohcicode[stat & 0x1f],
				stat & 0x1f
			);
		}else{
			printf(" Nostat\n");
		}
		if(key == OHCI_KEY_ST2 ){
			printf("0x%08x 0x%08x 0x%08x 0x%08x\n", 
				db[i+1].db.immed[0],
				db[i+1].db.immed[1],
				db[i+1].db.immed[2],
				db[i+1].db.immed[3]);
		}
		if(key == OHCI_KEY_DEVICE){
			return;
		}
		if((db[i].db.desc.cmd & OHCI_BRANCH_MASK) 
				== OHCI_BRANCH_ALWAYS){
			return;
		}
		if((db[i].db.desc.cmd & OHCI_CMD_MASK) 
				== OHCI_OUTPUT_LAST){
			return;
		}
		if((db[i].db.desc.cmd & OHCI_CMD_MASK) 
				== OHCI_INPUT_LAST){
			return;
		}
		if(key == OHCI_KEY_ST2 ){
			i++;
		}
	}
	return;
}

void
fwohci_ibr(struct firewire_comm *fc)
{
	struct fwohci_softc *sc;
	u_int32_t fun;

	sc = (struct fwohci_softc *)fc;

	/*
	 * Set root hold-off bit so that non cyclemaster capable node
	 * shouldn't became the root node.
	 */
#if 1
	fun = fwphy_rddata(sc, FW_PHY_IBR_REG);
	fun |= FW_PHY_IBR | FW_PHY_RHB;
	fun = fwphy_wrdata(sc, FW_PHY_IBR_REG, fun);
#else	/* Short bus reset */
	fun = fwphy_rddata(sc, FW_PHY_ISBR_REG);
	fun |= FW_PHY_ISBR | FW_PHY_RHB;
	fun = fwphy_wrdata(sc, FW_PHY_ISBR_REG, fun);
#endif
}

void
fwohci_txbufdb(struct fwohci_softc *sc, int dmach, struct fw_bulkxfer *bulkxfer)
{
	struct fwohcidb_tr *db_tr, *fdb_tr;
	struct fwohci_dbch *dbch;
	struct fw_pkt *fp;
	volatile struct fwohci_txpkthdr *ohcifp;
	unsigned short chtag;
	int idb;

	dbch = &sc->it[dmach];
	chtag = sc->it[dmach].xferq.flag & 0xff;

	db_tr = (struct fwohcidb_tr *)(bulkxfer->start);
	fdb_tr = (struct fwohcidb_tr *)(bulkxfer->end);
/*
device_printf(sc->fc.dev, "DB %08x %08x %08x\n", bulkxfer, vtophys(db_tr->db), vtophys(fdb_tr->db));
*/
	if(bulkxfer->flag != 0){
		return;
	}
	bulkxfer->flag = 1;
	for( idb = 0 ; idb < bulkxfer->npacket ; idb ++){
		db_tr->db[0].db.desc.cmd
			= OHCI_OUTPUT_MORE | OHCI_KEY_ST2 | 8;
		fp = (struct fw_pkt *)db_tr->buf;
		ohcifp = (volatile struct fwohci_txpkthdr *)
						db_tr->db[1].db.immed;
		ohcifp->mode.ld[0] = ntohl(fp->mode.ld[0]);
		ohcifp->mode.stream.len = ntohs(fp->mode.stream.len);
		ohcifp->mode.stream.chtag = chtag;
		ohcifp->mode.stream.tcode = 0xa;
		ohcifp->mode.stream.spd = 4;
		ohcifp->mode.ld[2] = ntohl(fp->mode.ld[1]);
		ohcifp->mode.ld[3] = ntohl(fp->mode.ld[2]);

		db_tr->db[2].db.desc.cmd
			= OHCI_OUTPUT_LAST
			| OHCI_UPDATE
			| OHCI_BRANCH_ALWAYS
			| ((ntohs(fp->mode.stream.len) ) & 0xffff);
		db_tr->db[2].db.desc.status = 0;
		db_tr->db[2].db.desc.count = 0;
		db_tr->db[0].db.desc.depend
			= vtophys(STAILQ_NEXT(db_tr, link)->db) | dbch->ndesc;
		db_tr->db[dbch->ndesc - 1].db.desc.depend
			= vtophys(STAILQ_NEXT(db_tr, link)->db) | dbch->ndesc;
		bulkxfer->end = (caddr_t)db_tr;
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	db_tr = (struct fwohcidb_tr *)bulkxfer->end;
	db_tr->db[0].db.desc.depend &= ~0xf;
	db_tr->db[dbch->ndesc - 1].db.desc.depend &= ~0xf;
#if 0
/**/
	db_tr->db[dbch->ndesc - 1].db.desc.cmd &= ~OHCI_BRANCH_ALWAYS;
	db_tr->db[dbch->ndesc - 1].db.desc.cmd |= OHCI_BRANCH_NEVER;
/**/
#endif
	db_tr->db[dbch->ndesc - 1].db.desc.cmd |= OHCI_INTERRUPT_ALWAYS;
	/* OHCI 1.1 and above */
	db_tr->db[0].db.desc.cmd |= OHCI_INTERRUPT_ALWAYS;

	db_tr = (struct fwohcidb_tr *)bulkxfer->start;
	fdb_tr = (struct fwohcidb_tr *)bulkxfer->end;
/*
device_printf(sc->fc.dev, "DB %08x %3d %08x %08x\n", bulkxfer, bulkxfer->npacket, vtophys(db_tr->db), vtophys(fdb_tr->db));
*/
	return;
}

static int
fwohci_add_tx_buf(struct fwohcidb_tr *db_tr, unsigned short size,
	int mode, void *buf)
{
	volatile struct fwohcidb *db = db_tr->db;
	int err = 0;
	if(buf == 0){
		err = EINVAL;
		return err;
	}
	db_tr->buf = buf;
	db_tr->dbcnt = 3;
	db_tr->dummy = NULL;

	db[0].db.desc.cmd = OHCI_OUTPUT_MORE | OHCI_KEY_ST2 | 8;

	db[2].db.desc.depend = 0;
	db[2].db.desc.addr = vtophys(buf) + sizeof(u_int32_t);
	db[2].db.desc.cmd = OHCI_OUTPUT_MORE;

	db[0].db.desc.status = 0;
	db[0].db.desc.count = 0;

	db[2].db.desc.status = 0;
	db[2].db.desc.count = 0;
	if( mode & FWXFERQ_STREAM ){
		db[2].db.desc.cmd |= OHCI_OUTPUT_LAST;
		if(mode & FWXFERQ_PACKET ){
			db[2].db.desc.cmd
					|= OHCI_INTERRUPT_ALWAYS;
		}
	}
	db[2].db.desc.cmd |= OHCI_BRANCH_ALWAYS;
	return 1;
}

int
fwohci_add_rx_buf(struct fwohcidb_tr *db_tr, unsigned short size, int mode,
	void *buf, void *dummy)
{
	volatile struct fwohcidb *db = db_tr->db;
	int i;
	void *dbuf[2];
	int dsiz[2];

	if(buf == 0){
		buf = malloc(size, M_DEVBUF, M_NOWAIT);
		if(buf == NULL) return 0;
		db_tr->buf = buf;
		db_tr->dbcnt = 1;
		db_tr->dummy = NULL;
		dsiz[0] = size;
		dbuf[0] = buf;
	}else if(dummy == NULL){
		db_tr->buf = buf;
		db_tr->dbcnt = 1;
		db_tr->dummy = NULL;
		dsiz[0] = size;
		dbuf[0] = buf;
	}else{
		db_tr->buf = buf;
		db_tr->dbcnt = 2;
		db_tr->dummy = dummy;
		dsiz[0] = sizeof(u_int32_t);
		dsiz[1] = size;
		dbuf[0] = dummy;
		dbuf[1] = buf;
	}
	for(i = 0 ; i < db_tr->dbcnt ; i++){
		db[i].db.desc.addr = vtophys(dbuf[i]) ;
		db[i].db.desc.cmd = OHCI_INPUT_MORE | dsiz[i];
		if( mode & FWXFERQ_STREAM ){
			db[i].db.desc.cmd |= OHCI_UPDATE;
		}
		db[i].db.desc.status = 0;
		db[i].db.desc.count = dsiz[i];
	}
	if( mode & FWXFERQ_STREAM ){
		db[db_tr->dbcnt - 1].db.desc.cmd |= OHCI_INPUT_LAST;
		if(mode & FWXFERQ_PACKET ){
			db[db_tr->dbcnt - 1].db.desc.cmd
					|= OHCI_INTERRUPT_ALWAYS;
		}
	}
	db[db_tr->dbcnt - 1].db.desc.cmd |= OHCI_BRANCH_ALWAYS;
	return 1;
}

static void
fwohci_ircv(struct fwohci_softc *sc, struct fwohci_dbch *dbch, int count)
{
	struct fwohcidb_tr *db_tr = dbch->top, *odb_tr;
	struct firewire_comm *fc = (struct firewire_comm *)sc;
	int z = 1;
	struct fw_pkt *fp;
	u_int8_t *ld;
	u_int32_t off = NULL;
	u_int32_t stat;
	u_int32_t *qld;
	u_int32_t reg;
	u_int spd;
	u_int dmach;
	int len, i, plen;
	caddr_t buf;

	for(dmach = 0 ; dmach < sc->fc.nisodma ; dmach++){
		if( &sc->ir[dmach] == dbch){
			off = OHCI_IROFF(dmach);
			break;
		}
	}
	if(off == NULL){
		return;
	}
	if(!(dbch->xferq.flag & FWXFERQ_RUNNING)){
		fwohci_irx_disable(&sc->fc, dmach);
		return;
	}

	odb_tr = NULL;
	db_tr = dbch->top;
	i = 0;
	while ((reg = db_tr->db[0].db.desc.status) & 0x1f) {
		if (count >= 0 && count-- == 0)
			break;
		ld = (u_int8_t *)db_tr->buf;
		if (dbch->xferq.flag & FWXFERQ_PACKET) {
			/* skip timeStamp */
			ld += sizeof(struct fwohci_trailer);
		}
		qld = (u_int32_t *)ld;
		len = dbch->xferq.psize - (db_tr->db[0].db.desc.count);
/*
{
device_printf(sc->fc.dev, "%04x %2x 0x%08x 0x%08x 0x%08x 0x%08x\n", len, 
		db_tr->db[0].db.desc.status & 0x1f, qld[0],qld[1],qld[2],qld[3]);
}
*/
		fp=(struct fw_pkt *)ld;
		qld[0] = htonl(qld[0]);
		plen = sizeof(struct fw_isohdr)
			+ ntohs(fp->mode.stream.len) + sizeof(u_int32_t);
		ld += plen;
		len -= plen;
		buf = db_tr->buf;
		db_tr->buf = NULL;
		stat = reg & 0x1f;
		spd =  reg & 0x3;
		switch(stat){
			case FWOHCIEV_ACKCOMPL:
			case FWOHCIEV_ACKPEND:
				fw_rcv(&sc->fc, buf, plen - sizeof(u_int32_t), dmach, sizeof(u_int32_t), spd);
				break;
			default:
				free(buf, M_DEVBUF);
				device_printf(sc->fc.dev, "Isochronous receive err %02x\n", stat);
				break;
		}
		i++;
		fwohci_add_rx_buf(db_tr, dbch->xferq.psize, 
					dbch->xferq.flag, 0, NULL);
		db_tr->db[0].db.desc.depend &= ~0xf;
		if(dbch->pdb_tr != NULL){
			dbch->pdb_tr->db[0].db.desc.depend |= z;
		} else {
			/* XXX should be rewritten in better way */
			dbch->bottom->db[0].db.desc.depend |= z;
		}
		dbch->pdb_tr = db_tr;
		db_tr = STAILQ_NEXT(db_tr, link);
	}
	dbch->top = db_tr;
	reg = OREAD(sc, OHCI_DMACTL(off));
	if (reg & OHCI_CNTL_DMA_ACTIVE)
		return;
	device_printf(sc->fc.dev, "IR DMA %d stopped at %x status=%x (%d)\n",
			dmach, OREAD(sc, OHCI_DMACMD(off)), reg, i);
	dbch->top = db_tr;
	fwohci_irx_enable(fc, dmach);
}

#define PLEN(x)	(((ntohs(x))+0x3) & ~0x3)
static int
fwohci_get_plen(struct fwohci_softc *sc, struct fw_pkt *fp, int hlen)
{
	int i;

	for( i = 4; i < hlen ; i+=4){
		fp->mode.ld[i/4] = htonl(fp->mode.ld[i/4]);
	}

	switch(fp->mode.common.tcode){
	case FWTCODE_RREQQ:
		return sizeof(fp->mode.rreqq) + sizeof(u_int32_t);
	case FWTCODE_WRES:
		return sizeof(fp->mode.wres) + sizeof(u_int32_t);
	case FWTCODE_WREQQ:
		return sizeof(fp->mode.wreqq) + sizeof(u_int32_t);
	case FWTCODE_RREQB:
		return sizeof(fp->mode.rreqb) + sizeof(u_int32_t);
	case FWTCODE_RRESQ:
		return sizeof(fp->mode.rresq) + sizeof(u_int32_t);
	case FWTCODE_WREQB:
		return sizeof(struct fw_asyhdr) + PLEN(fp->mode.wreqb.len)
						+ sizeof(u_int32_t);
	case FWTCODE_LREQ:
		return sizeof(struct fw_asyhdr) + PLEN(fp->mode.lreq.len)
						+ sizeof(u_int32_t);
	case FWTCODE_RRESB:
		return sizeof(struct fw_asyhdr) + PLEN(fp->mode.rresb.len)
						+ sizeof(u_int32_t);
	case FWTCODE_LRES:
		return sizeof(struct fw_asyhdr) + PLEN(fp->mode.lres.len)
						+ sizeof(u_int32_t);
	case FWOHCITCODE_PHY:
		return 16;
	}
	device_printf(sc->fc.dev, "Unknown tcode %d\n", fp->mode.common.tcode);
	return 0;
}

static void
fwohci_arcv(struct fwohci_softc *sc, struct fwohci_dbch *dbch, int count)
{
	struct fwohcidb_tr *db_tr;
	int z = 1;
	struct fw_pkt *fp;
	u_int8_t *ld;
	u_int32_t stat, off;
	u_int spd;
	int len, plen, hlen, pcnt, poff = 0, rlen;
	int s;
	caddr_t buf;
	int resCount;

	if(&sc->arrq == dbch){
		off = OHCI_ARQOFF;
	}else if(&sc->arrs == dbch){
		off = OHCI_ARSOFF;
	}else{
		return;
	}

	s = splfw();
	db_tr = dbch->top;
	pcnt = 0;
	/* XXX we cannot handle a packet which lies in more than two buf */
	while (db_tr->db[0].db.desc.status & OHCI_CNTL_DMA_ACTIVE) {
		ld = (u_int8_t *)db_tr->buf + dbch->buf_offset;
		resCount = db_tr->db[0].db.desc.count;
		len = dbch->xferq.psize - resCount
					- dbch->buf_offset;
		while (len > 0 ) {
			if (count >= 0 && count-- == 0)
				goto out;
			if(dbch->frag.buf != NULL){
				buf = dbch->frag.buf;
				if (dbch->frag.plen < 0) {
					/* incomplete header */
					int hlen;

					hlen = - dbch->frag.plen;
					rlen = hlen - dbch->frag.len;
					bcopy(ld, dbch->frag.buf + dbch->frag.len, rlen);
					ld += rlen;
					len -= rlen;
					dbch->frag.len += rlen; 
#if 0
					printf("(1)frag.plen=%d frag.len=%d rlen=%d len=%d\n", dbch->frag.plen, dbch->frag.len, rlen, len);
#endif
					fp=(struct fw_pkt *)dbch->frag.buf;
					dbch->frag.plen
						= fwohci_get_plen(sc, fp, hlen);
					if (dbch->frag.plen == 0)
						goto out;
				}
				rlen = dbch->frag.plen - dbch->frag.len;
#if 0
				printf("(2)frag.plen=%d frag.len=%d rlen=%d len=%d\n", dbch->frag.plen, dbch->frag.len, rlen, len);
#endif
				bcopy(ld, dbch->frag.buf + dbch->frag.len,
						rlen);
				ld += rlen;
				len -= rlen;
				plen = dbch->frag.plen;
				dbch->frag.buf = NULL;
				dbch->frag.plen = 0;
				dbch->frag.len = 0;
				poff = 0;
			}else{
				fp=(struct fw_pkt *)ld;
				fp->mode.ld[0] = htonl(fp->mode.ld[0]);
				switch(fp->mode.common.tcode){
				case FWTCODE_RREQQ:
				case FWTCODE_WRES:
				case FWTCODE_WREQQ:
				case FWTCODE_RRESQ:
				case FWOHCITCODE_PHY:
					hlen = 12;
					break; 
				case FWTCODE_RREQB:
				case FWTCODE_WREQB:
				case FWTCODE_LREQ:
				case FWTCODE_RRESB:
				case FWTCODE_LRES:
					hlen = 16;
					break; 
				default:
					device_printf(sc->fc.dev, "Unknown tcode %d\n", fp->mode.common.tcode);
					goto out;
				}
				if (len >= hlen) {
					plen = fwohci_get_plen(sc, fp, hlen);
					if (plen == 0)
						goto out;
					plen = (plen + 3) & ~3;
					len -= plen;
				} else {
					plen = -hlen;
					len -= hlen;
				}
				if(resCount > 0 || len > 0){
					buf = malloc( dbch->xferq.psize,
							M_DEVBUF, M_NOWAIT);
					if(buf == NULL){
						printf("cannot malloc!\n");
						free(db_tr->buf, M_DEVBUF);
						goto out;
					}
					bcopy(ld, buf, plen);
					poff = 0;
					dbch->frag.buf = NULL;
					dbch->frag.plen = 0;
					dbch->frag.len = 0;
				}else if(len < 0){
					dbch->frag.buf = db_tr->buf;
					if (plen < 0) {
#if 0
						printf("plen < 0:"
						"hlen: %d  len: %d\n",
						hlen, len);
#endif
						dbch->frag.len = hlen + len;
						dbch->frag.plen = -hlen;
					} else {
						dbch->frag.len = plen + len;
						dbch->frag.plen = plen;
					}
					bcopy(ld, db_tr->buf, dbch->frag.len);
					buf = NULL;
				}else{
					buf = db_tr->buf;
					poff = ld - (u_int8_t *)buf;
					dbch->frag.buf = NULL;
					dbch->frag.plen = 0;
					dbch->frag.len = 0;
				}
				ld += plen;
			}
			if( buf != NULL){
/* DMA result-code will be written at the tail of packet */
				stat = ((struct fwohci_trailer *)(ld - sizeof(struct fwohci_trailer)))->stat;
				spd = (stat >> 5) & 0x3;
				stat &= 0x1f;
				switch(stat){
				case FWOHCIEV_ACKPEND:
#if 0
					printf("fwohci_arcv: ack pending..\n");
#endif
					/* fall through */
				case FWOHCIEV_ACKCOMPL:
					if( poff != 0 )
						bcopy(buf+poff, buf, plen - 4);
					fw_rcv(&sc->fc, buf, plen - sizeof(struct fwohci_trailer), 0, 0, spd);
					break;
				case FWOHCIEV_BUSRST:
					free(buf, M_DEVBUF);
					if (sc->fc.status != FWBUSRESET) 
						printf("got BUSRST packet!?\n");
					break;
				default:
					device_printf(sc->fc.dev, "Async DMA Receive error err = %02x %s\n", stat, fwohcicode[stat]);
#if 0 /* XXX */
					goto out;
#endif
					break;
				}
			}
			pcnt ++;
		};
out:
		if (resCount == 0) {
			/* done on this buffer */
			fwohci_add_rx_buf(db_tr, dbch->xferq.psize,
						dbch->xferq.flag, 0, NULL);
			dbch->bottom->db[0].db.desc.depend |= z;
			dbch->bottom = db_tr;
			db_tr = STAILQ_NEXT(db_tr, link);
			dbch->top = db_tr;
			dbch->buf_offset = 0;
		} else {
			dbch->buf_offset = dbch->xferq.psize - resCount;
			break;
		}
		/* XXX make sure DMA is not dead */
	}
#if 0
	if (pcnt < 1)
		printf("fwohci_arcv: no packets\n");
#endif
	splx(s);
}
