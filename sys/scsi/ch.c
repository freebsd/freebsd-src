/*
 * Copyright (c) 1996 Jason R. Thorpe <thorpej@and.com>
 * All rights reserved.
 *
 * Partially based on an autochanger driver written by Stefan Grefen
 * and on an autochanger driver written by the Systems Programming Group
 * at the University of Utah Computer Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgements:
 *	This product includes software developed by Jason R. Thorpe
 *	for And Communications, http://www.and.com/
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $Id: ch.c,v 1.44 1998/04/16 12:28:30 peter Exp $
 */

#include "opt_devfs.h"
#include "opt_bounce.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/chio.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/
#ifdef BOUNCE_BUFFERS
#include <sys/buf.h>
#endif

#include <scsi/scsi_changer.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_driver.h>


#define CHRETRIES	2
#define CHUNIT(x)	(minor((x)))
#define CHSETUNIT(x, y)	makedev(major(x), y)

struct ch_softc {
	/*
	 * Human-readable external name.  FreeBSD doesn't have a
	 * generic hook for this, so we make it look NetBSD-like.  See
	 * comment in chattach().
	 */
	struct {
		char	dv_xname[16];
	} sc_dev;

	/*
	 * Pointer back to the scsi_link.  See comment in chattach().
	 */
	struct scsi_link *sc_link;

	int		sc_picker;	/* current picker */

	/*
	 * The following information is obtained from the
	 * element address assignment page.
	 */
	int		sc_firsts[4];	/* firsts, indexed by CHET_* */
	int		sc_counts[4];	/* counts, indexed by CHET_* */

	/*
	 * The following mask defines the legal combinations
	 * of elements for the MOVE MEDIUM command.
	 */
	u_int8_t	sc_movemask[4];

	/*
	 * As above, but for EXCHANGE MEDIUM.
	 */
	u_int8_t	sc_exchangemask[4];

	int		flags;		/* misc. info */
#ifdef	DEVFS
	void		*c_devfs_token;
	void		*ctl_devfs_token;
#endif
};

/* sc_flags */
#define CHF_ROTATE	0x01		/* picker can rotate */

static	d_open_t	chopen;
static	d_close_t	chclose;
static	d_ioctl_t	chioctl;

#define CDEV_MAJOR 17
static struct cdevsw ch_cdevsw = 
	{ chopen,	chclose,	noread,		nowrite,	/*17*/
	  chioctl,	nostop,		nullreset,	nodevtotty,/* ch */
	  seltrue,	nommap,		nostrat,	"ch",	NULL,	-1 };

/*
 * SCSI glue.
 */

/*
 * Under FreeBSD, this macro sets up a bunch of trampoline
 * functions that indirect through the SCSI subsystem.
 */
SCSI_DEVICE_ENTRIES(ch)

static	int chunit __P((dev_t));
static	dev_t chsetunit __P((dev_t, int));

/* So, like, why not "int"? */
static	errval	ch_devopen __P((dev_t, int, int, struct proc *,
				struct scsi_link *));
static	errval	ch_devioctl __P((dev_t, int, caddr_t, int, struct proc *,
				 struct scsi_link *));
static	errval	ch_devclose __P((dev_t, int, int, struct proc *,
				 struct scsi_link *));

static struct scsi_device ch_switch = {
	NULL,				/* (*err_handler) */
	NULL,				/* (*start) */
	NULL,				/* (*async) */
	NULL,				/* (*done) */
	"ch",				/* name */
	0,				/* flags */
	{ 0, 0 },			/* spare[2] */
	0,				/* link_flags */
	chattach,			/* (*attach) */
	"Medium-Changer",		/* desc */
	chopen,				/* (*open) */
	sizeof(struct ch_softc),	/* sizeof_scsi_data */
	T_CHANGER,			/* type */
	chunit,				/* (*getunit) */
	chsetunit,			/* (*setunit) */
	ch_devopen,			/* (*dev_open) */
	ch_devioctl,			/* (*dev_ioctl) */
	ch_devclose,			/* (*dev_close) */
};

static	int	ch_move __P((struct ch_softc *, struct changer_move *));
static	int	ch_exchange __P((struct ch_softc *, struct changer_exchange *));
static	int	ch_position __P((struct ch_softc *, struct changer_position *));
static	int	ch_usergetelemstatus __P((struct ch_softc *, int, u_int8_t *));
static	int	ch_getelemstatus __P((struct ch_softc *, int, int, caddr_t, size_t));
static	int	ch_get_params __P((struct ch_softc *, int));

static	errval
chattach(link)
	struct scsi_link *link;
{
	struct ch_softc *sc = (struct ch_softc *)(link->sd);
	u_int32_t unit = link->dev_unit;

	/*
	 * FreeBSD doesn't have any common way of carrying
	 * around a device's external name (i.e. <name><unit>),
	 * so emulate the structure used by NetBSD to keep the
	 * diffs lower.
	 */
	bzero(sc->sc_dev.dv_xname, sizeof(sc->sc_dev.dv_xname));
	sprintf(sc->sc_dev.dv_xname, "%s%d", ch_switch.name, unit);

	/*
	 * FreeBSD gets "softc" info for a device from the
	 * scsi_link argument passed to indirect entry point functions.
	 * NetBSD get scsi_link info from softcs that are
	 * obtained from indexes passed to direct entry point functions.
	 * We emulate the NetBSD behavior here to keep the diffs
	 * lower.
	 */
	sc->sc_link = link;

	/*
	 * Get information about the device.  Note we can't use
	 * interrupts yet.
	 */
	if (ch_get_params(sc, SCSI_NOSLEEP|SCSI_NOMASK))
		printf("offline");
	else {
		printf("%d slot%s, %d drive%s, %d picker%s",
		    sc->sc_counts[CHET_ST], (sc->sc_counts[CHET_ST] > 1) ?
		    "s" : "",
		    sc->sc_counts[CHET_DT], (sc->sc_counts[CHET_DT] > 1) ?
		    "s" : "",
		    sc->sc_counts[CHET_MT], (sc->sc_counts[CHET_MT] > 1) ?
		    "s" : "");
		if (sc->sc_counts[CHET_IE])
			printf(", %d portal%s", sc->sc_counts[CHET_IE],
			    (sc->sc_counts[CHET_IE] > 1) ? "s" : "");
		if (bootverbose) {
			printf("\n");	/* This will probably look ugly ... bummer. */
			printf("%s: move mask: 0x%x 0x%x 0x%x 0x%x\n",
			       sc->sc_dev.dv_xname,
			       sc->sc_movemask[CHET_MT], sc->sc_movemask[CHET_ST],
			       sc->sc_movemask[CHET_IE], sc->sc_movemask[CHET_DT]);
			printf("%s: exchange mask: 0x%x 0x%x 0x%x 0x%x\n",
			       sc->sc_dev.dv_xname,
			       sc->sc_exchangemask[CHET_MT], sc->sc_exchangemask[CHET_ST],
			       sc->sc_exchangemask[CHET_IE], sc->sc_exchangemask[CHET_DT]);
		}
	}

	/* Default the current picker. */
	sc->sc_picker = sc->sc_firsts[CHET_MT];

#ifdef DEVFS
	sc->c_devfs_token = devfs_add_devswf(&ch_cdevsw, unit << 4, DV_CHR,
					     UID_ROOT, GID_OPERATOR, 0600,
					     "ch%d", unit);
	sc->ctl_devfs_token = devfs_add_devswf(&ch_cdevsw,
					       (unit << 4) | SCSI_CONTROL_MASK,
					       DV_CHR,
					       UID_ROOT, GID_OPERATOR, 0600,
					       "ch%d.ctl", unit);
#endif
	return (0);
}

static	errval
ch_devopen(dev, flags, fmt, p, link)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
	struct scsi_link *link;
{
	struct ch_softc *sc = (struct ch_softc *)(link->sd);
	errval error = 0;
	int unit;

	unit = CHUNIT(dev);

	/*
	 * Only allow one open at a time.
	 */
	if (link->flags & SDEV_OPEN)
		return (EBUSY);

	link->flags |= SDEV_OPEN;

	/*
	 * Absorb any unit attention errors.  Ignore "not ready"
	 * since this might occur if e.g. a tape isn't actually
	 * loaded in the drive.
	 */
	(void)scsi_test_unit_ready(link, SCSI_SILENT);

	/*
	 * Make sure our parameters are up to date.
	 */
	if (error = ch_get_params(sc, 0))
		goto bad;

	return (0);

 bad:
	link->flags &= ~SDEV_OPEN;
	return (error);
}

static	errval
ch_devclose(dev, flags, fmt, p, link)
	dev_t dev;
	int flags, fmt;
	struct proc *p;
	struct scsi_link *link;
{

	link->flags &= ~SDEV_OPEN;
	return (0);
}

static	errval
ch_devioctl(dev, cmd, data, flags, p, link)
	dev_t dev;
	int cmd;
	caddr_t data;
	int flags;
	struct proc *p;
	struct scsi_link *link;
{
	struct ch_softc *sc = (struct ch_softc *)(link->sd);
	caddr_t elemdata;
	int error = 0;

	switch (cmd) {
	case CHIOMOVE:
		error = ch_move(sc, (struct changer_move *)data);
		break;

	case CHIOEXCHANGE:
		error = ch_exchange(sc, (struct changer_exchange *)data);
		break;

	case CHIOPOSITION:
		error = ch_position(sc, (struct changer_position *)data);
		break;

	case CHIOGPICKER:
		*(int *)data = sc->sc_picker - sc->sc_firsts[CHET_MT];
		break;

	case CHIOSPICKER:	{
		int new_picker = *(int *)data;

		if (new_picker > (sc->sc_counts[CHET_MT] - 1))
			return (EINVAL);
		sc->sc_picker = sc->sc_firsts[CHET_MT] + new_picker;
		break;		}

	case CHIOGPARAMS:	{
		struct changer_params *cp = (struct changer_params *)data;

		cp->cp_curpicker = sc->sc_picker - sc->sc_firsts[CHET_MT];
		cp->cp_npickers = sc->sc_counts[CHET_MT];
		cp->cp_nslots = sc->sc_counts[CHET_ST];
		cp->cp_nportals = sc->sc_counts[CHET_IE];
		cp->cp_ndrives = sc->sc_counts[CHET_DT];
		break;		}

	case CHIOGSTATUS:	{
		struct changer_element_status *ces =
		    (struct changer_element_status *)data;

		error = ch_usergetelemstatus(sc, ces->ces_type, ces->ces_data);
		break;		}

	/* Implement prevent/allow? */

	default:
		error = scsi_do_ioctl(dev, cmd, data, flags, p, link);
		break;
	}

	return (error);
}

static	int
ch_move(sc, cm)
	struct ch_softc *sc;
	struct changer_move *cm;
{
	struct scsi_move_medium cmd;
	u_int16_t fromelem, toelem;

	/*
	 * Check arguments.
	 */
	if ((cm->cm_fromtype > CHET_DT) || (cm->cm_totype > CHET_DT))
		return (EINVAL);
	if ((cm->cm_fromunit > (sc->sc_counts[cm->cm_fromtype] - 1)) ||
	    (cm->cm_tounit > (sc->sc_counts[cm->cm_totype] - 1)))
		return (ENODEV);

	/*
	 * Check the request against the changer's capabilities.
	 */
	if ((sc->sc_movemask[cm->cm_fromtype] & (1 << cm->cm_totype)) == 0)
		return (EINVAL);

	/*
	 * Calculate the source and destination elements.
	 */
	fromelem = sc->sc_firsts[cm->cm_fromtype] + cm->cm_fromunit;
	toelem = sc->sc_firsts[cm->cm_totype] + cm->cm_tounit;

	/*
	 * Build the SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = MOVE_MEDIUM;
	scsi_uto2b(sc->sc_picker, cmd.tea);
	scsi_uto2b(fromelem, cmd.src);
	scsi_uto2b(toelem, cmd.dst);
	if (cm->cm_flags & CM_INVERT)
		cmd.flags |= MOVE_MEDIUM_INVERT;

	/*
	 * Send command to changer.
	 */
	return (scsi_scsi_cmd(sc->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), NULL, 0, CHRETRIES, 100000, NULL, 0));
}

static	int
ch_exchange(sc, ce)
	struct ch_softc *sc;
	struct changer_exchange *ce;
{
	struct scsi_exchange_medium cmd;
	u_int16_t src, dst1, dst2;

	/*
	 * Check arguments.
	 */
	if ((ce->ce_srctype > CHET_DT) || (ce->ce_fdsttype > CHET_DT) ||
	    (ce->ce_sdsttype > CHET_DT))
		return (EINVAL);
	if ((ce->ce_srcunit > (sc->sc_counts[ce->ce_srctype] - 1)) ||
	    (ce->ce_fdstunit > (sc->sc_counts[ce->ce_fdsttype] - 1)) ||
	    (ce->ce_sdstunit > (sc->sc_counts[ce->ce_sdsttype] - 1)))
		return (ENODEV);

	/*
	 * Check the request against the changer's capabilities.
	 */
	if (((sc->sc_exchangemask[ce->ce_srctype] &
	     (1 << ce->ce_fdsttype)) == 0) ||
	    ((sc->sc_exchangemask[ce->ce_fdsttype] &
	     (1 << ce->ce_sdsttype)) == 0))
		return (EINVAL);

	/*
	 * Calculate the source and destination elements.
	 */
	src = sc->sc_firsts[ce->ce_srctype] + ce->ce_srcunit;
	dst1 = sc->sc_firsts[ce->ce_fdsttype] + ce->ce_fdstunit;
	dst2 = sc->sc_firsts[ce->ce_sdsttype] + ce->ce_sdstunit;

	/*
	 * Build the SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = EXCHANGE_MEDIUM;
	scsi_uto2b(sc->sc_picker, cmd.tea);
	scsi_uto2b(src, cmd.src);
	scsi_uto2b(dst1, cmd.fdst);
	scsi_uto2b(dst2, cmd.sdst);
	if (ce->ce_flags & CE_INVERT1)
		cmd.flags |= EXCHANGE_MEDIUM_INV1;
	if (ce->ce_flags & CE_INVERT2)
		cmd.flags |= EXCHANGE_MEDIUM_INV2;

	/*
	 * Send command to changer.
	 */
	return (scsi_scsi_cmd(sc->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), NULL, 0, CHRETRIES, 100000, NULL, 0));
}

static	int
ch_position(sc, cp)
	struct ch_softc *sc;
	struct changer_position *cp;
{
	struct scsi_position_to_element cmd;
	u_int16_t dst;

	/*
	 * Check arguments.
	 */
	if (cp->cp_type > CHET_DT)
		return (EINVAL);
	if (cp->cp_unit > (sc->sc_counts[cp->cp_type] - 1))
		return (ENODEV);

	/*
	 * Calculate the destination element.
	 */
	dst = sc->sc_firsts[cp->cp_type] + cp->cp_unit;

	/*
	 * Build the SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = POSITION_TO_ELEMENT;
	scsi_uto2b(sc->sc_picker, cmd.tea);
	scsi_uto2b(dst, cmd.dst);
	if (cp->cp_flags & CP_INVERT)
		cmd.flags |= POSITION_TO_ELEMENT_INVERT;

	/*
	 * Send command to changer.
	 */
	return (scsi_scsi_cmd(sc->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), NULL, 0, CHRETRIES, 100000, NULL, 0));
}

/*
 * Perform a READ ELEMENT STATUS on behalf of the user, and return to
 * the user only the data the user is interested in (i.e. an array of
 * flags bytes).
 */
static	int
ch_usergetelemstatus(sc, chet, uptr)
	struct ch_softc *sc;
	int chet;
	u_int8_t *uptr;
{
	struct read_element_status_header *st_hdr;
	struct read_element_status_page_header *pg_hdr;
	struct read_element_status_descriptor *desc;
	caddr_t data = NULL;
#ifdef BOUNCE_BUFFERS
	int datasize = 0;
#endif
	size_t size, desclen;
	int avail, i, error = 0;
	u_int8_t *user_data = NULL;

	/*
	 * If there are no elements of the requested type in the changer,
	 * the request is invalid.
	 */
	if (sc->sc_counts[chet] == 0)
		return (EINVAL);

	/*
	 * Request one descriptor for the given element type.  This
	 * is used to determine the size of the descriptor so that
	 * we can allocate enough storage for all of them.  We assume
	 * that the first one can fit into 1k.
	 */
#ifdef BOUNCE_BUFFERS
	data = (caddr_t)vm_bounce_kva_alloc(btoc(1024));
	if (!data)
		return (ENOMEM);
	datasize = 1024;
#else
	data = (caddr_t)malloc(1024, M_DEVBUF, M_WAITOK);
#endif
	if (error = ch_getelemstatus(sc, sc->sc_firsts[chet], 1, data, 1024))
		goto done;

	st_hdr = (struct read_element_status_header *)data;
	pg_hdr = (struct read_element_status_page_header *)((u_long)st_hdr +
	    sizeof(struct read_element_status_header));
	desclen = scsi_2btou(pg_hdr->edl);

	size = sizeof(struct read_element_status_header) +
	    sizeof(struct read_element_status_page_header) +
	    (desclen * sc->sc_counts[chet]);

	/*
	 * Reallocate storage for descriptors and get them from the
	 * device.
	 */
#ifdef BOUNCE_BUFFERS
	vm_bounce_kva_alloc_free((vm_offset_t)data, btoc(datasize));
	data = (caddr_t)vm_bounce_kva_alloc(btoc(size));
	if (!data) {
		error = ENOMEM;
		goto done;
	}
	datasize = size;
#else
	free(data, M_DEVBUF);
	data = (caddr_t)malloc(size, M_DEVBUF, M_WAITOK);
#endif
	if (error = ch_getelemstatus(sc, sc->sc_firsts[chet],
	    sc->sc_counts[chet], data, size))
		goto done;

	/*
	 * Fill in the user status array.
	 */
	st_hdr = (struct read_element_status_header *)data;
	avail = scsi_2btou(st_hdr->count);
	if (avail != sc->sc_counts[chet])
		printf("%s: warning, READ ELEMENT STATUS avail != count\n",
		    sc->sc_dev.dv_xname);

	user_data = (u_int8_t *)malloc(avail, M_DEVBUF, M_WAITOK);

	desc = (struct read_element_status_descriptor *)((u_long)data +
	    sizeof(struct read_element_status_header) +
	    sizeof(struct read_element_status_page_header));
	for (i = 0; i < avail; ++i) {
		user_data[i] = desc->flags1;
		(u_long)desc += desclen;
	}

	/* Copy flags array out to userspace. */
	error = copyout(user_data, uptr, avail);

 done:
#ifdef BOUNCE_BUFFERS
	if (data != NULL)
		vm_bounce_kva_alloc_free((vm_offset_t)data, btoc(datasize));
#else
	if (data != NULL)
		free(data, M_DEVBUF);
#endif
	if (user_data != NULL)
		free(user_data, M_DEVBUF);
	return (error);
}

static	int
ch_getelemstatus(sc, first, count, data, datalen)
	struct ch_softc *sc;
	int first, count;
	caddr_t data;
	size_t datalen;
{
	struct scsi_read_element_status cmd;

	/*
	 * Build SCSI command.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = READ_ELEMENT_STATUS;
	scsi_uto2b(first, cmd.sea);
	scsi_uto2b(count, cmd.count);
	scsi_uto3b(datalen, cmd.len);

	/*
	 * Send command to changer.
	 */
	return (scsi_scsi_cmd(sc->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), (u_char *)data, datalen, CHRETRIES, 100000, NULL, SCSI_DATA_IN));
}


/*
 * Ask the device about itself and fill in the parameters in our
 * softc.
 */
static	int
ch_get_params(sc, scsiflags)
	struct ch_softc *sc;
	int scsiflags;
{
	struct scsi_mode_sense cmd;
	struct scsi_mode_sense_data {
		struct scsi_mode_header header;
		union {
			struct page_element_address_assignment ea;
			struct page_transport_geometry_parameters tg;
			struct page_device_capabilities cap;
		} pages;
	} sense_data;
	int error, from;
	u_int8_t *moves, *exchanges;

	/*
	 * Grab info from the element address assignment page.
	 */
	bzero(&cmd, sizeof(cmd));
	bzero(&sense_data, sizeof(sense_data));
	cmd.op_code = MODE_SENSE;
	cmd.byte2 |= 0x08;	/* disable block descriptors */
	cmd.page = 0x1d;
	cmd.length = (sizeof(sense_data) & 0xff);
	error = scsi_scsi_cmd(sc->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), (u_char *)&sense_data, sizeof(sense_data), CHRETRIES,
	    6000, NULL, scsiflags | SCSI_DATA_IN);
	if (error) {
		printf("%s: could not sense element address page\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	sc->sc_firsts[CHET_MT] = scsi_2btou(sense_data.pages.ea.mtea);
	sc->sc_counts[CHET_MT] = scsi_2btou(sense_data.pages.ea.nmte);
	sc->sc_firsts[CHET_ST] = scsi_2btou(sense_data.pages.ea.fsea);
	sc->sc_counts[CHET_ST] = scsi_2btou(sense_data.pages.ea.nse);
	sc->sc_firsts[CHET_IE] = scsi_2btou(sense_data.pages.ea.fieea);
	sc->sc_counts[CHET_IE] = scsi_2btou(sense_data.pages.ea.niee);
	sc->sc_firsts[CHET_DT] = scsi_2btou(sense_data.pages.ea.fdtea);
	sc->sc_counts[CHET_DT] = scsi_2btou(sense_data.pages.ea.ndte);

	/* XXX ask for page trasport geom */

	/*
	 * Grab info from the capabilities page.
	 */
	bzero(&cmd, sizeof(cmd));
	bzero(&sense_data, sizeof(sense_data));
	cmd.op_code = MODE_SENSE;
	cmd.byte2 |= 0x08;	/* disable block descriptors */
	cmd.page = 0x1f;
	cmd.length = (sizeof(sense_data) & 0xff);
	error = scsi_scsi_cmd(sc->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), (u_char *)&sense_data, sizeof(sense_data), CHRETRIES,
	    6000, NULL, scsiflags | SCSI_DATA_IN);
	if (error) {
		printf("%s: could not sense capabilities page\n",
		    sc->sc_dev.dv_xname);
		return (error);
	}

	bzero(sc->sc_movemask, sizeof(sc->sc_movemask));
	bzero(sc->sc_exchangemask, sizeof(sc->sc_exchangemask));
	moves = &sense_data.pages.cap.move_from_mt;
	exchanges = &sense_data.pages.cap.exchange_with_mt;
	for (from = CHET_MT; from <= CHET_DT; ++from) {
		sc->sc_movemask[from] = moves[from];
		sc->sc_exchangemask[from] = exchanges[from];
	}

	sc->sc_link->flags |= SDEV_MEDIA_LOADED;
	return (0);
}

static int
chunit(dev)
	dev_t dev;
{

	return (CHUNIT(dev));
}

static dev_t
chsetunit(dev, unit)
	dev_t dev;
	int unit;
{

	return (CHSETUNIT(dev, unit));
}


static ch_devsw_installed = 0;

static void
ch_drvinit(void *unused)
{
	dev_t dev;

	if( ! ch_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&ch_cdevsw, NULL);
		ch_devsw_installed = 1;
    	}
}

SYSINIT(chdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ch_drvinit,NULL)
