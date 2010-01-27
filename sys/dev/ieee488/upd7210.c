/*-
 * Copyright (c) 2005 Poul-Henning Kamp <phk@FreeBSD.org>
 * Copyright (c) 2010 Joerg Wunsch <joerg@FreeBSD.org>
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
#include <sys/rman.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <isa/isavar.h>

#define UPD7210_HW_DRIVER
#define UPD7210_SW_DRIVER
#include <dev/ieee488/upd7210.h>

static MALLOC_DEFINE(M_GPIB, "GPIB", "GPIB");

/* upd7210 generic stuff */

void
upd7210_print_isr(u_int isr1, u_int isr2)
{
	printf("isr1=0x%b isr2=0x%b",
	    isr1, "\20\10CPT\7APT\6DET\5ENDRX\4DEC\3ERR\2DO\1DI",
	    isr2, "\20\10INT\7SRQI\6LOK\5REM\4CO\3LOKC\2REMC\1ADSC");
}

u_int
upd7210_rd(struct upd7210 *u, enum upd7210_rreg reg)
{
	u_int r;

	r = bus_read_1(u->reg_res[reg], u->reg_offset[reg]);
	u->rreg[reg] = r;
	return (r);
}

void
upd7210_wr(struct upd7210 *u, enum upd7210_wreg reg, u_int val)
{

	bus_write_1(u->reg_res[reg], u->reg_offset[reg], val);
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
	isr1 = upd7210_rd(u, ISR1);
	isr2 = upd7210_rd(u, ISR2);
	if (isr1 != 0 || isr2 != 0) {
		if (u->busy == 0 || u->irq == NULL || !u->irq(u, 1)) {
#if 0
			printf("upd7210intr [%02x %02x %02x",
			       upd7210_rd(u, DIR), isr1, isr2);
			printf(" %02x %02x %02x %02x %02x] ",
			       upd7210_rd(u, SPSR),
			       upd7210_rd(u, ADSR),
			       upd7210_rd(u, CPTR),
			       upd7210_rd(u, ADR0),
		    upd7210_rd(u, ADR1));
			upd7210_print_isr(isr1, isr2);
			printf("\n");
#endif
		}
		/*
		 * "special interrupt handling"
		 *
		 * In order to implement shared IRQs, the original
		 * PCIIa uses IO locations 0x2f0 + (IRQ#) as an output
		 * location.  If an ISR for a particular card has
		 * detected this card triggered the IRQ, it must reset
		 * the card's IRQ by writing (anything) to that IO
		 * location.
		 *
		 * Some clones apparently don't implement this
		 * feature, but National Instrument cards do.
		 */
		if (u->irq_clear_res != NULL)
			bus_write_1(u->irq_clear_res, 0, 42);
	}
	mtx_unlock(&u->mutex);
}

int
upd7210_take_ctrl_async(struct upd7210 *u)
{
	int i;

	upd7210_wr(u, AUXMR, AUXMR_TCA);

	if (!(upd7210_rd(u, ADSR) & ADSR_ATN))
		return (0);
	for (i = 0; i < 20; i++) {
		DELAY(1);
		if (!(upd7210_rd(u, ADSR) & ADSR_ATN))
			return (0);
	}
	return (1);
}

int
upd7210_goto_standby(struct upd7210 *u)
{
	int i;

	upd7210_wr(u, AUXMR, AUXMR_GTS);

	if (upd7210_rd(u, ADSR) & ADSR_ATN)
		return (0);
	for (i = 0; i < 20; i++) {
		DELAY(1);
		if (upd7210_rd(u, ADSR) & ADSR_ATN)
			return (0);
	}
	return (1);
}

/* Unaddressed Listen Only mode */

static int
gpib_l_irq(struct upd7210 *u, int intr __unused)
{
	int i;

	if (u->rreg[ISR1] & 1) {
		i = upd7210_rd(u, DIR);
		u->buf[u->buf_wp++] = i;
		u->buf_wp &= (u->bufsize - 1);
		i = (u->buf_rp + u->bufsize - u->buf_wp) & (u->bufsize - 1);
		if (i < 8)
			upd7210_wr(u, IMR1, 0);
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
	if (u->busy) {
		mtx_unlock(&u->mutex);
		return (EBUSY);
	}
	u->busy = 1;
	u->irq = gpib_l_irq;
	mtx_unlock(&u->mutex);

	u->buf = malloc(PAGE_SIZE, M_GPIB, M_WAITOK);
	u->bufsize = PAGE_SIZE;
	u->buf_wp = 0;
	u->buf_rp = 0;

	upd7210_wr(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	upd7210_wr(u, AUXMR, C_ICR | 8);
	DELAY(1000);
	upd7210_wr(u, ADR, 0x60);
	upd7210_wr(u, ADR, 0xe0);
	upd7210_wr(u, ADMR, 0x70);
	upd7210_wr(u, AUXMR, AUXMR_PON);
	upd7210_wr(u, IMR1, 0x01);
	return (0);
}

static int
gpib_l_close(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	struct upd7210 *u;

	u = dev->si_drv1;

	mtx_lock(&u->mutex);
	u->busy = 0;
	upd7210_wr(u, AUXMR, AUXMR_CRST);
	DELAY(10000);
	upd7210_wr(u, IMR1, 0x00);
	upd7210_wr(u, IMR2, 0x00);
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
		upd7210_wr(u, IMR1, 0x01);
	mtx_unlock(&u->mutex);
	return (error);
}

static struct cdevsw gpib_l_cdevsw = {
	.d_version =	D_VERSION,
	.d_name =	"gpib_l",
	.d_open	=	gpib_l_open,
	.d_close =	gpib_l_close,
	.d_read =	gpib_l_read,
};

/* Housekeeping */

static struct unrhdr *units;

void
upd7210attach(struct upd7210 *u)
{
	struct cdev *dev;

	if (units == NULL)
		units = new_unrhdr(0, INT_MAX, NULL);
	u->unit = alloc_unr(units);
	mtx_init(&u->mutex, "gpib", NULL, MTX_DEF);
	u->cdev = make_dev(&gpib_l_cdevsw, u->unit,
	    UID_ROOT, GID_WHEEL, 0444,
	    "gpib%ul", u->unit);
	u->cdev->si_drv1 = u;

	dev = make_dev(&gpib_ib_cdevsw, u->unit,
	    UID_ROOT, GID_WHEEL, 0444,
	    "gpib%uib", u->unit);
	dev->si_drv1 = u;
	dev_depends(u->cdev, dev);
}

void
upd7210detach(struct upd7210 *u)
{

	destroy_dev(u->cdev);
	mtx_destroy(&u->mutex);
	free_unr(units, u->unit);
}
