/*	$NecBSD: bshw.c,v 1.1 1997/07/18 09:19:03 kmatsuda Exp $	*/
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1994, 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
 * 
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
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
 */
/*
 * Copyright (c) 1994, 1995, 1996 Naofumi HONDA.  All rights reserved.
 *
 * $FreeBSD$
 */

#ifdef	__NetBSD__
#include <dev/isa/isadmareg.h>
#include <i386/Cbus/dev/bs/bsif.h>
#include <i386/Cbus/dev/bs/bshw.lst>
#endif
#ifdef	__FreeBSD__
#include "opt_pc98.h"
#include <i386/isa/ic/i8237.h>
#include <i386/isa/bs/bsif.h>
#include <i386/isa/bs/bshw.lst>
#include <sys/cons.h>
#endif

static struct bs_softc *gbsc;

/**************************************************
 * DECLARATION
 **************************************************/
static void bshw_force_bsmode __P((struct bs_softc *));

/**************************************************
 * STATIC VAL
 **************************************************/
static int irq_tbl[] = { 3, 5, 6, 9, 12, 13 };

/**************************************************
 * SCSI CMD BRANCH
 **************************************************/
#define	RS	(BSSAT | BSSMIT | BSLINK | BSREAD)
#define	WS	(BSSAT | BSSMIT | BSLINK)
#define	EOK	(BSERROROK)

u_int8_t bshw_cmd[256] = {
/*   0  1   2   3   4   5   6   7   8   9   A   B   C   E   D   F */
/*0*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,RS ,0  ,WS ,0  ,0  ,0  ,0  ,0  ,
/*1*/0  ,0  ,EOK,0  ,0  ,0  ,0  ,0  ,0  ,0  ,EOK,0  ,0  ,0  ,0  ,0  ,
/*2*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,RS ,0  ,WS ,0  ,0  ,0  ,0  ,0  ,
/*3*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*4*/0  ,0  ,EOK,EOK,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*5*/0  ,0  ,0  ,0  ,EOK,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*6*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*7*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*8*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*9*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*A*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*B*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*C*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*D*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*E*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
/*F*/0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,0  ,
};

#undef	RS
#undef	WS
#undef	EOK

/**********************************************
 * init
 **********************************************/
static void
bshw_force_bsmode(bsc)
	struct bs_softc *bsc;
{

	if (bsc->sc_flags & BSBSMODE)
		return;
	bsc->sc_flags |= BSBSMODE;

	/*
	 * If you have memory over 16M, some stupid boards always force to
	 * use the io polling mode. Check such a case and change mode into
	 * bus master DMA. However this depends heavily on the board's
	 * specifications!
	 */

	if (bsc->sc_hw->dma_init && ((*bsc->sc_hw->dma_init)(bsc)))
		printf("%s change mode using external DMA (%x)\n",
		    bsc->sc_dvname, (u_int)read_wd33c93(bsc, 0x37));
}

#define	RESET_DEFAULT	2000

int
bshw_chip_reset(bsc)
	struct bs_softc *bsc;
{
	int ct;
	u_int8_t aux;

	bshw_lock(bsc);

	bshw_abort_cmd(bsc);
	delay(10000);

	bshw_get_auxstat(bsc);
	bshw_get_busstat(bsc);

	write_wd33c93(bsc, wd3s_oid, IDR_EHP | bsc->sc_cspeed | bsc->sc_hostid);
	write_wd33c93(bsc, wd3s_cmd, WD3S_RESET);

	for (ct = RESET_DEFAULT; ct > 0; ct--)
	{
		aux = bshw_get_auxstat(bsc);
		if (aux != 0xff && (aux & STR_INT))
		{
			if (bshw_get_busstat(bsc) == 0)
				break;

			write_wd33c93(bsc, wd3s_cmd, WD3S_RESET);
		}
		delay(1);
	}

	if (ct == 0)
	{
		bshw_unlock(bsc);
		return ENXIO;
	}

	bshw_force_bsmode(bsc);

	write_wd33c93(bsc, wd3s_tout, BSHW_SEL_TIMEOUT);
	write_wd33c93(bsc, wd3s_sid, SIDR_RESEL);
	bsc->sc_flags |= BSDMATRANSFER;
	write_wd33c93(bsc, wd3s_ctrl, CR_DEFAULT);
	write_wd33c93(bsc, wd3s_synch, 0);

	bshw_get_auxstat(bsc);
	bsc->sc_busstat = bshw_get_busstat(bsc);
	bshw_unlock(bsc);

	return 0;
}

/* scsi bus hard reset */
#define	TWIDDLEWAIT	10000
static int tw_pos;
static char tw_chars[] = "|/-\\";

/* this is some jokes */
static void
twiddle_wait(void)
{

	cnputc('\b');
	cnputc(tw_chars[tw_pos++]);
	tw_pos %= (sizeof(tw_chars) - 1);
	delay(TWIDDLEWAIT);
}

static void bshw_set_vsp __P((struct bs_softc *, u_int, u_int8_t));

static void
bshw_set_vsp(bsc, chan, spva)
	struct bs_softc *bsc;
	u_int chan;
	u_int8_t spva;
{
	struct bshw *hw = bsc->sc_hw;

	if (hw->sregaddr == 0)
		return;

	write_wd33c93(bsc, hw->sregaddr + chan, spva);
	if (hw->hw_flags & BSHW_DOUBLE_DMACHAN)
		write_wd33c93(bsc, hw->sregaddr + chan + 8, spva);
}

void
bshw_bus_reset(bsc)
	struct bs_softc *bsc;
{
	struct targ_info *ti;
	int i, lpc;

	if (bsc->sc_RSTdelay == 0)
		bsc->sc_RSTdelay = 6 * 1000 * 1000;
	else
	{
		/* XXX:
		 * second time reset will be requested by hardware failuer.
		 */
		bsc->sc_RSTdelay = 12 * 1000 * 1000;
	}

	bshw_lock(bsc);
	write_wd33c93(bsc, wd3s_mbank, (bsc->sc_membank | MBR_RST) & ~MBR_IEN);
	delay(500000);
	write_wd33c93(bsc, wd3s_mbank, (bsc->sc_membank) & ~MBR_IEN);
	bshw_unlock(bsc);

	for (lpc = 0; lpc < 2; lpc ++)
	{
		cnputc(' ');
		for (i = 0; i <= bsc->sc_RSTdelay / TWIDDLEWAIT; i++)
			twiddle_wait();
		cnputc('\b');

		(void) read_wd33c93(bsc, wd3s_auxc);

		delay(10000);

		if ((read_wd33c93(bsc, wd3s_auxc) & AUXCR_RRST) == 0)
			break;

		printf("\nreset state still continue, wait ...");
	}

	for (i = 0; i < NTARGETS; i++)
	{
		if ((ti = bsc->sc_ti[i]) != NULL)
		{
			ti->ti_sync = 0;
			bshw_set_vsp(bsc, i, 0);
		}
	}
}

/* probe */
int
bshw_board_probe(bsc, drq, irq)
	struct bs_softc *bsc;
	u_int *drq;
	u_int *irq;
{

	gbsc = bsc;
#ifdef	SHOW_PORT
	bshw_print_port(bsc);
#endif	/* SHOW_PORT */

	bsc->sc_hostid = (read_wd33c93(bsc, wd3s_auxc) & AUXCR_HIDM);

	if ((*irq) == IRQUNK)
		*irq = irq_tbl[(read_wd33c93(bsc, wd3s_auxc) >> 3) & 7];

	if ((*drq) == DRQUNK)
		*drq = BUS_IOR(cmd_port) & 3;

	bsc->sc_dmachan = *drq;
	bsc->sc_irq = (*irq);

	bsc->sc_membank = read_wd33c93(bsc, wd3s_mbank);
	bsc->sc_membank &= ~MBR_RST;
	bsc->sc_membank |= MBR_IEN;

	bsc->sc_cspeed = (read_wd33c93(bsc, wd3s_oid) & (~IDR_IDM));
	switch (BSC_CHIP_CLOCK(bsc->sc_cfgflags))
	{
	case 0:
		break;

	case 1:
		bsc->sc_cspeed &= ~(IDR_FS_12_15 | IDR_FS_15_20);
		break;

	case 2:
		bsc->sc_cspeed &= ~(IDR_FS_12_15 | IDR_FS_15_20);
		bsc->sc_cspeed |= IDR_FS_12_15;
		break;

	case 3:
		bsc->sc_cspeed &= ~(IDR_FS_12_15 | IDR_FS_15_20);
		bsc->sc_cspeed |= IDR_FS_15_20;
		break;
	}

	/* XXX: host id fixed(7) */
	bsc->sc_hostid = 7;

	if (bshw_chip_reset(bsc))
		return ENXIO;

	return 0;
}

/*
 * XXX:
 * Assume the board clock rate must be 20Mhz (always satisfied, maybe)!
 * Only 10M/s 6.6M/s 5.0M/s 3.3M/s for synchronus transfer speed set.
 */
#define	ILLEGAL_SYNCH
#ifdef	ILLEGAL_SYNCH
/*  A  10    6.6   5.0   4.0   3.3   2.8   2.5   2.0  M/s */
/*  X  100   150   200   250   300   350   400   500  ns  */
static u_int bshw_scsi_period[] =
   {0, 25,   37,   50,   62,   75,   87,   100,  125};
static u_int8_t bshw_chip_pval[] =
   {0, 0xa0, 0xb0, 0x20, 0xd0, 0x30, 0xf0, 0x40, 0x50};
#else	/* !ILLEGAL_SYNCH */
/*  A  10    6.6   5.0   3.3   2.5 M/s */
/*  X  100   150   200   300   400 ns  */
static u_int bshw_scsi_period[] =
   {0, 25,   37,   50,   75,   100};
static u_int8_t bshw_chip_pval[] =
   {0, 0xa0, 0xb0, 0x20, 0x30, 0x40};
#endif	/* !ILLEGAL_SYNCH */

void
bshw_adj_syncdata(sdp)
	struct syncdata *sdp;
{
	int i;

	if (sdp->offset == 0 || sdp->period < 25 || sdp->period > 100)
		sdp->offset = sdp->period = 0;
	else
	{
		for (i = 0; sdp->period > bshw_scsi_period[i] + 2; i ++)
			;
		sdp->period = bshw_scsi_period[i];
	}
}

void
bshw_set_synchronous(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{
	struct syncdata sd;
	int i;

	sd = ti->ti_syncnow;
	bshw_adj_syncdata(&sd);
	for (i = 0; sd.period != bshw_scsi_period[i]; i++)
		;

	ti->ti_sync = ((sd.offset & 0x0f) | bshw_chip_pval[i]);
	bshw_set_vsp(bsc, ti->ti_id, ti->ti_sync);

	if (bsc->sc_nexus == ti)
		bshw_set_sync_reg(bsc, ti->ti_sync);
}

/* ctrl reg */
void
bshw_setup_ctrl_reg(bsc, flags)
	struct bs_softc *bsc;
	u_int flags;
{
	u_int8_t regval;

	regval = (flags & BS_SCSI_NOPARITY) ? CR_DEFAULT : CR_DEFAULT_HP;
	if (bsc->sc_flags & BSDMATRANSFER)
		regval |= CR_DMA;
	write_wd33c93(bsc, wd3s_ctrl, regval);
}

/* sat command */
void
bshw_issue_satcmd(bsc, cb, link)
	struct bs_softc *bsc;
	struct bsccb *cb;
	int link;
{
	int i;

	BUS_IOW(addr_port, wd3s_cdb);
	for (i = 0; i < cb->cmdlen - 1; i++)
		BUS_IOW(ctrl_port, cb->cmd[i]);
	BUS_IOW(ctrl_port, cb->cmd[i] | (link ? 1 : 0));
}

/* lock */
void
bshw_lock(bsc)
	struct bs_softc *bsc;
{

	bsc->sc_hwlock++;
	write_wd33c93(bsc, wd3s_mbank, bsc->sc_membank & (~MBR_IEN));
}

void
bshw_unlock(bsc)
	struct bs_softc *bsc;
{

	if ((--bsc->sc_hwlock) <= 0)
		write_wd33c93(bsc, wd3s_mbank, bsc->sc_membank);
}

/**********************************************
 * DMA OPERATIONS
 **********************************************/
#ifdef	__NetBSD__
#include <i386/Cbus/dev/bs/bshw_dma.c>
#include <i386/Cbus/dev/bs/bshw_pdma.c>
#endif
#ifdef	__FreeBSD__
#include <i386/isa/bs/bshw_dma.c>
#include <i386/isa/bs/bshw_pdma.c>
#endif

/**********************************************
 * DEBUG
 **********************************************/
/* misc */
void
bshw_print_port(bsc)
	struct bs_softc * bsc;
{
	int i, j;
	int port = 0x0;

	if (bsc == NULL)
		bsc = gbsc;

	printf("\n");
	for (j = 0; j <= 0x70; j += 0x10)
	{
		printf("port %x: ", port);
		for (i = 0; i < 0x10; i++)
			printf("%x ", (u_int) read_wd33c93(bsc, port++));
		printf("\n");
	}
}
