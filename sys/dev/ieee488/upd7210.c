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

#  define	GPIB_DEBUG
#  undef	GPIB_DEBUG

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

#include <dev/ieee488/upd7210.h>
#include <dev/ieee488/ugpib.h>

MALLOC_DEFINE(M_GPIB, "GPIB", "GPIB");

/* upd7210 generic stuff */

static void
print_isr(u_int isr1, u_int isr2)
{
	printf("isr1=0x%b isr2=0x%b",
	    isr1, "\20\10CPT\7APT\6DET\5ENDRX\4DEC\3ERR\2DO\1DI",
	    isr2, "\20\10INT\7SRQI\6LOK\5REM\4CO\3LOKC\2REMC\1ADSC");
}

static u_int
read_reg(struct upd7210 *u, enum upd7210_rreg reg)
{
	u_int r;

	r = bus_space_read_1(
	    u->reg_tag[reg],
	    u->reg_handle[reg],
	    u->reg_offset[reg]);
	u->rreg[reg] = r;
	return (r);
}

static void
write_reg(struct upd7210 *u, enum upd7210_wreg reg, u_int val)
{
	bus_space_write_1(
	    u->reg_tag[reg],
	    u->reg_handle[reg],
	    u->reg_offset[reg], val);
	u->wreg[reg] = val;
	if (reg == AUXMR)
		u->wreg[8 + (val >> 5)] = val & 0x1f;
}

void
upd7210intr(void *arg)
{
	u_int isr1, isr2;
	struct upd7210 *u;

	u = arg;
	mtx_lock(&u->mutex);
	isr1 = read_reg(u, ISR1);
	isr2 = read_reg(u, ISR2);
	if (u->busy == 0 || u->irq == NULL || !u->irq(u)) {
		printf("upd7210intr [%02x %02x %02x",
		    read_reg(u, DIR), isr1, isr2);
		printf(" %02x %02x %02x %02x %02x] ",
		    read_reg(u, SPSR),
		    read_reg(u, ADSR),
		    read_reg(u, CPTR),
		    read_reg(u, ADR0),
		    read_reg(u, ADR1));
		print_isr(isr1, isr2);
		printf("\n");
		write_reg(u, IMR1, 0);
		write_reg(u, IMR2, 0);
	}
	mtx_unlock(&u->mutex);
}

static int
upd7210_take_ctrl_async(struct upd7210 *u)
{
	int i;

	write_reg(u, AUXMR, AUXMR_TCA);

	if (!(read_reg(u, ADSR) & ADSR_ATN))
		return (0);
	for (i = 0; i < 20; i++) {
		DELAY(1);
		if (!(read_reg(u, ADSR) & ADSR_ATN))
			return (0);
	}
	return (1);
}

static int
upd7210_goto_standby(struct upd7210 *u)
{
	int i;

	write_reg(u, AUXMR, AUXMR_GTS);

	if (read_reg(u, ADSR) & ADSR_ATN)
		return (0);
	for (i = 0; i < 20; i++) {
		DELAY(1);
		if (read_reg(u, ADSR) & ADSR_ATN)
			return (0);
	}
	return (1);
}

static int
deadyet(struct upd7210 *u)
{
	struct timeval tv;

	if (!timevalisset(&u->deadline))
		return (0);

	getmicrouptime(&tv);
	if (timevalcmp(&u->deadline, &tv, <)) {
printf("DEADNOW\n");
		return (1);
	}

	return (0);
}

/* Unaddressed Listen Only mode */

static int
gpib_l_irq(struct upd7210 *u)
{
	int i;

	if (u->rreg[ISR1] & 1) {
		i = read_reg(u, DIR);
		u->buf[u->buf_wp++] = i;
		u->buf_wp &= (u->bufsize - 1);
		i = (u->buf_rp + u->bufsize - u->buf_wp) & (u->bufsize - 1);
		if (i < 8)
			write_reg(u, IMR1, 0);
		wakeup(u->buf);
		return (1);
	}
	return (0);
}

static int
gpib_l_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct upd7210 *u;

	u = dev->si_drv1;

	mtx_lock(&u->mutex);
	if (u->busy)
		return (EBUSY);
	u->busy = 1;
	u->irq = gpib_l_irq;
	mtx_unlock(&u->mutex);

	u->buf = malloc(PAGE_SIZE, M_GPIB, M_WAITOK);
	u->bufsize = PAGE_SIZE;
	u->buf_wp = 0;
	u->buf_rp = 0;

	write_reg(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	write_reg(u, AUXMR, C_ICR | 8);
	DELAY(1000);
	write_reg(u, ADR, 0x60);
	write_reg(u, ADR, 0xe0);
	write_reg(u, ADMR, 0x70);
	write_reg(u, AUXMR, AUXMR_PON);
	write_reg(u, IMR1, 0x01);
	return (0);
}

static int
gpib_l_close(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct upd7210 *u;

	u = dev->si_drv1;

	mtx_lock(&u->mutex);
	u->busy = 0;
	write_reg(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	write_reg(u, IMR1, 0x00);
	write_reg(u, IMR2, 0x00);
	free(u->buf, M_GPIB);
	u->buf = NULL;
	mtx_unlock(&u->mutex);
	return (0);
}

static int
gpib_l_read(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct upd7210 *u;
	int error;
	size_t z;

	u = dev->si_drv1;
	error = 0;

	mtx_lock(&u->mutex);
	while (u->buf_wp == u->buf_rp) {
		error = msleep(u->buf, &u->mutex, PZERO | PCATCH,
		    "gpibrd", hz);
		if (error && error != EWOULDBLOCK) {
			mtx_unlock(&u->mutex);
			return (error);
		}
	}
	while (uio->uio_resid > 0 && u->buf_wp != u->buf_rp) {
		if (u->buf_wp < u->buf_rp)
			z = u->bufsize - u->buf_rp;
		else
			z = u->buf_wp - u->buf_rp;
		if (z > uio->uio_resid)
			z = uio->uio_resid;
		mtx_unlock(&u->mutex);
		error = uiomove(u->buf + u->buf_rp, z, uio);
		mtx_lock(&u->mutex);
		if (error)
			break;
		u->buf_rp += z;
		u->buf_rp &= (u->bufsize - 1);
	}
	if (u->wreg[IMR1] == 0)
		write_reg(u, IMR1, 0x01);
	mtx_unlock(&u->mutex);
	return (error);
}

struct cdevsw gpib_l_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"gpib_l",
	.d_open	=	gpib_l_open,
	.d_close =	gpib_l_close,
	.d_read =	gpib_l_read,
};

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
};

struct ibfoo {
	struct upd7210		*upd7210;
	LIST_HEAD(,handle)	handles;
	struct unrhdr		*unrhdr;

	u_char			*cmdbuf;
	u_int			cmdlen;

	struct handle		*rdh;		/* addressed for read */
	struct handle		*wrh;		/* addressed for write */

	u_char			*dobuf;
	u_int			dolen;
	int			doeoi;
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

static u_int max_timeouts = sizeof timeouts / sizeof timeouts[0];

typedef int ibhandler_t(struct upd7210 *u, struct ibfoo_iocarg *ap);

static int
gpib_ib_irq(struct upd7210 *u)
{
	struct ibfoo *ib;

	ib = u->ibfoo;

	if ((u->rreg[ISR2] & IXR2_CO) && ib->cmdlen > 0) {
		write_reg(u, CDOR, *ib->cmdbuf);
		ib->cmdbuf++;
		ib->cmdlen--;
		if (ib->cmdlen == 0) {
			wakeup(ib);
			write_reg(u, IMR2, 0);
		}
		return (1);
	}
	if ((u->rreg[ISR1] & IXR1_DO) && ib->dolen > 0) {
		if (ib->dolen == 1 && ib->doeoi)
			write_reg(u, AUXMR, AUXMR_SEOI);
		write_reg(u, CDOR, *ib->dobuf);
		ib->dobuf++;
		ib->dolen--;
		if (ib->dolen == 0) {
			wakeup(ib);
			write_reg(u, IMR1, 0);
		}
		return (1);
	}
	if (u->rreg[ISR1] & IXR1_ENDRX) {
		write_reg(u, IMR1, 0);
		wakeup(ib);
		return (1);
	}
	
	return (0);
}

static void
config_eos(struct upd7210 *u, struct handle *h)
{
	int i;

	i = 0;
	if (h->eos & 0x0400) {
		write_reg(u, EOSR, h->eos & 0xff);
		i |= AUXA_REOS;
	}
	if (h->eos & 0x1000)
		i |= AUXA_BIN;
	write_reg(u, AUXRA, C_AUXA | i);
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
				getmicrouptime(&u->deadline);
				timevaladd(&u->deadline, &h->timeout);
			} else {
				timevalclear(&u->deadline);
			}
			return (0);
		}
	}
	ap->__iberr = EARG;
	return (1);
}

static int
do_cmd(struct upd7210 *u, u_char *cmd, int len)
{
	int i, i1, i2;
	struct ibfoo *ib;

	ib = u->ibfoo;

	if (ib->rdh != NULL || ib->wrh != NULL) {
		upd7210_take_ctrl_async(u);
		ib->rdh = NULL;
		ib->wrh = NULL;
	}
	mtx_lock(&u->mutex);
	ib->cmdbuf = cmd;
	ib->cmdlen = len;
	
	if (!(u->rreg[ISR2] & IXR2_CO)) {
		i1 = read_reg(u, ISR1);
		i2 = read_reg(u, ISR2);
#ifdef GPIB_DEBUG
		print_isr(i1, i2);
		printf("\n");
#endif
	}
	write_reg(u, IMR2, IXR2_CO);
	if (u->rreg[ISR2] & IXR2_CO) {
		write_reg(u, CDOR, *ib->cmdbuf);
		ib->cmdbuf++;
		ib->cmdlen--;
	}

	while (1) {
		i = msleep(ib, &u->mutex, PZERO | PCATCH, "gpib_cmd", hz/10);
		if (i == EINTR)
			break;
		if (u->rreg[ISR1] & IXR1_ERR)
			break;
		if (!ib->cmdlen)
			break;
		if (deadyet(u))
			break;
	}
	write_reg(u, IMR2, 0);
	mtx_unlock(&u->mutex);
	return (0);
}

static int
do_odata(struct upd7210 *u, u_char *data, int len, int eos)
{
	int i1, i2, i;
	struct ibfoo *ib;

	ib = u->ibfoo;

	mtx_lock(&u->mutex);
	ib->dobuf = data;
	ib->dolen = len;
	ib->doeoi = 1;
	
	if (!(u->rreg[ISR1] & IXR1_DO)) {
		i1 = read_reg(u, ISR1);
		i2 = read_reg(u, ISR2);
#ifdef GPIB_DEBUG
		print_isr(i1, i2);
		printf("\n");
#endif
	}
	write_reg(u, IMR1, IXR1_DO);
	if (u->rreg[ISR1] & IXR1_DO) {
		write_reg(u, CDOR, *ib->dobuf);
		ib->dobuf++;
		ib->dolen--;
	}
	while (1) {
		i = msleep(ib, &u->mutex, PZERO | PCATCH, "gpib_out", hz/100);
		if (i == EINTR)
			break;
		if (u->rreg[ISR1] & IXR1_ERR)
			break;
		if (!ib->dolen)
			break;
		if (deadyet(u))
			break;
	}
	write_reg(u, IMR2, 0);
	mtx_unlock(&u->mutex);
	return (len - ib->dolen);
}

static int
do_idata(struct upd7210 *u, u_char *data, int len, int eos)
{
	int i1, i2, i, j;

	write_reg(u, IMR1, IXR1_ENDRX);
	mtx_lock(&Giant);
	isa_dmastart(ISADMA_READ, data, len, u->dmachan);
	mtx_unlock(&Giant);
	mtx_lock(&u->mutex);
	write_reg(u, IMR2, IMR2_DMAI);
	while (1) {
		i = msleep(u->ibfoo, &u->mutex, PZERO | PCATCH,
		    "gpib_idata", hz/100);
		if (i == EINTR)
			break;
		if (isa_dmatc(u->dmachan))
			break;
		if (i == EWOULDBLOCK) {
			i1 = read_reg(u, ISR1);
			i2 = read_reg(u, ISR2);
		} else {
			i1 = u->rreg[ISR1];
			i2 = u->rreg[ISR2];
		}
		if (i1 & IXR1_ENDRX)
			break;
		if (deadyet(u))
			break;
	}
	write_reg(u, IMR1, 0);
	write_reg(u, IMR2, 0);
	mtx_unlock(&u->mutex);
	mtx_lock(&Giant);
	j = isa_dmastatus(u->dmachan);
	isa_dmadone(ISADMA_READ, data, len, u->dmachan);
	mtx_unlock(&Giant);
	if (deadyet(u)) {
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
ibdev(struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct handle *h;
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
	h = malloc(sizeof *h, M_GPIB, M_ZERO | M_WAITOK);
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
#define ibdma NULL

static int
ibeos(struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct handle *h;
	struct ibfoo *ib;

	if (gethandle(u, ap, &h))
		return (0);
	ib = u->ibfoo;
	h->eos = ap->eos;
	if (ib->rdh == h)
		config_eos(u, h);
	ap->__retval = 0;
	return (0);
}

#define ibeot NULL
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
ibrd(struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct ibfoo *ib;
	struct handle *h;
	u_char buf[10], *bp;
	int i, j, error, bl, bc;
	u_char *dp;

	ib = u->ibfoo;
	if (gethandle(u, ap, &h))
		return (0);
	bl = ap->cnt;
	if (bl > PAGE_SIZE)
		bl = PAGE_SIZE;
	bp = malloc(bl, M_GPIB, M_WAITOK);

	if (ib->rdh != h) {
		i = 0;
		buf[i++] = UNT;
		buf[i++] = UNL;
		buf[i++] = LAD | 0;
		buf[i++] = TAD | h->pad;
		if (h->sad)
			buf[i++] = h->sad;
		i = do_cmd(u, buf, i);
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
		i = do_idata(u, bp, j, 1);
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
	free(bp, M_GPIB);
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
ibtmo(struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct handle *h;

	if (gethandle(u, ap, &h))
		return (0);
	h->timeout = timeouts[ap->tmo];
	return (0);
}

#define ibtrap NULL
#define ibtrg NULL
#define ibwait NULL

static int
ibwrt(struct upd7210 *u, struct ibfoo_iocarg *ap)
{
	struct ibfoo *ib;
	struct handle *h;
	u_char buf[10], *bp;
	int i;

	ib = u->ibfoo;
	if (gethandle(u, ap, &h))
		return (0);
	bp = malloc(ap->cnt, M_GPIB, M_WAITOK);
	i = copyin(ap->buffer, bp, ap->cnt);
	if (i) {
		free(bp, M_GPIB);
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
		i = do_cmd(u, buf, i);
		ib->rdh = NULL;
		ib->wrh = h;
		upd7210_goto_standby(u);
	}
	i = do_odata(u, bp, ap->cnt, 1);
	ap->__ibcnt = i;
	ap->__retval = 0;
	free(bp, M_GPIB);
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
	[__ID_IBEOT] =		{ "ibeot",	ibeot,		__F_HANDLE | __F_V },
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

	ib = malloc(sizeof *ib, M_GPIB, M_WAITOK | M_ZERO);
	LIST_INIT(&ib->handles);
	ib->unrhdr = new_unrhdr(0, INT_MAX);
	dev->si_drv2 = ib;
	ib->upd7210 = u;
	u->ibfoo = ib;
	u->irq = gpib_ib_irq;

	write_reg(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	DELAY(1000);
	write_reg(u, IMR1, 0x00);
	write_reg(u, IMR2, 0x00);
	write_reg(u, SPMR, 0x00);
	write_reg(u, ADR, 0x00);
	write_reg(u, ADR, ADR_ARS | ADR_DL | ADR_DT);
	write_reg(u, ADMR, ADMR_ADM0 | ADMR_TRM0 | ADMR_TRM1);
	write_reg(u, EOSR, 0x00);
	write_reg(u, AUXMR, C_ICR | 8);
	write_reg(u, AUXMR, C_PPR | PPR_U);
	write_reg(u, AUXMR, C_AUXA);
	write_reg(u, AUXMR, C_AUXB + 3);
	write_reg(u, AUXMR, C_AUXE + 0);
	write_reg(u, AUXMR, AUXMR_PON);
	write_reg(u, AUXMR, AUXMR_CIFC);
	DELAY(100);
	write_reg(u, AUXMR, AUXMR_SIFC);
	write_reg(u, AUXMR, AUXMR_SREN);
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
	free(ib, M_GPIB);

	mtx_lock(&Giant);
	isa_dma_release(u->dmachan);
	mtx_unlock(&Giant);
	mtx_lock(&u->mutex);
	u->busy = 0;
	write_reg(u, IMR1, 0x00);
	write_reg(u, IMR2, 0x00);
	write_reg(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	mtx_unlock(&u->mutex);
	return (0);
}

static int
gpib_ib_ioctl(struct cdev *dev, u_long cmd, caddr_t data, int fflag, struct thread *td)
{
	struct ibfoo_iocarg *ap;
	struct ibhandler *ih;
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

#ifdef GPIB_DEBUG
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

	ap->__iberr = 0;
	error = EOPNOTSUPP;
	if (ih->func != NULL)
		error = ih->func(u, ap);
	if (error) {
		ap->__retval = EDVR;
		ap->__iberr = EDVR;
		ap->__ibcnt = error;
	} else if (ap->__iberr) {
		ap->__retval = -1;
	}
#ifdef GPIB_DEBUG
	printf("%s(...) = %d (error=%d)\n", ih->name, ap->__retval, error);
#endif
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

/* Housekeeping */

void
upd7210attach(struct upd7210 *u)
{
	int unit = 0;
	struct cdev *dev;

	mtx_init(&u->mutex, "gpib", NULL, MTX_DEF);
	u->cdev = make_dev(&gpib_l_cdevsw, unit,
	    UID_ROOT, GID_WHEEL, 0444,
	    "gpib%ul", unit);
	u->cdev->si_drv1 = u;

	dev = make_dev(&gpib_ib_cdevsw, unit,
	    UID_ROOT, GID_WHEEL, 0444,
	    "gpib%uib", unit);
	dev->si_drv1 = u;
	dev_depends(u->cdev, dev);
}
