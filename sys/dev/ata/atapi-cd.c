/*-
 * Copyright (c) 1998,1999,2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
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

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/disklabel.h>
#include <sys/devicestat.h>
#include <sys/cdio.h>
#include <sys/cdrio.h>
#include <sys/dvdio.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/ctype.h>
#include <machine/bus.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-all.h>
#include <dev/ata/atapi-cd.h>

/* device structures */
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
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	acdstrategy,
	/* name */	"acd",
	/* maj */	117,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_DISK | D_TRACKCLOSE,
};

/* prototypes */
static struct acd_softc *acd_init_lun(struct ata_device *);
static void acd_make_dev(struct acd_softc *);
static void acd_describe(struct acd_softc *);
static void lba2msf(u_int32_t, u_int8_t *, u_int8_t *, u_int8_t *);
static u_int32_t msf2lba(u_int8_t, u_int8_t, u_int8_t);
static int acd_done(struct atapi_request *);
static void acd_read_toc(struct acd_softc *);
static int acd_play(struct acd_softc *, int, int);
static int acd_setchan(struct acd_softc *, u_int8_t, u_int8_t, u_int8_t, u_int8_t);
static void acd_select_slot(struct acd_softc *);
static int acd_init_writer(struct acd_softc *, int);
static int acd_fixate(struct acd_softc *, int);
static int acd_init_track(struct acd_softc *, struct cdr_track *);
static int acd_flush(struct acd_softc *);
static int acd_read_track_info(struct acd_softc *, int32_t, struct acd_track_info *);
static int acd_get_progress(struct acd_softc *, int *);
static int acd_send_cue(struct acd_softc *, struct cdr_cuesheet *);
static int acd_report_key(struct acd_softc *, struct dvd_authinfo *);
static int acd_send_key(struct acd_softc *, struct dvd_authinfo *);
static int acd_read_structure(struct acd_softc *, struct dvd_struct *);
static int acd_eject(struct acd_softc *, int);
static int acd_blank(struct acd_softc *, int);
static int acd_prevent_allow(struct acd_softc *, int);
static int acd_start_stop(struct acd_softc *, int);
static int acd_pause_resume(struct acd_softc *, int);
static int acd_mode_sense(struct acd_softc *, int, caddr_t, int);
static int acd_mode_select(struct acd_softc *, caddr_t, int);
static int acd_set_speed(struct acd_softc *, int, int);
static void acd_get_cap(struct acd_softc *);

/* internal vars */
static u_int32_t acd_lun_map = 0;
static MALLOC_DEFINE(M_ACD, "ACD driver", "ATAPI CD driver buffers");

int
acdattach(struct ata_device *atadev)
{
    struct acd_softc *cdp;
    struct changer *chp;

    if ((cdp = acd_init_lun(atadev)) == NULL) {
	ata_prtdev(atadev, "acd: out of memory\n");
	return 0;
    }

    ata_set_name(atadev, "acd", cdp->lun);
    acd_get_cap(cdp);

    /* if this is a changer device, allocate the neeeded lun's */
    if (cdp->cap.mech == MST_MECH_CHANGER) {
	int8_t ccb[16] = { ATAPI_MECH_STATUS, 0, 0, 0, 0, 0, 0, 0, 
			   sizeof(struct changer)>>8, sizeof(struct changer),
			   0, 0, 0, 0, 0, 0 };

	chp = malloc(sizeof(struct changer), M_ACD, M_NOWAIT | M_ZERO);
	if (chp == NULL) {
	    ata_prtdev(atadev, "out of memory\n");
	    free(cdp, M_ACD);
	    return 0;
	}
	if (!atapi_queue_cmd(cdp->device, ccb, (caddr_t)chp, 
			     sizeof(struct changer),
			     ATPR_F_READ, 60, NULL, NULL)) {
	    struct acd_softc *tmpcdp = cdp;
	    struct acd_softc **cdparr;
	    char *name;
	    int count;

	    chp->table_length = htons(chp->table_length);
	    if (!(cdparr = malloc(sizeof(struct acd_softc) * chp->slots,
				  M_ACD, M_NOWAIT))) {
		ata_prtdev(atadev, "out of memory\n");
		free(chp, M_ACD);
		free(cdp, M_ACD);
		return 0;
	    }
	    for (count = 0; count < chp->slots; count++) {
		if (count > 0) {
		    tmpcdp = acd_init_lun(atadev);
		    if (!tmpcdp) {
			ata_prtdev(atadev, "out of memory\n");
			break;
		    }
		}
		cdparr[count] = tmpcdp;
		tmpcdp->driver = cdparr;
		tmpcdp->slot = count;
		tmpcdp->changer_info = chp;
		acd_make_dev(tmpcdp);
		devstat_add_entry(tmpcdp->stats, "acd", tmpcdp->lun, DEV_BSIZE,
				  DEVSTAT_NO_ORDERED_TAGS,
				  DEVSTAT_TYPE_CDROM | DEVSTAT_TYPE_IF_IDE,
				  DEVSTAT_PRIORITY_CD);
	    }
	    name = malloc(strlen(atadev->name) + 2, M_ACD, M_NOWAIT);
	    strcpy(name, atadev->name);
	    strcat(name, "-");
	    ata_free_name(atadev);
	    ata_set_name(atadev, name, cdp->lun + cdp->changer_info->slots - 1);
	    free(name, M_ACD);
	}
    }
    else {
	acd_make_dev(cdp);
	devstat_add_entry(cdp->stats, "acd", cdp->lun, DEV_BSIZE,
			  DEVSTAT_NO_ORDERED_TAGS,
			  DEVSTAT_TYPE_CDROM | DEVSTAT_TYPE_IF_IDE,
			  DEVSTAT_PRIORITY_CD);
    }
    acd_describe(cdp);
    atadev->driver = cdp;
    return 1;
}

void
acddetach(struct ata_device *atadev)
{   
    struct acd_softc *cdp = atadev->driver;
    struct acd_devlist *entry;
    struct bio *bp;
    int subdev;
    
    if (cdp->changer_info) {
	for (subdev = 0; subdev < cdp->changer_info->slots; subdev++) {
	    if (cdp->driver[subdev] == cdp)
		continue;
	    while ((bp = bioq_first(&cdp->driver[subdev]->queue))) {
		bioq_remove(&cdp->driver[subdev]->queue, bp);
		biofinish(bp, NULL, ENXIO);
	    }
	    destroy_dev(cdp->driver[subdev]->dev);
	    while ((entry = TAILQ_FIRST(&cdp->driver[subdev]->dev_list))) {
		destroy_dev(entry->dev);
		TAILQ_REMOVE(&cdp->driver[subdev]->dev_list, entry, chain);
		free(entry, M_ACD);
	    }
	    devstat_remove_entry(cdp->driver[subdev]->stats);
	    free(cdp->driver[subdev]->stats, M_ACD);
	    ata_free_lun(&acd_lun_map, cdp->driver[subdev]->lun);
	    free(cdp->driver[subdev], M_ACD);
	}
	free(cdp->driver, M_ACD);
	free(cdp->changer_info, M_ACD);
    }
    while ((bp = bioq_first(&cdp->queue)))
	biofinish(bp, NULL, ENXIO);
    while ((entry = TAILQ_FIRST(&cdp->dev_list))) {
	destroy_dev(entry->dev);
	TAILQ_REMOVE(&cdp->dev_list, entry, chain);
	free(entry, M_ACD);
    }
    destroy_dev(cdp->dev);
    EVENTHANDLER_DEREGISTER(dev_clone, cdp->clone_evh);
    devstat_remove_entry(cdp->stats);
    free(cdp->stats, M_ACD);
    ata_free_name(atadev);
    ata_free_lun(&acd_lun_map, cdp->lun);
    free(cdp, M_ACD);
    atadev->driver = NULL;
}

static struct acd_softc *
acd_init_lun(struct ata_device *atadev)
{
    struct acd_softc *cdp;

    if (!(cdp = malloc(sizeof(struct acd_softc), M_ACD, M_NOWAIT | M_ZERO)))
	return NULL;
    TAILQ_INIT(&cdp->dev_list);
    bioq_init(&cdp->queue);
    cdp->device = atadev;
    cdp->lun = ata_get_lun(&acd_lun_map);
    cdp->block_size = 2048;
    cdp->slot = -1;
    cdp->changer_info = NULL;
    if (!(cdp->stats = malloc(sizeof(struct devstat), M_ACD,
			      M_NOWAIT | M_ZERO))) {
	free(cdp, M_ACD);
	return NULL;
    }
    return cdp;
}

static void
acd_clone(void *arg, char *name, int namelen, dev_t *dev)
{
    struct acd_softc *cdp = arg;
    char *p;
    int unit;

    if (*dev != NODEV)
	return;
    if (!dev_stdclone(name, &p, "acd", &unit))
	return;
    if (*p != '\0' && strcmp(p, "a") != 0 && strcmp(p, "c") != 0)
	return;
    if (unit == cdp->lun)
	*dev = makedev(acd_cdevsw.d_maj, dkmakeminor(cdp->lun, 0, 0));
}

static void
acd_make_dev(struct acd_softc *cdp)
{
    dev_t dev;

    dev = make_dev(&acd_cdevsw, dkmakeminor(cdp->lun, 0, 0),
		   UID_ROOT, GID_OPERATOR, 0644, "acd%d", cdp->lun);
    make_dev_alias(dev, "acd%da", cdp->lun);
    make_dev_alias(dev, "acd%dc", cdp->lun);
    dev->si_drv1 = cdp;
    dev->si_iosize_max = 126 * DEV_BSIZE;
    dev->si_bsize_phys = 2048; /* XXX SOS */
    cdp->dev = dev;
    cdp->device->flags |= ATA_D_MEDIA_CHANGED;
    cdp->clone_evh = EVENTHANDLER_REGISTER(dev_clone, acd_clone, cdp, 1000);
}

static void 
acd_describe(struct acd_softc *cdp)
{
    int comma = 0;
    char *mechanism;

    if (bootverbose) {
	ata_prtdev(cdp->device, "<%.40s/%.8s> %s drive at ata%d as %s\n",
		   cdp->device->param->model, cdp->device->param->revision,
		   (cdp->cap.write_dvdr) ? "DVD-R" : 
		    (cdp->cap.write_dvdram) ? "DVD-RAM" : 
		     (cdp->cap.write_cdrw) ? "CD-RW" :
		      (cdp->cap.write_cdr) ? "CD-R" : 
		       (cdp->cap.read_dvdrom) ? "DVD-ROM" : "CDROM",
		   device_get_unit(cdp->device->channel->dev),
		   (cdp->device->unit == ATA_MASTER) ? "master" : "slave");

	ata_prtdev(cdp->device, "%s", "");
	if (cdp->cap.cur_read_speed) {
	    printf("read %dKB/s", cdp->cap.cur_read_speed * 1000 / 1024);
	    if (cdp->cap.max_read_speed) 
		printf(" (%dKB/s)", cdp->cap.max_read_speed * 1000 / 1024);
	    if ((cdp->cap.cur_write_speed) &&
		(cdp->cap.write_cdr || cdp->cap.write_cdrw || 
		 cdp->cap.write_dvdr || cdp->cap.write_dvdram)) {
		printf(" write %dKB/s", cdp->cap.cur_write_speed * 1000 / 1024);
		if (cdp->cap.max_write_speed)
		    printf(" (%dKB/s)", cdp->cap.max_write_speed * 1000 / 1024);
	    }
	    comma = 1;
	}
	if (cdp->cap.buf_size) {
	    printf("%s %dKB buffer", comma ? "," : "", cdp->cap.buf_size);
	    comma = 1;
	}
	printf("%s %s\n", comma ? "," : "", ata_mode2str(cdp->device->mode));

	ata_prtdev(cdp->device, "Reads:");
	comma = 0;
	if (cdp->cap.read_cdr) {
	    printf(" CD-R"); comma = 1;
	}
	if (cdp->cap.read_cdrw) {
	    printf("%s CD-RW", comma ? "," : ""); comma = 1;
	}
	if (cdp->cap.cd_da) {
	    if (cdp->cap.cd_da_stream)
		printf("%s CD-DA stream", comma ? "," : "");
	    else
		printf("%s CD-DA", comma ? "," : "");
	    comma = 1;
	}
	if (cdp->cap.read_dvdrom) {
	    printf("%s DVD-ROM", comma ? "," : ""); comma = 1;
	}
	if (cdp->cap.read_dvdr) {
	    printf("%s DVD-R", comma ? "," : ""); comma = 1;
	}
	if (cdp->cap.read_dvdram) {
	    printf("%s DVD-RAM", comma ? "," : ""); comma = 1;
	}
	if (cdp->cap.read_packet)
	    printf("%s packet", comma ? "," : "");

	printf("\n");
	ata_prtdev(cdp->device, "Writes:");
	if (cdp->cap.write_cdr || cdp->cap.write_cdrw || 
	    cdp->cap.write_dvdr || cdp->cap.write_dvdram) {
	    comma = 0;
	    if (cdp->cap.write_cdr) {
		printf(" CD-R" ); comma = 1;
	    }
	    if (cdp->cap.write_cdrw) {
		printf("%s CD-RW", comma ? "," : ""); comma = 1;
	    }
	    if (cdp->cap.write_dvdr) {
		printf("%s DVD-R", comma ? "," : ""); comma = 1;
	    }
	    if (cdp->cap.write_dvdram) {
		printf("%s DVD-RAM", comma ? "," : ""); comma = 1; 
	    }
	    if (cdp->cap.test_write) {
		printf("%s test write", comma ? "," : ""); comma = 1;
	    }
	    if (cdp->cap.burnproof)
		printf("%s burnproof", comma ? "," : "");
	}
	printf("\n");
	if (cdp->cap.audio_play) {
	    ata_prtdev(cdp->device, "Audio: ");
	    if (cdp->cap.audio_play)
		printf("play");
	    if (cdp->cap.max_vol_levels)
		printf(", %d volume levels", cdp->cap.max_vol_levels);
	    printf("\n");
	}
	ata_prtdev(cdp->device, "Mechanism: ");
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

	if (cdp->cap.lock)
	    printf(cdp->cap.locked ? ", locked" : ", unlocked");
	if (cdp->cap.prevent)
	    printf(", lock protected");
	printf("\n");

	if (cdp->cap.mech != MST_MECH_CHANGER) {
	    ata_prtdev(cdp->device, "Medium: ");
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
		printf("no/blank disc"); break;
	    case MST_FMT_ERROR:
		printf("medium format error"); break;
	    }
	    if ((cdp->cap.medium_type & MST_TYPE_MASK_HIGH)<MST_TYPE_MASK_HIGH){
		switch (cdp->cap.medium_type & MST_TYPE_MASK_LOW) {
		case MST_DATA_120:
		    printf("120mm data disc"); break;
		case MST_AUDIO_120:
		    printf("120mm audio disc"); break;
		case MST_COMB_120:
		    printf("120mm data/audio disc"); break;
		case MST_PHOTO_120:
		    printf("120mm photo disc"); break;
		case MST_DATA_80:
		    printf("80mm data disc"); break;
		case MST_AUDIO_80:
		    printf("80mm audio disc"); break;
		case MST_COMB_80:
		    printf("80mm data/audio disc"); break;
		case MST_PHOTO_80:
		    printf("80mm photo disc"); break;
		case MST_FMT_NONE:
		    switch (cdp->cap.medium_type & MST_TYPE_MASK_HIGH) {
		    case MST_CDROM:
			printf("unknown"); break;
		    case MST_CDR:
		    case MST_CDRW:
			printf("blank"); break;
		    }
		    break;
		default:
		    printf("unknown (0x%x)", cdp->cap.medium_type); break;
		}
	    }
	    printf("\n");
	}
    }
    else {
	ata_prtdev(cdp->device, "%s ",
		   (cdp->cap.write_dvdr) ? "DVD-R" : 
		    (cdp->cap.write_dvdram) ? "DVD-RAM" : 
		     (cdp->cap.write_cdrw) ? "CD-RW" :
		      (cdp->cap.write_cdr) ? "CD-R" : 
		       (cdp->cap.read_dvdrom) ? "DVD-ROM" : "CDROM");

	if (cdp->changer_info)
	    printf("with %d CD changer ", cdp->changer_info->slots);

	printf("<%.40s> at ata%d-%s %s\n", cdp->device->param->model,
	       device_get_unit(cdp->device->channel->dev),
	       (cdp->device->unit == ATA_MASTER) ? "master" : "slave",
	       ata_mode2str(cdp->device->mode) );
    }
}

static __inline void 
lba2msf(u_int32_t lba, u_int8_t *m, u_int8_t *s, u_int8_t *f)
{
    lba += 150;
    lba &= 0xffffff;
    *m = lba / (60 * 75);
    lba %= (60 * 75);
    *s = lba / 75;
    *f = lba % 75;
}

static __inline u_int32_t 
msf2lba(u_int8_t m, u_int8_t s, u_int8_t f)
{
    return (m * 60 + s) * 75 + f - 150;
}

static int
acdopen(dev_t dev, int flags, int fmt, struct thread *td)
{
    struct acd_softc *cdp = dev->si_drv1;
    
    if (!cdp)
	return ENXIO;

    if (flags & FWRITE) {
	if (count_dev(dev) > 1)
	    return EBUSY;
    }
    if (count_dev(dev) == 1) {
	if (cdp->changer_info && cdp->slot != cdp->changer_info->current_slot) {
	    acd_select_slot(cdp);
	    tsleep(&cdp->changer_info, PRIBIO, "acdopn", 0);
	}
	acd_prevent_allow(cdp, 1);
	cdp->flags |= F_LOCKED;
	acd_read_toc(cdp);
    }
    return 0;
}

static int 
acdclose(dev_t dev, int flags, int fmt, struct thread *td)
{
    struct acd_softc *cdp = dev->si_drv1;
    
    if (!cdp)
	return ENXIO;

    if (count_dev(dev) == 1) {
	if (cdp->changer_info && cdp->slot != cdp->changer_info->current_slot) {
	    acd_select_slot(cdp);
	    tsleep(&cdp->changer_info, PRIBIO, "acdclo", 0);
	}
	acd_prevent_allow(cdp, 0);
	cdp->flags &= ~F_LOCKED;
    }
    return 0;
}

static int 
acdioctl(dev_t dev, u_long cmd, caddr_t addr, int flags, struct thread *td)
{
    struct acd_softc *cdp = dev->si_drv1;
    int error = 0;

    if (!cdp)
	return ENXIO;

    if (cdp->changer_info && cdp->slot != cdp->changer_info->current_slot) {
	acd_select_slot(cdp);
	tsleep(&cdp->changer_info, PRIBIO, "acdctl", 0);
    }
    if (cdp->device->flags & ATA_D_MEDIA_CHANGED)
	switch (cmd) {
	case CDIOCRESET:
	    atapi_test_ready(cdp->device);
	    break;
	   
	default:
	    acd_read_toc(cdp);
	    acd_prevent_allow(cdp, 1);
	    cdp->flags |= F_LOCKED;
	    break;
	}
    switch (cmd) {

    case CDIOCRESUME:
	error = acd_pause_resume(cdp, 1);
	break;

    case CDIOCPAUSE:
	error = acd_pause_resume(cdp, 0);
	break;

    case CDIOCSTART:
	error = acd_start_stop(cdp, 1);
	break;

    case CDIOCSTOP:
	error = acd_start_stop(cdp, 0);
	break;

    case CDIOCALLOW:
	error = acd_prevent_allow(cdp, 0);
	cdp->flags &= ~F_LOCKED;
	break;

    case CDIOCPREVENT:
	error = acd_prevent_allow(cdp, 1);
	cdp->flags |= F_LOCKED;
	break;

    case CDIOCRESET:
	error = suser(td);
	if (error)
	    break;
	error = atapi_test_ready(cdp->device);
	break;

    case CDIOCEJECT:
	if (count_dev(dev) > 1) {
	    error = EBUSY;
	    break;
	}
	error = acd_eject(cdp, 0);
	break;

    case CDIOCCLOSE:
	if (count_dev(dev) > 1)
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
	    int starting_track = te->starting_track;
	    int len;

	    if (!toc->hdr.ending_track) {
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

		toc = malloc(sizeof(struct toc), M_ACD, M_NOWAIT | M_ZERO);
		bcopy(&cdp->toc, toc, sizeof(struct toc));
		entry = toc->tab + (toc->hdr.ending_track + 1 -
			toc->hdr.starting_track) + 1;
		while (--entry >= toc->tab)
		    lba2msf(ntohl(entry->addr.lba), &entry->addr.msf.minute,
			    &entry->addr.msf.second, &entry->addr.msf.frame);
	    }
	    error = copyout(toc->tab + starting_track - toc->hdr.starting_track,
			    te->data, len);
	    if (te->address_format == CD_MSF_FORMAT)
		free(toc, M_ACD);
	    break;
	}
    case CDIOREADTOCENTRY:
	{
	    struct ioc_read_toc_single_entry *te =
		(struct ioc_read_toc_single_entry *)addr;
	    struct toc *toc = &cdp->toc;
	    u_char track = te->track;

	    if (!toc->hdr.ending_track) {
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

		toc = malloc(sizeof(struct toc), M_ACD, M_NOWAIT | M_ZERO);
		bcopy(&cdp->toc, toc, sizeof(struct toc));

		entry = toc->tab + (track - toc->hdr.starting_track);
		lba2msf(ntohl(entry->addr.lba), &entry->addr.msf.minute,
			&entry->addr.msf.second, &entry->addr.msf.frame);
	    }
	    bcopy(toc->tab + track - toc->hdr.starting_track,
		  &te->entry, sizeof(struct cd_toc_entry));
	    if (te->address_format == CD_MSF_FORMAT)
		free(toc, M_ACD);
	}
	break;

    case CDIOCREADSUBCHANNEL:
	{
	    struct ioc_read_subchannel *args =
		(struct ioc_read_subchannel *)addr;
	    u_int8_t format;
	    int8_t ccb[16] = { ATAPI_READ_SUBCHANNEL, 0, 0x40, 1, 0, 0, 0,
			       sizeof(cdp->subchan)>>8, sizeof(cdp->subchan),
			       0, 0, 0, 0, 0, 0, 0 };

	    if (args->data_len > sizeof(struct cd_sub_channel_info) ||
		args->data_len < sizeof(struct cd_sub_channel_header)) {
		error = EINVAL;
		break;
	    }

	    format=args->data_format;
	    if ((format != CD_CURRENT_POSITION) &&
		(format != CD_MEDIA_CATALOG) && (format != CD_TRACK_INFO)) {
		error = EINVAL;
		break;
	    }

	    ccb[1] = args->address_format & CD_MSF_FORMAT;

	    if ((error = atapi_queue_cmd(cdp->device,ccb,(caddr_t)&cdp->subchan,
					 sizeof(cdp->subchan), ATPR_F_READ, 10,
					 NULL, NULL)))
		break;

	    if ((format == CD_MEDIA_CATALOG) || (format == CD_TRACK_INFO)) {
		if (cdp->subchan.header.audio_status == 0x11) {
		    error = EINVAL;
		    break;
		}

		ccb[3] = format;
		if (format == CD_TRACK_INFO)
		    ccb[6] = args->track;

		if ((error = atapi_queue_cmd(cdp->device, ccb,
					     (caddr_t)&cdp->subchan, 
					     sizeof(cdp->subchan), ATPR_F_READ,
					     10, NULL, NULL))) {
		    break;
		}
	    }
	    error = copyout(&cdp->subchan, args->data, args->data_len);
	    break;
	}

    case CDIOCPLAYMSF:
	{
	    struct ioc_play_msf *args = (struct ioc_play_msf *)addr;

	    error = 
		acd_play(cdp, 
			 msf2lba(args->start_m, args->start_s, args->start_f),
			 msf2lba(args->end_m, args->end_s, args->end_f));
	    break;
	}

    case CDIOCPLAYBLOCKS:
	{
	    struct ioc_play_blocks *args = (struct ioc_play_blocks *)addr;

	    error = acd_play(cdp, args->blk, args->blk + args->len);
	    break;
	}

    case CDIOCPLAYTRACKS:
	{
	    struct ioc_play_track *args = (struct ioc_play_track *)addr;
	    int t1, t2;

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
	    if (t1 < 0 || t2 < 0 ||
		t1 > (cdp->toc.hdr.ending_track-cdp->toc.hdr.starting_track)) {
		error = EINVAL;
		break;
	    }
	    error = acd_play(cdp, ntohl(cdp->toc.tab[t1].addr.lba),
			     ntohl(cdp->toc.tab[t2].addr.lba));
	    break;
	}

    case CDIOCREADAUDIO:
	{
	    struct ioc_read_audio *args = (struct ioc_read_audio *)addr;
	    int32_t lba;
	    caddr_t buffer, ubuf = args->buffer;
	    int8_t ccb[16];
	    int frames;

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
	    if (!(buffer = malloc(CD_BUFFER_BLOCKS * 2352, M_ACD, M_NOWAIT))){
		error = ENOMEM;
		break;
	    }
	    bzero(ccb, sizeof(ccb));
	    while (frames > 0) {
		int8_t blocks;
		int size;

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
		if ((error = atapi_queue_cmd(cdp->device, ccb, buffer, size, 
					     ATPR_F_READ, 30, NULL,NULL)))
		    break;

		if ((error = copyout(buffer, ubuf, size)))
		    break;
		    
		ubuf += size;
		frames -= blocks;
		lba += blocks;
	    }
	    free(buffer, M_ACD);
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
					(caddr_t)&cdp->au, sizeof(cdp->au))))
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
					(caddr_t)&cdp->au, sizeof(cdp->au))))
		break;
	    if (cdp->au.page_code != ATAPI_CDROM_AUDIO_PAGE) {
		error = EIO;
		break;
	    }
	    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_AUDIO_PAGE_MASK,
					(caddr_t)&cdp->aumask,
					sizeof(cdp->aumask))))
		break;
	    cdp->au.data_length = 0;
	    cdp->au.port[0].channels = CHANNEL_0;
	    cdp->au.port[1].channels = CHANNEL_1;
	    cdp->au.port[0].volume = arg->vol[0] & cdp->aumask.port[0].volume;
	    cdp->au.port[1].volume = arg->vol[1] & cdp->aumask.port[1].volume;
	    cdp->au.port[2].volume = arg->vol[2] & cdp->aumask.port[2].volume;
	    cdp->au.port[3].volume = arg->vol[3] & cdp->aumask.port[3].volume;
	    error =  acd_mode_select(cdp, (caddr_t)&cdp->au, sizeof(cdp->au));
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

    case CDRIOCBLANK:
	error = acd_blank(cdp, (*(int *)addr));
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
	    *(int*)addr = track_info.next_writeable_addr;
	}
	break;
 
    case CDRIOCINITWRITER:
	error = acd_init_writer(cdp, (*(int *)addr));
	break;

    case CDRIOCINITTRACK:
	error = acd_init_track(cdp, (struct cdr_track *)addr);
	break;

    case CDRIOCFLUSH:
	error = acd_flush(cdp);
	break;

    case CDRIOCFIXATE:
	error = acd_fixate(cdp, (*(int *)addr));
	break;

    case CDRIOCREADSPEED:
	error = acd_set_speed(cdp, 177 * (*(int *)addr), -1);
	break;

    case CDRIOCWRITESPEED:
	error = acd_set_speed(cdp, -1, 177 * (*(int *)addr));
	break;

    case CDRIOCGETBLOCKSIZE:
	*(int *)addr = cdp->block_size;
	break;

    case CDRIOCSETBLOCKSIZE:
	cdp->block_size = *(int *)addr;
	break;

    case CDRIOCGETPROGRESS:
	error = acd_get_progress(cdp, (int *)addr);
	break;

    case CDRIOCSENDCUE:
	error = acd_send_cue(cdp, (struct cdr_cuesheet *)addr);
	break;

    case DVDIOCREPORTKEY:
	if (!cdp->cap.read_dvdrom)
	    error = EINVAL;
	else
	    error = acd_report_key(cdp, (struct dvd_authinfo *)addr);
	break;

    case DVDIOCSENDKEY:
	if (!cdp->cap.read_dvdrom)
	    error = EINVAL;
	else
	    error = acd_send_key(cdp, (struct dvd_authinfo *)addr);
	break;

    case DVDIOCREADSTRUCTURE:
	if (!cdp->cap.read_dvdrom)
	    error = EINVAL;
	else
	    error = acd_read_structure(cdp, (struct dvd_struct *)addr);
	break;

    case DIOCGDINFO:
	*(struct disklabel *)addr = cdp->disklabel;
	break;

    case DIOCWDINFO:
    case DIOCSDINFO:
	if ((flags & FWRITE) == 0)
	    error = EBADF;
	else
	    error = setdisklabel(&cdp->disklabel, (struct disklabel *)addr, 0);
	break;

    case DIOCWLABEL:
	error = EBADF;
	break;

    default:
	error = ENOTTY;
    }
    return error;
}

static void 
acdstrategy(struct bio *bp)
{
    struct acd_softc *cdp = bp->bio_dev->si_drv1;
    int s;

    if (cdp->device->flags & ATA_D_DETACHING) {
	biofinish(bp, NULL, ENXIO);
	return;
    }

    /* if it's a null transfer, return immediatly. */
    if (bp->bio_bcount == 0) {
	bp->bio_resid = 0;
	biodone(bp);
	return;
    }
    
    bp->bio_pblkno = bp->bio_blkno;
    bp->bio_resid = bp->bio_bcount;

    s = splbio();
    bioqdisksort(&cdp->queue, bp);
    splx(s);
    ata_start(cdp->device->channel);
}

void 
acd_start(struct ata_device *atadev)
{
    struct acd_softc *cdp = atadev->driver;
    struct bio *bp = bioq_first(&cdp->queue);
    u_int32_t lba, lastlba, count;
    int8_t ccb[16];
    int track, blocksize;

    if (cdp->changer_info) {
	int i;

	cdp = cdp->driver[cdp->changer_info->current_slot];
	bp = bioq_first(&cdp->queue);

	/* check for work pending on any other slot */
	for (i = 0; i < cdp->changer_info->slots; i++) {
	    if (i == cdp->changer_info->current_slot)
		continue;
	    if (bioq_first(&(cdp->driver[i]->queue))) {
		if (!bp || time_second > (cdp->timestamp + 10)) {
		    acd_select_slot(cdp->driver[i]);
		    return;
		}
	    }
	}
    }
    if (!bp)
	return;
    bioq_remove(&cdp->queue, bp);

    /* reject all queued entries if media changed */
    if (cdp->device->flags & ATA_D_MEDIA_CHANGED) {
	biofinish(bp, NULL, EIO);
	return;
    }

    bzero(ccb, sizeof(ccb));

    track = (bp->bio_dev->si_udev & 0x00ff0000) >> 16;

    if (track) {
	blocksize = (cdp->toc.tab[track - 1].control & 4) ? 2048 : 2352;
	lastlba = ntohl(cdp->toc.tab[track].addr.lba);
	lba = bp->bio_offset / blocksize;
	lba += ntohl(cdp->toc.tab[track - 1].addr.lba);
    }
    else {
	blocksize = cdp->block_size;
	lastlba = cdp->disk_size;
	lba = bp->bio_offset / blocksize;
    }

    if (bp->bio_bcount % blocksize != 0) {
	biofinish(bp, NULL, EINVAL);
	return;
    }
    count = bp->bio_bcount / blocksize;

    if (bp->bio_cmd == BIO_READ) {
	/* if transfer goes beyond range adjust it to be within limits */
	if (lba + count > lastlba) {
	    /* if we are entirely beyond EOM return EOF */
	    if (lastlba <= lba) {
		bp->bio_resid = bp->bio_bcount;
		biodone(bp);
		return;
	    }
	    count = lastlba - lba;
	}
	switch (blocksize) {
	case 2048:
	    ccb[0] = ATAPI_READ_BIG;
	    break;

	case 2352: 
	    ccb[0] = ATAPI_READ_CD;
	    ccb[9] = 0xf8;
	    break;

	default:
	    ccb[0] = ATAPI_READ_CD;
	    ccb[9] = 0x10;
	}
    }
    else 
	ccb[0] = ATAPI_WRITE_BIG;
    
    ccb[1] = 0;
    ccb[2] = lba>>24;
    ccb[3] = lba>>16;
    ccb[4] = lba>>8;
    ccb[5] = lba;
    ccb[6] = count>>16;
    ccb[7] = count>>8;
    ccb[8] = count;

    devstat_start_transaction(cdp->stats);
    bp->bio_caller1 = cdp;
    atapi_queue_cmd(cdp->device, ccb, bp->bio_data, count * blocksize,
		    bp->bio_cmd == BIO_READ ? ATPR_F_READ : 0, 
		    (ccb[0] == ATAPI_WRITE_BIG) ? 60 : 30, acd_done, bp);
}

static int 
acd_done(struct atapi_request *request)
{
    struct bio *bp = request->driver;
    struct acd_softc *cdp = bp->bio_caller1;
    
    if (request->error) {
	bp->bio_error = request->error;
	bp->bio_flags |= BIO_ERROR;
    }	
    else
	bp->bio_resid = bp->bio_bcount - request->donecount;
    biofinish(bp, cdp->stats, 0);
    return 0;
}

static void 
acd_read_toc(struct acd_softc *cdp)
{
    struct acd_devlist *entry;
    int track, ntracks, len;
    u_int32_t sizes[2];
    int8_t ccb[16];

    bzero(&cdp->toc, sizeof(cdp->toc));
    bzero(ccb, sizeof(ccb));

    if (atapi_test_ready(cdp->device) != 0)
	return;

    cdp->device->flags &= ~ATA_D_MEDIA_CHANGED;

    len = sizeof(struct ioc_toc_header) + sizeof(struct cd_toc_entry);
    ccb[0] = ATAPI_READ_TOC;
    ccb[7] = len>>8;
    ccb[8] = len;
    if (atapi_queue_cmd(cdp->device, ccb, (caddr_t)&cdp->toc, len,
			ATPR_F_READ | ATPR_F_QUIET, 30, NULL, NULL)) {
	bzero(&cdp->toc, sizeof(cdp->toc));
	return;
    }
    ntracks = cdp->toc.hdr.ending_track - cdp->toc.hdr.starting_track + 1;
    if (ntracks <= 0 || ntracks > MAXTRK) {
	bzero(&cdp->toc, sizeof(cdp->toc));
	return;
    }

    len = sizeof(struct ioc_toc_header)+(ntracks+1)*sizeof(struct cd_toc_entry);
    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_READ_TOC;
    ccb[7] = len>>8;
    ccb[8] = len;
    if (atapi_queue_cmd(cdp->device, ccb, (caddr_t)&cdp->toc, len,
			ATPR_F_READ | ATPR_F_QUIET, 30, NULL, NULL)) {
	bzero(&cdp->toc, sizeof(cdp->toc));
	return;
    }
    cdp->toc.hdr.len = ntohs(cdp->toc.hdr.len);

    cdp->block_size = (cdp->toc.tab[0].control & 4) ? 2048 : 2352;
    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_READ_CAPACITY;
    if (atapi_queue_cmd(cdp->device, ccb, (caddr_t)sizes, sizeof(sizes),
			ATPR_F_READ | ATPR_F_QUIET, 30, NULL, NULL)) {
	bzero(&cdp->toc, sizeof(cdp->toc));
	return;
    }
    cdp->disk_size = ntohl(sizes[0]) + 1;

    bzero(&cdp->disklabel, sizeof(struct disklabel));
    strncpy(cdp->disklabel.d_typename, "	       ", 
	    sizeof(cdp->disklabel.d_typename));
    strncpy(cdp->disklabel.d_typename, cdp->device->name, 
	    min(strlen(cdp->device->name),sizeof(cdp->disklabel.d_typename)-1));
    strncpy(cdp->disklabel.d_packname, "unknown	       ", 
	    sizeof(cdp->disklabel.d_packname));
    cdp->disklabel.d_secsize = cdp->block_size;
    cdp->disklabel.d_nsectors = 100;
    cdp->disklabel.d_ntracks = 1;
    cdp->disklabel.d_ncylinders = (cdp->disk_size / 100) + 1;
    cdp->disklabel.d_secpercyl = 100;
    cdp->disklabel.d_secperunit = cdp->disk_size;
    cdp->disklabel.d_rpm = 300;
    cdp->disklabel.d_interleave = 1;
    cdp->disklabel.d_flags = D_REMOVABLE;
    cdp->disklabel.d_npartitions = 1;
    cdp->disklabel.d_partitions[0].p_offset = 0;
    cdp->disklabel.d_partitions[0].p_size = cdp->disk_size;
    cdp->disklabel.d_partitions[0].p_fstype = FS_BSDFFS;
    cdp->disklabel.d_magic = DISKMAGIC;
    cdp->disklabel.d_magic2 = DISKMAGIC;
    cdp->disklabel.d_checksum = dkcksum(&cdp->disklabel);

    while ((entry = TAILQ_FIRST(&cdp->dev_list))) {
	destroy_dev(entry->dev);
	TAILQ_REMOVE(&cdp->dev_list, entry, chain);
	free(entry, M_ACD);
    }
    for (track = 1; track <= ntracks; track ++) {
	char name[16];

	sprintf(name, "acd%dt%02d", cdp->lun, track);
	entry = malloc(sizeof(struct acd_devlist), M_ACD, M_NOWAIT | M_ZERO);
	entry->dev = make_dev(&acd_cdevsw, (cdp->lun << 3) | (track << 16),
			      0, 0, 0644, name, NULL);
	entry->dev->si_drv1 = cdp->dev->si_drv1;
	TAILQ_INSERT_TAIL(&cdp->dev_list, entry, chain);
    }

#ifdef ACD_DEBUG
    if (cdp->disk_size && cdp->toc.hdr.ending_track) {
	ata_prtdev(cdp->device, "(%d sectors (%d bytes)), %d tracks ", 
		   cdp->disk_size, cdp->block_size,
		   cdp->toc.hdr.ending_track - cdp->toc.hdr.starting_track + 1);
	if (cdp->toc.tab[0].control & 4)
	    printf("%dMB\n", cdp->disk_size / 512);
	else
	    printf("%d:%d audio\n",
		   cdp->disk_size / 75 / 60, cdp->disk_size / 75 % 60);
    }
#endif
}

static int
acd_play(struct acd_softc *cdp, int start, int end)
{
    int8_t ccb[16];

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_PLAY_MSF;
    lba2msf(start, &ccb[3], &ccb[4], &ccb[5]);
    lba2msf(end, &ccb[6], &ccb[7], &ccb[8]);
    return atapi_queue_cmd(cdp->device, ccb, NULL, 0, 0, 10, NULL, NULL);
}

static int 
acd_setchan(struct acd_softc *cdp,
	    u_int8_t c0, u_int8_t c1, u_int8_t c2, u_int8_t c3)
{
    int error;

    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_AUDIO_PAGE, (caddr_t)&cdp->au, 
				sizeof(cdp->au))))
	return error;
    if (cdp->au.page_code != ATAPI_CDROM_AUDIO_PAGE)
	return EIO;
    cdp->au.data_length = 0;
    cdp->au.port[0].channels = c0;
    cdp->au.port[1].channels = c1;
    cdp->au.port[2].channels = c2;
    cdp->au.port[3].channels = c3;
    return acd_mode_select(cdp, (caddr_t)&cdp->au, sizeof(cdp->au));
}

static int 
acd_select_done1(struct atapi_request *request)
{
    struct acd_softc *cdp = request->driver;

    cdp->changer_info->current_slot = cdp->slot;
    cdp->driver[cdp->changer_info->current_slot]->timestamp = time_second;
    wakeup(&cdp->changer_info);
    return 0;
}

static int 
acd_select_done(struct atapi_request *request)
{
    struct acd_softc *cdp = request->driver;
    int8_t ccb[16] = { ATAPI_LOAD_UNLOAD, 0, 0, 0, 3, 0, 0, 0, 
		       cdp->slot, 0, 0, 0, 0, 0, 0, 0 };

    /* load the wanted slot */
    atapi_queue_cmd(cdp->device, ccb, NULL, 0, ATPR_F_AT_HEAD, 30, 
		    acd_select_done1, cdp);
    return 0;
}

static void 
acd_select_slot(struct acd_softc *cdp)
{
    int8_t ccb[16] = { ATAPI_LOAD_UNLOAD, 0, 0, 0, 2, 0, 0, 0, 
		       cdp->changer_info->current_slot, 0, 0, 0, 0, 0, 0, 0 };

    /* unload the current media from player */
    atapi_queue_cmd(cdp->device, ccb, NULL, 0, ATPR_F_AT_HEAD, 30, 
		    acd_select_done, cdp);
}

static int
acd_init_writer(struct acd_softc *cdp, int test_write)
{
    int8_t ccb[16];

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_REZERO;
    atapi_queue_cmd(cdp->device, ccb, NULL, 0, ATPR_F_QUIET, 60, NULL, NULL);
    ccb[0] = ATAPI_SEND_OPC_INFO;
    ccb[1] = 0x01;
    atapi_queue_cmd(cdp->device, ccb, NULL, 0, ATPR_F_QUIET, 30, NULL, NULL);
    return 0;
}

static int
acd_fixate(struct acd_softc *cdp, int multisession)
{
    int8_t ccb[16] = { ATAPI_CLOSE_TRACK, 0x01, 0x02, 0, 0, 0, 0, 0, 
		       0, 0, 0, 0, 0, 0, 0, 0 };
    int timeout = 5*60*2;
    int error;
    struct write_param param;

    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_WRITE_PARAMETERS_PAGE,
				(caddr_t)&param, sizeof(param))))
	return error;

    param.data_length = 0;
    if (multisession)
	param.session_type = CDR_SESS_MULTI;
    else
	param.session_type = CDR_SESS_NONE;

    if ((error = acd_mode_select(cdp, (caddr_t)&param, param.page_length + 10)))
	return error;
  
    error = atapi_queue_cmd(cdp->device, ccb, NULL, 0, 0, 30, NULL, NULL);
    if (error)
	return error;

    /* some drives just return ready, wait for the expected fixate time */
    if ((error = atapi_test_ready(cdp->device)) != EBUSY) {
	timeout = timeout / (cdp->cap.cur_write_speed / 177);
	tsleep(&error, PRIBIO, "acdfix", timeout * hz / 2);
	return atapi_test_ready(cdp->device);
    }

    while (timeout-- > 0) {
	if ((error = atapi_test_ready(cdp->device)) != EBUSY)
	    return error;
	tsleep(&error, PRIBIO, "acdcld", hz/2);
    }
    return EIO;
}

static int
acd_init_track(struct acd_softc *cdp, struct cdr_track *track)
{
    struct write_param param;
    int error;

    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_WRITE_PARAMETERS_PAGE,
				(caddr_t)&param, sizeof(param))))
	return error;

    param.data_length = 0;
    param.page_code = ATAPI_CDROM_WRITE_PARAMETERS_PAGE;
    param.page_length = 0x32;
    param.test_write = track->test_write ? 1 : 0;
    param.write_type = CDR_WTYPE_TRACK;
    param.session_type = CDR_SESS_NONE;
    param.fp = 0;
    param.packet_size = 0;

    if (cdp->cap.burnproof) 
	param.burnproof = 1;

    switch (track->datablock_type) {

    case CDR_DB_RAW:
	if (track->preemp)
	    param.track_mode = CDR_TMODE_AUDIO_PREEMP;
	else
	    param.track_mode = CDR_TMODE_AUDIO;
	cdp->block_size = 2352;
	param.datablock_type = CDR_DB_RAW;
	param.session_format = CDR_SESS_CDROM;
	break;

    case CDR_DB_ROM_MODE1:
	cdp->block_size = 2048;
	param.track_mode = CDR_TMODE_DATA;
	param.datablock_type = CDR_DB_ROM_MODE1;
	param.session_format = CDR_SESS_CDROM;
	break;

    case CDR_DB_ROM_MODE2:
	cdp->block_size = 2336;
	param.track_mode = CDR_TMODE_DATA;
	param.datablock_type = CDR_DB_ROM_MODE2;
	param.session_format = CDR_SESS_CDROM;
	break;

    case CDR_DB_XA_MODE1:
	cdp->block_size = 2048;
	param.track_mode = CDR_TMODE_DATA;
	param.datablock_type = CDR_DB_XA_MODE1;
	param.session_format = CDR_SESS_CDROM_XA;
	break;

    case CDR_DB_XA_MODE2_F1:
	cdp->block_size = 2056;
	param.track_mode = CDR_TMODE_DATA;
	param.datablock_type = CDR_DB_XA_MODE2_F1;
	param.session_format = CDR_SESS_CDROM_XA;
	break;

    case CDR_DB_XA_MODE2_F2:
	cdp->block_size = 2324;
	param.track_mode = CDR_TMODE_DATA;
	param.datablock_type = CDR_DB_XA_MODE2_F2;
	param.session_format = CDR_SESS_CDROM_XA;
	break;

    case CDR_DB_XA_MODE2_MIX:
	cdp->block_size = 2332;
	param.track_mode = CDR_TMODE_DATA;
	param.datablock_type = CDR_DB_XA_MODE2_MIX;
	param.session_format = CDR_SESS_CDROM_XA;
	break;
    }

    return acd_mode_select(cdp, (caddr_t)&param, param.page_length + 10);
}

static int
acd_flush(struct acd_softc *cdp)
{
    int8_t ccb[16] = { ATAPI_SYNCHRONIZE_CACHE, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->device, ccb, NULL, 0, ATPR_F_QUIET, 60,
			   NULL, NULL);
}

static int
acd_read_track_info(struct acd_softc *cdp,
		    int32_t lba, struct acd_track_info *info)
{
    int8_t ccb[16] = { ATAPI_READ_TRACK_INFO, 1,
		     lba>>24, lba>>16, lba>>8, lba,
		     0,
		     sizeof(*info)>>8, sizeof(*info),
		     0, 0, 0, 0, 0, 0, 0 };
    int error;

    if ((error = atapi_queue_cmd(cdp->device, ccb, (caddr_t)info, sizeof(*info),
				 ATPR_F_READ, 30, NULL, NULL)))
	return error;
    info->track_start_addr = ntohl(info->track_start_addr);
    info->next_writeable_addr = ntohl(info->next_writeable_addr);
    info->free_blocks = ntohl(info->free_blocks);
    info->fixed_packet_size = ntohl(info->fixed_packet_size);
    info->track_length = ntohl(info->track_length);
    return 0;
}

static int
acd_get_progress(struct acd_softc *cdp, int *finished)
{
    int8_t ccb[16] = { ATAPI_READ_CAPACITY, 0, 0, 0, 0, 0, 0, 0,  
		       0, 0, 0, 0, 0, 0, 0, 0 };
    struct atapi_reqsense *sense = cdp->device->result;
    char tmp[8];

    if (atapi_test_ready(cdp->device) != EBUSY) {
	if (atapi_queue_cmd(cdp->device, ccb, tmp, sizeof(tmp),
			    ATPR_F_READ, 30, NULL, NULL) != EBUSY) {
	    *finished = 100;
	    return 0;
	}
    }
    if (sense->sksv)
	*finished = 
	    ((sense->sk_specific2 | (sense->sk_specific1 << 8)) * 100) / 65535;
    else
	*finished = 0;
    return 0;
}

static int
acd_send_cue(struct acd_softc *cdp, struct cdr_cuesheet *cuesheet)
{
    struct write_param param;
    int8_t ccb[16] = { ATAPI_SEND_CUE_SHEET, 0, 0, 0, 0, 0, 
		       cuesheet->len>>16, cuesheet->len>>8, cuesheet->len,
		       0, 0, 0, 0, 0, 0, 0 };
    int8_t *buffer;
    int32_t error;
#ifdef ACD_DEBUG
    int i;
#endif

    if ((error = acd_mode_sense(cdp, ATAPI_CDROM_WRITE_PARAMETERS_PAGE,
				(caddr_t)&param, sizeof(param))))
	return error;
    param.data_length = 0;
    param.page_code = ATAPI_CDROM_WRITE_PARAMETERS_PAGE;
    param.page_length = 0x32;
    param.test_write = cuesheet->test_write ? 1 : 0;
    param.write_type = CDR_WTYPE_SESSION;
    param.session_type = cuesheet->session_type;
    param.fp = 0;
    param.packet_size = 0;
    param.track_mode = CDR_TMODE_AUDIO;
    param.datablock_type = CDR_DB_RAW;
    param.session_format = cuesheet->session_format;
    if (cdp->cap.burnproof) 
	param.burnproof = 1;
    if ((error = acd_mode_select(cdp, (caddr_t)&param, param.page_length + 10)))
	return error;

    buffer = malloc(cuesheet->len, M_ACD, M_NOWAIT);
    if (!buffer)
	return ENOMEM;
    if ((error = copyin(cuesheet->entries, buffer, cuesheet->len)))
	return error;
#ifdef ACD_DEBUG
    printf("acd: cuesheet lenght = %d\n", cuesheet->len);
    for (i=0; i<cuesheet->len; i++)
	if (i%8)
	    printf(" %02x", buffer[i]);
	else
	    printf("\n%02x", buffer[i]);
    printf("\n");
#endif
    error = atapi_queue_cmd(cdp->device, ccb, buffer, cuesheet->len, 0,
			    30, NULL, NULL);
    free(buffer, M_ACD);
    return error;
}

static int
acd_report_key(struct acd_softc *cdp, struct dvd_authinfo *ai)
{
    struct dvd_miscauth *d;
    u_int32_t lba = 0;
    int16_t length;
    int8_t ccb[16];
    int error;

    switch (ai->format) {
    case DVD_REPORT_AGID:
    case DVD_REPORT_ASF:
    case DVD_REPORT_RPC:
	length = 8;
	break;
    case DVD_REPORT_KEY1:
	length = 12;
	break;
    case DVD_REPORT_TITLE_KEY:
	length = 12;
	lba = ai->lba;
	break;
    case DVD_REPORT_CHALLENGE:
	length = 16;
	break;
    case DVD_INVALIDATE_AGID:
	length = 0;
	break;
    default:
	return EINVAL;
    }

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_REPORT_KEY;
    ccb[2] = (lba >> 24) & 0xff;
    ccb[3] = (lba >> 16) & 0xff;
    ccb[4] = (lba >> 8) & 0xff;
    ccb[5] = lba & 0xff;
    ccb[8] = (length >> 8) & 0xff;
    ccb[9] = length & 0xff;
    ccb[10] = (ai->agid << 6) | ai->format;

    d = malloc(length, M_ACD, M_NOWAIT | M_ZERO);
    d->length = htons(length - 2);

    error = atapi_queue_cmd(cdp->device, ccb, (caddr_t)d, length,
			    ai->format == DVD_INVALIDATE_AGID ? 0 : ATPR_F_READ,
			    10, NULL, NULL);
    if (error) {
	free(d, M_ACD);
	return error;
    }

    switch (ai->format) {
    case DVD_REPORT_AGID:
	ai->agid = d->data[3] >> 6;
	break;
    
    case DVD_REPORT_CHALLENGE:
	bcopy(&d->data[0], &ai->keychal[0], 10);
	break;
    
    case DVD_REPORT_KEY1:
	bcopy(&d->data[0], &ai->keychal[0], 5);
	break;
    
    case DVD_REPORT_TITLE_KEY:
	ai->cpm = (d->data[0] >> 7);
	ai->cp_sec = (d->data[0] >> 6) & 0x1;
	ai->cgms = (d->data[0] >> 4) & 0x3;
	bcopy(&d->data[1], &ai->keychal[0], 5);
	break;
    
    case DVD_REPORT_ASF:
	ai->asf = d->data[3] & 1;
	break;
    
    case DVD_REPORT_RPC:
	ai->reg_type = (d->data[0] >> 6);
	ai->vend_rsts = (d->data[0] >> 3) & 0x7;
	ai->user_rsts = d->data[0] & 0x7;
	ai->region = d->data[1];
	ai->rpc_scheme = d->data[2];
	break;
    
    case DVD_INVALIDATE_AGID:
	break;

    default:
	error = EINVAL;
    }
    free(d, M_ACD);
    return error;
}

static int
acd_send_key(struct acd_softc *cdp, struct dvd_authinfo *ai)
{
    struct dvd_miscauth *d;
    int16_t length;
    int8_t ccb[16];
    int error;

    switch (ai->format) {
    case DVD_SEND_CHALLENGE:
	length = 16;
	d = malloc(length, M_ACD, M_NOWAIT | M_ZERO);
	bcopy(ai->keychal, &d->data[0], 10);
	break;

    case DVD_SEND_KEY2:
	length = 12;
	d = malloc(length, M_ACD, M_NOWAIT | M_ZERO);
	bcopy(&ai->keychal[0], &d->data[0], 5);
	break;
    
    case DVD_SEND_RPC:
	length = 8;
	d = malloc(length, M_ACD, M_NOWAIT | M_ZERO);
	d->data[0] = ai->region;
	break;

    default:
	return EINVAL;
    }

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_SEND_KEY;
    ccb[8] = (length >> 8) & 0xff;
    ccb[9] = length & 0xff;
    ccb[10] = (ai->agid << 6) | ai->format;
    d->length = htons(length - 2);
    error = atapi_queue_cmd(cdp->device, ccb, (caddr_t)d, length, 0,
			    10, NULL, NULL);
    free(d, M_ACD);
    return error;
}

static int
acd_read_structure(struct acd_softc *cdp, struct dvd_struct *s)
{
    struct dvd_miscauth *d;
    u_int16_t length;
    int8_t ccb[16];
    int error = 0;

    switch(s->format) {
    case DVD_STRUCT_PHYSICAL:
	length = 21;
	break;

    case DVD_STRUCT_COPYRIGHT:
	length = 8;
	break;

    case DVD_STRUCT_DISCKEY:
	length = 2052;
	break;

    case DVD_STRUCT_BCA:
	length = 192;
	break;

    case DVD_STRUCT_MANUFACT:
	length = 2052;
	break;

    case DVD_STRUCT_DDS:
    case DVD_STRUCT_PRERECORDED:
    case DVD_STRUCT_UNIQUEID:
    case DVD_STRUCT_LIST:
    case DVD_STRUCT_CMI:
    case DVD_STRUCT_RMD_LAST:
    case DVD_STRUCT_RMD_RMA:
    case DVD_STRUCT_DCB:
	return ENOSYS;

    default:
	return EINVAL;
    }

    d = malloc(length, M_ACD, M_NOWAIT | M_ZERO);
    d->length = htons(length - 2);
	
    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_READ_STRUCTURE;
    ccb[6] = s->layer_num;
    ccb[7] = s->format;
    ccb[8] = (length >> 8) & 0xff;
    ccb[9] = length & 0xff;
    ccb[10] = s->agid << 6;
    error = atapi_queue_cmd(cdp->device, ccb, (caddr_t)d, length, ATPR_F_READ,
			    30, NULL, NULL);
    if (error) {
	free(d, M_ACD);
	return error;
    }

    switch (s->format) {
    case DVD_STRUCT_PHYSICAL: {
	struct dvd_layer *layer = (struct dvd_layer *)&s->data[0];

	layer->book_type = d->data[0] >> 4;
	layer->book_version = d->data[0] & 0xf;
	layer->disc_size = d->data[1] >> 4;
	layer->max_rate = d->data[1] & 0xf;
	layer->nlayers = (d->data[2] >> 5) & 3;
	layer->track_path = (d->data[2] >> 4) & 1;
	layer->layer_type = d->data[2] & 0xf;
	layer->linear_density = d->data[3] >> 4;
	layer->track_density = d->data[3] & 0xf;
	layer->start_sector = d->data[5] << 16 | d->data[6] << 8 | d->data[7];
	layer->end_sector = d->data[9] << 16 | d->data[10] << 8 | d->data[11];
	layer->end_sector_l0 = d->data[13] << 16 | d->data[14] << 8|d->data[15];
	layer->bca = d->data[16] >> 7;
	break;
    }

    case DVD_STRUCT_COPYRIGHT:
	s->cpst = d->data[0];
	s->rmi = d->data[0];
	break;

    case DVD_STRUCT_DISCKEY:
	bcopy(&d->data[0], &s->data[0], 2048);
	break;

    case DVD_STRUCT_BCA:
	s->length = ntohs(d->length);
	bcopy(&d->data[0], &s->data[0], s->length);
	break;

    case DVD_STRUCT_MANUFACT:
	s->length = ntohs(d->length);
	bcopy(&d->data[0], &s->data[0], s->length);
	break;
		
    default:
	error = EINVAL;
    }
    free(d, M_ACD);
    return error;
}

static int 
acd_eject(struct acd_softc *cdp, int close)
{
    int error;

    if ((error = acd_start_stop(cdp, 0)) == EBUSY) {
	if (!close)
	    return 0;
	if ((error = acd_start_stop(cdp, 3)))
	    return error;
	acd_read_toc(cdp);
	acd_prevent_allow(cdp, 1);
	cdp->flags |= F_LOCKED;
	return 0;
    }
    if (error)
	return error;
    if (close)
	return 0;
    acd_prevent_allow(cdp, 0);
    cdp->flags &= ~F_LOCKED;
    cdp->device->flags |= ATA_D_MEDIA_CHANGED;
    return acd_start_stop(cdp, 2);
}

static int
acd_blank(struct acd_softc *cdp, int blanktype)
{
    int8_t ccb[16] = { ATAPI_BLANK, 0x10 | (blanktype & 0x7), 0, 0, 0, 0, 0, 0, 
		       0, 0, 0, 0, 0, 0, 0, 0 };

    cdp->device->flags |= ATA_D_MEDIA_CHANGED;
    return atapi_queue_cmd(cdp->device, ccb, NULL, 0, 0, 30, NULL, NULL);
}

static int
acd_prevent_allow(struct acd_softc *cdp, int lock)
{
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->device, ccb, NULL, 0, 0, 30, NULL, NULL);
}

static int
acd_start_stop(struct acd_softc *cdp, int start)
{
    int8_t ccb[16] = { ATAPI_START_STOP, 0, 0, 0, start,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->device, ccb, NULL, 0, 0, 30, NULL, NULL);
}

static int
acd_pause_resume(struct acd_softc *cdp, int pause)
{
    int8_t ccb[16] = { ATAPI_PAUSE, 0, 0, 0, 0, 0, 0, 0, pause,
		       0, 0, 0, 0, 0, 0, 0 };

    return atapi_queue_cmd(cdp->device, ccb, NULL, 0, 0, 30, NULL, NULL);
}

static int
acd_mode_sense(struct acd_softc *cdp, int page, caddr_t pagebuf, int pagesize)
{
    int8_t ccb[16] = { ATAPI_MODE_SENSE_BIG, 0, page, 0, 0, 0, 0,
		       pagesize>>8, pagesize, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    error = atapi_queue_cmd(cdp->device, ccb, pagebuf, pagesize, ATPR_F_READ,
			    10, NULL, NULL);
#ifdef ACD_DEBUG
    atapi_dump("acd: mode sense ", pagebuf, pagesize);
#endif
    return error;
}

static int
acd_mode_select(struct acd_softc *cdp, caddr_t pagebuf, int pagesize)
{
    int8_t ccb[16] = { ATAPI_MODE_SELECT_BIG, 0x10, 0, 0, 0, 0, 0,
		     pagesize>>8, pagesize, 0, 0, 0, 0, 0, 0, 0 };

#ifdef ACD_DEBUG
    ata_prtdev(cdp->device,
	       "modeselect pagesize=%d\n", pagesize);
    atapi_dump("mode select ", pagebuf, pagesize);
#endif
    return atapi_queue_cmd(cdp->device, ccb, pagebuf, pagesize, 0,
			   30, NULL, NULL);
}

static int
acd_set_speed(struct acd_softc *cdp, int rdspeed, int wrspeed)
{
    int8_t ccb[16] = { ATAPI_SET_SPEED, 0, rdspeed >> 8, rdspeed, 
		       wrspeed >> 8, wrspeed, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    error = atapi_queue_cmd(cdp->device, ccb, NULL, 0, 0, 30, NULL, NULL);
    if (!error)
	acd_get_cap(cdp);
    return error;
}

static void
acd_get_cap(struct acd_softc *cdp)
{
    int retry = 5;

    /* get drive capabilities, some drives needs this repeated */
    while (retry-- && acd_mode_sense(cdp, ATAPI_CDROM_CAP_PAGE,
				     (caddr_t)&cdp->cap, sizeof(cdp->cap)))

    cdp->cap.max_read_speed = ntohs(cdp->cap.max_read_speed);
    cdp->cap.cur_read_speed = ntohs(cdp->cap.cur_read_speed);
    cdp->cap.max_write_speed = ntohs(cdp->cap.max_write_speed);
    cdp->cap.cur_write_speed = max(ntohs(cdp->cap.cur_write_speed), 177);
    cdp->cap.max_vol_levels = ntohs(cdp->cap.max_vol_levels);
    cdp->cap.buf_size = ntohs(cdp->cap.buf_size);
}
