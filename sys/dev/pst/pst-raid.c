/*-
 * Copyright (c) 2001,2002 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/bio.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <pci/pcivar.h>
#include <pci/pcireg.h>

#include "dev/pst/pst-iop.h"

/* device structures */ 
static d_strategy_t pststrategy;
static struct cdevsw pst_cdevsw = {
    /* open */	nullopen,
    /* close */ nullclose,
    /* read */	physread,
    /* write */ physwrite,
    /* ioctl */ noioctl,
    /* poll */	nopoll,
    /* mmap */	nommap,
    /* strat */ pststrategy,
    /* name */	"pst",
    /* maj */	200,
    /* dump */	nodump,
    /* psize */ nopsize,
    /* flags */ D_DISK,
};
static struct cdevsw pstdisk_cdevsw;

struct pst_softc {
    struct iop_softc		*iop;
    struct i2o_lct_entry	*lct;
    struct i2o_bsa_device	*info;
    dev_t			device;
    struct devstat		stats;
    struct disk			disk;
    struct bio_queue_head	queue;
    struct mtx			mtx;
    int				outstanding;
};

struct pst_request {
    struct pst_softc		*psc;		/* pointer to softc */
    u_int32_t			mfa;		/* frame addreess */
    struct callout_handle	timeout_handle; /* handle for untimeout */
    struct bio			*bp;		/* associated bio ptr */
};

/* prototypes */
static int pst_probe(device_t);
static int pst_attach(device_t);
static int pst_shutdown(device_t);
static void pst_start(struct pst_softc *);
static void pst_done(struct iop_softc *, u_int32_t, struct i2o_single_reply *);
static int pst_rw(struct pst_request *);
static void pst_timeout(struct pst_request *);
static void bpack(int8_t *, int8_t *, int);

/* local vars */
static MALLOC_DEFINE(M_PSTRAID, "pst", "Promise SuperTrak RAID driver");

int
pst_add_raid(struct iop_softc *sc, struct i2o_lct_entry *lct)
{
    struct pst_softc *psc;
    device_t child = device_add_child(sc->dev, "pst", -1);

    if (!child)
	return ENOMEM;
    psc = malloc(sizeof(struct pst_softc), M_PSTRAID, M_NOWAIT | M_ZERO); 
    psc->iop = sc;
    psc->lct = lct;
    device_set_softc(child, psc);
    return bus_generic_attach(sc->dev);
}

static int
pst_probe(device_t dev)
{
    device_set_desc(dev, "Promise SuperTrak RAID");
    return 0;
}

static int
pst_attach(device_t dev)
{
    struct pst_softc *psc = device_get_softc(dev);
    struct i2o_get_param_reply *reply;
    struct i2o_device_identity *ident;
    int lun = device_get_unit(dev);
    int8_t name [32];

    if (!(reply = iop_get_util_params(psc->iop, psc->lct->local_tid,
				      I2O_PARAMS_OPERATION_FIELD_GET,
				      I2O_BSA_DEVICE_INFO_GROUP_NO)))
	return ENODEV;

    if (!(psc->info = (struct i2o_bsa_device *)
	    malloc(sizeof(struct i2o_bsa_device), M_PSTRAID, M_NOWAIT))) {
	contigfree(reply, PAGE_SIZE, M_PSTRAID);
	return ENOMEM;
    }
    bcopy(reply->result, psc->info, sizeof(struct i2o_bsa_device));
    contigfree(reply, PAGE_SIZE, M_PSTRAID);

    if (!(reply = iop_get_util_params(psc->iop, psc->lct->local_tid,
				      I2O_PARAMS_OPERATION_FIELD_GET,
				      I2O_UTIL_DEVICE_IDENTITY_GROUP_NO)))
	return ENODEV;
    ident = (struct i2o_device_identity *)reply->result;
#ifdef PSTDEBUG	   
    printf("pst: vendor=<%.16s> product=<%.16s>\n",
	   ident->vendor, ident->product);
    printf("pst: description=<%.16s> revision=<%.8s>\n",
	   ident->description, ident->revision);
    printf("pst: capacity=%lld blocksize=%d\n",
	   psc->info->capacity, psc->info->block_size);
#endif
    bpack(ident->vendor, ident->vendor, 16);
    bpack(ident->product, ident->product, 16);
    sprintf(name, "%s %s", ident->vendor, ident->product);
    contigfree(reply, PAGE_SIZE, M_PSTRAID);

    bioq_init(&psc->queue);
    mtx_init(&psc->mtx, "pst lock", MTX_DEF, 0);

    psc->device = disk_create(lun, &psc->disk, 0, &pst_cdevsw, &pstdisk_cdevsw);
    psc->device->si_drv1 = psc;
    psc->device->si_iosize_max = 64 * 1024; /*I2O_SGL_MAX_SEGS * PAGE_SIZE;*/

    psc->disk.d_sectorsize = psc->info->block_size;
    psc->disk.d_mediasize = psc->info->capacity;
    psc->disk.d_fssectors = 63;
    psc->disk.d_fsheads = 255;

    devstat_add_entry(&psc->stats, "pst", lun, psc->info->block_size,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_DIRECT | DEVSTAT_TYPE_IF_IDE,
		      DEVSTAT_PRIORITY_DISK);

    printf("pst%d: %lluMB <%.40s> [%d/%d/%d] on %.16s\n", lun,
	   (unsigned long long)psc->disk.d_label.d_secperunit / (1024 * 2),
	   name, psc->disk.d_label.d_ncylinders, 255, 63,
	   device_get_nameunit(psc->iop->dev));

    EVENTHANDLER_REGISTER(shutdown_post_sync, pst_shutdown,
			  dev, SHUTDOWN_PRI_FIRST);
    return 0;
}

static int
pst_shutdown(device_t dev)
{
    struct pst_softc *psc = device_get_softc(dev);
    struct i2o_bsa_cache_flush_message *msg;
    int mfa;

    mfa = iop_get_mfa(psc->iop);
    msg = (struct i2o_bsa_cache_flush_message *)(psc->iop->ibase + mfa);
    bzero(msg, sizeof(struct i2o_bsa_cache_flush_message));
    msg->version_offset = 0x01;
    msg->message_flags = 0x0;
    msg->message_size = sizeof(struct i2o_bsa_cache_flush_message) >> 2;
    msg->target_address = psc->lct->local_tid;
    msg->initiator_address = I2O_TID_HOST;
    msg->function = I2O_BSA_CACHE_FLUSH;
    msg->control_flags = 0x0; /* 0x80 = post progress reports */
    if (iop_queue_wait_msg(psc->iop, mfa, (struct i2o_basic_message *)msg))
	printf("pst: shutdown failed!\n");
    return 0;
}

static void
pststrategy(struct bio *bp)
{
    struct pst_softc *psc = bp->bio_dev->si_drv1;
    
    mtx_lock(&psc->mtx);
    bioqdisksort(&psc->queue, bp);
    pst_start(psc);
    mtx_unlock(&psc->mtx);
}

static void
pst_start(struct pst_softc *psc)
{
    struct pst_request *request;
    struct bio *bp;
    u_int32_t mfa;

    if (psc->outstanding < (I2O_IOP_OUTBOUND_FRAME_COUNT - 1) &&
	(bp = bioq_first(&psc->queue))) {
	if ((mfa = iop_get_mfa(psc->iop)) != 0xffffffff) {
	    if (!(request = malloc(sizeof(struct pst_request),
				   M_PSTRAID, M_NOWAIT | M_ZERO))) {
		printf("pst: out of memory in start\n");
		iop_free_mfa(psc->iop, mfa);
		return;
	    }
	    psc->outstanding++;
	    request->psc = psc;
	    request->mfa = mfa;
	    request->bp = bp;
	    if (dumping)
		request->timeout_handle.callout = NULL;
	    else
		request->timeout_handle =
		    timeout((timeout_t*)pst_timeout, request, 10 * hz);
	    bioq_remove(&psc->queue, bp);
	    devstat_start_transaction(&psc->stats);
	    if (pst_rw(request)) {
		biofinish(request->bp, &psc->stats, EIO);
		iop_free_mfa(request->psc->iop, request->mfa);
		psc->outstanding--;
		free(request, M_PSTRAID);
	    }
	}
    }
}

static void
pst_done(struct iop_softc *sc, u_int32_t mfa, struct i2o_single_reply *reply)
{
    struct pst_request *request =
	(struct pst_request *)reply->transaction_context;
    struct pst_softc *psc = request->psc;

    untimeout((timeout_t *)pst_timeout, request, request->timeout_handle);
    request->bp->bio_resid = request->bp->bio_bcount - reply->donecount;
    biofinish(request->bp, &psc->stats, reply->status ? EIO : 0);
    free(request, M_PSTRAID);
    mtx_lock(&psc->mtx);
    psc->iop->reg->oqueue = mfa;
    psc->outstanding--;
    pst_start(psc);
    mtx_unlock(&psc->mtx);
}

static void
pst_timeout(struct pst_request *request)
{
    printf("pst: timeout mfa=0x%08x cmd=0x%02x\n",
	   request->mfa, request->bp->bio_cmd);
    mtx_lock(&request->psc->mtx);
    iop_free_mfa(request->psc->iop, request->mfa);
    if ((request->mfa = iop_get_mfa(request->psc->iop)) == 0xffffffff) {
	printf("pst: timeout no mfa possible\n");
	biofinish(request->bp, &request->psc->stats, EIO);
	request->psc->outstanding--;
	mtx_unlock(&request->psc->mtx);
	return;
    }
    if (dumping)
	request->timeout_handle.callout = NULL;
    else
	request->timeout_handle =
	    timeout((timeout_t*)pst_timeout, request, 10 * hz);
    if (pst_rw(request)) {
	iop_free_mfa(request->psc->iop, request->mfa);
	biofinish(request->bp, &request->psc->stats, EIO);
	request->psc->outstanding--;
    }
    mtx_unlock(&request->psc->mtx);
}

int
pst_rw(struct pst_request *request)
{
    struct i2o_bsa_rw_block_message *msg;
    int sgl_flag;

    msg = (struct i2o_bsa_rw_block_message *)
	  (request->psc->iop->ibase + request->mfa);
    bzero(msg, sizeof(struct i2o_bsa_rw_block_message));
    msg->version_offset = 0x81;
    msg->message_flags = 0x0;
    msg->message_size = sizeof(struct i2o_bsa_rw_block_message) >> 2;
    msg->target_address = request->psc->lct->local_tid;
    msg->initiator_address = I2O_TID_HOST;
    switch (request->bp->bio_cmd) {
    case BIO_READ:
	msg->function = I2O_BSA_BLOCK_READ;
	msg->control_flags = 0x0; /* 0x0c = read cache + readahead */
	msg->fetch_ahead = 0x0; /* 8 Kb */
	sgl_flag = 0;
	break;
    case BIO_WRITE:
	msg->function = I2O_BSA_BLOCK_WRITE;
	msg->control_flags = 0x0; /* 0x10 = write behind cache */
	msg->fetch_ahead = 0x0;
	sgl_flag = I2O_SGL_DIR;
	break;
    default:
	printf("pst: unknown command type\n");
	return -1;
    }
    msg->initiator_context = (u_int32_t)pst_done;
    msg->transaction_context = (u_int32_t)request;
    msg->time_multiplier = 1;
    msg->bytecount = request->bp->bio_bcount;
    msg->lba = ((u_int64_t)request->bp->bio_pblkno) * (DEV_BSIZE * 1LL);
    if (!iop_create_sgl((struct i2o_basic_message *)msg, request->bp->bio_data,
			request->bp->bio_bcount, sgl_flag))
	return -1;
    request->psc->iop->reg->iqueue = request->mfa;
    return 0;
}

static void
bpack(int8_t *src, int8_t *dst, int len)
{
    int i, j, blank;
    int8_t *ptr, *buf = dst;

    for (i = j = blank = 0 ; i < len; i++) {
	if (blank && src[i] == ' ') continue;
	if (blank && src[i] != ' ') {
	    dst[j++] = src[i];
	    blank = 0;
	    continue;
	}
	if (src[i] == ' ') {
	    blank = 1;
	    if (i == 0)
		continue;
	}
	dst[j++] = src[i];
    }
    if (j < len) 
	dst[j] = 0x00;
    for (ptr = buf; ptr < buf+len; ++ptr)
	if (!*ptr)
	    *ptr = ' ';
    for (ptr = buf + len - 1; ptr >= buf && *ptr == ' '; --ptr)
        *ptr = 0;
}

static device_method_t pst_methods[] = {
    DEVMETHOD(device_probe,	pst_probe),
    DEVMETHOD(device_attach,	pst_attach),
    { 0, 0 }
};
	
static driver_t pst_driver = {
    "pst",
    pst_methods,
    sizeof(struct pst_softc),
};

static devclass_t pst_devclass;

DRIVER_MODULE(pst, pstpci, pst_driver, pst_devclass, 0, 0);
