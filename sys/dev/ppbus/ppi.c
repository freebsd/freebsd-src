/*-
 * Copyright (c) 1997, 1998 Nicolas Souchu, Michael Smith
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
 * $FreeBSD$
 *
 */
#include "ppi.h"

#if NPPI > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <machine/clock.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppb_msq.h>

#include "opt_ppb_1284.h"

#ifdef PERIPH_1284
#include <dev/ppbus/ppb_1284.h>
#endif

#include <dev/ppbus/ppi.h>

#define BUFSIZE		512

struct ppi_data {

    int		ppi_unit;
    int		ppi_flags;
#define HAVE_PPBUS	(1<<0)
#define HAD_PPBUS	(1<<1)

    int		ppi_count;
    int		ppi_mode;			/* IEEE1284 mode */
    char	ppi_buffer[BUFSIZE];

    struct ppb_device ppi_dev;
};

#define MAXPPI		8			/* XXX not much better! */
static int 		nppi = 0;
static struct ppi_data	*ppidata[MAXPPI];

/*
 * Make ourselves visible as a ppbus driver
 */

static struct ppb_device	*ppiprobe(struct ppb_data *ppb);
static int			ppiattach(struct ppb_device *dev);
static void			ppiintr(int unit);

static struct ppb_driver ppidriver = {
    ppiprobe, ppiattach, "ppi"
};
DATA_SET(ppbdriver_set, ppidriver);

static	d_open_t	ppiopen;
static	d_close_t	ppiclose;
static	d_ioctl_t	ppiioctl;
static	d_write_t	ppiwrite;
static	d_read_t	ppiread;

#define CDEV_MAJOR 82
static struct cdevsw ppi_cdevsw = {
	/* open */	ppiopen,
	/* close */	ppiclose,
	/* read */	ppiread,
	/* write */	ppiwrite,
	/* ioctl */	ppiioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"ppi",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
	/* bmaj */	-1
};

#ifdef PERIPH_1284

static void
ppi_enable_intr(struct ppi_data *ppi)
{
	char r;

	r = ppb_rctr(&ppi->ppi_dev);
	ppb_wctr(&ppi->ppi_dev, r | IRQENABLE);

	return;
}

static void
ppi_disable_intr(struct ppi_data *ppi)
{
	char r;

	r = ppb_rctr(&ppi->ppi_dev);
	ppb_wctr(&ppi->ppi_dev, r & ~IRQENABLE);

	return;
}

#endif /* PERIPH_1284 */

/*
 * ppiprobe()
 */
static struct ppb_device *
ppiprobe(struct ppb_data *ppb)
{
	struct ppi_data *ppi;
	static int once;

	if (!once++)
		cdevsw_add(&ppi_cdevsw);

	ppi = (struct ppi_data *) malloc(sizeof(struct ppi_data),
							M_TEMP, M_NOWAIT);
	if (!ppi) {
		printf("ppi: cannot malloc!\n");
		return 0;
	}
	bzero(ppi, sizeof(struct ppi_data));

	ppidata[nppi] = ppi;

	/*
	 * ppi dependent initialisation.
	 */
	ppi->ppi_unit = nppi;

	/*
	 * ppbus dependent initialisation.
	 */
	ppi->ppi_dev.id_unit = ppi->ppi_unit;
	ppi->ppi_dev.ppb = ppb;
	ppi->ppi_dev.intr = ppiintr;

	/* Ok, go to next device on next probe */
	nppi ++;

	return &ppi->ppi_dev;
}

static int
ppiattach(struct ppb_device *dev)
{
	/*
	 * Report ourselves
	 */
	printf("ppi%d: <generic parallel i/o> on ppbus %d\n",
	       dev->id_unit, dev->ppb->ppb_link->adapter_unit);

	return (1);
}

/*
 * Cable
 * -----
 *
 * Use an IEEE1284 compliant (DB25/DB25) cable with the following tricks:
 *
 * nStrobe   <-> nAck		1  <-> 10
 * nAutofd   <-> Busy		11 <-> 14
 * nSelectin <-> Select		17 <-> 13
 * nInit     <-> nFault		15 <-> 16
 *
 */
static void
ppiintr(int unit)
{
#ifdef PERIPH_1284
	struct ppi_data *ppi = ppidata[unit];

	ppi_disable_intr(ppi);

	switch (ppi->ppi_dev.ppb->state) {

	/* accept IEEE1284 negociation then wakeup an waiting process to
	 * continue negociation at process level */
	case PPB_FORWARD_IDLE:
		/* Event 1 */
		if ((ppb_rstr(&ppi->ppi_dev) & (SELECT | nBUSY)) ==
							(SELECT | nBUSY)) {
			/* IEEE1284 negociation */
#ifdef DEBUG_1284
			printf("N");
#endif

			/* Event 2 - prepare for reading the ext. value */
			ppb_wctr(&ppi->ppi_dev, (PCD | STROBE | nINIT) & ~SELECTIN);

			ppi->ppi_dev.ppb->state = PPB_NEGOCIATION;

		} else {
#ifdef DEBUG_1284
			printf("0x%x", ppb_rstr(&ppi->ppi_dev));
#endif
			ppb_peripheral_terminate(&ppi->ppi_dev, PPB_DONTWAIT);
			break;
		}

		/* wake up any process waiting for negociation from
		 * remote master host */

		/* XXX should set a variable to warn the process about
		 * the interrupt */

		wakeup(ppi);
		break;
	default:
#ifdef DEBUG_1284
		printf("?%d", ppi->ppi_dev.ppb->state);
#endif
		ppi->ppi_dev.ppb->state = PPB_FORWARD_IDLE;
		ppb_set_mode(&ppi->ppi_dev, PPB_COMPATIBLE);
		break;
	}

	ppi_enable_intr(ppi);
#endif /* PERIPH_1284 */

	return;
}

static int
ppiopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	u_int unit = minor(dev);
	struct ppi_data *ppi = ppidata[unit];
	int res;

	if (unit >= nppi)
		return (ENXIO);

	if (!(ppi->ppi_flags & HAVE_PPBUS)) {
		if ((res = ppb_request_bus(&ppi->ppi_dev,
			(flags & O_NONBLOCK) ? PPB_DONTWAIT :
						(PPB_WAIT | PPB_INTR))))
			return (res);

		ppi->ppi_flags |= HAVE_PPBUS;
	}
	ppi->ppi_count += 1;

	return (0);
}

static int
ppiclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	u_int unit = minor(dev);
	struct ppi_data *ppi = ppidata[unit];

	ppi->ppi_count --;
	if (!ppi->ppi_count) {

#ifdef PERIPH_1284
		switch (ppi->ppi_dev.ppb->state) {
		case PPB_PERIPHERAL_IDLE:
			ppb_peripheral_terminate(&ppi->ppi_dev, 0);
			break;
		case PPB_REVERSE_IDLE:
		case PPB_EPP_IDLE:
		case PPB_ECP_FORWARD_IDLE:
		default:
			ppb_1284_terminate(&ppi->ppi_dev);
			break;
		}
#endif /* PERIPH_1284 */

		ppb_release_bus(&ppi->ppi_dev);
		ppi->ppi_flags &= ~HAVE_PPBUS;
	}

	return (0);
}

/*
 * ppiread()
 *
 * IEEE1284 compliant read.
 *
 * First, try negociation to BYTE then NIBBLE mode
 * If no data is available, wait for it otherwise transfer as much as possible
 */
static int
ppiread(dev_t dev, struct uio *uio, int ioflag)
{
#ifdef PERIPH_1284
	u_int unit = minor(dev);
	struct ppi_data *ppi = ppidata[unit];
	int len, error = 0;

	switch (ppi->ppi_dev.ppb->state) {
	case PPB_PERIPHERAL_IDLE:
		ppb_peripheral_terminate(&ppi->ppi_dev, 0);
		/* fall throught */

	case PPB_FORWARD_IDLE:
		/* if can't negociate NIBBLE mode then try BYTE mode,
		 * the peripheral may be a computer
		 */
		if ((ppb_1284_negociate(&ppi->ppi_dev,
			ppi->ppi_mode = PPB_NIBBLE, 0))) {

			/* XXX Wait 2 seconds to let the remote host some
			 * time to terminate its interrupt
			 */
			tsleep(ppi, PPBPRI, "ppiread", 2*hz);
			
			if ((error = ppb_1284_negociate(&ppi->ppi_dev,
				ppi->ppi_mode = PPB_BYTE, 0)))
				return (error);
		}
		break;

	case PPB_REVERSE_IDLE:
	case PPB_EPP_IDLE:
	case PPB_ECP_FORWARD_IDLE:
	default:
		break;
	}

#ifdef DEBUG_1284
	printf("N");
#endif
	/* read data */
	len = 0;
	while (uio->uio_resid) {
		if ((error = ppb_1284_read(&ppi->ppi_dev, ppi->ppi_mode,
			ppi->ppi_buffer, min(BUFSIZE, uio->uio_resid),
			&len))) {
			goto error;
		}

		if (!len)
			goto error;		/* no more data */

#ifdef DEBUG_1284
		printf("d");
#endif
		if ((error = uiomove(ppi->ppi_buffer, len, uio)))
			goto error;
	}

error:

#else /* PERIPH_1284 */
	int error = ENODEV;
#endif

	return (error);
}

/*
 * ppiwrite()
 *
 * IEEE1284 compliant write
 *
 * Actually, this is the peripheral side of a remote IEEE1284 read
 *
 * The first part of the negociation (IEEE1284 device detection) is
 * done at interrupt level, then the remaining is done by the writing
 * process
 *
 * Once negociation done, transfer data
 */
static int
ppiwrite(dev_t dev, struct uio *uio, int ioflag)
{
#ifdef PERIPH_1284
	u_int unit = minor(dev);
	struct ppi_data *ppi = ppidata[unit];
	struct ppb_data *ppb = ppi->ppi_dev.ppb;
	int len, error = 0, sent;

#if 0
	int ret;

	#define ADDRESS		MS_PARAM(0, 0, MS_TYP_PTR)
	#define LENGTH		MS_PARAM(0, 1, MS_TYP_INT)

	struct ppb_microseq msq[] = {
		  { MS_OP_PUT, { MS_UNKNOWN, MS_UNKNOWN, MS_UNKNOWN } },
		  MS_RET(0)
	};

	/* negociate ECP mode */
	if (ppb_1284_negociate(&ppi->ppi_dev, PPB_ECP, 0)) {
		printf("ppiwrite: ECP negociation failed\n");
	}

	while (!error && (len = min(uio->uio_resid, BUFSIZE))) {
		uiomove(ppi->ppi_buffer, len, uio);

		ppb_MS_init_msq(msq, 2, ADDRESS, ppi->ppi_buffer, LENGTH, len);

		error = ppb_MS_microseq(&ppi->ppi_dev, msq, &ret);
	}
#endif

	/* we have to be peripheral to be able to send data, so
	 * wait for the appropriate state
	 */
	if (ppb->state < PPB_PERIPHERAL_NEGOCIATION)
		ppb_1284_terminate(&ppi->ppi_dev);

	while (ppb->state != PPB_PERIPHERAL_IDLE) {
		/* XXX should check a variable before sleeping */
#ifdef DEBUG_1284
		printf("s");
#endif

		ppi_enable_intr(ppi);

		/* sleep until IEEE1284 negociation starts */
		error = tsleep(ppi, PCATCH | PPBPRI, "ppiwrite", 0);

		switch (error) {
		case 0:
			/* negociate peripheral side with BYTE mode */
			ppb_peripheral_negociate(&ppi->ppi_dev, PPB_BYTE, 0);
			break;
		case EWOULDBLOCK:
			break;
		default:
			goto error;
		}
	}
#ifdef DEBUG_1284
	printf("N");
#endif

	/* negociation done, write bytes to master host */
	while ((len = min(uio->uio_resid, BUFSIZE)) != 0) {
		uiomove(ppi->ppi_buffer, len, uio);
		if ((error = byte_peripheral_write(&ppi->ppi_dev,
						ppi->ppi_buffer, len, &sent)))
			goto error;
#ifdef DEBUG_1284
		printf("d");
#endif
	}

error:

#else /* PERIPH_1284 */
	int error = ENODEV;
#endif

	return (error);
}

static int
ppiioctl(dev_t dev, u_long cmd, caddr_t data, int flags, struct proc *p)
{
	u_int unit = minor(dev);
	struct ppi_data *ppi = ppidata[unit];
	int error = 0;
	u_int8_t *val = (u_int8_t *)data;

	switch (cmd) {

	case PPIGDATA:			/* get data register */
		*val = ppb_rdtr(&ppi->ppi_dev);
		break;
	case PPIGSTATUS:		/* get status bits */
		*val = ppb_rstr(&ppi->ppi_dev);
		break;
	case PPIGCTRL:			/* get control bits */
		*val = ppb_rctr(&ppi->ppi_dev);
		break;
	case PPIGEPPD:			/* get EPP data bits */
		*val = ppb_repp_D(&ppi->ppi_dev);
		break;
	case PPIGECR:			/* get ECP bits */
		*val = ppb_recr(&ppi->ppi_dev);
		break;
	case PPIGFIFO:			/* read FIFO */
		*val = ppb_rfifo(&ppi->ppi_dev);
		break;

	case PPISDATA:			/* set data register */
		ppb_wdtr(&ppi->ppi_dev, *val);
		break;
	case PPISSTATUS:		/* set status bits */
		ppb_wstr(&ppi->ppi_dev, *val);
		break;
	case PPISCTRL:			/* set control bits */
		ppb_wctr(&ppi->ppi_dev, *val);
		break;
	case PPISEPPD:			/* set EPP data bits */
		ppb_wepp_D(&ppi->ppi_dev, *val);
		break;
	case PPISECR:			/* set ECP bits */
		ppb_wecr(&ppi->ppi_dev, *val);
		break;
	case PPISFIFO:			/* write FIFO */
		ppb_wfifo(&ppi->ppi_dev, *val);
		break;

	case PPIGEPPA:			/* get EPP address bits */
		*val = ppb_repp_A(&ppi->ppi_dev);
		break;
	case PPISEPPA:			/* set EPP address bits */
		ppb_wepp_A(&ppi->ppi_dev, *val);
		break;
	default:
		error = ENOTTY;
		break;
	}
    
	return (error);
}

#endif /* NPPI */
