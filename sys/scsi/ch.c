/* 
 * Written by grefen@?????
 * Based on scsi drivers by Julian Elischer (julian@tfs.com)
 *
 *      $Id: ch.c,v 1.5 1993/11/18 05:02:48 rgrimes Exp $
 */

#include	<sys/types.h>
#include	<ch.h>

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/chio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_changer.h>
#include <scsi/scsiconf.h>

struct scsi_xfer ch_scsi_xfer[NCH];
u_int32 ch_xfer_block_wait[NCH];

#define PAGESIZ 	4096
#define STQSIZE		4
#define	CHRETRIES	2

#define MODE(z)		(  (minor(z) & 0x0F) )
#define UNIT(z)		(  (minor(z) >> 4) )

#define ESUCCESS 0

errval  chattach();

/*
 * This driver is so simple it uses all the default services
 */
struct scsi_device ch_switch =
{
    NULL,
    NULL,
    NULL,
    NULL,
    "ch",
    0,
    0, 0
};

struct ch_data {
	u_int32 flags;
	struct scsi_link *sc_link;	/* all the inter level info */
	u_int16 chmo;			/* Offset of first CHM */
	u_int16 chms;			/* No. of CHM */
	u_int16 slots;			/* No. of Storage Elements */
	u_int16 sloto;			/* Offset of first SE */
	u_int16 imexs;			/* No. of Import/Export Slots */
	u_int16 imexo;			/* Offset of first IM/EX */
	u_int16 drives;			/* No. of CTS */
	u_int16 driveo;			/* Offset of first CTS */
	u_int16 rot;			/* CHM can rotate */
	u_long  op_matrix;		/* possible opertaions */
	u_int16 lsterr;			/* details of lasterror */
	u_char  stor;			/* posible Storage locations */
	u_int32 initialized;
} ch_data[NCH];

#define CH_OPEN		0x01
#define CH_KNOWN	0x02

static u_int32 next_ch_unit = 0;

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
errval 
chattach(sc_link)
	struct scsi_link *sc_link;
{
	u_int32 unit, i, stat;
	unsigned char *tbl;

	SC_DEBUG(sc_link, SDEV_DB2, ("chattach: "));
	/*
	 * Check we have the resources for another drive
	 */
	unit = next_ch_unit++;
	if (unit >= NCH) {
		printf("Too many scsi changers..(%d > %d) reconfigure kernel\n", (unit + 1), NCH);
		return (0);
	}
	/*
	 * Store information needed to contact our base driver
	 */
	ch_data[unit].sc_link = sc_link;
	sc_link->device = &ch_switch;
	sc_link->dev_unit = unit;

	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	if ((ch_mode_sense(unit, SCSI_NOSLEEP | SCSI_NOMASK /*| SCSI_SILENT */ ))) {
		printf("ch%d: scsi changer :- offline\n", unit);
		stat = CH_OPEN;
	} else {
		printf("ch%d: scsi changer, %d slot(s) %d drive(s) %d arm(s) %d i/e-slot(s)\n",
		    unit, ch_data[unit].slots, ch_data[unit].drives, ch_data[unit].chms, ch_data[unit].imexs);
		stat = CH_KNOWN;
	}
	ch_data[unit].initialized = 1;

	return 1;
				/* XXX ??? is this the right return val? */
}

/*
 *    open the device.
 */
errval 
chopen(dev)
	dev_t dev;
{
	errval  errcode = 0;
	u_int32 unit, mode;
	struct scsi_link *sc_link;

	unit = UNIT(dev);
	mode = MODE(dev);

	/*
	 * Check the unit is legal 
	 */
	if (unit >= NCH) {
		printf("ch%d: ch %d  > %d\n", unit, unit, NCH);
		errcode = ENXIO;
		return (errcode);
	}
	/*
	 * Only allow one at a time
	 */
	if (ch_data[unit].flags & CH_OPEN) {
		printf("ch%d: already open\n", unit);
		return ENXIO;
	}
	/*
	 * Make sure the device has been initialised
	 */
	if (!ch_data[unit].initialized)
		return (ENXIO);

	sc_link = ch_data[unit].sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("chopen: dev=0x%x (unit %d (of %d))\n"
		,dev, unit, NCH));
	/*
	 * Catch any unit attention errors.
	 */
	scsi_test_unit_ready(sc_link, SCSI_SILENT);

	sc_link->flags |= SDEV_OPEN;
	/*
	 * Check that it is still responding and ok.
	 */
	if (errcode = (scsi_test_unit_ready(sc_link, 0))) {
		printf("ch%d: not ready\n", unit);
		sc_link->flags &= ~SDEV_OPEN;
		return errcode;
	}
	/*
	 * Make sure data is loaded
	 */
	if (errcode = (ch_mode_sense(unit, SCSI_NOSLEEP | SCSI_NOMASK))) {
		printf("ch%d: scsi changer :- offline\n", unit);
		sc_link->flags &= ~SDEV_OPEN;
		return (errcode);
	}
	ch_data[unit].flags = CH_OPEN;
	return 0;
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
errval 
chclose(dev)
	dev_t dev;
{
	unsigned char unit, mode;
	struct scsi_link *sc_link;

	unit = UNIT(dev);
	mode = MODE(dev);
	sc_link = ch_data[unit].sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("Closing device"));
	ch_data[unit].flags = 0;
	sc_link->flags &= ~SDEV_OPEN;
	return (0);
}

/*
 * Perform special action on behalf of the user
 * Knows about the internals of this device
 */
errval 
chioctl(dev, cmd, arg, mode)
	dev_t   dev;
	u_int32 cmd;
	caddr_t arg;
	int mode;
{
	/* struct ch_cmd_buf *args; */
	union scsi_cmd *scsi_cmd;
	register i, j;
	u_int32 opri;
	errval  errcode = 0;
	unsigned char unit;
	u_int32 number, flags;
	errval  ret;
	struct scsi_link *sc_link;

	/*
	 * Find the device that the user is talking about
	 */
	flags = 0;		/* give error messages, act on errors etc. */
	unit = UNIT(dev);
	sc_link = ch_data[unit].sc_link;

	switch (cmd) {
	case CHIOOP:{
			struct chop *ch = (struct chop *) arg;
			SC_DEBUG(sc_link, SDEV_DB2,
			    ("[chtape_chop: %x]\n", ch->ch_op));

			switch ((short) (ch->ch_op)) {
			case CHGETPARAM:
				ch->u.getparam.chmo = ch_data[unit].chmo;
				ch->u.getparam.chms = ch_data[unit].chms;
				ch->u.getparam.sloto = ch_data[unit].sloto;
				ch->u.getparam.slots = ch_data[unit].slots;
				ch->u.getparam.imexo = ch_data[unit].imexo;
				ch->u.getparam.imexs = ch_data[unit].imexs;
				ch->u.getparam.driveo = ch_data[unit].driveo;
				ch->u.getparam.drives = ch_data[unit].drives;
				ch->u.getparam.rot = ch_data[unit].rot;
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
		return scsi_do_ioctl(sc_link, cmd, arg, mode);
	}
	return (ret ? ESUCCESS : EIO);
}

errval 
ch_getelem(unit, stat, type, from, data, flags)
	u_int32 unit, from, flags;
	int type;
	short  *stat;
	char   *data;
{
	struct scsi_read_element_status scsi_cmd;
	char    elbuf[32];
	errval  ret;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = READ_ELEMENT_STATUS;
	scsi_cmd.byte2 = type;
	scsi_cmd.starting_element_addr[0] = (from >> 8) & 0xff;
	scsi_cmd.starting_element_addr[1] = from & 0xff;
	scsi_cmd.number_of_elements[1] = 1;
	scsi_cmd.allocation_length[2] = 32;

	if ((ret = scsi_scsi_cmd(ch_data[unit].sc_link,
		    (struct scsi_generic *) &scsi_cmd,
		    sizeof(scsi_cmd),
		    (u_char *) elbuf,
		    32,
		    CHRETRIES,
		    100000,
		    NULL,
		    SCSI_DATA_IN | flags) != ESUCCESS)) {
		*stat = ch_data[unit].lsterr;
		bcopy(elbuf + 16, data, 16);
		return ret;
	}
	bcopy(elbuf + 16, data, 16);	/*Just a hack sh */
	return ret;
}

errval 
ch_move(unit, stat, chm, from, to, flags)
	u_int32 unit, chm, from, to, flags;
	short  *stat;
{
	struct scsi_move_medium scsi_cmd;
	errval  ret;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = MOVE_MEDIUM;
	scsi_cmd.transport_element_address[0] = (chm >> 8) & 0xff;
	scsi_cmd.transport_element_address[1] = chm & 0xff;
	scsi_cmd.source_address[0] = (from >> 8) & 0xff;
	scsi_cmd.source_address[1] = from & 0xff;
	scsi_cmd.destination_address[0] = (to >> 8) & 0xff;
	scsi_cmd.destination_address[1] = to & 0xff;
	scsi_cmd.invert = (chm & CH_INVERT) ? 1 : 0;
	if ((ret = scsi_scsi_cmd(ch_data[unit].sc_link,
		    (struct scsi_generic *) &scsi_cmd,
		    sizeof(scsi_cmd),
		    NULL,
		    0,
		    CHRETRIES,
		    100000,
		    NULL,
		    flags) != ESUCCESS)) {
		*stat = ch_data[unit].lsterr;
		return ret;
	}
	return ret;
}

errval 
ch_position(unit, stat, chm, to, flags)
	u_int32 unit, chm, to, flags;
	short  *stat;
{
	struct scsi_position_to_element scsi_cmd;
	errval  ret;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.op_code = POSITION_TO_ELEMENT;
	scsi_cmd.transport_element_address[0] = (chm >> 8) & 0xff;
	scsi_cmd.transport_element_address[1] = chm & 0xff;
	scsi_cmd.source_address[0] = (to >> 8) & 0xff;
	scsi_cmd.source_address[1] = to & 0xff;
	scsi_cmd.invert = (chm & CH_INVERT) ? 1 : 0;
	if ((ret = scsi_scsi_cmd(ch_data[unit].sc_link,
		    (struct scsi_generic *) &scsi_cmd,
		    sizeof(scsi_cmd),
		    NULL,
		    0,
		    CHRETRIES,
		    100000,
		    NULL,
		    flags) != ESUCCESS)) {
		*stat = ch_data[unit].lsterr;
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
errval 
ch_mode_sense(unit, flags)
	u_int32 unit, flags;
{
	struct scsi_mode_sense scsi_cmd;
	u_char  scsi_sense[128];	/* Can't use scsi_mode_sense_data because of
					 * missing block descriptor
					 */
	u_char *b;
	int32   i, l;
	errval  errcode;
	struct scsi_link *sc_link = ch_data[unit].sc_link;

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
	if (errcode = scsi_scsi_cmd(sc_link,
		(struct scsi_generic *) &scsi_cmd,
		sizeof(struct scsi_mode_sense),
		        (u_char *) & scsi_sense,
		sizeof  (scsi_sense),
		CHRETRIES,
		5000,
		NULL,
		flags | SCSI_DATA_IN) != 0) {
		if (!(flags & SCSI_SILENT))
			printf("ch%d: could not mode sense\n", unit);
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
		u_int32 pc = (*b++) & 0x3f;
		u_int32 pl = *b++;
		u_char *bb = b;
		switch (pc) {
		case 0x1d:
			ch_data[unit].chmo = p2copy(bb);
			ch_data[unit].chms = p2copy(bb);
			ch_data[unit].sloto = p2copy(bb);
			ch_data[unit].slots = p2copy(bb);
			ch_data[unit].imexo = p2copy(bb);
			ch_data[unit].imexs = p2copy(bb);
			ch_data[unit].driveo = p2copy(bb);
			ch_data[unit].drives = p2copy(bb);
			break;
		case 0x1e:
			ch_data[unit].rot = (*b) & 1;
			break;
		case 0x1f:
			ch_data[unit].stor = *b & 0xf;
			bb += 2;
			ch_data[unit].stor = p4copy(bb);
			break;
		default:
			break;
		}
		b += pl;
		i += pl + 2;
	}
	SC_DEBUG(sc_link, SDEV_DB2,
	    (" cht(%d-%d)slot(%d-%d)imex(%d-%d)cts(%d-%d) %s rotate\n",
		ch_data[unit].chmo, ch_data[unit].chms,
		ch_data[unit].sloto, ch_data[unit].slots,
		ch_data[unit].imexo, ch_data[unit].imexs,
		ch_data[unit].driveo, ch_data[unit].drives,
		ch_data[unit].rot ? "can" : "can't"));
	return (0);
}
