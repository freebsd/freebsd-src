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
 *	$Id: atapi-cd.c,v 1.10 1999/05/31 11:24:27 phk Exp $
 */

#include "ata.h"
#include "atapicd.h"
#include "opt_devfs.h"

#if NATA > 0 && NATAPICD > 0

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/disklabel.h>
#include <sys/devicestat.h>
#include <sys/cdio.h>
#include <sys/wormio.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/stat.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif
#include <pci/pcivar.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>
#include <dev/ata/atapi-cd.h>

static d_open_t		acdopen;
static d_close_t	acdclose;
static d_ioctl_t	acdioctl;
static d_strategy_t	acdstrategy;

static struct cdevsw acd_cdevsw = {
	/* open */	acdopen,
	/* close */	acdclose,
	/* read */	physread,
	/* write */	physwrite,
	/* ioctl */	acdioctl,
	/* stop */	nostop,
	/* reset */	noreset,
	/* devtotty */	nodevtotty,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	acdstrategy,
	/* name */	"acd",
	/* parms */	noparms,
	/* maj */	117,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_DISK,
	/* maxio */	0,
	/* bmaj */	31
};

#define NUNIT	16			/* max # of devices */

#define F_BOPEN         0x0001  	/* the block device is opened */
#define F_MEDIA_CHANGED 0x0002  	/* The media have changed since open */
#define F_LOCKED        0x0004 		/* this unit is locked (or should be) */
#define F_TRACK_PREP    0x0008  	/* track should be prep'ed */
#define F_TRACK_PREPED  0x0010  	/* track has been prep'ed */
#define F_DISK_PREPED   0x0020  	/* disk has been prep'ed */
#define F_WRITTEN   	0x0040  	/* the medium has been written to */

static struct acd_softc *acdtab[NUNIT];
static int32_t acdnlun = 0;     	/* number of configured drives */

int32_t acdattach(struct atapi_softc *);
static struct acd_softc *acd_init_lun(struct atapi_softc *, int, struct devstat *);
static void acd_start(struct acd_softc *);
static void acd_done(struct atapi_request *);
static int32_t acd_test_unit_ready (struct acd_softc *);
static int32_t acd_lock_device (struct acd_softc *, int32_t);
static int32_t acd_start_device (struct acd_softc *, int32_t);
static int32_t acd_pause_device (struct acd_softc *, int32_t);
static int32_t acd_mode_sense (struct acd_softc *, u_int8_t, void *, int32_t);
static int32_t acd_mode_select (struct acd_softc *, void *, int32_t);
static int32_t acd_read_toc(struct acd_softc *);
static void acd_describe(struct acd_softc *);
static int32_t acd_setchan(struct acd_softc *, u_int8_t, u_int8_t, u_int8_t, u_int8_t);
static int32_t acd_eject(struct acd_softc *, int);
static void acd_select_slot(struct acd_softc *);
static int32_t acd_open_disk(struct acd_softc *, int);
static int32_t acd_open_track(struct acd_softc *, struct wormio_prepare_track *);
static int32_t acd_close_track(struct acd_softc *);
static int32_t acd_close_disk(struct acd_softc *);
static int32_t acd_read_track_info(struct acd_softc *, int, struct acd_track_info*);
static int32_t acd_blank_disk(struct acd_softc *);
static void lba2msf(int32_t, u_int8_t *, u_int8_t *, u_int8_t *);
static int32_t msf2lba(u_int8_t, u_int8_t, u_int8_t);
static void acd_drvinit(void *);

int
acdattach(struct atapi_softc *atp)
{
    struct acd_softc *cdp;
    struct changer *chp;
    int32_t error, count;

    if (acdnlun >= NUNIT) {
        printf("acd: too many units\n");
        return -1;
    }
    if ((cdp = acd_init_lun(atp, acdnlun, NULL)) == NULL) {
        printf("acd: out of memory\n");
        return -1;
    }

    /* get drive capabilities, some drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
	if (!(error = acd_mode_sense(cdp, ATAPI_CDROM_CAP_PAGE,
				     &cdp->cap, sizeof(cdp->cap))))
            break;
    }
    if (error) {
	free(cdp, M_TEMP);
	return -1;
    }
    cdp->cap.max_speed = ntohs(cdp->cap.max_speed);
    cdp->cap.max_vol_levels = ntohs(cdp->cap.max_vol_levels);
    cdp->cap.buf_size = ntohs(cdp->cap.buf_size);
    cdp->cap.cur_speed = ntohs(cdp->cap.cur_speed);
    acd_describe(cdp);

    /* if this is a changer device, allocate the neeeded lun's */
    if (cdp->cap.mech == MST_MECH_CHANGER) {
	int8_t ccb[16] = { ATAPI_MECH_STATUS,
                           0, 0, 0, 0, 0, 0, 0, 
			   sizeof(struct changer)>>8, sizeof(struct changer),
                           0, 0, 0, 0, 0, 0 };

        chp = malloc(sizeof(struct changer), M_TEMP, M_NOWAIT);
        if (chp == NULL) {
            printf("acd: out of memory\n");
            return 0;
        }
        bzero(chp, sizeof(struct changer));
        error = atapi_queue_cmd(cdp->atp, ccb, chp, sizeof(struct changer), 
				A_READ, 60, NULL, NULL, NULL);

#ifdef ACD_DEBUG
        printf("error=%02x curr=%02x slots=%d len=%d\n",
               error, chp->current_slot, chp->slots, htons(chp->table_length));
#endif

        if (!error) {
    	    struct acd_softc *tmpcdp = cdp;
	    int32_t count;
	    int8_t string[16];

            chp->table_length = htons(chp->table_length);
            for (count = 0; count < chp->slots && acdnlun < NUNIT; count++) {
                if (count > 0) {
                    tmpcdp = acd_init_lun(atp, acdnlun, cdp->stats);
		    if (!tmpcdp) {
                        printf("acd: out of memory\n");
                        return -1;
                    }
                }
                tmpcdp->slot = count;
                tmpcdp->changer_info = chp;
                printf("acd%d: changer slot %d %s\n", acdnlun, count,
		       (chp->slot[count].present ? "CD present" : "empty"));
                acdtab[acdnlun++] = tmpcdp;
            }
            if (acdnlun >= NUNIT) {
                printf("acd: too many units\n");
                return 0;
            }
	    sprintf(string, "acd%d-", cdp->lun);
            devstat_add_entry(cdp->stats, string, tmpcdp->lun, DEV_BSIZE,
                              DEVSTAT_NO_ORDERED_TAGS,
                              DEVSTAT_TYPE_CDROM | DEVSTAT_TYPE_IF_IDE,
			      0x178);
        }
    }
    else {
        acdtab[acdnlun++] = cdp;
        devstat_add_entry(cdp->stats, "acd", cdp->lun, DEV_BSIZE,
                          DEVSTAT_NO_ORDERED_TAGS,
                          DEVSTAT_TYPE_CDROM | DEVSTAT_TYPE_IF_IDE,
			  0x178);
    }
    return 0;
}

static struct acd_softc *
acd_init_lun(struct atapi_softc *atp, int32_t lun, struct devstat *stats)
{
    struct acd_softc *acd;

    if (!(acd = malloc(sizeof(struct acd_softc), M_TEMP, M_NOWAIT)))
        return NULL;
    bzero(acd, sizeof(struct acd_softc));
    bufq_init(&acd->buf_queue);
    acd->atp = atp;
    acd->lun = lun;
    acd->flags = F_MEDIA_CHANGED;
    acd->flags &= ~(F_WRITTEN|F_TRACK_PREP|F_TRACK_PREPED);
    acd->block_size = 2048;
    acd->refcnt = 0;
    acd->slot = -1;
    acd->changer_info = NULL;
    if (stats == NULL) {
        if (!(acd->stats = malloc(sizeof(struct devstat), 
					 M_TEMP, M_NOWAIT)))
            return NULL;
	bzero(acd->stats, sizeof(struct devstat));
    }
    else
	acd->stats = stats;
#ifdef DEVFS
    acd->a_cdevfs_token = devfs_add_devswf(&acd_cdevsw, dkmakeminor(lun, 0, 0),
        				   DV_CHR, UID_ROOT, GID_OPERATOR, 0644,
        				   "racd%da", lun);
    acd->c_cdevfs_token = devfs_add_devswf(&acd_cdevsw, 
					   dkmakeminor(lun, 0, RAW_PART),
        				   DV_CHR, UID_ROOT, GID_OPERATOR, 0644,
        				   "racd%dc", lun);
    acd->a_bdevfs_token = devfs_add_devswf(&acd_cdevsw, dkmakeminor(lun, 0, 0),
        				   DV_BLK, UID_ROOT, GID_OPERATOR, 0644,
        				   "acd%da", lun);
    acd->c_bdevfs_token = devfs_add_devswf(&acd_cdevsw, 
					   dkmakeminor(lun, 0, RAW_PART),
        				   DV_BLK, UID_ROOT, GID_OPERATOR, 0644,
        				   "acd%dc", lun);
#endif
    return acd;
}

static void 
acd_describe(struct acd_softc *cdp)
{
    int32_t comma;
    int8_t *mechanism;
    int8_t model_buf[40+1];
    int8_t revision_buf[8+1];

    bpack(cdp->atp->atapi_parm->model, model_buf, sizeof(model_buf));
    bpack(cdp->atp->atapi_parm->revision, revision_buf, sizeof(revision_buf));
    printf("acd%d: <%s/%s> CDROM drive at ata%d as %s\n",
           cdp->lun, model_buf, revision_buf,
           cdp->atp->controller->lun,
           (cdp->atp->unit == ATA_MASTER) ? "master" : "slave ");

    printf("acd%d: drive speed ", cdp->lun);
    if (cdp->cap.cur_speed != cdp->cap.max_speed)
        printf("%d - ", cdp->cap.cur_speed * 1000 / 1024);
    printf("%dKB/sec", cdp->cap.max_speed * 1000 / 1024);
    if (cdp->cap.buf_size)
        printf(", %dKB cache", cdp->cap.buf_size);
    if (cdp->atp->flags & ATAPI_F_DMA_ENABLED)
	printf(", DMA");
    printf("\n");

    printf("acd%d: supported read types:", cdp->lun);
    comma = 0;
    if (cdp->cap.read_cdr) {
        printf(" CD-R"); comma = 1;
    }
    if (cdp->cap.read_cdrw) {
        printf("%s CD-RW", comma ? "," : ""); comma = 1;
    }
    if (cdp->cap.cd_da) {
        printf("%s CD-DA", comma ? "," : ""); comma = 1;
    }
    if (cdp->cap.method2)
        printf("%s packet track", comma ? "," : "");
    if (cdp->cap.write_cdr || cdp->cap.write_cdrw) {
    	printf("\nacd%d: supported write types:", cdp->lun);
        comma = 0;
    	if (cdp->cap.write_cdr) {
            printf(" CD-R" ); comma = 1;
	}
    	if (cdp->cap.write_cdrw) {
            printf("%s CD-RW", comma ? "," : ""); comma = 1;
	}
    	if (cdp->cap.test_write) {
            printf("%s test write", comma ? "," : ""); comma = 1;
	}
    }
    if (cdp->cap.audio_play) {
    	printf("\nacd%d: Audio: ", cdp->lun);
    	if (cdp->cap.audio_play)
            printf("play");
    	if (cdp->cap.max_vol_levels)
            printf(", %d volume levels", cdp->cap.max_vol_levels);
    }
    printf("\nacd%d: Mechanism: ", cdp->lun);
    switch (cdp->cap.mech) {
    case MST_MECH_CADDY:
        mechanism = "caddy"; break;
    case MST_MECH_TRAY:
        mechanism = "tray"; break;
    case MST_MECH_POPUP:
        mechanism = "popup"; break;
    case MST_MECH_CHANGER:
        mechanism = "changer"; break;
    case MST_MECH_CARTRIDGE:
        mechanism = "cartridge"; break;
    default:
        mechanism = 0; break;
    }
    if (mechanism)
        printf("%s%s", cdp->cap.eject ? "ejectable " : "", mechanism);
    else if (cdp->cap.eject)
        printf("ejectable");

    if (cdp->cap.mech != MST_MECH_CHANGER) {
        printf("\nacd%d: Medium: ", cdp->lun);
        switch (cdp->cap.medium_type & MST_TYPE_MASK_HIGH) {
        case MST_CDROM:
            printf("CD-ROM "); break;
        case MST_CDR:
            printf("CD-R "); break;
        case MST_CDRW:
            printf("CD-RW "); break;
        case MST_DOOR_OPEN:
            printf("door open"); break;
        case MST_NO_DISC:
            printf("no/blank disc inside"); break;
        case MST_FMT_ERROR:
            printf("medium format error"); break;
	}
        if ((cdp->cap.medium_type & MST_TYPE_MASK_HIGH) < MST_TYPE_MASK_HIGH) {
            switch (cdp->cap.medium_type & MST_TYPE_MASK_LOW) {
            case MST_DATA_120:
                printf("120mm data disc loaded"); break;
            case MST_AUDIO_120:
                printf("120mm audio disc loaded"); break;
            case MST_COMB_120:
                printf("120mm data/audio disc loaded"); break;
            case MST_PHOTO_120:
                printf("120mm photo disc loaded"); break;
            case MST_DATA_80:
                printf("80mm data disc loaded"); break;
            case MST_AUDIO_80:
                printf("80mm audio disc loaded"); break;
            case MST_COMB_80:
                printf("80mm data/audio disc loaded"); break;
            case MST_PHOTO_80:
                printf("80mm photo disc loaded"); break;
            case MST_FMT_NONE:
                switch (cdp->cap.medium_type & MST_TYPE_MASK_HIGH) {
	        case MST_CDROM:
		    printf("unknown medium"); break;
                case MST_CDR:
                case MST_CDRW:
                    printf("blank medium"); break;
	        }
                break;
            default:
                printf("unknown type=0x%x", cdp->cap.medium_type); break;
            }
	}
    }
    if (cdp->cap.lock)
        printf(cdp->cap.locked ? ", locked" : ", unlocked");
    if (cdp->cap.prevent)
        printf(", lock protected");
    printf("\n");
}

static __inline void 
lba2msf(int32_t lba, u_int8_t *m, u_int8_t *s, u_int8_t *f)
{
    lba += 150;
    lba &= 0xffffff;
    *m = lba / (60 * 75);
    lba %= (60 * 75);
    *s = lba / 75;
    *f = lba % 75;
}

static __inline int32_t 
msf2lba(u_int8_t m, u_int8_t s, u_int8_t f)
{
    return (m * 60 + s) * 75 + f - 150;
}

static int
acdopen(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    int32_t lun = dkunit(dev);
    struct acd_softc *cdp;

    if (lun >= acdnlun || !(cdp = acdtab[lun]))
        return ENXIO;

    if (!(cdp->flags & F_BOPEN) && !cdp->refcnt) {
	acd_lock_device(cdp, 1); 	/* prevent user eject */
        cdp->flags |= F_LOCKED;
    }
    if (fmt == S_IFBLK)
        cdp->flags |= F_BOPEN;
    else
        cdp->refcnt++;

    if (!(flags & O_NONBLOCK) && acd_read_toc(cdp) && !(flags & FWRITE))
        printf("acd%d: read_toc failed\n", lun);
    return 0;
}

static int 
acdclose(dev_t dev, int32_t flags, int32_t fmt, struct proc *p)
{
    int32_t lun = dkunit(dev);
    struct acd_softc *cdp;
    
    if (lun >= acdnlun || !(cdp = acdtab[lun]))
        return ENXIO;

    if (fmt == S_IFBLK)
    	cdp->flags &= ~F_BOPEN;
    else
    	cdp->refcnt--;

    /* are we the last open ?? */
    if (!(cdp->flags & F_BOPEN) && !cdp->refcnt) {
	/* yup, do we need to close any written tracks */
        if ((flags & FWRITE) != 0) {
            if ((cdp->flags & F_TRACK_PREPED) != 0) {
                acd_close_track(cdp);
                cdp->flags &= ~(F_TRACK_PREPED | F_TRACK_PREP);
            }
        }
	acd_lock_device(cdp, 0);	/* allow the user eject */
    }
    cdp->flags &= ~F_LOCKED;
    return 0;
}

static int 
acdioctl(dev_t dev, u_long cmd, caddr_t addr, int32_t flag, struct proc *p)
{
    int32_t lun = dkunit(dev);
    struct acd_softc *cdp = acdtab[lun];
    int32_t error = 0;

    if (cdp->flags & F_MEDIA_CHANGED)
        switch (cmd) {
        case CDIOCRESET:
            acd_test_unit_ready(cdp);
	    break;
           
        default:
            acd_read_toc(cdp);
	    acd_lock_device(cdp, 1);
            cdp->flags |= F_LOCKED;
            break;
        }
    switch (cmd) {

    case CDIOCRESUME:
        error = acd_pause_device(cdp, 1);
	break;

    case CDIOCPAUSE:
        error = acd_pause_device(cdp, 0);
	break;

    case CDIOCSTART:
        error = acd_start_device(cdp, 1);
	break;

    case CDIOCSTOP:
        error = acd_start_device(cdp, 0);
	break;

    case CDIOCALLOW:
        acd_select_slot(cdp);
        cdp->flags &= ~F_LOCKED;
	error = acd_lock_device(cdp, 0);
	break;

    case CDIOCPREVENT:
        acd_select_slot(cdp);
        cdp->flags |= F_LOCKED;
	error = acd_lock_device(cdp, 1);
	break;

    case CDIOCRESET:
        error = suser(p);
        if (error)
            break;
        error = acd_test_unit_ready(cdp);
	break;

    case CDIOCEJECT:
        if ((cdp->flags & F_BOPEN) && cdp->refcnt) {
            error = EBUSY;
	    break;
	}
        error = acd_eject(cdp, 0);
	break;

    case CDIOCCLOSE:
        if ((cdp->flags & F_BOPEN) && cdp->refcnt)
            break;
        error = acd_eject(cdp, 1);
	break;

    case CDIOREADTOCHEADER:
        if (!cdp->toc.hdr.ending_track) {
            error = EIO;
	    break;
	}
        bcopy(&cdp->toc.hdr, addr, sizeof(cdp->toc.hdr));
        break;

    case CDIOREADTOCENTRYS:
	{
            struct ioc_read_toc_entry *te = (struct ioc_read_toc_entry *)addr;
            struct toc *toc = &cdp->toc;
            struct toc buf;
            u_int32_t len;
            u_int8_t starting_track = te->starting_track;

            if (!cdp->toc.hdr.ending_track) {
                error = EIO;
		break;
	    }

            if (te->data_len < sizeof(toc->tab[0]) || 
		(te->data_len % sizeof(toc->tab[0])) != 0 || 
		(te->address_format != CD_MSF_FORMAT &&
		te->address_format != CD_LBA_FORMAT)) {
                error = EINVAL;
		break;
	    }

            if (!starting_track)
                starting_track = toc->hdr.starting_track;
            else if (starting_track == 170) 
                starting_track = toc->hdr.ending_track + 1;
            else if (starting_track < toc->hdr.starting_track ||
                     starting_track > toc->hdr.ending_track + 1) {
                error = EINVAL;
		break;
	    }

            len = ((toc->hdr.ending_track + 1 - starting_track) + 1) *
		  sizeof(toc->tab[0]);
            if (te->data_len < len)
                len = te->data_len;
            if (len > sizeof(toc->tab)) {
                error = EINVAL;
		break;
	    }

            if (te->address_format == CD_MSF_FORMAT) {
                struct cd_toc_entry *entry;

                buf = cdp->toc;
                toc = &buf;
                entry = toc->tab + (toc->hdr.ending_track + 1 -
                    	toc->hdr.starting_track) + 1;
                while (--entry >= toc->tab)
                    lba2msf(ntohl(entry->addr.lba), &entry->addr.msf.minute,
                            &entry->addr.msf.second, &entry->addr.msf.frame);
            }
            error = copyout(toc->tab + starting_track - toc->hdr.starting_track,
		 	    te->data, len);
	    break;
        }
    case CDIOREADTOCENTRY:
	{
            struct ioc_read_toc_single_entry *te =
            	(struct ioc_read_toc_single_entry *)addr;
            struct toc *toc = &cdp->toc;
            struct toc buf;
            u_int8_t track = te->track;

            if (!cdp->toc.hdr.ending_track) {
                error = EIO;
		break;
	    }

            if (te->address_format != CD_MSF_FORMAT && 
		te->address_format != CD_LBA_FORMAT) {
                error = EINVAL;
		break;
	    }

            if (!track)
                track = toc->hdr.starting_track;
            else if (track == 170)
                track = toc->hdr.ending_track + 1;
            else if (track < toc->hdr.starting_track ||
                     track > toc->hdr.ending_track + 1) {
                error = EINVAL;
		break;
	    }

            if (te->address_format == CD_MSF_FORMAT) {
                struct cd_toc_entry *entry;

                buf = cdp->toc;
                toc = &buf;
                entry = toc->tab + (track - toc->hdr.starting_track);
                lba2msf(ntohl(entry->addr.lba), &entry->addr.msf.minute,
                        &entry->addr.msf.second, &entry->addr.msf.frame);
            }
            bcopy(toc->tab + track - toc->hdr.starting_track,
                  &te->entry, sizeof(struct cd_toc_entry));
        }
	break;

    case CDIOCREADSUBCHANNEL:
	{
            struct ioc_read_subchannel *args =
            	(struct ioc_read_subchannel *)addr;
            struct cd_sub_channel_info data;
            u_int32_t len = args->data_len;
            int32_t abslba, rellba;
	    int8_t ccb[16] = { ATAPI_READ_SUBCHANNEL, 0, 0x40, 1, 0, 0, 0,
			       sizeof(cdp->subchan)>>8, sizeof(cdp->subchan),
			       0, 0, 0, 0, 0, 0, 0 };

            if (len > sizeof(data) ||
                len < sizeof(struct cd_sub_channel_header)) {
                error = EINVAL;
		break;
	    }

	    if ((error = atapi_queue_cmd(cdp->atp, ccb, &cdp->subchan, 
				         sizeof(cdp->subchan), A_READ, 10,
					 NULL, NULL, NULL)))
                break;

#ifdef ACD_DEBUG
            atapi_dump("acd: subchan", &cdp->subchan, sizeof(cdp->subchan));
#endif

            abslba = cdp->subchan.abslba;
            rellba = cdp->subchan.rellba;
            if (args->address_format == CD_MSF_FORMAT) {
                lba2msf(ntohl(abslba),
                    &data.what.position.absaddr.msf.minute,
                    &data.what.position.absaddr.msf.second,
                    &data.what.position.absaddr.msf.frame);
                lba2msf(ntohl(rellba),
                    &data.what.position.reladdr.msf.minute,
                    &data.what.position.reladdr.msf.second,
                    &data.what.position.reladdr.msf.frame);
            } else {
                data.what.position.absaddr.lba = abslba;
                data.what.position.reladdr.lba = rellba;
            }
            data.header.audio_status = cdp->subchan.audio_status;
            data.what.position.control = cdp->subchan.control & 0xf;
            data.what.position.addr_type = cdp->subchan.control >> 4;
            data.what.position.track_number = cdp->subchan.track;
            data.what.position.index_number = cdp->subchan.indx;
            error = copyout(&data, args->data, len);
	    break;
        }

    case CDIOCPLAYMSF:
	{
            struct ioc_play_msf *args = (struct ioc_play_msf *)addr;
            int8_t ccb[16] = { ATAPI_PLAY_MSF, 0, 0,
			       args->start_m, args->start_s, args->start_f,
			       args->end_m, args->end_s, args->end_f,
                               0, 0, 0, 0, 0, 0, 0 };

            error = atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 10, 
				    NULL, NULL,NULL);
	    break;
        }

    case CDIOCPLAYBLOCKS:
	{
            struct ioc_play_blocks *args = (struct ioc_play_blocks *)addr;
	    int8_t ccb[16]  = { ATAPI_PLAY_BIG, 0,
			        args->blk>>24, args->blk>>16, args->blk>>8,
				args->blk, args->len>>24, args->len>>16,
				args->len>>8, args->len,
			   	0, 0, 0, 0, 0, 0 };

            error = atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 10,
				    NULL, NULL, NULL);
	    break;
        }

    case CDIOCPLAYTRACKS:
	{
            struct ioc_play_track *args = (struct ioc_play_track *)addr;
            u_int32_t start, len;
            int32_t t1, t2;
	    int8_t ccb[16];

            if (!cdp->toc.hdr.ending_track) {
                error = EIO;
		break;
	    }

            if (args->end_track < cdp->toc.hdr.ending_track + 1)
                ++args->end_track;
            if (args->end_track > cdp->toc.hdr.ending_track + 1)
                args->end_track = cdp->toc.hdr.ending_track + 1;
            t1 = args->start_track - cdp->toc.hdr.starting_track;
            t2 = args->end_track - cdp->toc.hdr.starting_track;
            if (t1 < 0 || t2 < 0) {
                error = EINVAL;
		break;
	    }
            start = ntohl(cdp->toc.tab[t1].addr.lba);
            len = ntohl(cdp->toc.tab[t2].addr.lba) - start;

	    bzero(ccb, sizeof(ccb));
	    ccb[0] = ATAPI_PLAY_BIG;
	    ccb[2] = start>>24;
	    ccb[3] = start>>16;
	    ccb[4] = start>>8;
	    ccb[5] = start;
	    ccb[6] = len>>24;
	    ccb[7] = len>>16;
	    ccb[8] = len>>8;
	    ccb[9] = len;

            error = atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 10,
				    NULL, NULL, NULL);
	    break;
        }

    case CDIOCREADAUDIO:
	{
	    struct ioc_read_audio *args = (struct ioc_read_audio *)addr;
	    int32_t lba, frames, error = 0;
	    u_int8_t *buffer, *ubuf = args->buffer;
	    int8_t ccb[16];

	    if (!cdp->toc.hdr.ending_track) {
		error = EIO;
		break;
	    }
		
	    if ((frames = args->nframes) < 0) {
		error = EINVAL;
		break;
	    }

	    if (args->address_format == CD_LBA_FORMAT)
		lba = args->address.lba;
	    else if (args->address_format == CD_MSF_FORMAT)
	        lba = msf2lba(args->address.msf.minute,
			     args->address.msf.second,
			     args->address.msf.frame);
	    else {
		error = EINVAL;
		break;
	    }

#ifndef CD_BUFFER_BLOCKS
#define CD_BUFFER_BLOCKS 13
#endif
            if (!(buffer = malloc(CD_BUFFER_BLOCKS * 2352,
				  M_TEMP,M_NOWAIT))) {
                error = ENOMEM;
		break;
	    }
	    bzero(ccb, sizeof(ccb));
            while (frames > 0) {
                int32_t size;
                u_int8_t blocks;

                blocks = (frames>CD_BUFFER_BLOCKS) ? CD_BUFFER_BLOCKS : frames;
                size = blocks * 2352;

		ccb[0] = ATAPI_READ_CD;
		ccb[1] = 4;
		ccb[2] = lba>>24;
		ccb[3] = lba>>16;
		ccb[4] = lba>>8;
		ccb[5] = lba;
		ccb[8] = blocks;
		ccb[9] = 0xf0;
		if ((error = atapi_queue_cmd(cdp->atp, ccb,  buffer, size,
					     A_READ, 30, NULL, NULL, NULL)))
                    break;

                if ((error = copyout(buffer, ubuf, size)))
                    break;
                    
                ubuf += size;
                frames -= blocks;
                lba += blocks;
            }
            free(buffer, M_TEMP);
	    if (args->address_format == CD_LBA_FORMAT)
		args->address.lba = lba;
	    else if (args->address_format == CD_MSF_FORMAT)
	        lba2msf(lba, &args->address.msf.minute,
			     &args->address.msf.second,
			     &args->address.msf.frame);
            break;
        }

    case CDIOCGETVOL:
	{
            struct ioc_vol *arg = (struct ioc_vol *)addr;

	    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_AUDIO_PAGE,
				        &cdp->au, sizeof(cdp->au))))
                break;

            if (cdp->au.page_code != ATAPI_CDROM_AUDIO_PAGE) {
                error = EIO;
		break;
	    }
            arg->vol[0] = cdp->au.port[0].volume;
            arg->vol[1] = cdp->au.port[1].volume;
            arg->vol[2] = cdp->au.port[2].volume;
            arg->vol[3] = cdp->au.port[3].volume;
            break;
        }

    case CDIOCSETVOL:
	{
            struct ioc_vol *arg = (struct ioc_vol *)addr;

	    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_AUDIO_PAGE,
				        &cdp->au, sizeof(cdp->au))))
                break;
            if (cdp->au.page_code != ATAPI_CDROM_AUDIO_PAGE) {
                error = EIO;
		break;
	    }
	    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_AUDIO_PAGE_MASK,
				        &cdp->aumask, sizeof(cdp->aumask))))
                break;
            cdp->au.data_length = 0;
            cdp->au.port[0].channels = CHANNEL_0;
            cdp->au.port[1].channels = CHANNEL_1;
            cdp->au.port[0].volume = arg->vol[0] & cdp->aumask.port[0].volume;
            cdp->au.port[1].volume = arg->vol[1] & cdp->aumask.port[1].volume;
            cdp->au.port[2].volume = arg->vol[2] & cdp->aumask.port[2].volume;
            cdp->au.port[3].volume = arg->vol[3] & cdp->aumask.port[3].volume;
	    error =  acd_mode_select(cdp, &cdp->au, sizeof(cdp->au));
	    break;
        }
    case CDIOCSETPATCH:
	{
            struct ioc_patch *arg = (struct ioc_patch *)addr;

            error = acd_setchan(cdp, arg->patch[0], arg->patch[1],
                	        arg->patch[2], arg->patch[3]);
	    break;
        }

    case CDIOCSETMONO:
        error = acd_setchan(cdp, CHANNEL_0|CHANNEL_1, CHANNEL_0|CHANNEL_1, 0,0);
	break;

    case CDIOCSETSTEREO:
        error = acd_setchan(cdp, CHANNEL_0, CHANNEL_1, 0, 0);
	break;

    case CDIOCSETMUTE:
        error = acd_setchan(cdp, 0, 0, 0, 0);
	break;

    case CDIOCSETLEFT:
        error = acd_setchan(cdp, CHANNEL_0, CHANNEL_0, 0, 0);
	break;

    case CDIOCSETRIGHT:
        error = acd_setchan(cdp, CHANNEL_1, CHANNEL_1, 0, 0);
	break;

    case CDRIOCNEXTWRITEABLEADDR:
	{
	    struct acd_track_info track_info;

	    if ((error = acd_read_track_info(cdp, 0xff, &track_info)))
		break;

	    if (!track_info.nwa_valid) {
		error = EINVAL;
		break;
	    }
	    cdp->next_writeable_lba = track_info.next_writeable_addr;
	    *(int*)addr = track_info.next_writeable_addr;
	}
	break;
 
    case WORMIOCPREPDISK:
        {
            struct wormio_prepare_disk *w = (struct wormio_prepare_disk *)addr;

            if (w->dummy != 0 && w->dummy != 1)
                error = EINVAL;
            else {
                error = acd_open_disk(cdp, w->dummy);
                if (error == 0) {
                    cdp->flags |= F_DISK_PREPED;
                    cdp->dummy = w->dummy;
                    cdp->speed = w->speed;
                }
            }
            break;
        }

    case WORMIOCPREPTRACK:
        {
            struct wormio_prepare_track *w =(struct wormio_prepare_track *)addr;

            if (w->audio != 0 && w->audio != 1)
                error = EINVAL;
            else if (w->audio == 0 && w->preemp)
                error = EINVAL;
            else if ((cdp->flags & F_DISK_PREPED) == 0) {
                error = EINVAL;
                printf("acd%d: sequence error (PREP_TRACK)\n", cdp->lun);
            } else {
                cdp->flags |= F_TRACK_PREP;
                cdp->preptrack = *w;
            }
            break;
        }

    case WORMIOCFINISHTRACK:
        if ((cdp->flags & F_TRACK_PREPED) != 0)
            error = acd_close_track(cdp);
        cdp->flags &= ~(F_TRACK_PREPED | F_TRACK_PREP);
        break;

    case WORMIOCFIXATION:
        {
            struct wormio_fixation *w = (struct wormio_fixation *)addr;

            if ((cdp->flags & F_WRITTEN) == 0)
                error = EINVAL;
            else if (w->toc_type < 0 /* WORM_TOC_TYPE_AUDIO */ ||
                w->toc_type > 4 /* WORM_TOC_TYPE_CDI */ )
                error = EINVAL;
            else if (w->onp != 0 && w->onp != 1)
                error = EINVAL;
            else {
                /* no fixation needed if dummy write */
                if (cdp->dummy == 0)
                    error = acd_close_disk(cdp);
                cdp->flags &=
                    ~(F_WRITTEN|F_DISK_PREPED|F_TRACK_PREP|F_TRACK_PREPED);
            }
            break;
        }

    case CDRIOCBLANK:
        error = acd_blank_disk(cdp);
	break;

    default:
        error = ENOTTY;
    }
    return error;
}

static void 
acdstrategy(struct buf *bp)
{
    int32_t lun = dkunit(bp->b_dev);
    struct acd_softc *cdp = acdtab[lun];
    int32_t x;

#ifdef NOTYET
    /* allow write only on CD-R/RW media */   /* all for now SOS */
    if (!(bp->b_flags & B_READ) && !(writeable_media)) {
        bp->b_error = EROFS;
        bp->b_flags |= B_ERROR;
        biodone(bp);
        return;
    }
#endif

    if (bp->b_bcount == 0) {
        bp->b_resid = 0;
        biodone(bp);
        return;
    }
    
    /* check for valid blocksize SOS */

    bp->b_pblkno = bp->b_blkno;
    bp->b_resid = bp->b_bcount;

    x = splbio();
    bufqdisksort(&cdp->buf_queue, bp);
    acd_start(cdp);
    splx(x);
}

static void 
acd_start(struct acd_softc *cdp)
{
    struct buf *bp = bufq_first(&cdp->buf_queue);
    u_int32_t lba, count;
    int8_t ccb[16];

    if (!bp)
        return;

    bufq_remove(&cdp->buf_queue, bp);

    /* should reject all queued entries if media have changed. */
    if (cdp->flags & F_MEDIA_CHANGED) {
        bp->b_error = EIO;
        bp->b_flags |= B_ERROR;
        biodone(bp);
        return;
    }
    acd_select_slot(cdp);
    if ((bp->b_flags & B_READ) == B_WRITE) {
        if ((cdp->flags & F_TRACK_PREPED) == 0) {
            if ((cdp->flags & F_TRACK_PREP) == 0) {
                printf("acd%d: sequence error\n", cdp->lun);
                bp->b_error = EIO;
                bp->b_flags |= B_ERROR;
                biodone(bp);
                return;
            } else {
                if (acd_open_track(cdp, &cdp->preptrack) != 0) {
                    biodone(bp);
                    return;
                }
                cdp->flags |= F_TRACK_PREPED;
            }
        }
    }
    bzero(ccb, sizeof(ccb));
    if (bp->b_flags & B_READ) {
    	lba = bp->b_blkno / (cdp->block_size / DEV_BSIZE);
	ccb[0] = ATAPI_READ_BIG;
    }
    else {
	lba = cdp->next_writeable_lba + (bp->b_offset / cdp->block_size);
	ccb[0] = ATAPI_WRITE_BIG;
    }
    count = (bp->b_bcount + (cdp->block_size - 1)) / cdp->block_size;

#ifdef ACD_DEBUG
    printf("acd%d: lba=%d, count=%d\n", cdp->lun, lba, count);
#endif
    ccb[1] = 0;
    ccb[2] = lba>>24;
    ccb[3] = lba>>16;
    ccb[4] = lba>>8;
    ccb[5] = lba;
    ccb[7] = count>>8;
    ccb[8] = count;

    devstat_start_transaction(cdp->stats);

    atapi_queue_cmd(cdp->atp, ccb, bp->b_data, bp->b_bcount,
                    (bp->b_flags&B_READ)?A_READ : 0, 30, acd_done, cdp, bp);
}

static void 
acd_done(struct atapi_request *request)
{
    struct buf *bp = request->bp;
    struct acd_softc *cdp = request->driver;
    
    devstat_end_transaction(cdp->stats, request->donecount,
                            DEVSTAT_TAG_NONE,
                            (bp->b_flags&B_READ) ? DEVSTAT_READ:DEVSTAT_WRITE);  
    if (request->result) {
        bp->b_error = atapi_error(request->device, request->result);
        bp->b_flags |= B_ERROR;
    }   
    else {
        bp->b_resid = request->bytecount;
        if ((bp->b_flags & B_READ) == B_WRITE)
            cdp->flags |= F_WRITTEN;
    }
    biodone(bp);
    acd_start(cdp);
}

static int32_t
acd_test_unit_ready(struct acd_softc *cdp)
{
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 30, NULL, NULL, NULL);
}

static int32_t
acd_lock_device(struct acd_softc *cdp, int32_t lock)
{
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 30, NULL, NULL, NULL);
}

static int32_t
acd_start_device(struct acd_softc *cdp, int32_t start)
{
    int8_t ccb[16] = { ATAPI_START_STOP, 0, 0, 0, start,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 30, NULL, NULL, NULL);
}

static int32_t
acd_pause_device(struct acd_softc *cdp, int32_t pause)
{
    int8_t ccb[16] = { ATAPI_START_STOP, 0, 0, 0, 0, 0, 0, 0, pause,
		       0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 30, NULL, NULL, NULL);
}

static int32_t
acd_mode_sense(struct acd_softc *cdp, u_int8_t page,
	       void *pagebuf, int32_t pagesize)
{
    int32_t error;
    int8_t ccb[16] = { ATAPI_MODE_SENSE, 0, page, 0, 0, 0, 0,
                     pagesize>>8, pagesize, 0, 0, 0, 0, 0, 0, 0 };

    error = atapi_queue_cmd(cdp->atp, ccb, pagebuf, pagesize, A_READ, 30,
			    NULL, NULL, NULL);
#ifdef ACD_DEBUG
    atapi_dump("acd: mode sense ", pagebuf, pagesize);
#endif
    return error;
}

static int32_t
acd_mode_select(struct acd_softc *cdp, void *pagebuf, int32_t pagesize)
{
    int8_t ccb[16] = { ATAPI_MODE_SELECT, 0x10, 0, 0, 0, 0, 0,
                     pagesize>>8, pagesize, 0, 0, 0, 0, 0, 0, 0 };

#ifdef ACD_DEBUG
    printf("acd: modeselect pagesize=%d\n", pagesize);
    atapi_dump("acd: mode select ", pagebuf, pagesize);
#endif
    return atapi_queue_cmd(cdp->atp, ccb, pagebuf, pagesize, 0, 30,
			   NULL, NULL, NULL);
}

static int32_t 
acd_read_toc(struct acd_softc *cdp)
{
    int32_t error, ntracks, len;
    int8_t ccb[16];

    bzero(&cdp->toc, sizeof(cdp->toc));
    bzero(&cdp->info, sizeof(cdp->info));
    bzero(ccb, sizeof(ccb));

    acd_select_slot(cdp);

    error = acd_test_unit_ready(cdp);
    if (error == EAGAIN) {
        cdp->flags |= F_MEDIA_CHANGED;
    	cdp->flags &= ~(F_WRITTEN | F_TRACK_PREP | F_TRACK_PREPED);
        error = acd_test_unit_ready(cdp);
    }

    if (error)
        return error;

    cdp->flags &= ~F_MEDIA_CHANGED;

    len = sizeof(struct ioc_toc_header) + sizeof(struct cd_toc_entry);
    ccb[0] = ATAPI_READ_TOC;
    ccb[7] = len>>8;
    ccb[8] = len;
    if (atapi_queue_cmd(cdp->atp, ccb, &cdp->toc, len, A_READ, 30,
			NULL, NULL, NULL)){
        bzero(&cdp->toc, sizeof(cdp->toc));
        return 0;
    }
    ntracks = cdp->toc.hdr.ending_track - cdp->toc.hdr.starting_track + 1;
    if (ntracks <= 0 || ntracks > MAXTRK) {
        bzero(&cdp->toc, sizeof(cdp->toc));
        return 0;
    }

    len = sizeof(struct ioc_toc_header) + ntracks * sizeof(struct cd_toc_entry);
    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_READ_TOC;
    ccb[7] = len>>8;
    ccb[8] = len;
    if (atapi_queue_cmd(cdp->atp, ccb, &cdp->toc, len, A_READ, 30,
			NULL, NULL, NULL)){
        bzero(&cdp->toc, sizeof(cdp->toc));
        return 0;
    }

    cdp->toc.hdr.len = ntohs(cdp->toc.hdr.len);

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_READ_CAPACITY;
    if (atapi_queue_cmd(cdp->atp, ccb, &cdp->info, sizeof(cdp->info), 
			A_READ, 30, NULL, NULL, NULL))
        bzero(&cdp->info, sizeof(cdp->info));

    cdp->toc.tab[ntracks].control = cdp->toc.tab[ntracks - 1].control;
    cdp->toc.tab[ntracks].addr_type = cdp->toc.tab[ntracks - 1].addr_type;
    cdp->toc.tab[ntracks].track = 170;
    cdp->toc.tab[ntracks].addr.lba = cdp->info.volsize;

    cdp->info.volsize = ntohl(cdp->info.volsize);
    cdp->info.blksize = ntohl(cdp->info.blksize);

#ifdef ACD_DEBUG
    if (cdp->info.volsize && cdp->toc.hdr.ending_track) {
        printf("acd%d: ", cdp->lun);
        if (cdp->toc.tab[0].control & 4)
            printf("%dMB ", cdp->info.volsize / 512);
        else
            printf("%d:%d audio ", cdp->info.volsize / 75 / 60,
                cdp->info.volsize / 75 % 60);
        printf("(%d sectors (%d bytes)), %d tracks\n", 
	    cdp->info.volsize, cdp->info.blksize,
            cdp->toc.hdr.ending_track - cdp->toc.hdr.starting_track + 1);
    }
#endif
    return 0;
}

static int32_t 
acd_setchan(struct acd_softc *cdp,
	    u_int8_t c0, u_int8_t c1, u_int8_t c2, u_int8_t c3)
{
    int32_t error;

    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_AUDIO_PAGE, &cdp->au, 
				sizeof(cdp->au))))
        return error;
    if (cdp->au.page_code != ATAPI_CDROM_AUDIO_PAGE)
        return EIO;
    cdp->au.data_length = 0;
    cdp->au.port[0].channels = c0;
    cdp->au.port[1].channels = c1;
    cdp->au.port[2].channels = c2;
    cdp->au.port[3].channels = c3;
    return acd_mode_select(cdp, &cdp->au, sizeof(cdp->au));
}

static int32_t 
acd_eject(struct acd_softc *cdp, int32_t close)
{
    int32_t error;

    acd_select_slot(cdp);

    error = acd_start_device(cdp, 0);

    if (error == EBUSY || error == EAGAIN) {
        if (!close)
            return 0;
	if ((error = acd_start_device(cdp, 3)))
	    return error;
        acd_read_toc(cdp);
	acd_lock_device(cdp, 1);
        cdp->flags |= F_LOCKED;
        return 0;
    }
    if (error)
        return error;
    if (close)
        return 0;

    tsleep((caddr_t) &lbolt, PRIBIO, "acdej1", 0);
    tsleep((caddr_t) &lbolt, PRIBIO, "acdej2", 0);
    acd_lock_device(cdp, 0);
    cdp->flags &= ~F_LOCKED;
    cdp->flags |= F_MEDIA_CHANGED;
    cdp->flags &= ~(F_WRITTEN|F_TRACK_PREP|F_TRACK_PREPED);
    return acd_start_device(cdp, 2);
}

static void
acd_select_slot(struct acd_softc *cdp)
{
    int8_t ccb[16];

    if (cdp->slot < 0 || cdp->changer_info->current_slot == cdp->slot)
        return;

    /* unlock (might not be needed but its cheaper than asking) */
    acd_lock_device(cdp, 0);

    bzero(ccb, sizeof(ccb));
    /* unload the current media from player */
    ccb[0] = ATAPI_LOAD_UNLOAD;
    ccb[4] = 2;
    ccb[8] = cdp->changer_info->current_slot;
    atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 30, NULL, NULL, NULL);

    /* load the wanted slot */
    ccb[0] = ATAPI_LOAD_UNLOAD;
    ccb[4] = 3;
    ccb[8] = cdp->slot;
    atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 30, NULL, NULL, NULL);

    cdp->changer_info->current_slot = cdp->slot;

    /* lock the media if needed */
    if (cdp->flags & F_LOCKED)
        acd_lock_device(cdp, 1);
}

static int32_t
acd_open_disk(struct acd_softc *cdp, int32_t test)
{
    cdp->next_writeable_lba = 0;
    return 0;
}

static int32_t
acd_close_disk(struct acd_softc *cdp)
{
    int8_t ccb[16] = { ATAPI_CLOSE_TRACK, 0, 0x02, 0, 0, 0, 0, 0, 
		       0, 0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 600, NULL, NULL, NULL);
}

static int32_t
acd_open_track(struct acd_softc *cdp, struct wormio_prepare_track *ptp)
{
    struct write_param param;
    int32_t error;

    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_WRITE_PARAMETERS_PAGE,
				&param, sizeof(param))))
        return error;
    param.page_code = 0x05;
    param.page_length = 0x32;
    param.test_write = cdp->dummy ? 1 : 0;
    param.write_type = CDR_WTYPE_TRACK;

    switch (ptp->audio) {
/*    switch (data_type) { */

    case 0:
/*    case CDR_DATA: */
	cdp->block_size = 2048;
    	param.track_mode = CDR_TMODE_DATA;
    	param.data_block_type = CDR_DB_ROM_MODE1;
    	param.session_format = CDR_SESS_CDROM;
	break;

    default:
/*    case CDR_AUDIO: */
	cdp->block_size = 2352;
	if (ptp->preemp)
    	    param.track_mode = CDR_TMODE_AUDIO;
	else
    	    param.track_mode = 0;
    	param.data_block_type = CDR_DB_RAW;
    	param.session_format = CDR_SESS_CDROM;
	break;

/*
    case CDR_MODE2:
    	param.track_mode = CDR_TMODE_DATA;
    	param.data_block_type = CDR_DB_ROM_MODE2;
    	param.session_format = CDR_SESS_CDROM;
	break;

    case CDR_XA1:
    	param.track_mode = CDR_TMODE_DATA;
    	param.data_block_type = CDR_DB_XA_MODE1;
    	param.session_format = CDR_SESS_CDROM_XA;
	break;

    case CDR_XA2:
    	param.track_mode = CDR_TMODE_DATA;
    	param.data_block_type = CDR_DB_XA_MODE2_F1;
    	param.session_format = CDR_SESS_CDROM_XA;
	break;

    case CDR_CDI:
    	param.track_mode = CDR_TMODE_DATA;
    	param.data_block_type = CDR_DB_XA_MODE2_F1;
    	param.session_format = CDR_SESS_CDI;
	break;
    }
*/
    }

    param.multi_session = CDR_MSES_NONE;
    param.fp = 0;
    param.packet_size = 0;
    return acd_mode_select(cdp, &param, sizeof(param));
}

static int32_t
acd_close_track(struct acd_softc *cdp)
{
    int8_t ccb[16] = { ATAPI_SYNCHRONIZE_CACHE, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0};

    return atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 600, NULL, NULL, NULL);
}

static int32_t
acd_read_track_info(struct acd_softc *cdp,
		    int32_t lba, struct acd_track_info *info)
{
    int32_t error;
    int8_t ccb[16] = { ATAPI_READ_TRACK_INFO, 1,
		     lba>>24, lba>>16, lba>>8, lba,
		     0,
                     sizeof(*info)>>8, sizeof(*info),
                     0, 0, 0, 0, 0, 0, 0 };

    if ((error = atapi_queue_cmd(cdp->atp, ccb, info, sizeof(*info), 
			         A_READ, 30, NULL, NULL, NULL)))
	return error;
    info->track_start_addr = ntohl(info->track_start_addr);
    info->next_writeable_addr = ntohl(info->next_writeable_addr);
    info->free_blocks = ntohl(info->free_blocks);
    info->fixed_packet_size = ntohl(info->fixed_packet_size);
    info->track_length = ntohl(info->track_length);
    return 0;
}

static int32_t
acd_blank_disk(struct acd_softc *cdp)
{
    int32_t error;
    int8_t ccb[16] = { ATAPI_BLANK, 1, 0, 0, 0, 0, 0, 0, 
		       0, 0, 0, 0, 0, 0, 0, 0 };

    error = atapi_queue_cmd(cdp->atp, ccb, NULL, 0, 0, 60*60, NULL, NULL, NULL);
    cdp->flags |= F_MEDIA_CHANGED;
    cdp->flags &= ~(F_WRITTEN|F_TRACK_PREP|F_TRACK_PREPED);
    return error;
}

static void 
acd_drvinit(void *unused)
{
    static int32_t acd_devsw_installed = 0;

    if (!acd_devsw_installed) {
        if (!acd_cdevsw.d_maxio)
            acd_cdevsw.d_maxio = 254 * DEV_BSIZE;
        cdevsw_add(&acd_cdevsw);
        acd_devsw_installed = 1;
    }
}
SYSINIT(acddev, SI_SUB_DRIVERS, SI_ORDER_MIDDLE, acd_drvinit, NULL)
#endif /* NATA && NATAPICD */
