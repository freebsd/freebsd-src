/*	$Id$	*/
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
 */

#define	LC_SMIT_TIMEOUT	2	/* 2 sec: timeout for a fifo status ready */

static BS_INLINE void bshw_lc_smit_start __P((struct bs_softc *, int, u_int));
static int bshw_lc_smit_fstat __P((struct bs_softc *, int, int));
static BS_INLINE void bshw_lc_smit_stop __P((struct bs_softc *));

/*********************************************************
 * SM FIFO (GENERIC)
 *********************************************************/
void
bshw_smitabort(bsc)
	struct bs_softc *bsc;
{
	if (bsc->sc_hw->hw_flags & BSHW_SMFIFO)
		bshw_lc_smit_stop(bsc);

	bshw_set_count(bsc, 0);
	bsc->sc_flags &= ~BSSMITSTART;
}

void
bs_smit_xfer_end(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct sc_p *sp = &bsc->sc_p;
	u_int count;
	u_char *s;

	bshw_lc_smit_stop(bsc);
	bsc->sc_flags &= ~BSSMITSTART;

	if (ti->ti_phase == DATAPHASE)
	{
		count = bshw_get_count(bsc);
		if (count < (u_int) sp->datalen)
		{
			sp->data += (sp->datalen - count);
			sp->datalen = count;
			/* XXX:
			 * strict double checks!
			 * target   => wd33c93c transfer counts
			 * wd33c93c => memory	transfer counts
			 */
			if ((bsc->sc_dmadir & BSHW_READ) &&
			     count != bsc->sm_tdatalen)
			{
				s = "read count miss";
				goto bad;
			}
			return;
		}
		else if (count == (u_int) sp->datalen)
		{
			return;
		}

		s = "strange count";
	}
	else
		s = "extra smit interrupt";

bad:
	bs_printf(ti, "smit_xfer_end", s);
	ti->ti_error |= BSDMAABNORMAL;
}

/*********************************************************
 * LOGITEC's SMIT TRANSFER
 *********************************************************/

#define	BSHW_LC_FSET	0x36
#define	BSHW_LC_FCTRL	0x44
#define	FCTRL_EN	0x01
#define	FCTRL_WRITE	0x02

#define	SF_ABORT	0x08
#define	SF_RDY		0x10

#define	LC_FSZ		DEV_BSIZE
#define	LC_SFSZ		0x0c
#define	LC_REST		(LC_FSZ - LC_SFSZ)

static BS_INLINE void
bshw_lc_smit_stop(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, BSHW_LC_FCTRL, 0);
	BUS_IOW(cmd_port, CMDP_DMER);
}

static BS_INLINE void
bshw_lc_smit_start(bsc, count, direction)
	struct bs_softc *bsc;
	int count;
	u_int direction;
{
	u_int8_t pval, val = read_wd33c93(bsc, BSHW_LC_FSET);

	bsc->sc_flags |= BSSMITSTART;
	bshw_set_count(bsc, count);

	pval = FCTRL_EN;
	if ((direction & BSHW_READ) == 0)
		pval |= (val & 0xe0) | FCTRL_WRITE;
	write_wd33c93(bsc, BSHW_LC_FCTRL, pval);
	bshw_start_xfer(bsc);
}

static int
bshw_lc_smit_fstat(bsc, wc, read)
	struct bs_softc *bsc;
	int wc, read;
{
	u_int8_t stat;

#define	ALWAYS_ABORT
#ifdef	ALWAYS_ABORT
	if (read == BSHW_READ)
	{
		while (wc -- > 0)
		{
			BUS_IO_WEIGHT;
			stat = BUS_IOR(cmd_port);
			if (stat & SF_RDY)
				return 0;
			if (stat & SF_ABORT)
				return EIO;
		}
	}
	else
	{
#endif	/* ALWAYS_ABORT */
		while (wc -- > 0)
		{
			BUS_IO_WEIGHT;
			stat = BUS_IOR(cmd_port);
			if (stat & SF_ABORT)
				return EIO;
			if (stat & SF_RDY)
				return 0;
		}
#ifdef	ALWAYS_ABORT
	}
#endif	/* ALWAYS_ABORT */

	bs_poll_timeout(bsc, "bshw_lc_smit");
	return EIO;
}

void
bs_lc_smit_xfer(ti, direction)
	struct targ_info *ti;
	u_int direction;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct sc_p *sp = &bsc->sc_p;
	int datalen, count, wc = LC_SMIT_TIMEOUT * 1024 * 1024;
	u_int8_t *data;

	sp->bufp = NULL;
	sp->seglen = 0;
	data = sp->data;
	datalen = sp->datalen;

	bsc->sc_dmadir = direction;
	bshw_set_dma_trans(bsc, ti->ti_cfgflags);
	bshw_lc_smit_start(bsc, sp->datalen, direction);

	if (direction & BSHW_READ)
	{
		do
		{
			if (bshw_lc_smit_fstat(bsc, wc, BSHW_READ))
				break;

			count = (datalen > LC_FSZ ? LC_FSZ : datalen);
			memcopy(ti->sm_vaddr, data, count);
			data += count;
			datalen -= count;
		}
		while (datalen > 0);

		bsc->sm_tdatalen = datalen;
	}
	else
	{
		do
		{
			if (bshw_lc_smit_fstat(bsc, wc, BSHW_WRITE))
				break;

			count = (datalen > LC_SFSZ ? LC_SFSZ : datalen);
			memcopy(data, ti->sm_vaddr, count);
			data += count;
			datalen -= count;

			if (bshw_lc_smit_fstat(bsc, wc, BSHW_WRITE))
				break;

			count = (datalen > LC_REST ? LC_REST : datalen);
			memcopy(data, ti->sm_vaddr + LC_SFSZ, count);
			data += count;
			datalen -= count;
		}
		while (datalen > 0);
	}
}
