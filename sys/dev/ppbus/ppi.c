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
 *	$Id: ppi.c,v 1.5 1997/09/01 00:51:49 bde Exp $
 *
 */
#include "ppi.h"

#if NPPI > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>

#include <dev/ppbus/ppbconf.h>
#include <dev/ppbus/ppi.h>


struct ppi_data {

    int		ppi_unit;
    int		ppi_flags;
#define HAVE_PPBUS	(1<<0)

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

#define CDEV_MAJOR 82
static struct cdevsw ppi_cdevsw = 
	{ ppiopen,	ppiclose,	noread,		nowrite,	/* 82 */
	  ppiioctl,	nullstop,	nullreset,	nodevtotty,
	  seltrue,	nommap,		nostrat,	"ppi",	NULL,	-1 };

/*
 * ppiprobe()
 */
static struct ppb_device *
ppiprobe(struct ppb_data *ppb)
{
	struct ppi_data *ppi;

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
	struct ppi_data *ppi = ppidata[dev->id_unit];

	/*
	 * Report ourselves
	 */
	printf("ppi%d: <generic parallel i/o> on ppbus %d\n",
	       dev->id_unit, dev->ppb->ppb_link->adapter_unit);

	return (1);
}

static void
ppiintr(int unit)
{
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

	if (!(ppi->ppi_flags & HAVE_PPBUS))
		if ((res = ppb_request_bus(&ppi->ppi_dev, (flags & O_NONBLOCK) ? PPB_DONTWAIT : (PPB_WAIT | PPB_INTR))))
			return (res);

	ppi->ppi_flags |= HAVE_PPBUS;
	return (0);
}

static int
ppiclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	u_int unit = minor(dev);
	struct ppi_data *ppi = ppidata[unit];

	if (ppi->ppi_flags & HAVE_PPBUS)
		ppb_release_bus(&ppi->ppi_dev);
	ppi->ppi_flags &= ~HAVE_PPBUS;
	return (0);
}

static int
ppiioctl(dev_t dev, int cmd, caddr_t data, int flags, struct proc *p)
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
	case PPIGEPP:			/* get EPP bits */
		*val = ppb_repp(&ppi->ppi_dev);
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
	case PPISEPP:			/* set EPP bits */
		ppb_wepp(&ppi->ppi_dev, *val);
		break;
	case PPISECR:			/* set ECP bits */
		ppb_wecr(&ppi->ppi_dev, *val);
		break;
	case PPISFIFO:			/* write FIFO */
		ppb_wfifo(&ppi->ppi_dev, *val);
		break;
	
	default:
		error = ENOTTY;
		break;
	}
    
	return (error);
}

#ifdef PPI_MODULE

MOD_DEV(ppi, LM_DT_CHAR, CDEV_MAJOR, &ppi_cdevsw);

static int
ppi_load(struct lkm_table *lkmtp, int cmd)
{
	struct ppb_data *ppb;
	struct ppb_device *dev;
	int i;

	for (ppb = ppb_next_bus(NULL); ppb; ppb = ppb_next_bus(ppb)) {

		dev = ppiprobe(ppb);
		ppiattach(dev);

		ppb_attach_device(dev);
	}

	return (0);
}

static int
ppi_unload(struct lkm_table *lkmtp, int cmd)
{
	int i;

	for (i = nppi-1; i > 0; i--) {
		ppb_remove_device(&ppidata[i]->ppi_dev);
		free(ppidata[i], M_TEMP);
	}

	return (0);
}

int
ppi_mod(struct lkm_table *lkmtp, int cmd, int ver)
{
	DISPATCH(lkmtp, cmd, ver, ppi_load, ppi_unload, lkm_nullcmd);
}

#endif /* PPI_MODULE */

static ppi_devsw_installed = 0;

static void ppi_drvinit(void *unused)
{
	dev_t dev;

	if (!ppi_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev, &ppi_cdevsw, NULL);
		ppi_devsw_installed = 1;
    	}
}

SYSINIT(ppidev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ppi_drvinit,NULL)

#endif /* NPPI */
