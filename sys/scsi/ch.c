/*
 * Written by grefen@convex.com (probably moved by now)
 * Based on scsi drivers by Julian Elischer (julian@tfs.com)
 *
 *      $Id: ch.c,v 1.31 1996/03/10 18:17:53 davidg Exp $
 */

#include	<sys/types.h>
#include	<ch.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/chio.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/devconf.h>
#ifdef DEVFS
#include <sys/devfsext.h>
#endif /*DEVFS*/

#include <scsi/scsi_all.h>
#include <scsi/scsi_changer.h>
#include <scsi/scsiconf.h>

#define	CHRETRIES	2

#define CHUNIT(DEV)      ((minor(DEV)&0xF0) >> 4)    /* 4 bit unit.  */
#define CHSETUNIT(DEV, U) makedev(major(DEV), ((U) << 4))

#define MODE(z)		(  (minor(z) & 0x0F) )

#define ESUCCESS 0

struct scsi_data {
	u_int32_t flags;
	u_int16_t chmo;			/* Offset of first CHM */
	u_int16_t chms;			/* No. of CHM */
	u_int16_t slots;			/* No. of Storage Elements */
	u_int16_t sloto;			/* Offset of first SE */
	u_int16_t imexs;			/* No. of Import/Export Slots */
	u_int16_t imexo;			/* Offset of first IM/EX */
	u_int16_t drives;			/* No. of CTS */
	u_int16_t driveo;			/* Offset of first CTS */
	u_int16_t rot;			/* CHM can rotate */
	u_long    op_matrix;		/* possible opertaions */
	u_int16_t lsterr;			/* details of lasterror */
	u_char    stor;			/* posible Storage locations */
#ifdef	DEVFS
	void	  *c_devfs_token;
	void	  *ctl_devfs_token;
#endif
};

static errval ch_getelem __P((u_int32_t unit, short *stat, int type, u_int32_t from,
			      void *data, u_int32_t flags));
static errval ch_move __P((u_int32_t unit, short *stat, u_int32_t chm, u_int32_t from,
			   u_int32_t to, u_int32_t flags));
static errval ch_mode_sense __P((u_int32_t unit, u_int32_t flags));
static errval ch_position __P((u_int32_t unit, short *stat, u_int32_t chm,
			       u_int32_t to, u_int32_t flags));

static int chunit(dev_t dev) { return CHUNIT(dev); }
static dev_t chsetunit(dev_t dev, int unit) { return CHSETUNIT(dev, unit); }

static errval ch_open(dev_t dev, int flags, int fmt, struct proc *p,
		      struct scsi_link *sc_link);
static errval ch_ioctl(dev_t dev, int cmd, caddr_t addr, int flag,
		       struct proc *p, struct scsi_link *sc_link);
static errval ch_close(dev_t dev, int flag, int fmt, struct proc *p,
		       struct scsi_link *sc_link);

static	d_open_t	chopen;
static	d_close_t	chclose;
static	d_ioctl_t	chioctl;

#define CDEV_MAJOR 17
static struct cdevsw ch_cdevsw = 
	{ chopen,	chclose,	noread,		nowrite,	/*17*/
	  chioctl,	nostop,		nullreset,	nodevtotty,/* ch */
	  noselect,	nommap,		nostrat,	"ch",	NULL,	-1 };

SCSI_DEVICE_ENTRIES(ch)

static struct scsi_device ch_switch =
{
    NULL,
    NULL,
    NULL,
    NULL,
    "ch",
    0,
	{0, 0},
	0,				/* Link flags */
	chattach,
	"Medium-Changer",
	chopen,
    sizeof(struct scsi_data),
	T_CHANGER,
	chunit,
	chsetunit,
	ch_open,
	ch_ioctl,
	ch_close,
	0,
};

#define CH_OPEN		0x01

static int
ch_externalize(struct kern_devconf *kdc, struct sysctl_req *req)
{
	return scsi_externalize(SCSI_LINK(&ch_switch, kdc->kdc_unit), req);
}

static struct kern_devconf kdc_ch_template = {
	0, 0, 0,		/* filled in by dev_attach */
	"ch", 0, MDDC_SCSI,
	ch_externalize, 0, scsi_goaway, SCSI_EXTERNALLEN,
	&kdc_scbus0,		/* parent */
	0,			/* parentdata */
	DC_UNKNOWN,		/* not supported */
};

static inline void
ch_registerdev(int unit)
{
	struct kern_devconf *kdc;

	MALLOC(kdc, struct kern_devconf *, sizeof *kdc, M_TEMP, M_NOWAIT);
	if(!kdc) return;
	*kdc = kdc_ch_template;
	kdc->kdc_unit = unit;
	kdc->kdc_description = ch_switch.desc;
	dev_attach(kdc);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
static errval
chattach(struct scsi_link *sc_link)
{
	u_int32_t unit;

	struct scsi_data *ch = sc_link->sd;

	unit = sc_link->dev_unit;

	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	if ((ch_mode_sense(unit, SCSI_NOSLEEP | SCSI_NOMASK /*| SCSI_SILENT */ ))) {
		printf("offline");
	} else {
		printf("%d slot(s) %d drive(s) %d arm(s) %d i/e-slot(s)",
		    ch->slots, ch->drives, ch->chms, ch->imexs);
	}
	ch_registerdev(unit);

#ifdef DEVFS
	ch->c_devfs_token = devfs_add_devswf(&ch_cdevsw, unit << 4, DV_CHR,
					     UID_ROOT, GID_OPERATOR, 0600,
					     "ch%d", unit);
	ch->ctl_devfs_token = devfs_add_devswf(&ch_cdevsw,
					       (unit << 4) | SCSI_CONTROL_MASK,
					       DV_CHR,
					       UID_ROOT, GID_OPERATOR, 0600,
					       "ch%d.ctl", unit);
#endif
	return 0;
}

/*
 *    open the device.
 */
static errval
ch_open(dev_t dev, int flags, int fmt, struct proc *p,
	struct scsi_link *sc_link)
{
	errval  errcode = 0;
	u_int32_t unit, mode;
	struct scsi_data *cd;

	unit = CHUNIT(dev);
	mode = MODE(dev);

	cd = sc_link->sd;
	/*
	 * Only allow one at a time
	 */
	if (cd->flags & CH_OPEN) {
		printf("ch%ld: already open\n", unit);
		return EBUSY;
	}
	/*
	 * Catch any unit attention errors.
	 */
	scsi_test_unit_ready(sc_link, SCSI_SILENT);

	sc_link->flags |= SDEV_OPEN;
	/*
	 * Check that it is still responding and ok.
	 */
	if ( (errcode = (scsi_test_unit_ready(sc_link, 0))) ) {
		printf("ch%ld: not ready\n", unit);
		sc_link->flags &= ~SDEV_OPEN;
		return errcode;
	}
	/*
	 * Make sure data is loaded
	 */
	if ( (errcode = (ch_mode_sense(unit, SCSI_NOSLEEP | SCSI_NOMASK))) ) {
		printf("ch%ld: scsi changer :- offline\n", unit);
		sc_link->flags &= ~SDEV_OPEN;
		return (errcode);
	}
	cd->flags = CH_OPEN;
	return 0;
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
static errval
ch_close(dev_t dev, int flag, int fmt, struct proc *p,
        struct scsi_link *sc_link)
{
	sc_link->sd->flags = 0;
	sc_link->flags &= ~SDEV_OPEN;
	return (0);
}

/*
 * Perform special action on behalf of the user
 * Knows about the internals of this device
 */
static errval
ch_ioctl(dev_t dev, int cmd, caddr_t arg, int mode, struct proc *p,
    	 struct scsi_link *sc_link)
{
	/* struct ch_cmd_buf *args; */
	unsigned char unit;
	u_int32_t flags;
	errval  ret;
	struct scsi_data *cd;

	/*
	 * Find the device that the user is talking about
	 */
	flags = 0;		/* give error messages, act on errors etc. */
	unit = CHUNIT(dev);
	cd = sc_link->sd;

	switch ((int)cmd) {
	case CHIOOP:{
			struct chop *ch = (struct chop *) arg;
			SC_DEBUG(sc_link, SDEV_DB2,
			    ("[chtape_chop: %x]\n", ch->ch_op));

			switch ((short) (ch->ch_op)) {
			case CHGETPARAM:
				ch->u.getparam.chmo = cd->chmo;
				ch->u.getparam.chms = cd->chms;
				ch->u.getparam.sloto = cd->sloto;
				ch->u.getparam.slots = cd->slots;
				ch->u.getparam.imexo = cd->imexo;
				ch->u.getparam.imexs = cd->imexs;
				ch->u.getparam.driveo = cd->driveo;
				ch->u.getparam.drives = cd->drives;
				ch->u.getparam.rot = cd->rot;
				ch->result = 0;
				return 0;
				break;
			case CHPOSITION:
				return ch_position(unit, &ch->result, ch->u.position.chm,
				    ch->u.position.to,
				    flags);
			case CHMOVE:
				return ch_move(unit, &ch->result, ch->u.position.chm,
				    ch->u.move.from, ch->u.move.to,
				    flags);
			case CHGETELEM:
				return ch_getelem(unit, &ch->result, ch->u.get_elem_stat.type,
				    ch->u.get_elem_stat.from, &ch->u.get_elem_stat.elem_data,
				    flags);
			default:
				return EINVAL;
			}
		}
	default:
		return scsi_do_ioctl(dev, cmd, arg, mode, p, sc_link);
	}
	return (ret ? ESUCCESS : EIO);
}

static errval
ch_getelem(unit, stat, type, from, data, flags)
	u_int32_t unit, from, flags;
	int type;
	short  *stat;
	void   *data;		/* XXX `struct untagged *' - see chio.h */
{
	struct scsi_read_element_status scsi_cmd;
	char    elbuf[32];
	errval  ret;
	struct scsi_link *sc_link;

	if ((sc_link = SCSI_LINK(&ch_switch, unit)) == 0)
		return ENXIO;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_ELEMENT_STATUS;
	scsi_cmd.byte2 = type;
	scsi_cmd.starting_element_addr[0] = (from >> 8) & 0xff;
	scsi_cmd.starting_element_addr[1] = from & 0xff;
	scsi_cmd.number_of_elements[1] = 1;
	scsi_cmd.allocation_length[2] = 32;

	if ((ret = scsi_scsi_cmd(sc_link,
		    (struct scsi_generic *) &scsi_cmd,
		    sizeof(scsi_cmd),
		    (u_char *) elbuf,
		    32,
		    CHRETRIES,
		    100000,
		    NULL,
		    SCSI_DATA_IN | flags) != ESUCCESS)) {
		*stat = sc_link->sd->lsterr;
		bcopy(elbuf + 16, data, 16);
		return ret;
	}
	bcopy(elbuf + 16, data, 16);	/*Just a hack sh */
	return ret;
}

static errval
ch_move(unit, stat, chm, from, to, flags)
	u_int32_t unit, chm, from, to, flags;
	short  *stat;
{
	struct scsi_move_medium scsi_cmd;
	errval  ret;
	struct scsi_link *sc_link;

	if ((sc_link = SCSI_LINK(&ch_switch, unit)) == 0)
		return ENXIO;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MOVE_MEDIUM;
	scsi_cmd.transport_element_address[0] = (chm >> 8) & 0xff;
	scsi_cmd.transport_element_address[1] = chm & 0xff;
	scsi_cmd.source_address[0] = (from >> 8) & 0xff;
	scsi_cmd.source_address[1] = from & 0xff;
	scsi_cmd.destination_address[0] = (to >> 8) & 0xff;
	scsi_cmd.destination_address[1] = to & 0xff;
	scsi_cmd.invert = (chm & CH_INVERT) ? 1 : 0;
	if ((ret = scsi_scsi_cmd(sc_link,
		    (struct scsi_generic *) &scsi_cmd,
		    sizeof(scsi_cmd),
		    NULL,
		    0,
		    CHRETRIES,
		    100000,
		    NULL,
		    flags) != ESUCCESS)) {
		*stat = sc_link->sd->lsterr;
		return ret;
	}
	return ret;
}

static errval
ch_position(unit, stat, chm, to, flags)
	u_int32_t unit, chm, to, flags;
	short  *stat;
{
	struct scsi_position_to_element scsi_cmd;
	errval  ret;
	struct scsi_link *sc_link;

	if ((sc_link = SCSI_LINK(&ch_switch, unit)) == 0)
		return ENXIO;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = POSITION_TO_ELEMENT;
	scsi_cmd.transport_element_address[0] = (chm >> 8) & 0xff;
	scsi_cmd.transport_element_address[1] = chm & 0xff;
	scsi_cmd.source_address[0] = (to >> 8) & 0xff;
	scsi_cmd.source_address[1] = to & 0xff;
	scsi_cmd.invert = (chm & CH_INVERT) ? 1 : 0;
	if ((ret = scsi_scsi_cmd(sc_link,
		    (struct scsi_generic *) &scsi_cmd,
		    sizeof(scsi_cmd),
		    NULL,
		    0,
		    CHRETRIES,
		    100000,
		    NULL,
		    flags) != ESUCCESS)) {
		*stat = sc_link->sd->lsterr;
		return ret;
	}
	return ret;
}

#ifdef	__STDC__
#define b2tol(a)	(((unsigned)(a##_1) << 8) + (unsigned)a##_0 )
#else
#define b2tol(a)	(((unsigned)(a/**/_1) << 8) + (unsigned)a/**/_0 )
#endif

/*
 * Get the scsi driver to send a full inquiry to the
 * device and use the results to fill out the global
 * parameter structure.
 */
static errval
ch_mode_sense(unit, flags)
	u_int32_t unit, flags;
{
	struct scsi_mode_sense scsi_cmd;
	u_char  scsi_sense[128];	/* Can't use scsi_mode_sense_data because of
					 * missing block descriptor
					 */
	u_char *b;
	int32_t   i, l;
	errval  errcode;
	struct scsi_data *cd;
	struct scsi_link *sc_link;

	if ((sc_link = SCSI_LINK(&ch_switch, unit)) == 0)
		return ENXIO;

	cd = sc_link->sd;

	/*
	 * First check if we have it all loaded
	 */
	if (sc_link->flags & SDEV_MEDIA_LOADED)
		return 0;

	/*
	 * First do a mode sense
	 */
	/* sc_link->flags &= ~SDEV_MEDIA_LOADED; *//*XXX */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MODE_SENSE;
	scsi_cmd.byte2 = SMS_DBD;
	scsi_cmd.page = 0x3f;	/* All Pages */
	scsi_cmd.length = sizeof(scsi_sense);

	/*
	 * Read in the pages
	 */
	if ( (errcode = scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(struct scsi_mode_sense),
		        (u_char *) & scsi_sense,
		sizeof  (scsi_sense),
		CHRETRIES,
		5000,
		NULL,
		flags | SCSI_DATA_IN) != 0) ) {
		if (!(flags & SCSI_SILENT))
			printf("ch%ld: could not mode sense\n", unit);
		return (errcode);
	}
	sc_link->flags |= SDEV_MEDIA_LOADED;
	l = scsi_sense[0] - 3;
	b = &scsi_sense[4];

	/*
	 * To avoid alignment problems
	 */
/* XXX - FIX THIS FOR MSB */
#define p2copy(valp)	 (valp[1]+ (valp[0]<<8));valp+=2
#define p4copy(valp)	 (valp[3]+ (valp[2]<<8) + (valp[1]<<16) + (valp[0]<<24));valp+=4
#if 0
	printf("\nmode_sense %d\n", l);
	for (i = 0; i < l + 4; i++) {
		printf("%x%c", scsi_sense[i], i % 8 == 7 ? '\n' : ':');
	} printf("\n");
#endif
	for (i = 0; i < l;) {
		u_int32_t pc = (*b++) & 0x3f;
		u_int32_t pl = *b++;
		u_char *bb = b;
		switch ((int)pc) {
		case 0x1d:
			cd->chmo = p2copy(bb);
			cd->chms = p2copy(bb);
			cd->sloto = p2copy(bb);
			cd->slots = p2copy(bb);
			cd->imexo = p2copy(bb);
			cd->imexs = p2copy(bb);
			cd->driveo = p2copy(bb);
			cd->drives = p2copy(bb);
			break;
		case 0x1e:
			cd->rot = (*b) & 1;
			break;
		case 0x1f:
			cd->stor = *b & 0xf;
			bb += 2;
			cd->stor = p4copy(bb);
			break;
		default:
			break;
		}
		b += pl;
		i += pl + 2;
	}
	SC_DEBUG(sc_link, SDEV_DB2,
	    (" cht(%d-%d)slot(%d-%d)imex(%d-%d)cts(%d-%d) %s rotate\n",
		cd->chmo, cd->chms,
		cd->sloto, cd->slots,
		cd->imexo, cd->imexs,
		cd->driveo, cd->drives,
		cd->rot ? "can" : "can't"));
	return (0);
}


static ch_devsw_installed = 0;

static void 	ch_drvinit(void *unused)
{
	dev_t dev;

	if( ! ch_devsw_installed ) {
		dev = makedev(CDEV_MAJOR, 0);
		cdevsw_add(&dev,&ch_cdevsw, NULL);
		ch_devsw_installed = 1;
    	}
}

SYSINIT(chdev,SI_SUB_DRIVERS,SI_ORDER_MIDDLE+CDEV_MAJOR,ch_drvinit,NULL)


