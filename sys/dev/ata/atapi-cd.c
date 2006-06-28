/*-
 * Copyright (c) 1998 - 2006 Søren Schmidt <sos@FreeBSD.org>
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
#include <sys/module.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/cdio.h>
#include <sys/cdrio.h>
#include <sys/dvdio.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/conf.h>
#include <sys/ctype.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/bus.h>
#include <geom/geom.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/atapi-cd.h>
#include <ata_if.h>

/* prototypes */
static void acd_geom_attach(void *, int);
static void acd_geom_detach(void *, int);
static void acd_set_ioparm(device_t);
static void acd_describe(device_t);
static void lba2msf(u_int32_t, u_int8_t *, u_int8_t *, u_int8_t *);
static u_int32_t msf2lba(u_int8_t, u_int8_t, u_int8_t);
static int acd_geom_access(struct g_provider *, int, int, int);
static g_ioctl_t acd_geom_ioctl;
static void acd_geom_start(struct bio *);
static void acd_strategy(struct bio *);
static void acd_done(struct ata_request *);
static void acd_read_toc(device_t);
static int acd_play(device_t, int, int);
static int acd_setchan(device_t, u_int8_t, u_int8_t, u_int8_t, u_int8_t);
static int acd_init_writer(device_t, int);
static int acd_fixate(device_t, int);
static int acd_init_track(device_t, struct cdr_track *);
static int acd_flush(device_t);
static int acd_read_track_info(device_t, int32_t, struct acd_track_info *);
static int acd_get_progress(device_t, int *);
static int acd_send_cue(device_t, struct cdr_cuesheet *);
static int acd_report_key(device_t, struct dvd_authinfo *);
static int acd_send_key(device_t, struct dvd_authinfo *);
static int acd_read_structure(device_t, struct dvd_struct *);
static int acd_tray(device_t, int);
static int acd_blank(device_t, int);
static int acd_prevent_allow(device_t, int);
static int acd_start_stop(device_t, int);
static int acd_pause_resume(device_t, int);
static int acd_mode_sense(device_t, int, caddr_t, int);
static int acd_mode_select(device_t, caddr_t, int);
static int acd_set_speed(device_t, int, int);
static void acd_get_cap(device_t);
static int acd_read_format_caps(device_t, struct cdr_format_capacities *);
static int acd_format(device_t, struct cdr_format_params *);
static int acd_test_ready(device_t);

/* internal vars */
static MALLOC_DEFINE(M_ACD, "acd_driver", "ATAPI CD driver buffers");
static struct g_class acd_class = {
	.name = "ACD",
	.version = G_VERSION,
	.access = acd_geom_access,
	.ioctl = acd_geom_ioctl,
	.start = acd_geom_start,
};
//DECLARE_GEOM_CLASS(acd_class, acd);

static int
acd_probe(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);

    if ((atadev->param.config & ATA_PROTO_ATAPI) &&
	(atadev->param.config & ATA_ATAPI_TYPE_MASK) == ATA_ATAPI_TYPE_CDROM)
	return 0;
    else
	return ENXIO;
}

static int
acd_attach(device_t dev)
{
    struct acd_softc *cdp;

    if (!(cdp = malloc(sizeof(struct acd_softc), M_ACD, M_NOWAIT | M_ZERO))) {
	device_printf(dev, "out of memory\n");
	return ENOMEM;
    }
    cdp->block_size = 2048;
    device_set_ivars(dev, cdp);
    ATA_SETMODE(device_get_parent(dev), dev);
    ata_controlcmd(dev, ATA_DEVICE_RESET, 0, 0, 0);
    acd_get_cap(dev);
    g_post_event(acd_geom_attach, dev, M_WAITOK, NULL);

    /* announce we are here */
    acd_describe(dev);
    return 0;
}

static int
acd_detach(device_t dev)
{   
    g_waitfor_event(acd_geom_detach, dev, M_WAITOK, NULL);
    return 0;
}

static void
acd_shutdown(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);

    if (atadev->param.support.command2 & ATA_SUPPORT_FLUSHCACHE)
	ata_controlcmd(dev, ATA_FLUSHCACHE, 0, 0, 0);
}

static int
acd_reinit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    struct acd_softc *cdp = device_get_ivars(dev);

    if (((atadev->unit == ATA_MASTER) && !(ch->devices & ATA_ATAPI_MASTER)) ||
	((atadev->unit == ATA_SLAVE) && !(ch->devices & ATA_ATAPI_SLAVE))) {
	device_set_ivars(dev, NULL);
	free(cdp, M_ACD);
	return 1;   
    }
    ATA_SETMODE(device_get_parent(dev), dev);
    return 0;
}

static void
acd_geom_attach(void *arg, int flag)
{
    struct ata_device *atadev = device_get_softc(arg);
    struct acd_softc *cdp = device_get_ivars(arg);
    struct g_geom *gp;
    struct g_provider *pp;

    g_topology_assert();
    gp = g_new_geomf(&acd_class, "acd%d", device_get_unit(arg));
    gp->softc = arg;
    cdp->gp = gp;
    pp = g_new_providerf(gp, "acd%d", device_get_unit(arg));
    pp->index = 0;
    cdp->pp[0] = pp;
    g_error_provider(pp, 0);
    atadev->flags |= ATA_D_MEDIA_CHANGED;
    acd_set_ioparm(arg);
}

static void
acd_geom_detach(void *arg, int flag)
{   
    struct acd_softc *cdp = device_get_ivars(arg);

    /* signal geom so we dont get any further requests */
    g_wither_geom(cdp->gp, ENXIO);

    /* fail requests on the queue and any thats "in flight" for this device */
    ata_fail_requests(arg);

    /* dont leave anything behind */
    device_set_ivars(arg, NULL);
    free(cdp, M_ACD);
}

static int 
acd_geom_ioctl(struct g_provider *pp, u_long cmd, void *addr, int fflag, struct thread *td)
{
    device_t dev = pp->geom->softc;
    struct ata_device *atadev = device_get_softc(dev);
    struct acd_softc *cdp = device_get_ivars(dev);
    int error = 0, nocopyout = 0;

    if (!cdp)
	return ENXIO;

    if (atadev->flags & ATA_D_MEDIA_CHANGED) {
	switch (cmd) {
	case CDIOCRESET:
	    acd_test_ready(dev);
	    break;
	   
	default:
	    acd_read_toc(dev);
	    acd_prevent_allow(dev, 1);
	    cdp->flags |= F_LOCKED;
	    break;
	}
    }

    switch (cmd) {

    case CDIOCRESUME:
	error = acd_pause_resume(dev, 1);
	break;

    case CDIOCPAUSE:
	error = acd_pause_resume(dev, 0);
	break;

    case CDIOCSTART:
	error = acd_start_stop(dev, 1);
	break;

    case CDIOCSTOP:
	error = acd_start_stop(dev, 0);
	break;

    case CDIOCALLOW:
	error = acd_prevent_allow(dev, 0);
	cdp->flags &= ~F_LOCKED;
	break;

    case CDIOCPREVENT:
	error = acd_prevent_allow(dev, 1);
	cdp->flags |= F_LOCKED;
	break;

    case CDIOCRESET:
	error = suser(td);
	if (error)
	    break;
	error = acd_test_ready(dev);
	break;

    case CDIOCEJECT:
	if (pp->acr != 1) {
	    error = EBUSY;
	    break;
	}
	error = acd_tray(dev, 0);
	break;

    case CDIOCCLOSE:
	if (pp->acr != 1)
	    break;
	error = acd_tray(dev, 1);
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

		if (!(toc = malloc(sizeof(struct toc), M_ACD, M_NOWAIT))) {
		    error = ENOMEM;
		    break;
		}
		bcopy(&cdp->toc, toc, sizeof(struct toc));
		entry = toc->tab + (toc->hdr.ending_track + 1 -
			toc->hdr.starting_track) + 1;
		while (--entry >= toc->tab) {
		    lba2msf(ntohl(entry->addr.lba), &entry->addr.msf.minute,
			    &entry->addr.msf.second, &entry->addr.msf.frame);
		    entry->addr_type = CD_MSF_FORMAT;
		}
	    }
	    error = copyout(toc->tab + starting_track - toc->hdr.starting_track,
			    te->data, len);
	    if (te->address_format == CD_MSF_FORMAT)
		free(toc, M_ACD);
	}
	break;

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

		if (!(toc = malloc(sizeof(struct toc), M_ACD, M_NOWAIT))) {
		    error = ENOMEM;
		    break;
		}
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

#if __FreeBSD_version > 600008
    case CDIOCREADSUBCHANNEL_SYSSPACE:
	nocopyout = 1;
	/* FALLTHROUGH */

#endif
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

	    format = args->data_format;
	    if ((format != CD_CURRENT_POSITION) &&
		(format != CD_MEDIA_CATALOG) && (format != CD_TRACK_INFO)) {
		error = EINVAL;
		break;
	    }

	    ccb[1] = args->address_format & CD_MSF_FORMAT;

	    if ((error = ata_atapicmd(dev, ccb, (caddr_t)&cdp->subchan,
				      sizeof(cdp->subchan), ATA_R_READ, 10)))
		break;

	    if ((format == CD_MEDIA_CATALOG) || (format == CD_TRACK_INFO)) {
		if (cdp->subchan.header.audio_status == 0x11) {
		    error = EINVAL;
		    break;
		}

		ccb[3] = format;
		if (format == CD_TRACK_INFO)
		    ccb[6] = args->track;

		if ((error = ata_atapicmd(dev, ccb, (caddr_t)&cdp->subchan, 
					  sizeof(cdp->subchan),ATA_R_READ,10))){
		    break;
		}
	    }
	    if (nocopyout == 0) {
		error = copyout(&cdp->subchan, args->data, args->data_len);
	    } else {
		error = 0;
		bcopy(&cdp->subchan, args->data, args->data_len);
	    }
	}
	break;

    case CDIOCPLAYMSF:
	{
	    struct ioc_play_msf *args = (struct ioc_play_msf *)addr;

	    error = 
		acd_play(dev, 
			 msf2lba(args->start_m, args->start_s, args->start_f),
			 msf2lba(args->end_m, args->end_s, args->end_f));
	}
	break;

    case CDIOCPLAYBLOCKS:
	{
	    struct ioc_play_blocks *args = (struct ioc_play_blocks *)addr;

	    error = acd_play(dev, args->blk, args->blk + args->len);
	}
	break;

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
	    error = acd_play(dev, ntohl(cdp->toc.tab[t1].addr.lba),
			     ntohl(cdp->toc.tab[t2].addr.lba));
	}
	break;

    case CDIOCGETVOL:
	{
	    struct ioc_vol *arg = (struct ioc_vol *)addr;

	    if ((error = acd_mode_sense(dev, ATAPI_CDROM_AUDIO_PAGE,
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
	}
	break;

    case CDIOCSETVOL:
	{
	    struct ioc_vol *arg = (struct ioc_vol *)addr;

	    if ((error = acd_mode_sense(dev, ATAPI_CDROM_AUDIO_PAGE,
					(caddr_t)&cdp->au, sizeof(cdp->au))))
		break;
	    if (cdp->au.page_code != ATAPI_CDROM_AUDIO_PAGE) {
		error = EIO;
		break;
	    }
	    if ((error = acd_mode_sense(dev, ATAPI_CDROM_AUDIO_PAGE_MASK,
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
	    error =  acd_mode_select(dev, (caddr_t)&cdp->au, sizeof(cdp->au));
	}
	break;

    case CDIOCSETPATCH:
	{
	    struct ioc_patch *arg = (struct ioc_patch *)addr;

	    error = acd_setchan(dev, arg->patch[0], arg->patch[1],
				arg->patch[2], arg->patch[3]);
	}
	break;

    case CDIOCSETMONO:
	error = acd_setchan(dev, CHANNEL_0|CHANNEL_1, CHANNEL_0|CHANNEL_1, 0,0);
	break;

    case CDIOCSETSTEREO:
	error = acd_setchan(dev, CHANNEL_0, CHANNEL_1, 0, 0);
	break;

    case CDIOCSETMUTE:
	error = acd_setchan(dev, 0, 0, 0, 0);
	break;

    case CDIOCSETLEFT:
	error = acd_setchan(dev, CHANNEL_0, CHANNEL_0, 0, 0);
	break;

    case CDIOCSETRIGHT:
	error = acd_setchan(dev, CHANNEL_1, CHANNEL_1, 0, 0);
	break;

    case CDRIOCBLANK:
	error = acd_blank(dev, (*(int *)addr));
	break;

    case CDRIOCNEXTWRITEABLEADDR:
	{
	    struct acd_track_info track_info;

	    if ((error = acd_read_track_info(dev, 0xff, &track_info)))
		break;

	    if (!track_info.nwa_valid) {
		error = EINVAL;
		break;
	    }
	    *(int*)addr = track_info.next_writeable_addr;
	}
	break;
 
    case CDRIOCINITWRITER:
	error = acd_init_writer(dev, (*(int *)addr));
	break;

    case CDRIOCINITTRACK:
	error = acd_init_track(dev, (struct cdr_track *)addr);
	break;

    case CDRIOCFLUSH:
	error = acd_flush(dev);
	break;

    case CDRIOCFIXATE:
	error = acd_fixate(dev, (*(int *)addr));
	break;

    case CDRIOCREADSPEED:
	{
	    int speed = *(int *)addr;

	    /* Preserve old behavior: units in multiples of CDROM speed */
	    if (speed < 177)
		speed *= 177;
	    error = acd_set_speed(dev, speed, CDR_MAX_SPEED);
	}
	break;

    case CDRIOCWRITESPEED:
	{
	    int speed = *(int *)addr;

	    if (speed < 177)
		speed *= 177;
	    error = acd_set_speed(dev, CDR_MAX_SPEED, speed);
	}
	break;

    case CDRIOCGETBLOCKSIZE:
	*(int *)addr = cdp->block_size;
	break;

    case CDRIOCSETBLOCKSIZE:
	cdp->block_size = *(int *)addr;
	pp->sectorsize = cdp->block_size;       /* hack for GEOM SOS */
	acd_set_ioparm(dev);
	break;

    case CDRIOCGETPROGRESS:
	error = acd_get_progress(dev, (int *)addr);
	break;

    case CDRIOCSENDCUE:
	error = acd_send_cue(dev, (struct cdr_cuesheet *)addr);
	break;

    case CDRIOCREADFORMATCAPS:
	error = acd_read_format_caps(dev, (struct cdr_format_capacities *)addr);
	break;

    case CDRIOCFORMAT:
	error = acd_format(dev, (struct cdr_format_params *)addr);
	break;

    case DVDIOCREPORTKEY:
	if (cdp->cap.media & MST_READ_DVDROM)
	    error = acd_report_key(dev, (struct dvd_authinfo *)addr);
	else
	    error = EINVAL;
	break;

    case DVDIOCSENDKEY:
	if (cdp->cap.media & MST_READ_DVDROM)
	    error = acd_send_key(dev, (struct dvd_authinfo *)addr);
	else
	    error = EINVAL;
	break;

    case DVDIOCREADSTRUCTURE:
	if (cdp->cap.media & MST_READ_DVDROM)
	    error = acd_read_structure(dev, (struct dvd_struct *)addr);
	else
	    error = EINVAL;
	break;

    default:
	error = ata_device_ioctl(dev, cmd, addr);
    }
    return error;
}

static int
acd_geom_access(struct g_provider *pp, int dr, int dw, int de)
{
    device_t dev = pp->geom->softc;
    struct acd_softc *cdp = device_get_ivars(dev);
    struct ata_request *request;
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int timeout = 60, track;

    if (!(request = ata_alloc_request()))
	return ENOMEM;

    /* wait if drive is not finished loading the medium */
    while (timeout--) {
	bzero(request, sizeof(struct ata_request));
	request->dev = dev;
	bcopy(ccb, request->u.atapi.ccb, 16);
	request->flags = ATA_R_ATAPI;
	request->timeout = 5;
	ata_queue_request(request);
	if (!request->error &&
	    (request->u.atapi.sense.key == 2 ||
	     request->u.atapi.sense.key == 7) &&
	    request->u.atapi.sense.asc == 4 &&
	    request->u.atapi.sense.ascq == 1)
	    tsleep(&timeout, PRIBIO, "acdld", hz / 2);
	else
	    break;
    }
    ata_free_request(request);

    if (pp->acr == 0) {
	acd_prevent_allow(dev, 1);
	cdp->flags |= F_LOCKED;
	acd_read_toc(dev);
    }

    if (dr + pp->acr == 0) {
	acd_prevent_allow(dev, 0);
	cdp->flags &= ~F_LOCKED;
    }

    if ((track = pp->index)) {
	pp->sectorsize = (cdp->toc.tab[track - 1].control & 4) ? 2048 : 2352;
	pp->mediasize = ntohl(cdp->toc.tab[track].addr.lba) -
			ntohl(cdp->toc.tab[track - 1].addr.lba);
    }
    else {
	pp->sectorsize = cdp->block_size;
	pp->mediasize = cdp->disk_size;
    }
    pp->mediasize *= pp->sectorsize;

    return 0;
}

static void 
acd_geom_start(struct bio *bp)
{
    device_t dev = bp->bio_to->geom->softc;
    struct acd_softc *cdp = device_get_ivars(dev);

    if (bp->bio_cmd != BIO_READ && bp->bio_cmd != BIO_WRITE) {
	g_io_deliver(bp, EOPNOTSUPP);
	return;
    }

    if (bp->bio_cmd == BIO_READ && cdp->disk_size == -1) {
	g_io_deliver(bp, EIO);
	return;
    }

    /* GEOM classes must do their own request limiting */
    if (bp->bio_length <= cdp->iomax) {
	bp->bio_pblkno = bp->bio_offset / bp->bio_to->sectorsize;
	acd_strategy(bp);
    }
    else {
	u_int pos, size = cdp->iomax - cdp->iomax % bp->bio_to->sectorsize;
	struct bio *bp2, *bp3;

	if (!(bp2 = g_clone_bio(bp)))
	    g_io_deliver(bp, EIO);

	for (pos = 0; bp2; pos += size) {
	    bp3 = NULL;
	    bp2->bio_done = g_std_done;
	    bp2->bio_to = bp->bio_to;
	    bp2->bio_offset += pos;
	    bp2->bio_data += pos;
	    bp2->bio_length = bp->bio_length - pos;
	    if (bp2->bio_length > size) {
		bp2->bio_length = size;
		if (!(bp3 = g_clone_bio(bp)))
		    bp->bio_error = ENOMEM;
	    }
	    bp2->bio_pblkno = bp2->bio_offset / bp2->bio_to->sectorsize;
	    acd_strategy(bp2);
	    bp2 = bp3;
	}
    }
}

static void 
acd_strategy(struct bio *bp)
{
    device_t dev = bp->bio_to->geom->softc;
    struct ata_device *atadev = device_get_softc(dev);
    struct acd_softc *cdp = device_get_ivars(dev);
    struct ata_request *request;
    u_int32_t lba, lastlba, count;
    int8_t ccb[16];
    int track, blocksize;

    /* reject all queued entries if media changed */
    if (atadev->flags & ATA_D_MEDIA_CHANGED) {
	g_io_deliver(bp, EIO);
	return;
    }

    bzero(ccb, sizeof(ccb));

    track = bp->bio_to->index;

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

    count = bp->bio_length / blocksize;

    if (bp->bio_cmd == BIO_READ) {
	/* if transfer goes beyond range adjust it to be within limits */
	if (lba + count > lastlba) {
	    /* if we are entirely beyond EOM return EOF */
	    if (lastlba <= lba) {
		g_io_deliver(bp, 0);
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

    if (!(request = ata_alloc_request())) {
	g_io_deliver(bp, ENOMEM);
	return;
    }
    request->dev = dev;
    request->bio = bp;
    bcopy(ccb, request->u.atapi.ccb,
	  (atadev->param.config & ATA_PROTO_MASK) == 
	  ATA_PROTO_ATAPI_12 ? 16 : 12);
    request->data = bp->bio_data;
    request->bytecount = count * blocksize;
    request->transfersize = min(request->bytecount, 65534);
    request->timeout = (ccb[0] == ATAPI_WRITE_BIG) ? 60 : 30;
    request->retries = 2;
    request->callback = acd_done;
    request->flags = ATA_R_ATAPI;
    if (atadev->mode >= ATA_DMA)
	request->flags |= ATA_R_DMA;
    switch (bp->bio_cmd) {
    case BIO_READ:
	request->flags |= ATA_R_READ;
	break;
    case BIO_WRITE:
	request->flags |= ATA_R_WRITE;
	break;
    default:
	device_printf(dev, "unknown BIO operation\n");
	ata_free_request(request);
	g_io_deliver(bp, EIO);
	return;
    }
    ata_queue_request(request);
}

static void 
acd_done(struct ata_request *request)
{
    struct bio *bp = request->bio;
    
    /* finish up transfer */
    bp->bio_completed = request->donecount;
    g_io_deliver(bp, request->result);
    ata_free_request(request);
}

static void
acd_set_ioparm(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct acd_softc *cdp = device_get_ivars(dev);

    if (ch->dma)
	cdp->iomax = min(ch->dma->max_iosize, 65534);
    else
	cdp->iomax = min(DFLTPHYS, 65534);
}

static void 
lba2msf(u_int32_t lba, u_int8_t *m, u_int8_t *s, u_int8_t *f)
{
    lba += 150;
    lba &= 0xffffff;
    *m = lba / (60 * 75);
    lba %= (60 * 75);
    *s = lba / 75;
    *f = lba % 75;
}

static u_int32_t 
msf2lba(u_int8_t m, u_int8_t s, u_int8_t f)
{
    return (m * 60 + s) * 75 + f - 150;
}

static void 
acd_read_toc(device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);
    struct acd_softc *cdp = device_get_ivars(dev);
    struct g_provider *pp;
    u_int32_t sizes[2];
    int8_t ccb[16];
    int track, ntracks, len;

    atadev->flags &= ~ATA_D_MEDIA_CHANGED;
    bzero(&cdp->toc, sizeof(cdp->toc));
    cdp->disk_size = -1;                        /* hack for GEOM SOS */

    if (acd_test_ready(dev))
	return;

    bzero(ccb, sizeof(ccb));
    len = sizeof(struct ioc_toc_header) + sizeof(struct cd_toc_entry);
    ccb[0] = ATAPI_READ_TOC;
    ccb[7] = len>>8;
    ccb[8] = len;
    if (ata_atapicmd(dev, ccb, (caddr_t)&cdp->toc, len,
		     ATA_R_READ | ATA_R_QUIET, 30)) {
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
    if (ata_atapicmd(dev, ccb, (caddr_t)&cdp->toc, len,
		     ATA_R_READ | ATA_R_QUIET, 30)) {
	bzero(&cdp->toc, sizeof(cdp->toc));
	return;
    }
    cdp->toc.hdr.len = ntohs(cdp->toc.hdr.len);

    cdp->block_size = (cdp->toc.tab[0].control & 4) ? 2048 : 2352;
    acd_set_ioparm(dev);
    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_READ_CAPACITY;
    if (ata_atapicmd(dev, ccb, (caddr_t)sizes, sizeof(sizes),
		     ATA_R_READ | ATA_R_QUIET, 30)) {
	bzero(&cdp->toc, sizeof(cdp->toc));
	return;
    }
    cdp->disk_size = ntohl(sizes[0]) + 1;

    for (track = 1; track <= ntracks; track ++) {
	if (cdp->pp[track] != NULL)
	    continue;
	pp = g_new_providerf(cdp->gp, "acd%dt%02d", device_get_unit(dev),track);
	pp->index = track;
	cdp->pp[track] = pp;
	g_error_provider(pp, 0);
    }
    for (; track < MAXTRK; track ++) {
	if (cdp->pp[track] == NULL)
	    continue;
	cdp->pp[track]->flags |= G_PF_WITHER;
	g_orphan_provider(cdp->pp[track], ENXIO);
	cdp->pp[track] = NULL;
    }

#ifdef ACD_DEBUG
    if (cdp->disk_size && cdp->toc.hdr.ending_track) {
	device_printf(dev, "(%d sectors (%d bytes)), %d tracks ", 
		      cdp->disk_size, cdp->block_size,
		      cdp->toc.hdr.ending_track-cdp->toc.hdr.starting_track+1);
	if (cdp->toc.tab[0].control & 4)
	    printf("%dMB\n", cdp->disk_size * cdp->block_size / 1048576);
	else
	    printf("%d:%d audio\n",
		   cdp->disk_size / 75 / 60, cdp->disk_size / 75 % 60);
    }
#endif
}

static int
acd_play(device_t dev, int start, int end)
{
    int8_t ccb[16];

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_PLAY_MSF;
    lba2msf(start, &ccb[3], &ccb[4], &ccb[5]);
    lba2msf(end, &ccb[6], &ccb[7], &ccb[8]);
    return ata_atapicmd(dev, ccb, NULL, 0, 0, 10);
}

static int 
acd_setchan(device_t dev, u_int8_t c0, u_int8_t c1, u_int8_t c2, u_int8_t c3)
{
    struct acd_softc *cdp = device_get_ivars(dev);
    int error;

    if ((error = acd_mode_sense(dev, ATAPI_CDROM_AUDIO_PAGE, (caddr_t)&cdp->au, 
				sizeof(cdp->au))))
	return error;
    if (cdp->au.page_code != ATAPI_CDROM_AUDIO_PAGE)
	return EIO;
    cdp->au.data_length = 0;
    cdp->au.port[0].channels = c0;
    cdp->au.port[1].channels = c1;
    cdp->au.port[2].channels = c2;
    cdp->au.port[3].channels = c3;
    return acd_mode_select(dev, (caddr_t)&cdp->au, sizeof(cdp->au));
}

static int
acd_init_writer(device_t dev, int test_write)
{
    int8_t ccb[16];

    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_REZERO;
    ata_atapicmd(dev, ccb, NULL, 0, ATA_R_QUIET, 60);
    ccb[0] = ATAPI_SEND_OPC_INFO;
    ccb[1] = 0x01;
    ata_atapicmd(dev, ccb, NULL, 0, ATA_R_QUIET, 30);
    return 0;
}

static int
acd_fixate(device_t dev, int multisession)
{
    struct acd_softc *cdp = device_get_ivars(dev);
    int8_t ccb[16] = { ATAPI_CLOSE_TRACK, 0x01, 0x02, 0, 0, 0, 0, 0, 
		       0, 0, 0, 0, 0, 0, 0, 0 };
    int timeout = 5*60*2;
    int error, dummy;
    struct write_param param;

    if ((error = acd_mode_sense(dev, ATAPI_CDROM_WRITE_PARAMETERS_PAGE,
				(caddr_t)&param, sizeof(param))))
	return error;

    param.data_length = 0;
    if (multisession)
	param.session_type = CDR_SESS_MULTI;
    else
	param.session_type = CDR_SESS_NONE;

    if ((error = acd_mode_select(dev, (caddr_t)&param, param.page_length + 10)))
	return error;
  
    error = ata_atapicmd(dev, ccb, NULL, 0, 0, 30);
    if (error)
	return error;

    /* some drives just return ready, wait for the expected fixate time */
    if ((error = acd_test_ready(dev)) != EBUSY) {
	timeout = timeout / (cdp->cap.cur_write_speed / 177);
	tsleep(&error, PRIBIO, "acdfix", timeout * hz / 2);
	return acd_test_ready(dev);
    }

    while (timeout-- > 0) {
	if ((error = acd_get_progress(dev, &dummy)))
	    return error;
	if ((error = acd_test_ready(dev)) != EBUSY)
	    return error;
	tsleep(&error, PRIBIO, "acdcld", hz / 2);
    }
    return EIO;
}

static int
acd_init_track(device_t dev, struct cdr_track *track)
{
    struct acd_softc *cdp = device_get_ivars(dev);
    struct write_param param;
    int error;

    if ((error = acd_mode_sense(dev, ATAPI_CDROM_WRITE_PARAMETERS_PAGE,
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

    if (cdp->cap.capabilities & MST_BURNPROOF) 
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
    acd_set_ioparm(dev);
    return acd_mode_select(dev, (caddr_t)&param, param.page_length + 10);
}

static int
acd_flush(device_t dev)
{
    int8_t ccb[16] = { ATAPI_SYNCHRONIZE_CACHE, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(dev, ccb, NULL, 0, ATA_R_QUIET, 60);
}

static int
acd_read_track_info(device_t dev, int32_t lba, struct acd_track_info *info)
{
    int8_t ccb[16] = { ATAPI_READ_TRACK_INFO, 1,
		       lba>>24, lba>>16, lba>>8, lba, 0,
		       sizeof(*info)>>8, sizeof(*info),
		       0, 0, 0, 0, 0, 0, 0 };
    int error;

    if ((error = ata_atapicmd(dev, ccb, (caddr_t)info, sizeof(*info),
			      ATA_R_READ, 30)))
	return error;
    info->track_start_addr = ntohl(info->track_start_addr);
    info->next_writeable_addr = ntohl(info->next_writeable_addr);
    info->free_blocks = ntohl(info->free_blocks);
    info->fixed_packet_size = ntohl(info->fixed_packet_size);
    info->track_length = ntohl(info->track_length);
    return 0;
}

static int
acd_get_progress(device_t dev, int *finished)
{
    int8_t ccb[16] = { ATAPI_READ_CAPACITY, 0, 0, 0, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0 };
    struct ata_request *request;
    int8_t dummy[8];

    if (!(request = ata_alloc_request()))
	return ENOMEM;

    request->dev = dev;
    bcopy(ccb, request->u.atapi.ccb, 16);
    request->data = dummy;
    request->bytecount = sizeof(dummy);
    request->transfersize = min(request->bytecount, 65534);
    request->flags = ATA_R_ATAPI | ATA_R_READ;
    request->timeout = 30;
    ata_queue_request(request);
    if (!request->error && request->u.atapi.sense.error & ATA_SENSE_VALID)
	*finished = ((request->u.atapi.sense.specific2 |
		     (request->u.atapi.sense.specific1 << 8)) * 100) / 65535;
    else
	*finished = 0;
    ata_free_request(request);
    return 0;
}

static int
acd_send_cue(device_t dev, struct cdr_cuesheet *cuesheet)
{
    struct acd_softc *cdp = device_get_ivars(dev);
    struct write_param param;
    int8_t ccb[16] = { ATAPI_SEND_CUE_SHEET, 0, 0, 0, 0, 0, 
		       cuesheet->len>>16, cuesheet->len>>8, cuesheet->len,
		       0, 0, 0, 0, 0, 0, 0 };
    int8_t *buffer;
    int32_t error;

    if ((error = acd_mode_sense(dev, ATAPI_CDROM_WRITE_PARAMETERS_PAGE,
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
    if (cdp->cap.capabilities & MST_BURNPROOF) 
	param.burnproof = 1;

    if ((error = acd_mode_select(dev, (caddr_t)&param, param.page_length + 10)))
	return error;

    if (!(buffer = malloc(cuesheet->len, M_ACD, M_NOWAIT)))
	return ENOMEM;

    if (!(error = copyin(cuesheet->entries, buffer, cuesheet->len)))
	error = ata_atapicmd(dev, ccb, buffer, cuesheet->len, 0, 30);
    free(buffer, M_ACD);
    return error;
}

static int
acd_report_key(device_t dev, struct dvd_authinfo *ai)
{
    struct dvd_miscauth *d = NULL;
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

    if (length) {
	if (!(d = malloc(length, M_ACD, M_NOWAIT | M_ZERO)))
	    return ENOMEM;
	d->length = htons(length - 2);
    }

    error = ata_atapicmd(dev, ccb, (caddr_t)d, length,
			 ai->format == DVD_INVALIDATE_AGID ? 0 : ATA_R_READ,10);
    if (error) {
	if (length)
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
    if (length)
	free(d, M_ACD);
    return error;
}

static int
acd_send_key(device_t dev, struct dvd_authinfo *ai)
{
    struct dvd_miscauth *d;
    int16_t length;
    int8_t ccb[16];
    int error;

    switch (ai->format) {
    case DVD_SEND_CHALLENGE:
	length = 16;
	if (!(d = malloc(length, M_ACD, M_NOWAIT | M_ZERO)))
	    return ENOMEM;
	bcopy(ai->keychal, &d->data[0], 10);
	break;

    case DVD_SEND_KEY2:
	length = 12;
	if (!(d = malloc(length, M_ACD, M_NOWAIT | M_ZERO)))
	    return ENOMEM;
	bcopy(&ai->keychal[0], &d->data[0], 5);
	break;
    
    case DVD_SEND_RPC:
	length = 8;
	if (!(d = malloc(length, M_ACD, M_NOWAIT | M_ZERO)))
	    return ENOMEM;
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
    error = ata_atapicmd(dev, ccb, (caddr_t)d, length, 0, 10);
    free(d, M_ACD);
    return error;
}

static int
acd_read_structure(device_t dev, struct dvd_struct *s)
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

    if (!(d = malloc(length, M_ACD, M_NOWAIT | M_ZERO)))
	return ENOMEM;
    d->length = htons(length - 2);
	
    bzero(ccb, sizeof(ccb));
    ccb[0] = ATAPI_READ_STRUCTURE;
    ccb[6] = s->layer_num;
    ccb[7] = s->format;
    ccb[8] = (length >> 8) & 0xff;
    ccb[9] = length & 0xff;
    ccb[10] = s->agid << 6;
    error = ata_atapicmd(dev, ccb, (caddr_t)d, length, ATA_R_READ, 30);
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
	s->rmi = d->data[1];
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
acd_tray(device_t dev, int close)
{
    struct ata_device *atadev = device_get_softc(dev);
    struct acd_softc *cdp = device_get_ivars(dev);
    int error = ENODEV;

    if (cdp->cap.mechanism & MST_EJECT) {
	if (close) {
	    if (!(error = acd_start_stop(dev, 3))) {
		acd_read_toc(dev);
		acd_prevent_allow(dev, 1);
		cdp->flags |= F_LOCKED;
	    }
	}
	else {
	    acd_start_stop(dev, 0);
	    acd_prevent_allow(dev, 0);
	    cdp->flags &= ~F_LOCKED;
	    atadev->flags |= ATA_D_MEDIA_CHANGED;
	    error = acd_start_stop(dev, 2);
	}
    }
    return error;
}

static int
acd_blank(device_t dev, int blanktype)
{
    struct ata_device *atadev = device_get_softc(dev);
    int8_t ccb[16] = { ATAPI_BLANK, 0x10 | (blanktype & 0x7), 0, 0, 0, 0, 0, 0, 
		       0, 0, 0, 0, 0, 0, 0, 0 };

    atadev->flags |= ATA_D_MEDIA_CHANGED;
    return ata_atapicmd(dev, ccb, NULL, 0, 0, 30);
}

static int
acd_prevent_allow(device_t dev, int lock)
{
    int8_t ccb[16] = { ATAPI_PREVENT_ALLOW, 0, 0, 0, lock,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(dev, ccb, NULL, 0, 0, 30);
}

static int
acd_start_stop(device_t dev, int start)
{
    int8_t ccb[16] = { ATAPI_START_STOP, 0, 0, 0, start,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(dev, ccb, NULL, 0, 0, 30);
}

static int
acd_pause_resume(device_t dev, int pause)
{
    int8_t ccb[16] = { ATAPI_PAUSE, 0, 0, 0, 0, 0, 0, 0, pause,
		       0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(dev, ccb, NULL, 0, 0, 30);
}

static int
acd_mode_sense(device_t dev, int page, caddr_t pagebuf, int pagesize)
{
    int8_t ccb[16] = { ATAPI_MODE_SENSE_BIG, 0, page, 0, 0, 0, 0,
		       pagesize>>8, pagesize, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    error = ata_atapicmd(dev, ccb, pagebuf, pagesize, ATA_R_READ, 10);
    return error;
}

static int
acd_mode_select(device_t dev, caddr_t pagebuf, int pagesize)
{
    int8_t ccb[16] = { ATAPI_MODE_SELECT_BIG, 0x10, 0, 0, 0, 0, 0,
		     pagesize>>8, pagesize, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(dev, ccb, pagebuf, pagesize, 0, 30);
}

static int
acd_set_speed(device_t dev, int rdspeed, int wrspeed)
{
    int8_t ccb[16] = { ATAPI_SET_SPEED, 0, rdspeed >> 8, rdspeed, 
		       wrspeed >> 8, wrspeed, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int error;

    error = ata_atapicmd(dev, ccb, NULL, 0, 0, 30);
    if (!error)
	acd_get_cap(dev);
    return error;
}

static void
acd_get_cap(device_t dev)
{
    struct acd_softc *cdp = device_get_ivars(dev);
    int count;

    /* get drive capabilities, some bugridden drives needs this repeated */
    for (count = 0 ; count < 5 ; count++) {
	if (!acd_mode_sense(dev, ATAPI_CDROM_CAP_PAGE,
			    (caddr_t)&cdp->cap, sizeof(cdp->cap)) &&
			    cdp->cap.page_code == ATAPI_CDROM_CAP_PAGE) {
	    cdp->cap.max_read_speed = ntohs(cdp->cap.max_read_speed);
	    cdp->cap.cur_read_speed = ntohs(cdp->cap.cur_read_speed);
	    cdp->cap.max_write_speed = ntohs(cdp->cap.max_write_speed);
	    cdp->cap.cur_write_speed = max(ntohs(cdp->cap.cur_write_speed),177);
	    cdp->cap.max_vol_levels = ntohs(cdp->cap.max_vol_levels);
	    cdp->cap.buf_size = ntohs(cdp->cap.buf_size);
	}
    }
}

static int
acd_read_format_caps(device_t dev, struct cdr_format_capacities *caps)
{
    int8_t ccb[16] = { ATAPI_READ_FORMAT_CAPACITIES, 0, 0, 0, 0, 0, 0,
		       (sizeof(struct cdr_format_capacities) >> 8) & 0xff,
		       sizeof(struct cdr_format_capacities) & 0xff, 
		       0, 0, 0, 0, 0, 0, 0 };
    
    return ata_atapicmd(dev, ccb, (caddr_t)caps,
			sizeof(struct cdr_format_capacities), ATA_R_READ, 30);
}

static int
acd_format(device_t dev, struct cdr_format_params* params)
{
    int8_t ccb[16] = { ATAPI_FORMAT, 0x11, 0, 0, 0, 0, 0, 0, 0, 0, 
		       0, 0, 0, 0, 0, 0 };
    int error;

    error = ata_atapicmd(dev, ccb, (u_int8_t *)params, 
			 sizeof(struct cdr_format_params), 0, 30);
    return error;
}

static int
acd_test_ready(device_t dev)
{
    int8_t ccb[16] = { ATAPI_TEST_UNIT_READY, 0, 0, 0, 0,
		       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

    return ata_atapicmd(dev, ccb, NULL, 0, 0, 30);
}

static void 
acd_describe(device_t dev)
{
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    struct acd_softc *cdp = device_get_ivars(dev);
    int comma = 0;
    char *mechanism;

    if (bootverbose) {
	device_printf(dev, "<%.40s/%.8s> %s drive at ata%d as %s\n",
		      atadev->param.model, atadev->param.revision,
		      (cdp->cap.media & MST_WRITE_DVDR) ? "DVDR" : 
		       (cdp->cap.media & MST_WRITE_DVDRAM) ? "DVDRAM" : 
			(cdp->cap.media & MST_WRITE_CDRW) ? "CDRW" :
			 (cdp->cap.media & MST_WRITE_CDR) ? "CDR" : 
			  (cdp->cap.media & MST_READ_DVDROM) ? "DVDROM":"CDROM",
		      device_get_unit(ch->dev),
		      (atadev->unit == ATA_MASTER) ? "master" : "slave");

	device_printf(dev, "%s", "");
	if (cdp->cap.cur_read_speed) {
	    printf("read %dKB/s", cdp->cap.cur_read_speed * 1000 / 1024);
	    if (cdp->cap.max_read_speed) 
		printf(" (%dKB/s)", cdp->cap.max_read_speed * 1000 / 1024);
	    if ((cdp->cap.cur_write_speed) &&
		(cdp->cap.media & (MST_WRITE_CDR | MST_WRITE_CDRW |
				   MST_WRITE_DVDR | MST_WRITE_DVDRAM))) {
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
	printf("%s %s\n", comma ? "," : "", ata_mode2str(atadev->mode));

	device_printf(dev, "Reads:");
	comma = 0;
	if (cdp->cap.media & MST_READ_CDR) {
	    printf(" CDR"); comma = 1;
	}
	if (cdp->cap.media & MST_READ_CDRW) {
	    printf("%s CDRW", comma ? "," : ""); comma = 1;
	}
	if (cdp->cap.capabilities & MST_READ_CDDA) {
	    if (cdp->cap.capabilities & MST_CDDA_STREAM)
		printf("%s CDDA stream", comma ? "," : "");
	    else
		printf("%s CDDA", comma ? "," : "");
	    comma = 1;
	}
	if (cdp->cap.media & MST_READ_DVDROM) {
	    printf("%s DVDROM", comma ? "," : ""); comma = 1;
	}
	if (cdp->cap.media & MST_READ_DVDR) {
	    printf("%s DVDR", comma ? "," : ""); comma = 1;
	}
	if (cdp->cap.media & MST_READ_DVDRAM) {
	    printf("%s DVDRAM", comma ? "," : ""); comma = 1;
	}
	if (cdp->cap.media & MST_READ_PACKET)
	    printf("%s packet", comma ? "," : "");

	printf("\n");
	device_printf(dev, "Writes:");
	if (cdp->cap.media & (MST_WRITE_CDR | MST_WRITE_CDRW |
			      MST_WRITE_DVDR | MST_WRITE_DVDRAM)) {
	    comma = 0;
	    if (cdp->cap.media & MST_WRITE_CDR) {
		printf(" CDR" ); comma = 1;
	    }
	    if (cdp->cap.media & MST_WRITE_CDRW) {
		printf("%s CDRW", comma ? "," : ""); comma = 1;
	    }
	    if (cdp->cap.media & MST_WRITE_DVDR) {
		printf("%s DVDR", comma ? "," : ""); comma = 1;
	    }
	    if (cdp->cap.media & MST_WRITE_DVDRAM) {
		printf("%s DVDRAM", comma ? "," : ""); comma = 1; 
	    }
	    if (cdp->cap.media & MST_WRITE_TEST) {
		printf("%s test write", comma ? "," : ""); comma = 1;
	    }
	    if (cdp->cap.capabilities & MST_BURNPROOF)
		printf("%s burnproof", comma ? "," : "");
	}
	printf("\n");
	if (cdp->cap.capabilities & MST_AUDIO_PLAY) {
	    device_printf(dev, "Audio: ");
	    if (cdp->cap.capabilities & MST_AUDIO_PLAY)
		printf("play");
	    if (cdp->cap.max_vol_levels)
		printf(", %d volume levels", cdp->cap.max_vol_levels);
	    printf("\n");
	}
	device_printf(dev, "Mechanism: ");
	switch (cdp->cap.mechanism & MST_MECH_MASK) {
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
	    printf("%s%s", (cdp->cap.mechanism & MST_EJECT) ?
		   "ejectable " : "", mechanism);
	else if (cdp->cap.mechanism & MST_EJECT)
	    printf("ejectable");

	if (cdp->cap.mechanism & MST_LOCKABLE)
	    printf((cdp->cap.mechanism & MST_LOCKED) ? ", locked":", unlocked");
	if (cdp->cap.mechanism & MST_PREVENT)
	    printf(", lock protected");
	printf("\n");

	if ((cdp->cap.mechanism & MST_MECH_MASK) != MST_MECH_CHANGER) {
	    device_printf(dev, "Medium: ");
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
	device_printf(dev, "%s ",
		      (cdp->cap.media & MST_WRITE_DVDR) ? "DVDR" : 
		       (cdp->cap.media & MST_WRITE_DVDRAM) ? "DVDRAM" : 
			(cdp->cap.media & MST_WRITE_CDRW) ? "CDRW" :
			 (cdp->cap.media & MST_WRITE_CDR) ? "CDR" : 
			  (cdp->cap.media & MST_READ_DVDROM) ? "DVDROM" :
			  "CDROM");
	if (cdp->changer_info)
	    printf("with %d CD changer ", cdp->changer_info->slots);
	printf("<%.40s/%.8s> at ata%d-%s %s\n",
	       atadev->param.model, atadev->param.revision,
	       device_get_unit(ch->dev),
	       (atadev->unit == ATA_MASTER) ? "master" : "slave",
	       ata_mode2str(atadev->mode) );
    }
}

static device_method_t acd_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,     acd_probe),
    DEVMETHOD(device_attach,    acd_attach),
    DEVMETHOD(device_detach,    acd_detach),
    DEVMETHOD(device_shutdown,  acd_shutdown),
    
    /* ATA methods */
    DEVMETHOD(ata_reinit,       acd_reinit),
    
    { 0, 0 }
};
    
static driver_t acd_driver = {
    "acd",
    acd_methods,
    0,
};

static devclass_t acd_devclass;

static int
acd_modevent(module_t mod, int what, void *arg)  
{
    g_modevent(0, what, &acd_class);
    return 0;
}
 
DRIVER_MODULE(acd, ata, acd_driver, acd_devclass, acd_modevent, NULL);
MODULE_VERSION(acd, 1);
MODULE_DEPEND(acd, ata, 1, 1, 1);
