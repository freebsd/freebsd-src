/*-
 * Copyright (c) 1998,1999 Søren Schmidt
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
 *	$Id: atapi-tape.c,v 1.9 1999/05/30 16:51:16 phk Exp $
 */

#include "ata.h"
#include "atapist.h"
#include "opt_devfs.h"

#if NATA > 0 && NATAPIST > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/mtio.h>
#include <sys/disklabel.h>
#include <sys/devicestat.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <machine/clock.h>
#include <pci/pcivar.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>
#include <dev/ata/atapi-tape.h>

static  d_open_t	astopen;
static  d_close_t	astclose;
static  d_ioctl_t	astioctl;
static  d_strategy_t	aststrategy;

#define BDEV_MAJOR 33
#define CDEV_MAJOR 119

static struct cdevsw ast_cdevsw = {
	/* open */	astopen,
	/* close */	astclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	astioctl,
	/* stop */	nostop,
	/* reset */	noreset,
	/* devtotty */	nodevtotty,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	aststrategy,
	/* name */	"ast",
	/* parms */	noparms,
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TAPE,
	/* maxio */	0,
	/* bmaj */	BDEV_MAJOR
};

static u_int32_t ast_total = 0;

#define NUNIT 			8
#define UNIT(d)         	((minor(d) >> 3) & 3)

#define F_OPEN            	0x0001	/* The device is opened */
#define F_MEDIA_CHANGED   	0x0002	/* The media have changed */
#define F_DATA_WRITTEN		0x0004	/* Data has been written */
#define F_FM_WRITTEN		0x0008	/* Filemark has been written */
#define F_CTL_WARN         	0x0010	/* Have we warned about CTL wrong? */

static struct ast_softc *asttab[NUNIT];	/* Drive info by unit number */
static int32_t astnlun = 0;                 /* Number of config'd drives */

int32_t astattach(struct atapi_softc *);
static int32_t ast_sense(struct ast_softc *);
static void ast_describe(struct ast_softc *);
static void ast_start(struct ast_softc *);
static void ast_done(struct atapi_request *);
static void ast_drvinit(void *);
static int32_t ast_space_cmd(struct ast_softc *, u_int8_t, u_int32_t);
static int32_t ast_write_filemark(struct ast_softc *, u_int8_t);
static int32_t ast_erase(struct ast_softc *);
static int32_t ast_load_unload(struct ast_softc *, u_int8_t);
static int32_t ast_rewind(struct ast_softc *);

int32_t 
astattach(struct atapi_softc *atp)
{
    struct ast_softc *stp;

    if (astnlun >= NUNIT) {
        printf("ast: too many units\n");
        return -1;
    }
    stp = malloc(sizeof(struct ast_softc), M_TEMP, M_NOWAIT);
    if (!stp) {
        printf("ast: out of memory\n");
        return -1;
    }
    bzero(stp, sizeof(struct ast_softc));
    bufq_init(&stp->buf_queue);
    stp->atp = atp;
    stp->lun = astnlun;
    stp->flags = F_MEDIA_CHANGED;

    if (ast_sense(stp)) {
	free(stp, M_TEMP);
	return -1;
    }

    ast_describe(stp);
    asttab[astnlun++] = stp;
    devstat_add_entry(&stp->stats, "ast", stp->lun, DEV_BSIZE,
                      DEVSTAT_NO_ORDERED_TAGS,
                      DEVSTAT_TYPE_SEQUENTIAL | DEVSTAT_TYPE_IF_IDE,
                      0x178);
#ifdef DEVFS
    stp->cdevs_token = devfs_add_devswf(&ast_cdevsw, dkmakeminor(stp->lun, 0,0),
					DV_CHR, UID_ROOT, GID_OPERATOR, 0640, 
					"rast%d", stp->lun);
#endif
    return 0;
}

static int32_t 
ast_sense(struct ast_softc *stp)
{
    int32_t error, count;
    int8_t buffer[256];
    int8_t ccb[16] = { ATAPI_TAPE_MODE_SENSE,
            	     8, /* DBD = 1 no block descr */
            	     ATAPI_TAPE_CAP_PAGE,
            	     sizeof(buffer)>>8, sizeof(buffer) & 0xff,
            	     0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    bzero(buffer, sizeof(buffer));
    /* Get drive capabilities, some drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
        if (!(error = atapi_queue_cmd(stp->atp, ccb, buffer, sizeof(buffer),
				      A_READ, NULL, NULL, NULL)))
            break;
    }
#ifdef AST_DEBUG
    atapi_dump("ast: sense", buffer, sizeof(buffer));
#endif
    if (error)
        return error;
    bcopy(buffer, &stp->header, sizeof(struct ast_header));
    bcopy(buffer+sizeof(struct ast_header), &stp->cap, 
	  sizeof(struct ast_cappage));
    if (stp->cap.page_code != ATAPI_TAPE_CAP_PAGE)
        return 1;   
    stp->cap.max_speed = ntohs(stp->cap.max_speed);
    stp->cap.max_defects = ntohs(stp->cap.max_defects);
    stp->cap.ctl = ntohs(stp->cap.ctl);
    stp->cap.speed = ntohs(stp->cap.speed);
    stp->cap.buffer_size = ntohs(stp->cap.buffer_size);
    stp->blksize = (stp->cap.blk512 ? 512 : (stp->cap.blk1024 ? 1024 : 0));
    return 0;
}

static void 
ast_describe(struct ast_softc *stp)
{
    int8_t model_buf[40+1];
    int8_t revision_buf[8+1];

    bpack(stp->atp->atapi_parm->model, model_buf, sizeof(model_buf));
    bpack(stp->atp->atapi_parm->revision, revision_buf, sizeof(revision_buf));
    printf("ast%d: <%s/%s> tape drive at ata%d as %s\n",
	   stp->lun, model_buf, revision_buf,
           stp->atp->controller->lun,
	   (stp->atp->unit == ATA_MASTER) ? "master" : "slave ");
    printf("ast%d: ", stp->lun);
    switch (stp->header.medium_type) {
	case 0x00:	printf("Drive empty"); break;
	case 0x17:	printf("Travan 1 (400 Mbyte) media"); break;
	case 0xb6:	printf("Travan 4 (4 Gbyte) media"); break;
	default: printf("Unknown media (0x%x)", stp->header.medium_type);
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
    if (stp->cap.slowb) printf(", slowb");
    printf("\nast%d: ", stp->lun);
    printf("Max speed=%dKb/s, ", stp->cap.max_speed);
    printf("Transfer limit=%d blocks, ", stp->cap.ctl);
    printf("Buffer size=%d blocks", stp->cap.buffer_size);
    printf("\n");
}

static int
astopen(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    int32_t lun = UNIT(dev);
    struct ast_softc *stp;

    if (lun >= astnlun || !(stp = asttab[lun])) 
        return ENXIO;
    if (stp->flags == F_OPEN)
        return EBUSY;
    if (ast_sense(stp))
        printf("ast%d: sense media type failed\n", stp->lun);
    stp->flags &= ~F_MEDIA_CHANGED;
    stp->flags &= ~(F_DATA_WRITTEN | F_FM_WRITTEN);
    stp->flags |= F_OPEN;
    ast_total = 0;
    return 0;
}

static int 
astclose(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    int32_t lun = UNIT(dev);
    struct ast_softc *stp;

    if (lun >= astnlun || !(stp = asttab[lun]))      
        return ENXIO;

    /* Flush buffers, some drives fail here, but they should report ctl = 0 */
    if (stp->cap.ctl && (stp->flags & F_DATA_WRITTEN))
        ast_write_filemark(stp, 0);

    /* Write filemark if data written to tape */
    if ((stp->flags & (F_DATA_WRITTEN | F_FM_WRITTEN)) == F_DATA_WRITTEN)
        ast_write_filemark(stp, WEOF_WRITE_MASK);

    /* If minor is even rewind on close */
    if (!(minor(dev) & 0x01))	
	ast_rewind(stp);

    stp->flags &= ~F_OPEN;
#ifdef AST_DEBUG
	printf("ast%d: %ud total bytes transferred\n", stp->lun, ast_total);
#endif
    stp->flags &= ~F_CTL_WARN;
    return 0;
}

static int 
astioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    int32_t lun = UNIT(dev);
    int32_t error = 0;
    struct ast_softc *stp;

    if (lun >= astnlun || !(stp = asttab[lun]))
        return ENXIO;

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
            break;       
	}
    case MTIOCTOP:
        {       
	    int32_t i;
    	    struct mtop *mt = (struct mtop *)addr;

            switch ((int16_t) (mt->mt_op)) {
            case MTWEOF:
		for (i=0; i < mt->mt_count && !error; i++)
			error = ast_write_filemark(stp, WEOF_WRITE_MASK);
                break;
            case MTFSF:
		if (mt->mt_count)
			error = ast_space_cmd(stp, SP_FM, mt->mt_count);
                break;
            case MTBSF:
		if (mt->mt_count)
			error = ast_space_cmd(stp, SP_FM, -(mt->mt_count));
                break;
            case MTFSR:
                error = EINVAL; break;
            case MTBSR:
                error = EINVAL; break;
            case MTREW:
                error = ast_rewind(stp);
		break;
            case MTOFFL:
                if ((error = ast_rewind(stp)))
		    break;
                error = ast_load_unload(stp, !LU_LOAD_MASK);
		    break;
            case MTNOP:
		error = ast_write_filemark(stp, 0);
                break;
            case MTCACHE:
                error = EINVAL; break;
            case MTNOCACHE:
                error = EINVAL; break;
            case MTSETBSIZ:
                error = EINVAL; break;
            case MTSETDNSTY:
                error = EINVAL; break;
            case MTERASE:
                error = ast_erase(stp);
		break;
            case MTEOD:
		error = ast_space_cmd(stp, SP_EOD, 0);
                break;
            case MTCOMP:
                error = EINVAL; break;
            case MTRETENS:
                error = ast_load_unload(stp, LU_RETENSION_MASK|LU_LOAD_MASK);
		break;
            default:
                error = EINVAL;
            }
            return error;
        }
    default:
        return ENOTTY;
    }
    return error;
}

static void 
aststrategy(struct buf *bp)
{
    int32_t lun = UNIT(bp->b_dev);
    struct ast_softc *stp = asttab[lun];
    int32_t x;

    /* If it's a null transfer, return immediatly. */
    if (bp->b_bcount == 0) {
        bp->b_resid = 0;
        biodone(bp);
        return;
    }

    /* Check for != blocksize requests */
    if (bp->b_bcount % stp->blksize) {
        printf("ast%d: bad request, must be multiple of %d\n",
	       lun, stp->blksize);
        bp->b_error = EIO;
	bp->b_flags |= B_ERROR;
        biodone(bp);
        return;
    }
    if (bp->b_bcount > stp->blksize * stp->cap.ctl) {  
	if ((stp->flags & F_CTL_WARN) == 0) {
            printf("ast%d: WARNING: CTL exceeded %ld>%d\n", 
		    lun, bp->b_bcount, stp->blksize * stp->cap.ctl);
	    stp->flags |= F_CTL_WARN;
	}
    }

    x = splbio();
    ast_total += bp->b_bcount;
    bufq_insert_tail(&stp->buf_queue, bp);
    ast_start(stp);
    splx(x);
}

static void 
ast_start(struct ast_softc *stp)
{
    struct buf *bp = bufq_first(&stp->buf_queue);
    u_int32_t blkcount;
    int8_t ccb[16];
    
    if (!bp)
        return;
    bzero(ccb, sizeof(ccb));
    bufq_remove(&stp->buf_queue, bp);
    blkcount = bp->b_bcount / stp->blksize;
    if (bp->b_flags & B_READ) {
        ccb[0] = ATAPI_TAPE_READ_CMD;
    } else {
        ccb[0] = ATAPI_TAPE_WRITE_CMD;
	stp->flags |= F_DATA_WRITTEN;
    }
    ccb[1] = 1;
    ccb[2] = blkcount>>16;
    ccb[3] = blkcount>>8;
    ccb[4] = blkcount;

    devstat_start_transaction(&stp->stats);

    atapi_queue_cmd(stp->atp, ccb, bp->b_data, bp->b_bcount, 
		    (bp->b_flags & B_READ) ? A_READ : 0, ast_done, stp, bp);
}

static void 
ast_done(struct atapi_request *request)
{
    struct buf *bp = request->bp;
    struct ast_softc *stp = request->driver;

    devstat_end_transaction(&stp->stats, request->donecount,
                            DEVSTAT_TAG_NONE,
                            (bp->b_flags&B_READ) ? DEVSTAT_READ:DEVSTAT_WRITE);
 
    if (request->result) {
	/* check for EOM and return ENOSPC */
        atapi_error(request->device, request->result);
        bp->b_error = EIO;
        bp->b_flags |= B_ERROR;
    }
    else
	bp->b_resid = request->bytecount;
    biodone(bp);
    ast_start(stp);
}

static int32_t
ast_space_cmd(struct ast_softc *stp, u_int8_t function, u_int32_t count)
{
    int32_t error;
    int8_t ccb[16];

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_TAPE_SPACE_CMD;
    ccb[1] = function;
    ccb[2] = count>>16;
    ccb[3] = count>>8;
    ccb[4] = count;

    if ((error = atapi_queue_cmd(stp->atp, ccb, NULL, 0, 0, NULL, NULL, NULL))){
        atapi_error(stp->atp, error);
        return EIO;
    }
    return 0;
}

static int32_t
ast_write_filemark(struct ast_softc *stp, u_int8_t function)
{
    int32_t error;
    int8_t ccb[16];

    if (function) {
	if (stp->flags & F_FM_WRITTEN)
	    stp->flags &= ~F_DATA_WRITTEN;
        else
	    stp->flags |= F_FM_WRITTEN;
    }
    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_TAPE_WEOF;
    ccb[4] = function;

    if ((error = atapi_queue_cmd(stp->atp, ccb, NULL, 0, 0, NULL, NULL, NULL))){
        atapi_error(stp->atp, error);
        return EIO;
    }
    return 0;
}

static int32_t
ast_load_unload(struct ast_softc *stp, u_int8_t function)
{
    int32_t error;
    int8_t ccb[16];

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_TAPE_LOAD_UNLOAD;
    ccb[4] = function;

    if ((error = atapi_queue_cmd(stp->atp, ccb, NULL, 0, 0, NULL, NULL, NULL))){
        atapi_error(stp->atp, error);
        return EIO;
    }
    return 0;
}

static int32_t
ast_erase(struct ast_softc *stp)
{
    int32_t error;
    int8_t ccb[16];

    if ((error = ast_rewind(stp)))
        return error;

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_TAPE_ERASE;
    ccb[1] = 3;

    if ((error = atapi_queue_cmd(stp->atp, ccb, NULL, 0, 0, NULL, NULL, NULL))){
        atapi_error(stp->atp, error);
        return EIO;
    }
    return 0;
}

static int32_t
ast_rewind(struct ast_softc *stp)
{
    int32_t error;
    int8_t ccb[16];

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_TAPE_REWIND;

    if ((error = atapi_queue_cmd(stp->atp, ccb, NULL, 0, 0, NULL, NULL, NULL))){
        atapi_error(stp->atp, error);
        return EIO;
    }
    return 0;
}

static void 
ast_drvinit(void *unused)
{
    static int32_t ast_devsw_installed = 0;

    if (!ast_devsw_installed) {
        cdevsw_add(&ast_cdevsw);
        ast_devsw_installed = 1;
    }
}

SYSINIT(astdev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, ast_drvinit, NULL)
#endif /* NATA & NATAPIST */
