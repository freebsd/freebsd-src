/*-
 * Copyright (c) 1998 Søren Schmidt
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

#include "wdc.h"
#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/mtio.h>
#include <machine/clock.h>
#include <pc98/pc98/atapi.h>

static  d_open_t    wstopen;
static  d_close_t   wstclose;
static  d_ioctl_t   wstioctl;
static  d_strategy_t    wststrategy;

#define CDEV_MAJOR 90

static struct cdevsw wst_cdevsw = {
	/* open */	wstopen,
	/* close */	wstclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	wstioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	wststrategy,
	/* name */	"wst",
	/* maj */	CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	0,
};

static unsigned int wst_total = 0;

#define NUNIT 			(NWDC*2)
#define UNIT(d)         	((minor(d) >> 3) & 3)

#define WST_OPEN            	0x0001	/* The device is opened */
#define WST_MEDIA_CHANGED   	0x0002	/* The media have changed */
#define WST_DATA_WRITTEN	0x0004	/* Data has been written */
#define WST_FM_WRITTEN		0x0008	/* Filemark has been written */
#define WST_DEBUG           	0x0010	/* Print debug info */
#define WST_CTL_WARN           	0x0020	/* Have we warned about CTL wrong? */

/* ATAPI tape commands not in std ATAPI command set */
#define ATAPI_TAPE_REWIND   		0x01
#define ATAPI_TAPE_REQUEST_SENSE   	0x03
#define ATAPI_TAPE_READ_CMD   		0x08
#define ATAPI_TAPE_WRITE_CMD   		0x0a
#define ATAPI_TAPE_WEOF 		0x10
#define     WEOF_WRITE_MASK    			0x01
#define ATAPI_TAPE_SPACE_CMD 		0x11
#define     SP_FM				0x01
#define     SP_EOD				0x03
#define ATAPI_TAPE_ERASE    		0x19
#define ATAPI_TAPE_MODE_SENSE   	0x1a
#define ATAPI_TAPE_LOAD_UNLOAD  	0x1b
#define     LU_LOAD_MASK       			0x01
#define     LU_RETENSION_MASK 			0x02
#define     LU_EOT_MASK     			0x04

#define DSC_POLL_INTERVAL	10

/* 
 * MODE SENSE parameter header
 */
struct wst_header {
    u_char  data_length;        	/* Total length of data */
    u_char  medium_type;       	 	/* Medium type (if any) */
    u_char  dsp;            		/* Device specific parameter */
    u_char  bdl;            		/* Block Descriptor Length */
};

/*
 * ATAPI tape drive Capabilities and Mechanical Status Page
 */
#define ATAPI_TAPE_CAP_PAGE     0x2a

struct wst_cappage {
    u_int   page_code		:6;	/* Page code == 0x2a */
    u_int   reserved1_67	:2;
    u_char  page_length;        	/* Page Length == 0x12 */
    u_char  reserved2;
    u_char  reserved3;
    u_int   readonly		:1;	/* Read Only Mode */
    u_int   reserved4_1234	:4;
    u_int   reverse		:1;	/* Supports reverse direction */
    u_int   reserved4_67	:2;
    u_int   reserved5_012	:3;
    u_int   eformat		:1;	/* Supports ERASE formatting */
    u_int   reserved5_4		:1;
    u_int   qfa     		:1;	/* Supports QFA formats */
    u_int   reserved5_67    	:2;
    u_int   lock        	:1;	/* Supports locking media */
    u_int   locked      	:1;	/* The media is locked */
    u_int   prevent     	:1;	/* Defaults  to prevent state */
    u_int   eject       	:1;	/* Supports eject */
    u_int   disconnect		:1;	/* Can break request > ctl */
    u_int   reserved6_5    	:1;
    u_int   ecc     		:1;	/* Supports error correction */
    u_int   compress    	:1;	/* Supports data compression */
    u_int   reserved7_0 	:1;
    u_int   blk512      	:1;	/* Supports 512b block size */
    u_int   blk1024     	:1;	/* Supports 1024b block size */
    u_int   reserved7_3456  	:4;
    u_int   slowb       	:1;	/* Restricts byte count */
    u_short max_speed;      		/* Supported speed in KBps */
    u_short max_defects;       		/* Max stored defect entries */
    u_short ctl;            		/* Continuous Transfer Limit */
    u_short speed;          		/* Current Speed, in KBps */
    u_short buffer_size;        	/* Buffer Size, in 512 bytes */
    u_char  reserved18;
    u_char  reserved19;
};

/*
 * REQUEST SENSE structure
 */
struct wst_reqsense {
    u_int   error_code      	:7;	/* Current or deferred errors */
    u_int   valid          	:1;	/* Follows QIC-157C */
    u_char  reserved1;			/* Segment Number - Reserved */
    u_int   sense_key		:4;	/* Sense Key */
    u_int   reserved2_4		:1;	/* Reserved */
    u_int   ili			:1;	/* Incorrect Length Indicator */
    u_int   eom			:1;	/* End Of Medium */
    u_int   filemark		:1;	/* Filemark */
    u_int   info __attribute__((packed)); /* Cmd specific info */
    u_char  asl;			/* Additional sense length (n-7) */
    u_int   command_specific;		/* Additional cmd specific info */
    u_char  asc;			/* Additional Sense Code */
    u_char  ascq;			/* Additional Sense Code Qualifier */
    u_char  replaceable_unit_code;	/* Field Replaceable Unit Code */
    u_int   sk_specific1	:7;	/* Sense Key Specific */
    u_int   sksv		:1;	/* Sense Key Specific info valid */
    u_char  sk_specific2;		/* Sense Key Specific */
    u_char  sk_specific3;		/* Sense Key Specific */
    u_char  pad[2];			/* Padding */
};

struct wst {
    struct atapi *ata;          	/* Controller structure */
    int unit;               		/* IDE bus drive unit */
    int lun;                		/* Logical device unit */
    int flags;              		/* Device state flags */
    int blksize;                	/* Block size (512 | 1024) */
    struct bio_queue_head buf_queue;    /* Queue of i/o requests */
    struct atapi_params *param;     	/* Drive parameters table */
    struct wst_header header;       	/* MODE SENSE param header */
    struct wst_cappage cap;         	/* Capabilities page info */
};

static struct wst *wsttab[NUNIT];       /* Drive info by unit number */
static int wstnlun = 0;                 /* Number of config'd drives */

int wstattach(struct atapi *ata, int unit, struct atapi_params *ap, int debug);
static int wst_sense(struct wst *t);
static void wst_describe(struct wst *t);
static void wst_poll_dsc(struct wst *t);
static void wst_start(struct wst *t);
static void wst_done(struct wst *t, struct bio *bp, int resid, struct atapires result);
static int wst_error(struct wst *t, struct atapires result);
static void wst_drvinit(void *unused);
static int wst_space_cmd(struct wst *t, u_char function, u_int count);
static int wst_write_filemark(struct wst *t, u_char function);
static int wst_erase(struct wst *t);
static int wst_load_unload(struct wst *t, u_char finction);
static int wst_rewind(struct wst *t);
static void wst_reset(struct wst *t);

#ifdef DDB
void  wst_dump(int lun, char *label, void *data, int len);

void 
wst_dump(int lun, char *label, void *data, int len)
{
    u_char *p = data;

    printf("wst%d: %s %x", lun, label, *p++);
    while(--len > 0)
        printf("-%x", *p++);
    printf("\n");
}
#endif

int 
wstattach(struct atapi *ata, int unit, struct atapi_params *ap, int debug)
{
    struct wst *t;
    int lun;

    if (wstnlun >= NUNIT) {
        printf("wst: too many units\n");
        return(-1);
    }
    if (!atapi_request_immediate) {
        printf("wst: configuration error, ATAPI core code not present!\n");
        printf(
	    "wst: check `options ATAPI_STATIC' in your kernel config file!\n");
        return(-1);
    }
    t = malloc(sizeof(struct wst), M_TEMP, M_NOWAIT | M_ZERO);
    if (!t) {
        printf("wst: out of memory\n");
        return(-1);
    }
    wsttab[wstnlun] = t;
    bioq_init(&t->buf_queue);
    t->ata = ata;
    t->unit = unit;
    t->ata->use_dsc = 1;
    lun = t->lun = wstnlun;
    t->param = ap;
    t->flags = WST_MEDIA_CHANGED | WST_DEBUG;

    if (wst_sense(t))
	return -1;

    wst_describe(t);
    wstnlun++;

    make_dev(&wst_cdevsw, 0, UID_ROOT, GID_OPERATOR, 0640, "rwst%d", t->lun);
    return(1);
}

static int 
wst_sense(struct wst *t)
{
    struct atapires result;
    int count;
    char buffer[255];

    /* Get drive capabilities, some drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
        result = atapi_request_immediate(t->ata, t->unit,
            ATAPI_TAPE_MODE_SENSE,
            8, /* DBD = 1 no block descr */
            ATAPI_TAPE_CAP_PAGE,
            sizeof(buffer)>>8, sizeof(buffer),
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, buffer, sizeof(buffer));
        if (result.code == 0 || result.code == RES_UNDERRUN)
            break;
    }

    /* Some drives have shorter capabilities page. */
    if (result.code == RES_UNDERRUN)
        result.code = 0;

    if (result.code == 0) {
        bcopy(buffer, &t->header, sizeof(struct wst_header));
        bcopy(buffer+sizeof(struct wst_header),
            &t->cap, sizeof(struct wst_cappage));
        if (t->cap.page_code != ATAPI_TAPE_CAP_PAGE)
            return 1;   
        t->cap.max_speed = ntohs(t->cap.max_speed);
        t->cap.max_defects = ntohs(t->cap.max_defects);
        t->cap.ctl = ntohs(t->cap.ctl);
        t->cap.speed = ntohs(t->cap.speed);
        t->cap.buffer_size = ntohs(t->cap.buffer_size);
        t->blksize = (t->cap.blk512 ? 512 : (t->cap.blk1024 ? 1024 : 0));
        return 0;
    }
    return 1;
}

static void 
wst_describe(struct wst *t)
{
    printf("wst%d: ", t->lun);
    switch (t->header.medium_type) {
	case 0x00:	printf("Drive empty"); break;
	case 0x17:	printf("Travan 1 (400 Mbyte) media"); break;
	case 0xb6:	printf("Travan 4 (4 Gbyte) media"); break;
	default: printf("Unknown media (0x%x)", t->header.medium_type);
    }
    if (t->cap.readonly) printf(", readonly");
    if (t->cap.reverse) printf(", reverse");
    if (t->cap.eformat) printf(", eformat");
    if (t->cap.qfa) printf(", qfa");
    if (t->cap.lock) printf(", lock");
    if (t->cap.locked) printf(", locked");
    if (t->cap.prevent) printf(", prevent");
    if (t->cap.eject) printf(", eject");
    if (t->cap.disconnect) printf(", disconnect");
    if (t->cap.ecc) printf(", ecc");
    if (t->cap.compress) printf(", compress");
    if (t->cap.blk512) printf(", 512b");
    if (t->cap.blk1024) printf(", 1024b");
    if (t->cap.slowb) printf(", slowb");
    printf("\nwst%d: ", t->lun);
    printf("Max speed=%dKb/s, ", t->cap.max_speed);
    printf("Transfer limit=%d blocks, ", t->cap.ctl);
    printf("Buffer size=%d blocks", t->cap.buffer_size);
    printf("\n");
}

int
wstopen(dev_t dev, int flags, int fmt, struct proc *p)
{
    int lun = UNIT(dev);
    struct wst *t;

    /* Check that the device number and that the ATAPI driver is loaded. */
    if (lun >= wstnlun || !atapi_request_immediate) {
        printf("ENXIO lun=%d, wstnlun=%d, im=%p\n",
               lun, wstnlun, (void *)atapi_request_immediate);
        return(ENXIO);
    }
    t = wsttab[lun];
    if (t->flags == WST_OPEN)
        return EBUSY;
    if (wst_sense(t))
        printf("wst%d: Sense media type failed\n", t->lun);
    t->flags &= ~WST_MEDIA_CHANGED;
    t->flags &= ~(WST_DATA_WRITTEN | WST_FM_WRITTEN);
    t->flags |= WST_OPEN;
    return(0);
}

int 
wstclose(dev_t dev, int flags, int fmt, struct proc *p)
{
    int lun = UNIT(dev);
    struct wst *t = wsttab[lun];

    /* Flush buffers, some drives fail here, but they should report ctl = 0 */
    if (t->cap.ctl && (t->flags & WST_DATA_WRITTEN))
        wst_write_filemark(t, 0);

    /* Write filemark if data written to tape */
    if ((t->flags & (WST_DATA_WRITTEN | WST_FM_WRITTEN)) == WST_DATA_WRITTEN)
        wst_write_filemark(t, WEOF_WRITE_MASK);

    /* If minor is even rewind on close */
    if (!(minor(dev) & 0x01))	
	wst_rewind(t);

    t->flags &= ~WST_OPEN;
    if (t->flags & WST_DEBUG)
	printf("wst%d: %ud total bytes transferred\n", t->lun, wst_total);
    t->flags &= ~WST_CTL_WARN;
    return(0);
}

void 
wststrategy(struct bio *bp)
{
    int lun = UNIT(bp->bio_dev);
    struct wst *t = wsttab[lun];
    int x;

    /* If it's a null transfer, return immediatly. */
    if (bp->bio_bcount == 0) {
        bp->bio_resid = 0;
        biodone(bp);
        return;
    }

    /* Check for != blocksize requests */
    if (bp->bio_bcount % t->blksize) {
        printf("wst%d: bad request, must be multiple of %d\n", lun, t->blksize);
        bp->bio_error = EIO;
	bp->bio_flags |= BIO_ERROR;
        biodone(bp);
        return;
    }

    if (bp->bio_bcount > t->blksize*t->cap.ctl) {  
	if ((t->flags & WST_CTL_WARN) == 0) {
            printf("wst%d: WARNING: CTL exceeded %ld>%d\n", 
		    lun, bp->bio_bcount, t->blksize*t->cap.ctl);
	    t->flags |= WST_CTL_WARN;
	}
    }

    x = splbio();
    wst_total += bp->bio_bcount;
    bioq_insert_tail(&t->buf_queue, bp);
    wst_start(t);
    splx(x);
}

static void     
wst_poll_dsc(struct wst *t)
{ 
    /* We should use a final timeout here SOS XXX */
    if (!(inb(t->ata->port + AR_STATUS) & ARS_DSC)) {
        timeout((timeout_t*)wst_poll_dsc, t, DSC_POLL_INTERVAL);
        return;
    }       
    t->ata->wait_for_dsc = 0;
    wakeup((caddr_t)t);
}

static void 
wst_start(struct wst *t)
{
    struct bio *bp = bioq_first(&t->buf_queue);
    u_long blk_count;
    u_char op_code;
    long byte_count;
    
    if (!bp)
        return;

    if (t->ata->wait_for_dsc)
	printf("wst%d: ERROR! allready waiting for DSC\n", t->lun);

    /* Sleep waiting for a ready drive (DSC) */
    if (t->ata->use_dsc && !(inb(t->ata->port + AR_STATUS) & ARS_DSC)) {
        t->ata->wait_for_dsc = 1;
        timeout((timeout_t*)wst_poll_dsc, t, DSC_POLL_INTERVAL);
        tsleep((caddr_t) t, 0, "wstdsc", 0);
    }

    bioq_remove(&t->buf_queue, bp);
    blk_count = bp->bio_bcount / t->blksize;

    if (bp->bio_cmd & BIO_READ) {
        op_code = ATAPI_TAPE_READ_CMD;
        byte_count = bp->bio_bcount;
    } else {
        op_code = ATAPI_TAPE_WRITE_CMD;
	t->flags |= WST_DATA_WRITTEN;
        byte_count = -bp->bio_bcount;
    }

    atapi_request_callback(t->ata, t->unit, op_code, 1,
                    	   blk_count>>16, blk_count>>8, blk_count,
                    	   0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                    	   (u_char*) bp->bio_data, byte_count, 
                    	   (void*)wst_done, t, bp);
}

static void 
wst_done(struct wst *t, struct bio *bp, int resid,
    struct atapires result)
{
    if (result.code) {
	printf("wst_done: ");
        wst_error(t, result);
        bp->bio_error = EIO;
        bp->bio_flags |= BIO_ERROR;
    }
    else
	bp->bio_resid = resid;

    biodone(bp);
    /*wst_start(t);*/
}

static int 
wst_error(struct wst *t, struct atapires result)
{
    struct wst_reqsense sense;

    if (result.code != RES_ERR) {
    	printf("wst%d: ERROR code=%d, status=%b, error=%b\n", t->lun,
               result.code, result.status, ARS_BITS, result.error, AER_BITS);
        return 1;
    }

    if ((result.error & AER_SKEY) && (result.status & ARS_CHECK)) {
        atapi_request_immediate(t->ata, t->unit,
            ATAPI_TAPE_REQUEST_SENSE,
            0, 0, 0, sizeof(sense),
            0, 0, 0, 0, 0, 0, 0,
            0, 0, 0, 0, (char*) &sense, sizeof(struct wst_reqsense));
        /*wst_dump(t->lun, "req_sense", &sense, sizeof(struct wst_reqsense));*/
    }
    switch (result.error & AER_SKEY) {
    case AER_SK_NOT_READY:
        if (result.error & ~AER_SKEY) {
            if (t->flags & WST_DEBUG)
                printf("wst%d: not ready\n", t->lun);
            break;
        }
        if (!(t->flags & WST_MEDIA_CHANGED))
            if (t->flags & WST_DEBUG)
                printf("wst%d: no media\n", t->lun);
        t->flags |= WST_MEDIA_CHANGED;
        break;

    case AER_SK_BLANK_CHECK:
        if (t->flags & WST_DEBUG)
            printf("wst%d: EOD encountered\n", t->lun);
        break;

    case AER_SK_MEDIUM_ERROR:
        if (t->flags & WST_DEBUG)
            printf("wst%d: nonrecovered data error\n", t->lun);
        break;

    case AER_SK_HARDWARE_ERROR:
        if (t->flags & WST_DEBUG)
            printf("wst%d: nonrecovered hardware error\n", t->lun);
        break;

    case AER_SK_ILLEGAL_REQUEST:
        if (t->flags & WST_DEBUG)
            printf("wst%d: invalid command\n", t->lun);
        break;

    case AER_SK_UNIT_ATTENTION:
        if (!(t->flags & WST_MEDIA_CHANGED))
            printf("wst%d: media changed\n", t->lun);
        t->flags |= WST_MEDIA_CHANGED;
        break;

    case AER_SK_DATA_PROTECT:
        if (t->flags & WST_DEBUG)
            printf("wst%d: reading read protected data\n", t->lun);
        break;

    case AER_SK_ABORTED_COMMAND:
        if (t->flags & WST_DEBUG)
            printf("wst%d: command aborted\n", t->lun);
        break;

    case AER_SK_MISCOMPARE:
        if (t->flags & WST_DEBUG)
            printf("wst%d: data don't match medium\n", t->lun);
        break;

    default:
        printf("wst%d: i/o error, status=%b, error=%b\n", t->lun,
        	result.status, ARS_BITS, result.error, AER_BITS);
    }
    printf("total=%u ERR=%x len=%ld ASC=%x ASCQ=%x\n", 
	   wst_total, sense.error_code, (long)ntohl(sense.info), 
	   sense.asc, sense.ascq);
    return 1;
}

int 
wstioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
    int lun = UNIT(dev);
    int error = 0;
    struct wst *t = wsttab[lun];

    switch (cmd) {
    case MTIOCGET:
	{
            struct mtget *g = (struct mtget *) addr;

            bzero(g, sizeof(struct mtget));
            g->mt_type = 7;
            g->mt_density = 1;
            g->mt_blksiz = t->blksize;
            g->mt_comp = t->cap.compress;
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
	    int i;
    	    struct mtop *mt = (struct mtop *)addr;

            switch ((short) (mt->mt_op)) {
            case MTWEOF:
		for (i=0; i < mt->mt_count && !error; i++)
			error = wst_write_filemark(t, WEOF_WRITE_MASK);
                break;
            case MTFSF:
		if (mt->mt_count)
			error = wst_space_cmd(t, SP_FM, mt->mt_count);
                break;
            case MTBSF:
		if (mt->mt_count)
			error = wst_space_cmd(t, SP_FM, -(mt->mt_count));
                break;
            case MTFSR:
                error = EINVAL; break;
            case MTBSR:
                error = EINVAL; break;
            case MTREW:
                error = wst_rewind(t);
		break;
            case MTOFFL:
#if 1				/* Misuse as a reset func for now */
		wst_reset(t);
		wst_sense(t);
		wst_describe(t);
#else
                if (error = wst_rewind(t))
		    break;
                error = wst_load_unload(t, !LU_LOAD_MASK);
#endif
		    break;
            case MTNOP:
		error = wst_write_filemark(t, 0);
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
                error = wst_erase(t);
		break;
            case MTEOD:
		error = wst_space_cmd(t, SP_EOD, 0);
                break;
            case MTCOMP:
                error = EINVAL; break;
            case MTRETENS:
                error = wst_load_unload(t, LU_RETENSION_MASK|LU_LOAD_MASK);
		break;
            default:
                error = EINVAL;
            }
            return error;
        }
    default:
        return(ENOTTY);
    }
    return(error);
}

static int
wst_space_cmd(struct wst *t, u_char function, u_int count)
{
    struct atapires result;

    result = atapi_request_wait(t->ata, t->unit, 
        			ATAPI_TAPE_SPACE_CMD, function,
                    	        count>>16, count>>8, count,
        			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0);
    if (result.code) {
	printf("wst_space_cmd: ");
        wst_error(t, result);
        return EIO;
    }
    return 0;
}

static int
wst_write_filemark(struct wst *t, u_char function)
{
    struct atapires result;

    if (function) {
	if (t->flags & WST_FM_WRITTEN)
	    t->flags &= ~WST_DATA_WRITTEN;
        else
	    t->flags |= WST_FM_WRITTEN;
    }
    result = atapi_request_wait(t->ata, t->unit, 
        			ATAPI_TAPE_WEOF, 0, 0, 0, function,
        			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0);
    if (result.code) {
	printf("wst_write_filemark: ");
        wst_error(t, result);
        return EIO;
    }
    return 0;
}

static int
wst_load_unload(struct wst *t, u_char function)
{
    struct atapires result;

    result = atapi_request_wait(t->ata, t->unit, 
        			ATAPI_TAPE_LOAD_UNLOAD, 0, 0, 0, function,
        			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, NULL, 0);
    if (result.code) {
	printf("wst_load_unload: ");
        wst_error(t, result);
        return EIO;
    }
    return 0;
}

static int
wst_erase(struct wst *t)
{
    int error;
    struct atapires result;

    error = wst_rewind(t);
    if (error)
        return error;
    result = atapi_request_wait(t->ata, t->unit, 
			        ATAPI_TAPE_ERASE, 3, 0, 0, 0,
        			0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				NULL, 0);
    if (result.code) {
	printf("wst_erase: ");
        wst_error(t, result);
        return EIO;
    }
    return 0;
}

static int
wst_rewind(struct wst *t)
{
    struct atapires result;       

    result = atapi_request_wait(t->ata, t->unit,
                		    ATAPI_TAPE_REWIND, 0, 0, 0, 0,
                		    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
				    NULL, 0);
    if (result.code) {
	printf("wst_rewind: ");
        wst_error(t, result);
        return EIO;
    }
    return 0;
}

static void
wst_reset(struct wst *t)
{
    outb(t->ata->port + AR_DRIVE, ARD_DRIVE1);
    DELAY(30);
    outb(t->ata->port + AR_COMMAND, 0x08);
    DELAY(30);
}

static void 
wst_drvinit(void *unused)
{
    cdevsw_add(&wst_cdevsw);
}

SYSINIT(wstdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,wst_drvinit,NULL)
