/*-
 * Copyright (c) 2005 Poul-Henning Kamp <phk@FreeBSD.org>
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
 * High-level driver for µPD7210 based GPIB cards.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#  define	IBDEBUG
#  undef	IBDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <isa/isavar.h>

#include <dev/ieee488/ugpib.h>

#define UPD7210_SW_DRIVER
#include <dev/ieee488/upd7210.h>

static MALLOC_DEFINE(M_IBFOO, "IBFOO", "IBFOO");


/* ibfoo API */

#include <dev/ieee488/ibfoo_int.h>

struct handle {
	LIST_ENTRY(handle)	list;
	int			handle;
	int			pad;
	int			sad;
	struct timeval		timeout;
	int			eot;
	int			eos;
	int			dma;
};

struct ibfoo {
	struct upd7210		*upd7210;
	LIST_HEAD(,handle)	handles;
	struct unrhdr		*unrhdr;

	enum {
		IDLE,
		PIO_IDATA,
		DMA_IDATA,
		PIO_ODATA,
		PIO_CMD
	}			mode;

	struct timeval		deadline;

	struct handle		*rdh;		/* addressed for read */
	struct handle		*wrh;		/* addressed for write */

	int		 	doeoi;

	u_char			*buf;
	u_int			buflen;
};

static struct timeval timeouts[] = {
	[TNONE] =	{    0,      0},
	[T10us] =	{    0,     10},
	[T30us] =	{    0,     30},
	[T100us] =	{    0,    100},
	[T300us] =	{    0,    300},
	[T1ms] =	{    0,   1000},
	[T3ms] =	{    0,   3000},
	[T10ms] =	{    0,  10000},
	[T30ms] =	{    0,  30000},
	[T100ms] =	{    0, 100000},
	[T300ms] =	{    0, 300000},
	[T1s] =		{    1,      0},
	[T3s] =		{    3,      0},
	[T10s] =	{   10,      0},
	[T30s] =	{   30,      0},
	[T100s] =	{  100,      0},
	[T300s] =	{  300,      0},
	[T1000s] =	{ 1000,      0}
};

static const u_int max_timeouts = sizeof timeouts / sizeof timeouts[0];

static int
deadyet(struct ibfoo *ib)
{
	struct timeval tv;

	if (!timevalisset(&ib->deadline))
		return (0);

	getmicrouptime(&tv);
	if (timevalcmp(&ib->deadline, &tv, <)) {
printf("DEADNOW\n");
		return (1);
	}

	return (0);
}

typedef int ibhandler_t(struct handle *h, struct upd7210 *u, struct ibfoo_iocarg *ap);

static int
gpib_ib_irq(struct upd7210 *u, int intr __unused)
{
	struct ibfoo *ib;

	ib = u->ibfoo;

	switch (ib->mode) {
	case PIO_CMD:
		if (!(u->rreg[ISR2] & IXR2_CO))
			return (0);
		if (ib->buflen == 0)
			break;
		upd7210_wr(u, CDOR, *ib->buf);
		ib->buf++;
		ib->buflen--;
		return (1);
	case PIO_IDATA:
		if (!(u->rreg[ISR1] & IXR1_DI))
			return (0);
		*ib->buf = upd7210_rd(u, DIR);
		ib->buf++;
		ib->buflen--;
		if (ib->buflen == 0 || (u->rreg[ISR1] & IXR1_ENDRX))
			break;
		return (1);
	case PIO_ODATA:
		if (!(u->rreg[ISR1] & IXR1_DO))
			return (0);
		if (ib->buflen == 0)
			break;
		if (ib->buflen == 1 && ib->doeoi)
			upd7210_wr(u, AUXMR, AUXMR_SEOI);
		upd7210_wr(u, CDOR, *ib->buf);
		ib->buf++;
		ib->buflen--;
		return (1);
	case DMA_IDATA:
		if (!(u->rreg[ISR1] & IXR1_ENDRX))
			return (0);
		break;
	default:
		return (0);
	}
	upd7210_wr(u, IMR1, 0);
	upd7210_wr(u, IMR2, 0);
	ib->mode = IDLE;
	wakeup(ib);
	return (1);
}

static void
config_eos(struct upd7210 *u, struct handle *h)
{
	int i;

	i = 0;
	if (h->eos & REOS) {
		upd7210_wr(u, EOSR, h->eos & 0xff);
		i |= AUXA_REOS;
	}
	if (h->eos & XEOS) {
		upd7210_wr(u, EOSR, h->eos & 0xff);
		i |= AUXA_XEOS;
	}
	if (h->eos & BIN)
		i |= AUXA_BIN;
	upd7210_wr(u, AUXRA, C_AUXA | i);
}

/*
 * Look up the handle, and set the deadline if the handle has a timeout.
 */
static int
gethandle(struct upd7210 *u, struct ibfoo_iocarg *ap, struct handle **hp)
{
	struct ibfoo *ib;
	struct handle *h;

	KASSERT(ap->__field & __F_HANDLE, ("gethandle without __F_HANDLE"));
	ib = u->ibfoo;
	LIST_FOREACH(h, &ib->handles, list) {
		if (h->handle == ap->handle) {
			*hp = h;
			if (timevalisset(&h->timeout)) {
				getmicrouptime(&ib->deadline);
				timevaladd(&ib->deadline, &h->timeout);
			} else {
				timevalclear(&ib->deadline);
			}
			return (0);
		}
	}
	ap->__iberr = EARG;
	return (1);
}

static int
pio_cmd(struct upd7210 *u, u_char *cmd, int len)
{
	int i;
	struct ibfoo *ib;

	ib = u->ibfoo;

	if (ib->rdh != NULL || ib->wrh != NULL) {
		upd7210_take_ctrl_async(u);
		ib->rdh = NULL;
		ib->wrh = NULL;
	}
	mtx_lock(&u->mutex);
	ib->mode = PIO_CMD;
	ib->buf = cmd;
	ib->buflen = len;
	upd7210_wr(u, IMR2, IXR2_CO);

	gpib_ib_irq(u, 1);

	while (1) {
		i = msleep(ib, &u->mutex, PZERO | PCATCH, "gpib_cmd", hz/10);
		if (i == EINTR)
			break;
		if (u->rreg[ISR1] & IXR1_ERR)
			break;
		if (!ib->buflen)
			break;
		if (deadyet(ib))
			break;
	}
	upd7210_wr(u, IMR2, 0);
	mtx_unlock(&u->mutex);
	return (0);
}

static int
pio_odata(struct upd7210 *u, u_char *data, int len)
{
	int i;
	struct ibfoo *ib;

	ib = u->ibfoo;

	if (len == 0)
		return (0);
	mtx_lock(&u->mutex);
	ib->mode = PIO_ODATA;
	ib->buf = data;
	ib->buflen = len;
	upd7210_wr(u, IMR1, IXR1_DO);

	gpib_ib_irq(u, 1);

	while (1) {
		i = msleep(ib, &u->mutex, PZERO | PCATCH, "gpib_out", hz/100);
		if (i == EINTR || i == 0)
			break;
#if 0
		if (u->rreg[ISR1] & IXR1_ERR)
			break;
#endif
		if (deadyet(ib))
			break;
	}
	ib->mode = IDLE;
	upd7210_wr(u, IMR1, 0);
	upd7210_wr(u, IMR2, 0);
	mtx_unlock(&u->mutex);
	return (len - ib->buflen);
}

static int
pio_idata(struct upd7210 *u, u_char *data, int len)
{
	int i;
	struct ibfoo *ib;

	ib = u->ibfoo;

	mtx_lock(&u->mutex);
	ib->mode = PIO_IDATA;
	ib->buf = data;
	ib->buflen = len;
	upd7210_wr(u, IMR1, IXR1_DI);
	while (1) {
		i = msleep(ib, &u->mutex, PZERO | PCATCH,
		    "ib_pioidata", hz/100);
		if (i == EINTR || i == 0)
			break;
		if (deadyet(u->ibfoo))
			break;
	}
	ib->mode = IDLE;
	upd7210_wr(u, IMR1, 0);
	upd7210_wr(u, IMR2, 0);
	mtx_unlock(&u->mutex);
	if (deadyet(u->ibfoo)) {
		return (-1);
	} else {
		return (len - ib->buflen);
	}
}

static int
dma_idata(struct upd7210 *u, u_char *data, int len)
{
	int i1, i2, i, j;
	struct ibfoo *ib;

	ib = u->ibfoo;
	ib->mode = DMA_IDATA;
	upd7210_wr(u, IMR1, IXR1_ENDRX);
	mtx_lock(&Giant);
	isa_dmastart(ISADMA_READ, data, len, u->dmachan);
	mtx_unlock(&Giant);
	mtx_lock(&u->mutex);
	upd7210_wr(u, IMR2, IMR2_DMAI);
	while (1) {
		i = msleep(ib, &u->mutex, PZERO | PCATCH,
		    "gpib_idata", hz/100);
		if (i == EINTR)
			break;
		if (isa_dmatc(u->dmachan))
			break;
		if (i == EWOULDBLOCK) {
			i1 = upd7210_rd(u, ISR1);
			i2 = upd7210_rd(u, ISR2);
		} else {
			i1 = u->rreg[ISR1];
			i2 = u->rreg[ISR2];
		}
		if (i1 & IXR1_ENDRX)
			break;
		if (deadyet(ib))
			break;
	}
	upd7210_wr(u, IMR1, 0);
	upd7210_wr(u, IMR2, 0);
	mtx_unlock(&u->mutex);
	mtx_lock(&Giant);
	j = isa_dmastatus(u->dmachan);
	isa_dmadone(ISADMA_READ, data, len, u->dmachan);
	mtx_unlock(&Giant);
	if (deadyet(ib)) {
		return (-1);
	} else {
		return (len - j);
	}
}

#define ibask NULL
#define ibbna NULL
#define ibcac NULL
#define ibclr NULL
#define ibcmd NULL
#define ibcmda NULL
#define ibconfig NULL

static int
ibdev(struct handle *h, struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct ibfoo *ib;

	if (ap->pad < 0 ||
	    ap->pad > 30 ||
	    (ap->sad != 0 && ap->sad < 0x60) ||
	    ap->sad > 126) {
		ap->__retval = -1;
		ap->__iberr = EARG;
		return (0);
	}
	
	ib = u->ibfoo;
	h = malloc(sizeof *h, M_IBFOO, M_ZERO | M_WAITOK);
	h->handle = alloc_unr(ib->unrhdr);
	LIST_INSERT_HEAD(&ib->handles, h, list);
	h->pad = ap->pad;
	h->sad = ap->sad;
	h->timeout = timeouts[ap->tmo];
	h->eot = ap->eot;
	h->eos = ap->eos;
	ap->__retval = h->handle;
	return (0);
}

#define ibdiag NULL

static int
ibdma(struct handle *h, struct upd7210 *u, struct ibfoo_iocarg *ap)
{

	h->dma = ap->v;
	return (0);
}

static int
ibeos(struct handle *h, struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct ibfoo *ib;

	ib = u->ibfoo;
	h->eos = ap->eos;
	if (ib->rdh == h)
		config_eos(u, h);
	ap->__retval = 0;
	return (0);
}

static int
ibeot(struct handle *h, struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct ibfoo *ib;

	ib = u->ibfoo;

	h->eot = ap->eot;
	return (0);
}

#define ibevent NULL
#define ibfind NULL
#define ibgts NULL
#define ibist NULL
#define iblines NULL
#define ibllo NULL
#define ibln NULL
#define ibloc NULL
#define ibonl NULL
#define ibpad NULL
#define ibpct NULL
#define ibpoke NULL
#define ibppc NULL

static int
ibrd(struct handle *h, struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct ibfoo *ib;
	u_char buf[10], *bp;
	int i, j, error, bl, bc;
	u_char *dp;

	ib = u->ibfoo;
	bl = ap->cnt;
	if (bl > PAGE_SIZE)
		bl = PAGE_SIZE;
	bp = malloc(bl, M_IBFOO, M_WAITOK);

	if (ib->rdh != h) {
		i = 0;
		buf[i++] = UNT;
		buf[i++] = UNL;
		buf[i++] = LAD | 0;
		buf[i++] = TAD | h->pad;
		if (h->sad)
			buf[i++] = h->sad;
		i = pio_cmd(u, buf, i);
		config_eos(u, h);
		ib->rdh = h;
		ib->wrh = NULL;
		upd7210_goto_standby(u);
	}
	ap->__ibcnt = 0;
	dp = ap->buffer;
	bc = ap->cnt;
	error = 0;
	while (bc > 0) {
		j = imin(bc, PAGE_SIZE);
		if (h->dma)
			i = dma_idata(u, bp, j);
		else
			i = pio_idata(u, bp, j);
		if (i <= 0)
			break;
		error = copyout(bp, dp , i);
		if (error)
			break;
		ap->__ibcnt += i;
		if (i != j)
			break;
		bc -= i;
		dp += i;
	}
	free(bp, M_IBFOO);
	ap->__retval = 0;
	return (error);
}

#define ibrda NULL
#define ibrdf NULL
#define ibrdkey NULL
#define ibrpp NULL
#define ibrsc NULL
#define ibrsp NULL
#define ibrsv NULL
#define ibsad NULL
#define ibsgnl NULL
#define ibsic NULL
#define ibsre NULL
#define ibsrq NULL
#define ibstop NULL

static int
ibtmo(struct handle *h, struct upd7210 *u, struct ibfoo_iocarg *ap)
{

	h->timeout = timeouts[ap->tmo];
	return (0);
}

#define ibtrap NULL
#define ibtrg NULL
#define ibwait NULL

static int
ibwrt(struct handle *h, struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct ibfoo *ib;
	u_char buf[10], *bp;
	int i;

	ib = u->ibfoo;
	bp = malloc(ap->cnt, M_IBFOO, M_WAITOK);
	i = copyin(ap->buffer, bp, ap->cnt);
	if (i) {
		free(bp, M_IBFOO);
		return (i);
	}
	if (ib->wrh != h) {
		i = 0;
		buf[i++] = UNT;
		buf[i++] = UNL;
		buf[i++] = LAD | h->pad;
		if (h->sad)
			buf[i++] = LAD | TAD | h->sad;
		buf[i++] = TAD | 0;
		i = pio_cmd(u, buf, i);
		ib->rdh = NULL;
		ib->wrh = h;
		upd7210_goto_standby(u);
		config_eos(u, h);
	}
	ib->doeoi = h->eot;
	i = pio_odata(u, bp, ap->cnt);
	ap->__ibcnt = i;
	ap->__retval = 0;
	free(bp, M_IBFOO);
	return (0);
}

#define ibwrta NULL
#define ibwrtf NULL
#define ibwrtkey NULL
#define ibxtrc NULL

static struct ibhandler {
	const char 	*name;
	ibhandler_t	*func;
	u_int		args;
} ibhandlers[] = {
	[__ID_IBASK] =		{ "ibask",	ibask,		__F_HANDLE | __F_OPTION | __F_RETVAL },
	[__ID_IBBNA] =		{ "ibbna",	ibbna,		__F_HANDLE | __F_BDNAME },
	[__ID_IBCAC] =		{ "ibcac",	ibcac,		__F_HANDLE | __F_V },
	[__ID_IBCLR] =		{ "ibclr",	ibclr,		__F_HANDLE },
	[__ID_IBCMDA] =		{ "ibcmda",	ibcmda,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBCMD] =		{ "ibcmd",	ibcmd,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBCONFIG] =	{ "ibconfig",	ibconfig,	__F_HANDLE | __F_OPTION | __F_VALUE },
	[__ID_IBDEV] =		{ "ibdev",	ibdev,		__F_BOARDID | __F_PAD | __F_SAD | __F_TMO | __F_EOT | __F_EOS },
	[__ID_IBDIAG] =		{ "ibdiag",	ibdiag,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBDMA] =		{ "ibdma",	ibdma,		__F_HANDLE | __F_V },
	[__ID_IBEOS] =		{ "ibeos",	ibeos,		__F_HANDLE | __F_EOS },
	[__ID_IBEOT] =		{ "ibeot",	ibeot,		__F_HANDLE | __F_EOT },
	[__ID_IBEVENT] =	{ "ibevent",	ibevent,	__F_HANDLE | __F_EVENT },
	[__ID_IBFIND] =		{ "ibfind",	ibfind,		__F_BDNAME },
	[__ID_IBGTS] =		{ "ibgts",	ibgts,		__F_HANDLE | __F_V },
	[__ID_IBIST] =		{ "ibist",	ibist,		__F_HANDLE | __F_V },
	[__ID_IBLINES] =	{ "iblines",	iblines,	__F_HANDLE | __F_LINES },
	[__ID_IBLLO] =		{ "ibllo",	ibllo,		__F_HANDLE },
	[__ID_IBLN] =		{ "ibln",	ibln,		__F_HANDLE | __F_PADVAL | __F_SADVAL | __F_LISTENFLAG },
	[__ID_IBLOC] =		{ "ibloc",	ibloc,		__F_HANDLE },
	[__ID_IBONL] =		{ "ibonl",	ibonl,		__F_HANDLE | __F_V },
	[__ID_IBPAD] =		{ "ibpad",	ibpad,		__F_HANDLE | __F_V },
	[__ID_IBPCT] =		{ "ibpct",	ibpct,		__F_HANDLE },
	[__ID_IBPOKE] =		{ "ibpoke",	ibpoke,		__F_HANDLE | __F_OPTION | __F_VALUE },
	[__ID_IBPPC] =		{ "ibppc",	ibppc,		__F_HANDLE | __F_V },
	[__ID_IBRDA] =		{ "ibrda",	ibrda,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBRDF] =		{ "ibrdf",	ibrdf,		__F_HANDLE | __F_FLNAME },
	[__ID_IBRDKEY] =	{ "ibrdkey",	ibrdkey,	__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBRD] =		{ "ibrd",	ibrd,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBRPP] =		{ "ibrpp",	ibrpp,		__F_HANDLE | __F_PPR },
	[__ID_IBRSC] =		{ "ibrsc",	ibrsc,		__F_HANDLE | __F_V },
	[__ID_IBRSP] =		{ "ibrsp",	ibrsp,		__F_HANDLE | __F_SPR },
	[__ID_IBRSV] =		{ "ibrsv",	ibrsv,		__F_HANDLE | __F_V },
	[__ID_IBSAD] =		{ "ibsad",	ibsad,		__F_HANDLE | __F_V },
	[__ID_IBSGNL] =		{ "ibsgnl",	ibsgnl,		__F_HANDLE | __F_V },
	[__ID_IBSIC] =		{ "ibsic",	ibsic,		__F_HANDLE },
	[__ID_IBSRE] =		{ "ibsre",	ibsre,		__F_HANDLE | __F_V },
	[__ID_IBSRQ] =		{ "ibsrq",	ibsrq,		__F_FUNC },
	[__ID_IBSTOP] =		{ "ibstop",	ibstop,		__F_HANDLE },
	[__ID_IBTMO] =		{ "ibtmo",	ibtmo,		__F_HANDLE | __F_TMO },
	[__ID_IBTRAP] =		{ "ibtrap",	ibtrap,		__F_MASK | __F_MODE },
	[__ID_IBTRG] =		{ "ibtrg",	ibtrg,		__F_HANDLE },
	[__ID_IBWAIT] =		{ "ibwait",	ibwait,		__F_HANDLE | __F_MASK },
	[__ID_IBWRTA] =		{ "ibwrta",	ibwrta,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBWRTF] =		{ "ibwrtf",	ibwrtf,		__F_HANDLE | __F_FLNAME },
	[__ID_IBWRTKEY] =	{ "ibwrtkey",	ibwrtkey,	__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBWRT] =		{ "ibwrt",	ibwrt,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBXTRC] =		{ "ibxtrc",	ibxtrc,		__F_HANDLE | __F_BUFFER | __F_CNT },
};

static u_int max_ibhandler = sizeof ibhandlers / sizeof ibhandlers[0];

static int
gpib_ib_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct upd7210 *u;
	struct ibfoo *ib;
	int error;

	u = dev->si_drv1;

	mtx_lock(&u->mutex);
	if (u->busy) {
		mtx_unlock(&u->mutex);
		return (EBUSY);
	}
	u->busy = 1;
	mtx_unlock(&u->mutex);

	mtx_lock(&Giant);
	error = isa_dma_acquire(u->dmachan);
	if (!error) {
		error = isa_dma_init(u->dmachan, PAGE_SIZE, M_WAITOK);
		if (error)
			isa_dma_release(u->dmachan);
	}
	mtx_unlock(&Giant);
	if (error) {
		mtx_lock(&u->mutex);
		u->busy = 0;
		mtx_unlock(&u->mutex);
		return (error);
	}

	ib = malloc(sizeof *ib, M_IBFOO, M_WAITOK | M_ZERO);
	LIST_INIT(&ib->handles);
	ib->unrhdr = new_unrhdr(0, INT_MAX);
	dev->si_drv2 = ib;
	ib->upd7210 = u;
	u->ibfoo = ib;
	u->irq = gpib_ib_irq;

	upd7210_wr(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	DELAY(1000);
	upd7210_wr(u, IMR1, 0x00);
	upd7210_wr(u, IMR2, 0x00);
	upd7210_wr(u, SPMR, 0x00);
	upd7210_wr(u, ADR, 0x00);
	upd7210_wr(u, ADR, ADR_ARS | ADR_DL | ADR_DT);
	upd7210_wr(u, ADMR, ADMR_ADM0 | ADMR_TRM0 | ADMR_TRM1);
	upd7210_wr(u, EOSR, 0x00);
	upd7210_wr(u, AUXMR, C_ICR | 8);
	upd7210_wr(u, AUXMR, C_PPR | PPR_U);
	upd7210_wr(u, AUXMR, C_AUXA);
	upd7210_wr(u, AUXMR, C_AUXB + 3);
	upd7210_wr(u, AUXMR, C_AUXE + 0);
	upd7210_wr(u, AUXMR, AUXMR_PON);
	upd7210_wr(u, AUXMR, AUXMR_CIFC);
	DELAY(100);
	upd7210_wr(u, AUXMR, AUXMR_SIFC);
	upd7210_wr(u, AUXMR, AUXMR_SREN);
	return (0);
}

static int
gpib_ib_close(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct upd7210 *u;
	struct ibfoo *ib;

	u = dev->si_drv1;
	ib = dev->si_drv2;
	/* XXX: assert pointer consistency */

	u->ibfoo = NULL;
	/* XXX: free handles */
	dev->si_drv2 = NULL;
	free(ib, M_IBFOO);

	mtx_lock(&Giant);
	isa_dma_release(u->dmachan);
	mtx_unlock(&Giant);
	mtx_lock(&u->mutex);
	u->busy = 0;
	upd7210_wr(u, IMR1, 0x00);
	upd7210_wr(u, IMR2, 0x00);
	upd7210_wr(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	mtx_unlock(&u->mutex);
	return (0);
}

static int
gpib_ib_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct ibfoo_iocarg *ap;
	struct ibhandler *ih;
	struct handle *h;
	struct upd7210 *u;
	int error;

	u = dev->si_drv1;

	if (cmd != GPIB_IBFOO)
		return (ENOIOCTL);

	ap = (void *)data;
	if (ap->__ident < 0 || ap->__ident >= max_ibhandler)
		return (EINVAL);
	ih = &ibhandlers[ap->__ident];
	if (ap->__field != ih->args)
		return (EINVAL);

	if (ap->__field & __F_TMO) {
		if (ap->tmo < 0 || ap->tmo >= max_timeouts) {
			ap->__retval = -1;
			ap->__iberr = EARG;
			return (0);
		}
	}

	if (ap->__field & __F_EOS) {
		if (ap->eos & ~(REOS | XEOS | BIN | 0xff)) {
			ap->__retval = -1;
			ap->__iberr = EARG;
			return (0);
		}
		if (ap->eos & (REOS | XEOS)) {
			if ((ap->eos & (BIN | 0x80)) == 0x80) {
				ap->__retval = -1;
				ap->__iberr = EARG;
				return (0);
			}
		} else if (ap->eos != 0) {
			ap->__retval = -1;
			ap->__iberr = EARG;
			return (0);
		}
	}

	mtx_lock(&u->mutex);
	while(u->busy != 1) {
		error = msleep(u->ibfoo, &u->mutex, PZERO | PCATCH,
		    "gpib_ibioctl", 0);
		if (error) {
			mtx_unlock(&u->mutex);
			return (EINTR);
		}
	}
	u->busy = 2;
	mtx_unlock(&u->mutex);

#ifdef IBDEBUG
	if (ih->name != NULL)
		printf("%s(", ih->name);
	else
		printf("ibinvalid(");
	printf("[0x%x]", ap->__field);
	if (ap->__field & __F_HANDLE)	printf(" handle=%d", ap->handle);
	if (ap->__field & __F_EOS)	printf(" eos=%d", ap->eos);
	if (ap->__field & __F_EOT)	printf(" eot=%d", ap->eot);
	if (ap->__field & __F_TMO)	printf(" tmo=%d", ap->tmo);
	if (ap->__field & __F_PAD)	printf(" pad=%d", ap->pad);
	if (ap->__field & __F_SAD)	printf(" sad=%d", ap->sad);
	if (ap->__field & __F_BUFFER)	printf(" buffer=%p", ap->buffer);
	if (ap->__field & __F_CNT)	printf(" cnt=%ld", ap->cnt);
	/* XXX more ... */
	printf(")\n");
#endif

	if (ap->__field & __F_HANDLE) {
		if (gethandle(u, ap, &h)) {
			error = 0; /* XXX iberr */
			goto bail;
		}
	} else	
		h = NULL;
	ap->__iberr = 0;
	error = EOPNOTSUPP;
	if (ih->func != NULL)
		error = ih->func(h, u, ap);
	if (error) {
		ap->__retval = EDVR;
		ap->__iberr = EDVR;
		ap->__ibcnt = error;
	} else if (ap->__iberr) {
		ap->__retval = -1;
	}
#ifdef IBDEBUG
	printf("%s(...) = %d (error=%d)\n", ih->name, ap->__retval, error);
#endif

bail:
	mtx_lock(&u->mutex);
	u->busy = 1;
	wakeup(u->ibfoo);
	mtx_unlock(&u->mutex);
	return (error);
}

struct cdevsw gpib_ib_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"gpib_ib",
	.d_open	=	gpib_ib_open,
	.d_ioctl =	gpib_ib_ioctl,
	.d_close =	gpib_ib_close,
};
