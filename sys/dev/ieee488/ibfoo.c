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
__FBSDID("$FreeBSD: src/sys/dev/ieee488/ibfoo.c,v 1.5.12.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

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

/* XXX: This is really a bitmap */
enum h_kind {
	H_DEV = 1,
	H_BOARD = 2,
	H_EITHER = 3
};

struct handle {
	LIST_ENTRY(handle)	list;
	int			handle;
	enum h_kind		kind;
	int			pad;
	int			sad;
	struct timeval		timeout;
	int			eot;
	int			eos;
	int			dma;
};

struct ibfoo {
	struct upd7210		*u;
	LIST_HEAD(,handle)	handles;
	struct unrhdr		*unrhdr;
	struct callout		callout;
	struct handle		*h;
	struct ibarg		*ap;

	enum {
		IDLE,
		BUSY,
		PIO_IDATA,
		PIO_ODATA,
		PIO_CMD,
		DMA_IDATA
	}			mode;

	struct timeval		deadline;

	struct handle		*rdh;		/* addressed for read */
	struct handle		*wrh;		/* addressed for write */

	int		 	doeoi;

	u_char			*buf;
	u_int			buflen;
};

typedef int ibhandler_t(struct ibfoo *ib);

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

static int ibdebug;

static int
ib_set_error(struct ibarg *ap, int error)
{

	if (ap->__iberr == 0)
		ap->__iberr = error;
	ap->__ibsta |= ERR;
	ap->__retval = ap->__ibsta;
	return (0);
}

static int
ib_had_timeout(struct ibarg *ap)
{

	ib_set_error(ap, EABO);
	ap->__ibsta |= TIMO;
	ap->__retval = ap->__ibsta;
	return (0);
}

static int
ib_set_errno(struct ibarg *ap, int errno)
{

	if (ap->__iberr == 0) {
		ap->__iberr = EDVR;
		ap->__ibcnt = errno;
	}
	ap->__ibsta |= ERR;
	ap->__retval = ap->__ibsta;
	return (0);
}

static int
gpib_ib_irq(struct upd7210 *u, int intr __unused)
{
	struct ibfoo *ib;

	ib = u->ibfoo;

	mtx_assert(&u->mutex, MA_OWNED);
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
	ib->mode = BUSY;
	wakeup(&ib->buflen);
	return (1);
}

static void
gpib_ib_timeout(void *arg)
{
	struct upd7210 *u;
	struct ibfoo *ib;
	struct timeval tv;

	u = arg;
	ib = u->ibfoo;
	mtx_lock(&u->mutex);
	if (ib->mode == DMA_IDATA && isa_dmatc(u->dmachan)) {
		KASSERT(u->dmachan >= 0, ("Bogus dmachan = %d", u->dmachan));
		upd7210_wr(u, IMR1, 0);
		upd7210_wr(u, IMR2, 0);
		ib->mode = BUSY;
		wakeup(&ib->buflen);
	}
	if (ib->mode > BUSY) {
		upd7210_rd(u, ISR1);
		upd7210_rd(u, ISR2);
		gpib_ib_irq(u, 2);
	}
	if (ib->mode != IDLE && timevalisset(&ib->deadline)) {
		getmicrouptime(&tv);
		if (timevalcmp(&ib->deadline, &tv, <)) {
			ib_had_timeout(ib->ap);
			upd7210_wr(u, IMR1, 0);
			upd7210_wr(u, IMR2, 0);
			ib->mode = BUSY;
			wakeup(&ib->buflen);
		}
	}
	if (ib->mode != IDLE)
		callout_reset(&ib->callout, hz / 5, gpib_ib_timeout, arg);
	mtx_unlock(&u->mutex);
}

static void
gpib_ib_wait_xfer(struct upd7210 *u, struct ibfoo *ib)
{
	int i;

	mtx_assert(&u->mutex, MA_OWNED);
	while (ib->mode > BUSY) {
		i = msleep(&ib->buflen, &u->mutex,
		    PZERO | PCATCH, "ibwxfr", 0);
		if (i == EINTR) {
			ib_set_errno(ib->ap, i);
			break;
		}
		if (u->rreg[ISR1] & IXR1_ERR) {
			ib_set_error(ib->ap, EABO);	/* XXX ? */
			break;
		}
	}
	ib->mode = BUSY;
	ib->buf = NULL;
	upd7210_wr(u, IMR1, 0);
	upd7210_wr(u, IMR2, 0);
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
gethandle(struct upd7210 *u, struct ibarg *ap, struct handle **hp)
{
	struct ibfoo *ib;
	struct handle *h;

	KASSERT(ap->__field & __F_HANDLE, ("gethandle without __F_HANDLE"));
	ib = u->ibfoo;
	LIST_FOREACH(h, &ib->handles, list) {
		if (h->handle == ap->handle) {
			*hp = h;
			return (0);
		}
	}
	ib_set_error(ap, EARG);
	return (1);
}

static int
pio_cmd(struct upd7210 *u, u_char *cmd, int len)
{
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

	gpib_ib_wait_xfer(u, ib);

	mtx_unlock(&u->mutex);
	return (len - ib->buflen);
}

static int
pio_odata(struct upd7210 *u, u_char *data, int len)
{
	struct ibfoo *ib;

	ib = u->ibfoo;

	if (len == 0)
		return (0);
	mtx_lock(&u->mutex);
	ib->mode = PIO_ODATA;
	ib->buf = data;
	ib->buflen = len;
	upd7210_wr(u, IMR1, IXR1_DO);

	gpib_ib_wait_xfer(u, ib);

	mtx_unlock(&u->mutex);
	return (len - ib->buflen);
}

static int
pio_idata(struct upd7210 *u, u_char *data, int len)
{
	struct ibfoo *ib;

	ib = u->ibfoo;

	mtx_lock(&u->mutex);
	ib->mode = PIO_IDATA;
	ib->buf = data;
	ib->buflen = len;
	upd7210_wr(u, IMR1, IXR1_DI);

	gpib_ib_wait_xfer(u, ib);

	mtx_unlock(&u->mutex);
	return (len - ib->buflen);
}

static int
dma_idata(struct upd7210 *u, u_char *data, int len)
{
	int j;
	struct ibfoo *ib;

	KASSERT(u->dmachan >= 0, ("Bogus dmachan %d", u->dmachan));
	ib = u->ibfoo;
	ib->mode = DMA_IDATA;
	mtx_lock(&Giant);
	isa_dmastart(ISADMA_READ, data, len, u->dmachan);
	mtx_unlock(&Giant);
	mtx_lock(&u->mutex);
	upd7210_wr(u, IMR1, IXR1_ENDRX);
	upd7210_wr(u, IMR2, IMR2_DMAI);
	gpib_ib_wait_xfer(u, ib);
	mtx_unlock(&u->mutex);
	mtx_lock(&Giant);
	j = isa_dmastatus(u->dmachan);
	isa_dmadone(ISADMA_READ, data, len, u->dmachan);
	mtx_unlock(&Giant);
	return (len - j);
}

static int
ib_send_msg(struct ibfoo *ib, int msg)
{
	u_char buf[10];
	int i, j;

	i = 0;
	buf[i++] = UNT;
	buf[i++] = UNL;
	buf[i++] = LAD | ib->h->pad;
	if (ib->h->sad)
		buf[i++] = LAD | TAD | ib->h->sad;
	buf[i++] = TAD | 0;
	buf[i++] = msg;
	j = pio_cmd(ib->u, buf, i);
	if (i != j)
		ib_set_error(ib->ap, EABO); /* XXX ? */
	return (0);
}

static int
ibask(struct ibfoo *ib)
{	/* XXX */

	ibdebug = ib->ap->option;
	return (0);
}

#define ibbna NULL
#define ibcac NULL

static int
ibclr(struct ibfoo *ib)
{

	return (ib_send_msg(ib, SDC));
}

#define ibcmd NULL
#define ibcmda NULL
#define ibconfig NULL

static int
ibdev(struct ibfoo *ib)
{	/* TBD */
	struct handle *h;

	h = malloc(sizeof *h, M_IBFOO, M_ZERO | M_WAITOK);
	h->handle = alloc_unr(ib->unrhdr);
	h->kind = H_DEV;
	h->pad = ib->ap->pad;
	h->sad = ib->ap->sad;
	h->timeout = timeouts[ib->ap->tmo];
	h->eot = ib->ap->eot;
	h->eos = ib->ap->eos;
	mtx_lock(&ib->u->mutex);
	LIST_INSERT_HEAD(&ib->handles, h, list);
	mtx_unlock(&ib->u->mutex);
	ib->ap->__retval = h->handle;
	return (0);
}

#define ibdiag NULL

static int
ibdma(struct ibfoo *ib)
{

	if (ib->u->dmachan < 0 && ib->ap->v)
		return (ib_set_error(ib->ap, EARG));
	ib->h->dma = ib->ap->v;
	return (0);
}

static int
ibeos(struct ibfoo *ib)
{

	ib->ap->__iberr = ib->h->eos;
	ib->h->eos = ib->ap->eos;
	if (ib->rdh == ib->h)
		config_eos(ib->u, ib->h);
	return (0);
}

static int
ibeot(struct ibfoo *ib)
{

	ib->h->eot = ib->ap->eot;
	return (0);
}

#define ibevent NULL
#define ibfind NULL
#define ibgts NULL
#define ibist NULL
#define iblines NULL
#define ibllo NULL
#define ibln NULL

static int
ibloc(struct ibfoo *ib)
{	/* XXX */

	if (ib->h->kind == H_BOARD)
		return (EOPNOTSUPP); /* XXX */
	return (ib_send_msg(ib, GTL));
}

static int
ibonl(struct ibfoo *ib)
{	/* XXX */

	if (ib->ap->v)
		return (EOPNOTSUPP);	/* XXX */
	mtx_lock(&ib->u->mutex);
	LIST_REMOVE(ib->h, list);
	mtx_unlock(&ib->u->mutex);
	free(ib->h, M_IBFOO);
	ib->h = NULL;
	return (0);
}

static int
ibpad(struct ibfoo *ib)
{

	ib->h->pad = ib->ap->pad;
	return (0);
}

#define ibpct NULL
#define ibpoke NULL
#define ibppc NULL

static int
ibrd(struct ibfoo *ib)
{	/* TBD */
	u_char buf[10], *bp;
	int i, j, error, bl, bc;
	u_char *dp;

	if (ib->h->kind == H_BOARD)
		return (EOPNOTSUPP); /* XXX */
	bl = ib->ap->cnt;
	if (bl > PAGE_SIZE)
		bl = PAGE_SIZE;
	bp = malloc(bl, M_IBFOO, M_WAITOK);

	if (ib->rdh != ib->h) {
		i = 0;
		buf[i++] = UNT;
		buf[i++] = UNL;
		buf[i++] = LAD | 0;
		buf[i++] = TAD | ib->h->pad;
		if (ib->h->sad)
			buf[i++] = ib->h->sad;
		i = pio_cmd(ib->u, buf, i);
		config_eos(ib->u, ib->h);
		ib->rdh = ib->h;
		ib->wrh = NULL;
	}
	upd7210_goto_standby(ib->u);
	dp = ib->ap->buffer;
	bc = ib->ap->cnt;
	error = 0;
	while (bc > 0 && ib->ap->__iberr == 0) {
		j = imin(bc, PAGE_SIZE);
		if (ib->h->dma)
			i = dma_idata(ib->u, bp, j);
		else
			i = pio_idata(ib->u, bp, j);
		error = copyout(bp, dp , i);
		if (error)
			break;
		ib->ap->__ibcnt += i;
		if (i != j)
			break;
		bc -= i;
		dp += i;
	}
	upd7210_take_ctrl_async(ib->u);
	free(bp, M_IBFOO);
	return (error);
}

#define ibrda NULL
#define ibrdf NULL
#define ibrdkey NULL
#define ibrpp NULL
#define ibrsc NULL
#define ibrsp NULL
#define ibrsv NULL

static int
ibsad(struct ibfoo *ib)
{

	ib->h->sad = ib->ap->sad;
	return (0);
}

#define ibsgnl NULL

static int
ibsic(struct ibfoo *ib)
{	/* TBD */

	upd7210_wr(ib->u, AUXMR, AUXMR_SIFC);
	DELAY(100);
	upd7210_wr(ib->u, AUXMR, AUXMR_CIFC);
	return (0);
}

#define ibsre NULL
#define ibsrq NULL
#define ibstop NULL

static int
ibtmo(struct ibfoo *ib)
{

	ib->h->timeout = timeouts[ib->ap->tmo];
	return (0);
}

#define ibtrap NULL

static int
ibtrg(struct ibfoo *ib)
{

	return (ib_send_msg(ib, GET));
}

#define ibwait NULL

static int
ibwrt(struct ibfoo *ib)
{	/* XXX */
	u_char buf[10], *bp;
	int i;

	if (ib->h->kind == H_BOARD)
		return (EOPNOTSUPP);
	bp = malloc(ib->ap->cnt, M_IBFOO, M_WAITOK);
	/* XXX: bigger than PAGE_SIZE handling */
	i = copyin(ib->ap->buffer, bp, ib->ap->cnt);
	if (i) {
		free(bp, M_IBFOO);
		return (i);
	}
	if (ib->wrh != ib->h) {
		i = 0;
		buf[i++] = UNT;
		buf[i++] = UNL;
		buf[i++] = LAD | ib->h->pad;
		if (ib->h->sad)
			buf[i++] = LAD | TAD | ib->h->sad;
		buf[i++] = TAD | 0;
		i = pio_cmd(ib->u, buf, i);
		ib->rdh = NULL;
		ib->wrh = ib->h;
		config_eos(ib->u, ib->h);
	}
	upd7210_goto_standby(ib->u);
	ib->doeoi = ib->h->eot;
	i = pio_odata(ib->u, bp, ib->ap->cnt);
	upd7210_take_ctrl_async(ib->u);
	ib->ap->__ibcnt = i;
	free(bp, M_IBFOO);
	return (0);
}

#define ibwrta NULL
#define ibwrtf NULL
#define ibwrtkey NULL
#define ibxtrc NULL

static struct ibhandler {
	const char 	*name;
	enum h_kind	kind;
	ibhandler_t	*func;
	u_int		args;
} ibhandlers[] = {
	[__ID_IBASK] =		{ "ibask",	H_EITHER,	ibask,		__F_HANDLE | __F_OPTION | __F_RETVAL },
	[__ID_IBBNA] =		{ "ibbna",	H_DEV,		ibbna,		__F_HANDLE | __F_BDNAME },
	[__ID_IBCAC] =		{ "ibcac",	H_BOARD,	ibcac,		__F_HANDLE | __F_V },
	[__ID_IBCLR] =		{ "ibclr",	H_DEV,		ibclr,		__F_HANDLE },
	[__ID_IBCMD] =		{ "ibcmd",	H_BOARD,	ibcmd,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBCMDA] =		{ "ibcmda",	H_BOARD,	ibcmda,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBCONFIG] =	{ "ibconfig",	H_EITHER,	ibconfig,	__F_HANDLE | __F_OPTION | __F_VALUE },
	[__ID_IBDEV] =		{ "ibdev",	0,		ibdev,		__F_BOARDID | __F_PAD | __F_SAD | __F_TMO | __F_EOT | __F_EOS },
	[__ID_IBDIAG] =		{ "ibdiag",	H_EITHER,	ibdiag,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBDMA] =		{ "ibdma",	H_EITHER,	ibdma,		__F_HANDLE | __F_V },
	[__ID_IBEOS] =		{ "ibeos",	H_EITHER,	ibeos,		__F_HANDLE | __F_EOS },
	[__ID_IBEOT] =		{ "ibeot",	H_EITHER,	ibeot,		__F_HANDLE | __F_EOT },
	[__ID_IBEVENT] =	{ "ibevent",	H_BOARD,	ibevent,	__F_HANDLE | __F_EVENT },
	[__ID_IBFIND] =		{ "ibfind",	0,		ibfind,		__F_BDNAME },
	[__ID_IBGTS] =		{ "ibgts",	H_BOARD,	ibgts,		__F_HANDLE | __F_V },
	[__ID_IBIST] =		{ "ibist",	H_BOARD,	ibist,		__F_HANDLE | __F_V },
	[__ID_IBLINES] =	{ "iblines",	H_BOARD,	iblines,	__F_HANDLE | __F_LINES },
	[__ID_IBLLO] =		{ "ibllo",	H_EITHER,	ibllo,		__F_HANDLE },
	[__ID_IBLN] =		{ "ibln",	H_BOARD,	ibln,		__F_HANDLE | __F_PADVAL | __F_SADVAL | __F_LISTENFLAG },
	[__ID_IBLOC] =		{ "ibloc",	H_EITHER,	ibloc,		__F_HANDLE },
	[__ID_IBONL] =		{ "ibonl",	H_EITHER,	ibonl,		__F_HANDLE | __F_V },
	[__ID_IBPAD] =		{ "ibpad",	H_EITHER,	ibpad,		__F_HANDLE | __F_PAD },
	[__ID_IBPCT] =		{ "ibpct",	H_DEV,		ibpct,		__F_HANDLE },
	[__ID_IBPOKE] =		{ "ibpoke",	H_EITHER,	ibpoke,		__F_HANDLE | __F_OPTION | __F_VALUE },
	[__ID_IBPPC] =		{ "ibppc",	H_EITHER,	ibppc,		__F_HANDLE | __F_V },
	[__ID_IBRD] =		{ "ibrd",	H_EITHER,	ibrd,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBRDA] =		{ "ibrda",	H_EITHER,	ibrda,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBRDF] =		{ "ibrdf",	H_EITHER,	ibrdf,		__F_HANDLE | __F_FLNAME },
	[__ID_IBRDKEY] =	{ "ibrdkey",	H_EITHER,	ibrdkey,	__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBRPP] =		{ "ibrpp",	H_EITHER,	ibrpp,		__F_HANDLE | __F_PPR },
	[__ID_IBRSC] =		{ "ibrsc",	H_BOARD,	ibrsc,		__F_HANDLE | __F_V },
	[__ID_IBRSP] =		{ "ibrsp",	H_DEV,		ibrsp,		__F_HANDLE | __F_SPR },
	[__ID_IBRSV] =		{ "ibrsv",	H_EITHER,	ibrsv,		__F_HANDLE | __F_V },
	[__ID_IBSAD] =		{ "ibsad",	H_EITHER,	ibsad,		__F_HANDLE | __F_SAD },
	[__ID_IBSGNL] =		{ "ibsgnl",	H_EITHER,	ibsgnl,		__F_HANDLE | __F_V },
	[__ID_IBSIC] =		{ "ibsic",	H_BOARD,	ibsic,		__F_HANDLE },
	[__ID_IBSRE] =		{ "ibsre",	H_BOARD,	ibsre,		__F_HANDLE | __F_V },
	[__ID_IBSRQ] =		{ "ibsrq",	H_EITHER,	ibsrq,		__F_FUNC },
	[__ID_IBSTOP] =		{ "ibstop",	H_EITHER,	ibstop,		__F_HANDLE },
	[__ID_IBTMO] =		{ "ibtmo",	H_EITHER,	ibtmo,		__F_HANDLE | __F_TMO },
	[__ID_IBTRAP] =		{ "ibtrap",	H_EITHER,	ibtrap,		__F_MASK | __F_MODE },
	[__ID_IBTRG] =		{ "ibtrg",	H_DEV,		ibtrg,		__F_HANDLE },
	[__ID_IBWAIT] =		{ "ibwait",	H_EITHER,	ibwait,		__F_HANDLE | __F_MASK },
	[__ID_IBWRT] =		{ "ibwrt",	H_EITHER,	ibwrt,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBWRTA] =		{ "ibwrta",	H_EITHER,	ibwrta,		__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBWRTF] =		{ "ibwrtf",	H_EITHER,	ibwrtf,		__F_HANDLE | __F_FLNAME },
	[__ID_IBWRTKEY] =	{ "ibwrtkey",	H_EITHER,	ibwrtkey,	__F_HANDLE | __F_BUFFER | __F_CNT },
	[__ID_IBXTRC] =		{ "ibxtrc",	H_EITHER,	ibxtrc,		__F_HANDLE | __F_BUFFER | __F_CNT },
};

static const u_int max_ibhandler = sizeof ibhandlers / sizeof ibhandlers[0];

static void
ib_dump_args(struct ibhandler *ih, struct ibarg *ap)
{

	if (ih->name != NULL)
		printf("%s(", ih->name);
	else
		printf("ibinvalid(");
	printf("[0x%x]", ap->__field);
	if (ap->__field & __F_HANDLE)	printf(" handle=%d", ap->handle);
	if (ap->__field & __F_EOS)	printf(" eos=0x%x", ap->eos);
	if (ap->__field & __F_EOT)	printf(" eot=%d", ap->eot);
	if (ap->__field & __F_TMO)	printf(" tmo=%d", ap->tmo);
	if (ap->__field & __F_PAD)	printf(" pad=0x%x", ap->pad);
	if (ap->__field & __F_SAD)	printf(" sad=0x%x", ap->sad);
	if (ap->__field & __F_BUFFER)	printf(" buffer=%p", ap->buffer);
	if (ap->__field & __F_CNT)	printf(" cnt=%ld", ap->cnt);
	if (ap->__field & __F_V)	printf(" v=%d/0x%x", ap->v, ap->v);
	/* XXX more ... */
	printf(")\n");
}

static int
gpib_ib_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct upd7210 *u;
	struct ibfoo *ib;
	int error = 0;

	u = dev->si_drv1;

	mtx_lock(&u->mutex);
	if (u->busy) {
		mtx_unlock(&u->mutex);
		return (EBUSY);
	}
	u->busy = 1;
	mtx_unlock(&u->mutex);

	if (u->dmachan >= 0) {
		mtx_lock(&Giant);
		error = isa_dma_acquire(u->dmachan);
		if (!error) {
			error = isa_dma_init(u->dmachan, PAGE_SIZE, M_WAITOK);
			if (error)
				isa_dma_release(u->dmachan);
		}
		mtx_unlock(&Giant);
	}

	if (error) {
		mtx_lock(&u->mutex);
		u->busy = 0;
		mtx_unlock(&u->mutex);
		return (error);
	}

	ib = malloc(sizeof *ib, M_IBFOO, M_WAITOK | M_ZERO);
	LIST_INIT(&ib->handles);
	callout_init(&ib->callout, CALLOUT_MPSAFE);
	ib->unrhdr = new_unrhdr(0, INT_MAX, NULL);
	dev->si_drv2 = ib;
	ib->u = u;
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

	if (u->dmachan >= 0) {
		mtx_lock(&Giant);
		isa_dma_release(u->dmachan);
		mtx_unlock(&Giant);
	}
	mtx_lock(&u->mutex);
	u->busy = 0;
	ibdebug = 0;
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
	struct ibarg *ap;
	struct ibhandler *ih;
	struct handle *h;
	struct upd7210 *u;
	struct ibfoo *ib;
	int error;
	struct timeval deadline, tv;

	u = dev->si_drv1;
	ib = u->ibfoo;

	/* We only support a single ioctl, everything else is a mistake */
	if (cmd != GPIB_IBFOO)
		return (ENOIOCTL);

	/* Check the identifier and field-bitmap in the arguments.  */
	ap = (void *)data;
	if (ap->__ident < 0 || ap->__ident >= max_ibhandler)
		return (EINVAL);
	ih = &ibhandlers[ap->__ident];
	if (ap->__field != ih->args)
		return (EINVAL);

	if (ibdebug)
		ib_dump_args(ih, ap);

	if (ih->func == NULL)
		return (EOPNOTSUPP);

	ap->__iberr = 0;
	ap->__ibsta = 0;
	ap->__ibcnt = 0;
	ap->retval = 0;

	if (ap->__field & __F_TMO) {
		if (ap->tmo < 0 || ap->tmo >= max_timeouts)
			return (ib_set_error(ap, EARG));
	}

	if (ap->__field & __F_EOS) {
		if ((ap->eos & ~(REOS | XEOS | BIN | 0xff)) ||
		    ((ap->eos & (BIN | 0x80)) == 0x80))
			return (ib_set_error(ap, EARG));
	}
	if (ap->__field & __F_PAD) {
		if (ap->pad < 0 || ap->pad > 30)
			return (ib_set_error(ap, EARG));
	}
	if (ap->__field & __F_SAD) {
		if (ap->sad != 0 && (ap->sad < 0x60 || ap->sad > 126))
			return (ib_set_error(ap, EARG));
	}
	

	mtx_lock(&u->mutex);

	
	/* Find the handle, if any */
	h = NULL;
	if ((ap->__field & __F_HANDLE) && gethandle(u, ap, &h)) {
		mtx_unlock(&u->mutex);
		return (0);
	}

	/* Check that the handle is the right kind */
	if (h != NULL && !(h->kind & ih->kind)) {
		mtx_unlock(&u->mutex);
		return (ib_set_error(ap, EARG));
	}

	/* Set up handle and deadline */
	if (h != NULL && timevalisset(&h->timeout)) {
		getmicrouptime(&deadline);
		timevaladd(&deadline, &h->timeout);
	} else {
		timevalclear(&deadline);
	}

	/* Wait for the card to be(come) available, respect deadline */
	while(u->busy != 1) {
		error = msleep(ib, &u->mutex,
		    PZERO | PCATCH, "gpib_ibioctl", hz / 10);
		if (error == 0)
			continue;
		mtx_unlock(&u->mutex);
		if (error == EINTR)
			return(ib_set_error(ap, EABO));
		if (error == EWOULDBLOCK && timevalisset(&deadline)) {
			getmicrouptime(&tv);
			if (timevalcmp(&deadline, &tv, <))
				return(ib_had_timeout(ap));
		}
		mtx_lock(&u->mutex);
	}
	u->busy = 2;
	mtx_unlock(&u->mutex);

	/* Hand over deadline handling to the callout routine */
	ib->ap = ap;
	ib->h = h;
	ib->mode = BUSY;
	ib->deadline = deadline;
	callout_reset(&ib->callout, hz / 5, gpib_ib_timeout, u);

	error = ih->func(ib);

	/* Release card */
	ib->mode = IDLE;
	ib->ap = NULL;
	ib->h = NULL;
	timevalclear(&deadline);
	callout_stop(&ib->callout);

	mtx_lock(&u->mutex);
	u->busy = 1;
	wakeup(ib);
	mtx_unlock(&u->mutex);

	if (error) 
		return(ib_set_errno(ap, error));
	return (0);
}

struct cdevsw gpib_ib_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"gpib_ib",
	.d_open	=	gpib_ib_open,
	.d_ioctl =	gpib_ib_ioctl,
	.d_close =	gpib_ib_close,
};
