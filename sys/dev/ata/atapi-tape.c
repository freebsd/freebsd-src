/*-
 * Copyright (c) 1998 - 2004 Søren Schmidt <sos@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/mtio.h>
#include <sys/devicestat.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/bus.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-tape.h>

/* device structures */
static	d_open_t	ast_open;
static	d_close_t	ast_close;
static	d_ioctl_t	ast_ioctl;
static	d_strategy_t	ast_strategy;
static struct cdevsw ast_cdevsw = {
	.d_version =	D_VERSION,
	.d_open =	ast_open,
	.d_close =	ast_close,
	.d_read =	physread,
	.d_write =	physwrite,
	.d_ioctl =	ast_ioctl,
	.d_strategy =	ast_strategy,
	.d_name =	"ast",
	.d_flags =	D_TAPE | D_TRACKCLOSE,
};

/* prototypes */
static void ast_detach(struct ata_device *);
static void ast_start(struct ata_device *);
static int ast_sense(struct ast_softc *);
static void ast_describe(struct ast_softc *);
static void ast_done(struct ata_request *);
static int ast_mode_sense(struct ast_softc *, int, void *, int); 
static int ast_mode_select(struct ast_softc *, void *, int);
static int ast_write_filemark(struct ast_softc *, u_int8_t);
static int ast_read_position(struct ast_softc *, int, struct ast_readposition *);
static int ast_space(struct ast_softc *, u_int8_t, int32_t);
static int ast_locate(struct ast_softc *, int, u_int32_t);
static int ast_prevent_allow(struct ast_softc *stp, int);
static int ast_load_unload(struct ast_softc *, u_int8_t);
static int ast_rewind(struct ast_softc *);
static int ast_erase(struct ast_softc *);
static int ast_test_ready(struct ata_device *);
static int ast_wait_dsc(struct ata_device *, int);

/* internal vars */
static u_int32_t ast_lun_map = 0;
static u_int64_t ast_total = 0;
static MALLOC_DEFINE(M_AST, "AST driver", "ATAPI tape driver buffers");

void 
ast_attach(struct ata_device *atadev)
{
    struct ast_softc *stp;
    struct ast_readposition position;
    struct cdev *dev;

    stp = malloc(sizeof(struct ast_softc), M_AST, M_NOWAIT | M_ZERO);
    if (!stp) {
	ata_prtdev(atadev, "out of memory\n");
	return;
    }

    stp->device = atadev;
    stp->lun = ata_get_lun(&ast_lun_map);
    ata_set_name(atadev, "ast", stp->lun);
    bioq_init(&stp->queue);
    mtx_init(&stp->queue_mtx, "ATAPI TAPE bioqueue lock", NULL, MTX_DEF);

    if (ast_sense(stp)) {
	free(stp, M_AST);
	return;
    }

    if (!strcmp(atadev->param->model, "OnStream DI-30")) {
	struct ast_transferpage transfer;
	struct ast_identifypage identify;

	stp->flags |= F_ONSTREAM;
	bzero(&transfer, sizeof(struct ast_transferpage));
	ast_mode_sense(stp, ATAPI_TAPE_TRANSFER_PAGE,
		       &transfer, sizeof(transfer));
	bzero(&identify, sizeof(struct ast_identifypage));
	ast_mode_sense(stp, ATAPI_TAPE_IDENTIFY_PAGE,
		       &identify, sizeof(identify));
	strncpy(identify.ident, "FBSD", 4);
	ast_mode_select(stp, &identify, sizeof(identify));
	ast_read_position(stp, 0, &position);
    }

    stp->stats = devstat_new_entry("ast", stp->lun, DEV_BSIZE,
		      DEVSTAT_NO_ORDERED_TAGS,
		      DEVSTAT_TYPE_SEQUENTIAL | DEVSTAT_TYPE_IF_IDE,
		      DEVSTAT_PRIORITY_TAPE);
    dev = make_dev(&ast_cdevsw, 2 * stp->lun,
		   UID_ROOT, GID_OPERATOR, 0640, "ast%d", stp->lun);
    dev->si_drv1 = stp;
    if (atadev->channel->dma)
	dev->si_iosize_max = atadev->channel->dma->max_iosize;
    else
	dev->si_iosize_max = DFLTPHYS;
    stp->dev1 = dev;
    dev = make_dev(&ast_cdevsw, 2 * stp->lun + 1,
		   UID_ROOT, GID_OPERATOR, 0640, "nast%d", stp->lun);

    dev->si_drv1 = stp;
    if (atadev->channel->dma)
	dev->si_iosize_max = atadev->channel->dma->max_iosize;
    else
	dev->si_iosize_max = DFLTPHYS;
    stp->dev2 = dev;

    /* setup the function ptrs */ 
    atadev->detach = ast_detach;
    atadev->start = ast_start;
    atadev->softc = stp;
    atadev->flags |= ATA_D_MEDIA_CHANGED;

    /* announce we are here */
    ast_describe(stp);
}

static void	
ast_detach(struct ata_device *atadev)
{   
    struct ast_softc *stp = atadev->softc;
    
    mtx_lock(&stp->queue_mtx);
    bioq_flush(&stp->queue, NULL, ENXIO);
    mtx_unlock(&stp->queue_mtx);
    mtx_destroy(&stp->queue_mtx);
    destroy_dev(stp->dev1);
    destroy_dev(stp->dev2);
    devstat_remove_entry(stp->stats);
    ata_prtdev(atadev, "WARNING - removed from configuration\n");
    ata_free_name(atadev);
    ata_free_lun(&ast_lun_map, stp->lun);
    atadev->attach = NULL;
    atadev->detach = NULL;
    atadev->start = NULL;
    atadev->softc = NULL;
    atadev->flags = 0;
    free(stp, M_AST);
}

static int
ast_sense(struct ast_softc *stp)
{
    int count;

    /* get drive capabilities, some bugridden drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
	if (!ast_mode_sense(stp, ATAPI_TAPE_CAP_PAGE,
			    &stp->cap, sizeof(stp->cap)) &&
	    stp->cap.page_code == ATAPI_TAPE_CAP_PAGE) {
	    if (stp->cap.blk32k)
		stp->blksize = 32768;
	    if (stp->cap.blk1024)
		stp->blksize = 1024;
	    if (stp->cap.blk512)
		stp->blksize = 512;
	    if (!stp->blksize)
		continue;
	    stp->cap.max_speed = ntohs(stp->cap.max_speed);
	    stp->cap.max_defects = ntohs(stp->cap.max_defects);
	    stp->cap.ctl = ntohs(stp->cap.ctl);
	    stp->cap.speed = ntohs(stp->cap.speed);
	    stp->cap.buffer_size = ntohs(stp->cap.buffer_size);
	    return 0;
	}
    }
    return 1;
}

static void 
ast_describe(struct ast_softc *stp)
{
    if (bootverbose) {
	ata_prtdev(stp->device, "<%.40s/%.8s> tape drive at ata%d as %s\n",
		   stp->device->param->model, stp->device->param->revision,
		   device_get_unit(stp->device->channel->dev),
		   (stp->device->unit == ATA_MASTER) ? "master" : "slave");
	ata_prtdev(stp->device, "%dKB/s, ", stp->cap.max_speed);
	printf("transfer limit %d blk%s, ",
	       stp->cap.ctl, (stp->cap.ctl > 1) ? "s" : "");
	printf("%dKB buffer, ", (stp->cap.buffer_size * DEV_BSIZE) / 1024);
	printf("%s\n", ata_mode2str(stp->device->mode));
	ata_prtdev(stp->device, "Medium: ");
	switch (stp->cap.medium_type) {
	    case 0x00:
		printf("none"); break;
	    case 0x17:
		printf("Travan 1 (400 Mbyte)"); break;
	    case 0xb6:
		printf("Travan 4 (4 Gbyte)"); break;
	    case 0xda:
		printf("OnStream ADR (15Gyte)"); break;
	    default:
		printf("unknown (0x%x)", stp->cap.medium_type);
	}
	if (stp->cap.readonly) printf(", readonly");
	if (stp->cap.reverse) printf(", reverse");
	if (stp->cap.eformat) printf(", eformat");
	if (stp->cap.qfa) printf(", qfa");
	if (stp->cap.lock) printf(", lock");
	if (stp->cap.locked) printf(", locked");
	if (stp->cap.prevent) printf(", prevent");
	if (stp->cap.eject) printf(", eject");
	if (stp->cap.disconnect) printf(", disconnect");
	if (stp->cap.ecc) printf(", ecc");
	if (stp->cap.compress) printf(", compress");
	if (stp->cap.blk512) printf(", 512b");
	if (stp->cap.blk1024) printf(", 1024b");
	if (stp->cap.blk32k) printf(", 32kb");
	printf("\n");
    }
    else {
	ata_prtdev(stp->device, "TAPE <%.40s/%.8s> at ata%d-%s %s\n",
		   stp->device->param->model, stp->device->param->revision,
		   device_get_unit(stp->device->channel->dev),
		   (stp->device->unit == ATA_MASTER) ? "master" : "slave",
		   ata_mode2str(stp->device->mode));
    }
}

static int
ast_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
    struct ast_softc *stp = dev->si_drv1;

    if (!stp || stp->device->flags & ATA_D_DETACHING)
	return ENXIO;

    if (count_dev(dev) > 1)
	return EBUSY;

    ast_test_ready(stp->device);

    if (stp->cap.lock)
	ast_prevent_allow(stp, 1);

    if (ast_sense(stp))
	ata_prtdev(stp->device, "sense media type failed\n");

    stp->device->flags &= ~ATA_D_MEDIA_CHANGED;
    stp->flags &= ~(F_DATA_WRITTEN | F_FM_WRITTEN);
    ast_total = 0;
    return 0;
}

static int 
ast_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
    struct ast_softc *stp = dev->si_drv1;

    /* flush buffers, some drives fail here, they should report ctl = 0 */
    if (stp->cap.ctl && (stp->flags & F_DATA_WRITTEN))
	ast_write_filemark(stp, 0);

    /* write filemark if data written to tape */
    if (!(stp->flags & F_ONSTREAM) &&
	(stp->flags & (F_DATA_WRITTEN | F_FM_WRITTEN)) == F_DATA_WRITTEN)
	ast_write_filemark(stp, ATAPI_WF_WRITE);

    /* if minor is even rewind on close */
    if (!(minor(dev) & 0x01))
	ast_rewind(stp);

    if (stp->cap.lock && count_dev(dev) == 1)
	ast_prevent_allow(stp, 0);

    stp->flags &= ~F_CTL_WARN;
#ifdef AST_DEBUG
    ata_prtdev(stp->device, "%ju total bytes transferred\n",
	       (uintmax_t)ast_total);
#endif
    return 0;
}

static int 
ast_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag, struct thread *td)
{
    struct ast_softc *stp = dev->si_drv1;
    int error = 0;

    switch (cmd) {
    case MTIOCGET:
	{
	    struct mtget *g = (struct mtget *) addr;

	    bzero(g, sizeof(struct mtget));
	    g->mt_type = 7;
	    g->mt_density = 1;
	    g->mt_blksiz = stp->blksize;
	    g->mt_comp = stp->cap.compress;
	    g->mt_density0 = 0; g->mt_density1 = 0;
	    g->mt_density2 = 0; g->mt_density3 = 0;
	    g->mt_blksiz0 = 0; g->mt_blksiz1 = 0;
	    g->mt_blksiz2 = 0; g->mt_blksiz3 = 0;
	    g->mt_comp0 = 0; g->mt_comp1 = 0;
	    g->mt_comp2 = 0; g->mt_comp3 = 0;
	}
	break;	 

    case MTIOCTOP:
	{	
	    int i;
	    struct mtop *mt = (struct mtop *)addr;

	    switch ((int16_t) (mt->mt_op)) {

	    case MTWEOF:
		for (i=0; i < mt->mt_count && !error; i++)
		    error = ast_write_filemark(stp, ATAPI_WF_WRITE);
		break;

	    case MTFSF:
		if (mt->mt_count)
		    error = ast_space(stp, ATAPI_SP_FM, mt->mt_count);
		break;

	    case MTBSF:
		if (mt->mt_count)
		    error = ast_space(stp, ATAPI_SP_FM, -(mt->mt_count));
		break;

	    case MTREW:
		error = ast_rewind(stp);
		break;

	    case MTOFFL:
		error = ast_load_unload(stp, ATAPI_SS_EJECT);
		break;

	    case MTNOP:
		error = ast_write_filemark(stp, 0);
		break;

	    case MTERASE:
		error = ast_erase(stp);
		break;

	    case MTEOD:
		error = ast_space(stp, ATAPI_SP_EOD, 0);
		break;

	    case MTRETENS:
		error = ast_load_unload(stp, ATAPI_SS_RETENSION|ATAPI_SS_LOAD);
		break;

	    case MTFSR:		
	    case MTBSR:
	    case MTCACHE:
	    case MTNOCACHE:
	    case MTSETBSIZ:
	    case MTSETDNSTY:
	    case MTCOMP:
	    default:
		error = EINVAL;
	    }
	}
	break;

    case MTIOCRDSPOS:
	{
	    struct ast_readposition position;

	    if ((error = ast_read_position(stp, 0, &position)))
		break;
	    *(u_int32_t *)addr = position.tape;
	}
	break;

    case MTIOCRDHPOS:
	{
	    struct ast_readposition position;

	    if ((error = ast_read_position(stp, 1, &position)))
		break;
	    *(u_int32_t *)addr = position.tape;
	}
	break;

    case MTIOCSLOCATE:
	error = ast_locate(stp, 0, *(u_int32_t *)addr);
	break;

    case MTIOCHLOCATE:
	error = ast_locate(stp, 1, *(u_int32_t *)addr);
	break;

    default:
	error = ENOTTY;
    }
    return error;
}

static void 
ast_strategy(struct bio *bp)
{
    struct ast_softc *stp = bp->bio_dev->si_drv1;

    if (stp->device->flags & ATA_D_DETACHING) {
	biofinish(bp, NULL, ENXIO);
	return;
    }

    /* if it's a null transfer, return immediatly. */
    if (bp->bio_bcount == 0) {
	bp->bio_resid = 0;
	biodone(bp);
	return;
    }
    if (!(bp->bio_cmd == BIO_READ) && stp->flags & F_WRITEPROTECT) {
	biofinish(bp, NULL, EPERM);
	return;
    }
	
    /* check for != blocksize requests */
    if (bp->bio_bcount % stp->blksize) {
	ata_prtdev(stp->device, "transfers must be multiple of %d\n",
		   stp->blksize);
	biofinish(bp, NULL, EIO);
	return;
    }

    /* warn about transfers bigger than the device suggests */
    if (bp->bio_bcount > stp->blksize * stp->cap.ctl) {	 
	if ((stp->flags & F_CTL_WARN) == 0) {
	    ata_prtdev(stp->device, "WARNING: CTL exceeded %ld>%d\n",
		       bp->bio_bcount, stp->blksize * stp->cap.ctl);
	    stp->flags |= F_CTL_WARN;
	}
    }

    mtx_lock(&stp->queue_mtx);
    bioq_insert_tail(&stp->queue, bp);
    mtx_unlock(&stp->queue_mtx);
    ata_start(stp->device->channel);
}

static void 
ast_start(struct ata_device *atadev)
{
    struct ast_softc *stp = atadev->softc;
    struct bio *bp;
    struct ata_request *request;
    u_int32_t blkcount;
    int8_t ccb[16];
    
    mtx_lock(&stp->queue_mtx);
    bp = bioq_first(&stp->queue);
    if (!bp) {
	mtx_unlock(&stp->queue_mtx);
	return;
    }
    bioq_remove(&stp->queue, bp);
    mtx_unlock(&stp->queue_mtx);

    bzero(ccb, sizeof(ccb));

    if (bp->bio_cmd == BIO_READ)
	ccb[0] = ATAPI_READ;
    else
	ccb[0] = ATAPI_WRITE;
    
    blkcount = bp->bio_bcount / stp->blksize;

    ccb[1] = 1;
    ccb[2] = blkcount>>16;
    ccb[3] = blkcount>>8;
    ccb[4] = blkcount;

    if (!(request = ata_alloc_request())) {
	biofinish(bp, NULL, ENOMEM);
	return;
    }
    request->device = atadev;
    request->driver = bp;
    bcopy(ccb, request->u.atapi.ccb,
	  (request->device->param->config & ATA_PROTO_MASK) == 
	  ATA_PROTO_ATAPI_12 ? 16 : 12);
    request->data = bp->bio_data;
    request->bytecount = blkcount * stp->blksize;
    request->transfersize = min(request->bytecount, 65534);
    request->timeout = (ccb[0] == ATAPI_WRITE_BIG) ? 180 : 120;
    request->retries = 2;
    request->callback = ast_done;
    switch (bp->bio_cmd) {
    case BIO_READ:
	request->flags |= (ATA_R_ATAPI | ATA_R_READ);
	break;
    case BIO_WRITE:
	request->flags |= (ATA_R_ATAPI | ATA_R_WRITE);
	break;
    default:
	ata_prtdev(atadev, "unknown BIO operation\n");
	ata_free_request(request);
	biofinish(bp, NULL, EIO);
	return;
    }
    devstat_start_transaction_bio(stp->stats, bp);
    ata_queue_request(request);
}

static void 
ast_done(struct ata_request *request)
{
    struct bio *bp = request->driver;
    struct ast_softc *stp = request->device->softc;

    /* finish up transfer */
    if ((bp->bio_error = request->result))
	bp->bio_flags |= BIO_ERROR;
    if (bp->bio_cmd == BIO_WRITE)
	stp->flags |= F_DATA_WRITTEN;
    bp->bio_resid = bp->bio_bcount - request->donecount;
    ast_total += (bp->bio_bcount - bp->bio_resid);
    biofinish(bp, stp->stats, 0);
    ata_free_request(request);
}

static int
ast_mode_sense(struct ast_softc *stp, int page, void *pagebuf, int pagesize)
{
    int8_t ccb[16] = { ATAPI_MODE_SENSE, 0x08, page, pagesize>>8, pagesize,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int error;
 
    error = ata_atapicmd(stp->device, ccb, pagebuf, pagesize, ATA_R_READ, 10);
#ifdef AST_DEBUG
    atapi_dump("ast: mode sense ", pagebuf, pagesize);
#endif
    return error;
}

static int	 
ast_mode_select(struct ast_softc *stp, void *pagebuf, int pagesize)
{
    int8_t ccb[16] = { ATAPI_MODE_SELECT, 0x10, 0, pagesize>>8, pagesize,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
     
#ifdef AST_DEBUG
    ata_prtdev(stp->device, "modeselect pagesize=%d\n", pagesize);
    atapi_dump("mode select ", pagebuf, pagesize);
#endif
    return ata_atapicmd(stp->device, ccb, pagebuf, pagesize, 0, 10);
}

static int
ast_write_filemark(struct ast_softc *stp, u_int8_t function)
{
    int8_t ccb[16] = { ATAPI_WEOF, 0x01, 0, 0, function, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    if (stp->flags & F_ONSTREAM)
	ccb[4] = 0x00;		/* only flush buffers supported */
    else {
	if (function) {
	    if (stp->flags & F_FM_WRITTEN)
		stp->flags &= ~F_DATA_WRITTEN;
	    else
		stp->flags |= F_FM_WRITTEN;
	}
    }
    error = ata_atapicmd(stp->device, ccb, NULL, 0, 0, 10);
    if (error)
	return error;
    return ast_wait_dsc(stp->device, 10*60);
}

static int
ast_read_position(struct ast_softc *stp, int hard,
		  struct ast_readposition *position)
{
    int8_t ccb[16] = { ATAPI_READ_POSITION, (hard ? 0x01 : 0), 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    error = ata_atapicmd(stp->device, ccb, (caddr_t)position, 
			 sizeof(struct ast_readposition), ATA_R_READ, 10);
    position->tape = ntohl(position->tape);
    position->host = ntohl(position->host);
    return error;
}

static int
ast_space(struct ast_softc *stp, u_int8_t function, int32_t count)
{
    int8_t ccb[16] = { ATAPI_SPACE, function, count>>16, count>>8, count,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(stp->device, ccb, NULL, 0, 0, 60*60);
}

static int
ast_locate(struct ast_softc *stp, int hard, u_int32_t pos)
{
    int8_t ccb[16] = { ATAPI_LOCATE, 0x01 | (hard ? 0x4 : 0), 0,
		       pos>>24, pos>>16, pos>>8, pos,
		       0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    error = ata_atapicmd(stp->device, ccb, NULL, 0, 0, 10);
    if (error)
	return error;
    return ast_wait_dsc(stp->device, 60*60);
}

static int
ast_prevent_allow(struct ast_softc *stp, int lock)
{
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(stp->device, ccb, NULL, 0, 0, 30);
}

static int
ast_load_unload(struct ast_softc *stp, u_int8_t function)
{
    int8_t ccb[16] = { ATAPI_START_STOP, 0x01, 0, 0, function, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    if ((function & ATAPI_SS_EJECT) && !stp->cap.eject)
	return 0;
    error = ata_atapicmd(stp->device, ccb, NULL, 0, 0, 10);
    if (error)
	return error;
    tsleep((caddr_t)&error, PRIBIO, "astlu", 1 * hz);
    if (function == ATAPI_SS_EJECT)
	return 0;
    return ast_wait_dsc(stp->device, 60*60);
}

static int
ast_rewind(struct ast_softc *stp)
{
    int8_t ccb[16] = { ATAPI_REZERO, 0x01, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    error = ata_atapicmd(stp->device, ccb, NULL, 0, 0, 10);
    if (error)
	return error;
    return ast_wait_dsc(stp->device, 60*60);
}

static int
ast_erase(struct ast_softc *stp)
{
    int8_t ccb[16] = { ATAPI_ERASE, 3, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    if ((error = ast_rewind(stp)))
	return error;

    return ata_atapicmd(stp->device, ccb, NULL, 0, 0, 60*60);
}

static int
ast_test_ready(struct ata_device *atadev)
{
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(atadev, ccb, NULL, 0, 0, 30);
}

static int
ast_wait_dsc(struct ata_device *atadev, int timeout)
{
    int error = 0;
    int8_t ccb[16] = { ATAPI_POLL_DSC, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    timeout *= hz;
    while (timeout > 0) {
	error = ata_atapicmd(atadev, ccb, NULL, 0, 0, 0);
	if (error != EBUSY)
	    break;
	tsleep(&error, PRIBIO, "atpwt", hz / 2);
	timeout -= (hz / 2);
    }
    return error;
}
