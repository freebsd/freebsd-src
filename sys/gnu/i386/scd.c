/*-
 * Copyright (c) 1995 Mikael Hybsch
 *
 * Portions of this file are copied from mcd.c
 * which has the following copyrights:
 *
 *	Copyright 1993 by Holger Veit (data part)
 *	Copyright 1993 by Brian Moore (audio part)
 *	Changes Copyright 1993 by Gary Clark II
 *	Changes Copyright (C) 1994 by Andrew A. Chernov
 *
 *	Rewrote probe routine to work on newer Mitsumi drives.
 *	Additional changes (C) 1994 by Jordan K. Hubbard
 *
 * The Linux driver cdu31a has been used as a reference when writing this
 * code, there fore bringing it under the GNU Public License.  The following
 * conditions of redistribution therefore apply:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

/* First released version. Please send any comments to micke@dynas.se */

#define	DEBUG	0	/* 0(no debug), 1(debug), 2(debug on each read) */

#include "scd.h"
#if NSCD > 0
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/buf.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/cdio.h>
#include <sys/errno.h>
#include <sys/dkbad.h>
#include <sys/disklabel.h>
#include <sys/devconf.h>

#include <i386/isa/isa.h>
#include <i386/isa/isa_device.h>
#include <i386/isa/scdreg.h>

#define scd_part(dev)	((minor(dev)) & 7)
#define scd_unit(dev)	(((minor(dev)) & 0x38) >> 3)
#define scd_phys(dev)	(((minor(dev)) & 0x40) >> 6)
#define RAW_PART        2

/* flags */
#define SCDOPEN		0x0001	/* device opened */
#define SCDVALID	0x0002	/* parameters loaded */
#define SCDINIT		0x0004	/* device is init'd */
#define	SCDPROBING	0x0020	/* probing */
#define	SCDTOC		0x0100	/* already read toc */
#define	SCDMBXBSY	0x0200	/* local mbx is busy */
#define	SCDSPINNING	0x0400  /* drive is spun up */

#define SCD_S_BEGIN	0
#define SCD_S_BEGIN1	1
#define SCD_S_WAITSTAT	2
#define	SCD_S_WAITFIFO	3
#define SCD_S_WAITSPIN	4
#define SCD_S_WAITREAD	5
#define	SCD_S_WAITPARAM 6

#define RDELAY_WAIT	300
#define RDELAY_WAITREAD	300

#define	SCDBLKSIZE	2048

struct scd_mbx {
	short		unit;
	short		port;
	short		retry;
	short		nblk;
	int		sz;
	u_long		skip;
	struct buf	*bp;
	int		p_offset;
	short		count;
};

struct scd_data {
	int	iobase;
	char	double_speed;
	char	*name;
	short	flags;
	int	blksize;
	u_long	disksize;
	struct disklabel dlabel;
	int	openflags;
	struct {
		unsigned char start_msf[3];
	} toc[MAX_TRACKS];
	short	first_track;
	short	last_track;
/*	struct s_sony_subcode subcode; */
	
	short	audio_status;
	struct buf head;		/* head of buf queue */
	struct scd_mbx mbx;
} scd_data[NSCD];

/* prototypes */
int	scdopen(dev_t dev);
int	scdclose(dev_t dev);
void	scdstrategy(struct buf *bp);
int	scdioctl(dev_t dev, int cmd, caddr_t addr, int flags);
int	scdsize(dev_t dev);

static	int	bcd2bin(bcd_t b);
static	bcd_t	bin2bcd(int b);
static	void	hsg2msf(int hsg, bcd_t *msf);
static	int	msf2hsg(bcd_t *msf);

static void get_drive_configuration(unsigned short unit,
                        unsigned char res_reg[],
                        unsigned int *res_size);
static int handle_attention(unsigned unit);
static inline void write_control(unsigned port, unsigned data);
static int write_params(unsigned port, unsigned char *params, int num_params);
static int do_cmd(unsigned int unit,
		       unsigned char cmd,
	               unsigned char *params,
	               unsigned int num_params,
	               unsigned char *result_buffer,
	               unsigned int *result_size);
static void set_drive_params(unsigned unit);

static void scd_start(int unit);
static void scd_doread(int state, struct scd_mbx *mbxin);

static int scd_eject(int unit);
static int scd_stop(int unit);
static int scd_playtracks(int unit, struct ioc_play_track *pt);
static int scd_playmsf(int unit, struct ioc_play_msf *msf);
static int scd_subchan(int unit, struct ioc_read_subchannel *sc);
static int read_subcode(int unit, struct s_sony_subcode *sc);

extern	int	hz;

int	scd_probe(struct isa_device *dev);
int	scd_attach(struct isa_device *dev);
struct	isa_driver	scddriver = { scd_probe, scd_attach, "scd" };

static struct kern_devconf kdc_scd[NSCD] = { {
	0, 0, 0,		/* filled in by dev_attach */
	"scd", 0, { MDDT_ISA, 0, "bio" },
	isa_generic_externalize, 0, 0, ISA_EXTERNALLEN,
	&kdc_isa0,		/* parent */
	0,			/* parentdata */
	DC_IDLE,		/* status */
	"Sony CD-ROM drive"     /* properly filled later */
} };

static inline void
scd_registerdev(struct isa_device *id)
{
	if(id->id_unit)
		kdc_scd[id->id_unit] = kdc_scd[0];
	kdc_scd[id->id_unit].kdc_unit = id->id_unit;
	kdc_scd[id->id_unit].kdc_isa = id;
	dev_attach(&kdc_scd[id->id_unit]);
}

int scd_attach(struct isa_device *dev)
{
	struct scd_data *cd = scd_data + dev->id_unit;
	int i;
	
	cd->iobase = dev->id_iobase;	/* Already set by probe, but ... */

	scd_registerdev(dev);
	/* name filled in probe */
	kdc_scd[dev->id_unit].kdc_description = scd_data[dev->id_unit].name;
	printf("scd%d: <%s>\n", dev->id_unit, scd_data[dev->id_unit].name);

	set_drive_params(dev->id_unit);

	cd->flags = SCDINIT;
	cd->audio_status = CD_AS_AUDIO_INVALID;

	return 1;
}

static int
bcd2bin(bcd_t b)
{
	return (b >> 4) * 10 + (b & 15);
}

static bcd_t
bin2bcd(int b)
{
	return ((b / 10) << 4) | (b % 10);
}

static void
hsg2msf(int hsg, bcd_t *msf)
{
	hsg += 150;
	M_msf(msf) = bin2bcd(hsg / 4500);
	hsg %= 4500;
	S_msf(msf) = bin2bcd(hsg / 75);
	F_msf(msf) = bin2bcd(hsg % 75);
}

static int
msf2hsg(bcd_t *msf)
{
	return (bcd2bin(M_msf(msf)) * 60 +
		bcd2bin(S_msf(msf))) * 75 +
		bcd2bin(F_msf(msf)) - 150;
}

static int
handle_attention(unsigned unit)
{
	unsigned port = scd_data[unit].iobase;
	unsigned char atten_code;
	static int loop_count = 0;
	int i;

	if (!IS_ATTENTION(port)) {
		loop_count = 0;
		return 0;
	}

	if (loop_count > 20) {
		printf("scd%d: Too many attentions: %d\n", unit, loop_count);
		loop_count = 0;
		return 0;
	}

	write_control(port, SCD_ATTN_CLR_BIT);
	atten_code = inb(port+SCD_RESULT_REG);

#if DEBUG
	printf("scd%d: DEBUG: ********* ATTENTION = 0x%x\n", unit, atten_code);
#endif

	switch (atten_code) {
	/* Someone changed the CD.  Mark it as changed */
	case SONY_MECH_LOADED_ATTN:
		scd_data[unit].flags &= ~(SCDTOC|SCDSPINNING|SCDVALID);
		break;

	case SONY_SPIN_DOWN_COMPLETE_ATTN:
		scd_data[unit].flags &= ~SCDSPINNING;
		break;

	case SONY_SPIN_UP_COMPLETE_ATTN:
		scd_data[unit].flags |= SCDSPINNING;
		break;

	case SONY_AUDIO_PLAY_DONE_ATTN:
		scd_data[unit].audio_status = CD_AS_PLAY_COMPLETED;
/*		read_subcode(); */
		break;

	case SONY_EJECT_PUSHED_ATTN:
		scd_data[unit].flags &= ~SCDVALID;
		break;

	case SONY_LEAD_IN_ERR_ATTN:
	case SONY_LEAD_OUT_ERR_ATTN:
	case SONY_DATA_TRACK_ERR_ATTN:
	case SONY_AUDIO_PLAYBACK_ERR_ATTN:
		break;
	default:
#if DEBUG
		printf("scd%d: unknown ATTENTION = 0x%x\n", unit, atten_code);
#endif
		break;
	}
	loop_count++;
	return 1;
}

/* Returns 0 OR sony error code */
static int
spin_up(unsigned unit)
{
	unsigned char res_reg[12];
	unsigned int res_size;
	int res;
	int loop_count = 0;

again:
	res = do_cmd(unit, SCD_SPIN_UP_CMD, NULL, 0, res_reg, &res_size);
	if (res != 0) {
#if DEBUG > 1
		printf("scd%d: CMD_SPIN_UP error 0x%x\n", unit, res);
#endif
		return res;
	}

	if (!(scd_data[unit].flags & SCDTOC)) {	/* XXX probably no good ... */
		res = do_cmd(unit, SCD_READ_TOC_CMD, NULL, 0, res_reg, &res_size);
		if (res == SONY_NOT_SPIN_ERR) {
			if (loop_count++ < 3)
				goto again;
			return res;
		}
	}

	if (res != 0)
		return res;

	scd_data[unit].flags |= SCDSPINNING;

	return 0;
}
      
static struct s_sony_tracklist *
get_tl(struct s_sony_session_toc *toc, int size)
{
	struct s_sony_tracklist *tl = (struct s_sony_tracklist *)&toc->pointb0;

	(char *)tl -= 1;	/* Cannot take address of bitfield (above) */

	if (toc->pointb0 != 0xb0) {
#if DEBUG
		if (toc->pointb0 != tl->track)
			printf("scd: WARNING: pointb0 != track0\n");
#endif
		return tl;
	}
	(char *)tl += 9;
	if (toc->pointb1 != 0xb1) 
		return tl;
	(char *)tl += 9;
	if (toc->pointb2 != 0xb2) 
		return tl;
	(char *)tl += 9;
	if (toc->pointb3 != 0xb3) 
		return tl;
	(char *)tl += 9;
	if (toc->pointb4 != 0xb4) 
		return tl;
	(char *)tl += 9;
	if (toc->pointc0 != 0xc0) 
		return tl;
	(char *)tl += 9;
	return tl;
}

/* Return value same as do_cmd() */
static int
read_toc(dev_t dev)
{
	unsigned unit;
	struct scd_data *cd;
	unsigned part = 0;	/* For now ... */
	unsigned char params[1];
	unsigned char msf[3];
	unsigned int res_size;
	struct s_sony_session_toc toc;
	struct s_sony_tracklist *tl;
	int i, j;
	u_long first, last;

	unit = scd_unit(dev);
	cd = scd_data + unit;

	params[0] = part+1;
	i = do_cmd(unit, SCD_REQ_TOC_DATA_SPEC_CMD,
			params,
			1, 
			(unsigned char *)toc.exec_status,
			&res_size);
	if (i != 0)
		return i;

	tl = get_tl(&toc, res_size);
	first = msf2hsg(tl->track_start_msf);
	last = msf2hsg(toc.lead_out_start_msf);
	cd->blksize = SCDBLKSIZE;
	cd->disksize = last*cd->blksize/DEV_BSIZE;

#if DEBUG
	printf("scd%d: firstsector = %d, lastsector = %d", unit,
			first, last);
#endif

	cd->first_track = bcd2bin(toc.first_track_num);
	cd->last_track = bcd2bin(toc.last_track_num);
	if (cd->last_track > (MAX_TRACKS-2))
		cd->last_track = MAX_TRACKS-2;
	for (j = 0, i = cd->first_track; i <= cd->last_track; i++, j++) {
		bcopy(tl[j].track_start_msf, cd->toc[i].start_msf, 3);
#if DEBUG
		if ((j % 3) == 0)
			printf("\nscd%d: tracks ", unit);
		printf("[%03d: %2d %2d %2d]  ", i,
			bcd2bin(cd->toc[i].start_msf[0]),
			bcd2bin(cd->toc[i].start_msf[1]),
			bcd2bin(cd->toc[i].start_msf[2]));
#endif
	}
	hsg2msf(msf2hsg(toc.lead_out_start_msf)-1, msf);
	bcopy(msf, cd->toc[cd->last_track+1].start_msf, 3);
#if DEBUG
	i = cd->last_track+1;
	printf("[END: %2d %2d %2d]\n",
		bcd2bin(cd->toc[i].start_msf[0]),
		bcd2bin(cd->toc[i].start_msf[1]),
		bcd2bin(cd->toc[i].start_msf[2]));
#endif

	bzero(&cd->dlabel,sizeof(struct disklabel));
	/* filled with spaces first */
	strncpy(cd->dlabel.d_typename,"               ",
		sizeof(cd->dlabel.d_typename));
	strncpy(cd->dlabel.d_typename, cd->name,
		min(strlen(cd->name), sizeof(cd->dlabel.d_typename) - 1));
	strncpy(cd->dlabel.d_packname,"unknown        ",
		sizeof(cd->dlabel.d_packname));
	cd->dlabel.d_secsize 	= cd->blksize;
	cd->dlabel.d_nsectors	= 100;
	cd->dlabel.d_ntracks	= 1;
	cd->dlabel.d_ncylinders	= (cd->disksize/100)+1;
	cd->dlabel.d_secpercyl	= 100;
	cd->dlabel.d_secperunit	= cd->disksize;
	cd->dlabel.d_rpm	= 300;
	cd->dlabel.d_interleave	= 1;
	cd->dlabel.d_flags	= D_REMOVABLE;
	cd->dlabel.d_npartitions= 1;
	cd->dlabel.d_partitions[0].p_offset = 0;
	cd->dlabel.d_partitions[0].p_size = cd->disksize;
	cd->dlabel.d_partitions[0].p_fstype = 9;
	cd->dlabel.d_magic	= DISKMAGIC;
	cd->dlabel.d_magic2	= DISKMAGIC;
	cd->dlabel.d_checksum	= dkcksum(&cd->dlabel);

	cd->flags |= SCDTOC;

	return 0;
}

static inline void
write_control(unsigned port, unsigned data)
{
	outb(port + SCD_CONTROL_REG, data);
}

static int
write_params(unsigned port, unsigned char *params, int num_params)
{
   	int retry_count = 20000;

	while ((retry_count-- > 0) && (!FIFOST_BIT(port, SCD_PARAM_WRITE_RDY_BIT)))
		;
	if (!FIFOST_BIT(port, SCD_PARAM_WRITE_RDY_BIT))
		return EIO;

	while (num_params-- > 0)
		outb(port+SCD_PARAM_REG, *params++);

	return 0;
}

static void
set_drive_params(unsigned unit)
{
	unsigned char res_reg[12];
	unsigned int res_size;
	unsigned char params[3];
	int res;

	params[0] = SCD_SD_MECH_CONTROL;
	params[1] = 0x03; /* Set auto spin up and auto eject */
	if (scd_data[unit].double_speed)
		params[1] |= 0x04; /* Set the drive to double speed if possible */

	res = do_cmd(unit, SCD_SET_DRIVE_PARAM_CMD,
		params,
		2,
		res_reg,
		&res_size);

	if (res != 0)
		printf("scd%d: Unable to set mechanical parameters. Errcode = 0x%x\n", res);
}

static int
get_result(unsigned unit, unsigned char *result)
{
	unsigned int port = scd_data[unit].iobase;
	unsigned int flags = scd_data[unit].flags;
	unsigned int res_reg = port + SCD_RESULT_REG;
	unsigned char c;
	int len = 0;
	int datalen = 0;
	int loop_index = 0;
	unsigned int retry_count;

	while (handle_attention(unit))
		;

	if (flags & SCDPROBING) {
		retry_count = 50; /* While probing tsleep doesn't sleep */
		while ((retry_count-- > 0)
			&& (IS_BUSY(port) || !STATUS_BIT(port, SCD_RES_RDY_BIT)))
		{
			DELAY(100000);
			while (handle_attention(unit))
				;
		}
	} else {
		retry_count = 100;	/* Wait max 10 sec. */
		while ((retry_count-- > 0)
			&& (IS_BUSY(port) || !STATUS_BIT(port, SCD_RES_RDY_BIT)))
		{
			tsleep(get_result, PZERO - 1, "get_result", hz/10);
			while (handle_attention(unit))
				;
		}
	}
	while (handle_attention(unit))
		;

	if (IS_BUSY(port) || !STATUS_BIT(port, SCD_RES_RDY_BIT)) {
		printf("scd%d: timeout @%d\n", unit, __LINE__);
		return -EIO;
	}

	write_control(port, SCD_RES_RDY_CLR_BIT);
	switch ((*result++ = inb(res_reg)) & 0xf0) {
	case 0x50:
		return 1;
	case 0x20:
		*result = inb(res_reg);
		return 2;
	case 0x00:
	default:
		datalen = inb(res_reg);
		len = 2;
		loop_index = len;
		*result++ = datalen;
	}

#if DEBUG
	printf("scd%d: DEBUG: get_result bytes = %d\n", unit, datalen);
#endif

	while (datalen-- > 0) {
		if (loop_index++ >= 10) {
			loop_index = 1;
			retry_count = 200;
			if (flags & SCDPROBING)
				retry_count += 10000;
			while ((retry_count > 0) && !STATUS_BIT(port, SCD_RES_RDY_BIT)) {
				retry_count--;
				if (retry_count < 100)
					tsleep(get_result, PZERO - 1, "get_result", hz/50);
				else
					DELAY(10);
			}
			if (!STATUS_BIT(port, SCD_RES_RDY_BIT)) {
				printf("scd%d: timeout @%d\n", unit, __LINE__);
				return -EIO;
			}
			write_control(port, SCD_RES_RDY_CLR_BIT);
		}
		*result++ = inb(res_reg);
		len++;
	}
	return len;
}

/* Returns 0 or (sony error code) or -(errno code) */
static int
do_cmd(unsigned int unit,
	       unsigned char cmd,
               unsigned char *params,
               unsigned int num_params,
               unsigned char *result_buffer,
               unsigned int *result_size)
{
	unsigned port = scd_data[unit].iobase;
	unsigned int retry_count;
	int res;

#if DEBUG
	printf("SCD: do_cmd: 0x%x\n", cmd);
#endif

	while (handle_attention(unit))
		;
	retry_count = 40;
	while (retry_count-- > 0 && IS_BUSY(port)) {
		while (handle_attention(unit))
			;
		tsleep(do_cmd, PZERO - 1, "do_cmd", hz/4);
	}
	if (IS_BUSY(port)) {
#if DEBUG
		printf("SCD: timeout out @%d\n", __LINE__);
#endif
		return -EIO;
	}
	write_control(port, SCD_RES_RDY_CLR_BIT);
	write_control(port, SCD_PARAM_CLR_BIT);

	retry_count = 40;
	while ((retry_count-- > 0) && (!FIFOST_BIT(port, SCD_PARAM_WRITE_RDY_BIT)))
		;
	if (!FIFOST_BIT(port, SCD_PARAM_WRITE_RDY_BIT))
		return -EIO;

	while (num_params-- > 0)
		outb(port+SCD_PARAM_REG, *params++);
	outb(port+SCD_CMD_REG, cmd);

	if ((res = get_result(unit, result_buffer)) < 0)
		return res;
	if (res < 2)
		return -EIO;
	*result_size = res;
	if (result_buffer[0] == 0x20)
		return result_buffer[1];
	return 0;
}

static void
print_error(int unit, int errcode)
{
	switch (errcode) {
	case SONY_NOT_LOAD_ERR:
		printf("scd%d: door is open\n", unit);
		break;
	case SONY_NO_DISK_ERR:
		printf("scd%d: no cd inside\n", unit);
		break;
	default:
		if (errcode < 0)
			printf("scd%d: device timeout\n", unit);
		else
			printf("scd%d: unexpected error 0x%x\n", unit, errcode);
		break;
	}
}

int
scdopen(dev_t dev)
{
	int unit,part,phys;
	int rc;
	struct scd_data *cd;
	
	unit = scd_unit(dev);
	if (unit >= NSCD)
		return ENXIO;

	cd = scd_data + unit;
	part = scd_part(dev);
	phys = scd_phys(dev);
	
	/* not initialized*/
	if (!(cd->flags & SCDINIT))
		return ENXIO;

	/* invalidated in the meantime? mark all open part's invalid */
	if (cd->openflags)
		return ENXIO;

#if DEBUG
	printf("scd%d: DEBUG: status = 0x%x\n", unit, inb(cd->iobase+SCD_STATUS_REG));
#endif

	if ((rc = spin_up(unit)) != 0) {
		print_error(unit, rc);
		return EIO;
	}
	if (!(cd->flags & SCDTOC)) {
		int loop_count = 3;

		while (loop_count-- > 0 && (rc = read_toc(dev)) != 0) {
			if (rc == SONY_NOT_SPIN_ERR) {
				rc = spin_up(unit);
				if (rc) {
					print_error(unit, rc);\
					return EIO;
				}
				continue;
			}
			printf("scd%d: TOC read error 0x%x\n", unit, rc);
			return EIO;
		}
	}

	cd->openflags = 1;
	cd->flags |= SCDVALID;
	kdc_scd[unit].kdc_state = DC_BUSY;

	return 0;
}

int
scdclose(dev_t dev)
{
	int unit,part,phys;
	struct scd_data *cd;
	int rlen;
	char rdata[10];
	
	unit = scd_unit(dev);
	if (unit >= NSCD)
		return ENXIO;

	cd = scd_data + unit;
	part = scd_part(dev);
	phys = scd_phys(dev);
	
	if (!(cd->flags & SCDINIT) || !cd->openflags) 
		return ENXIO;

	(void)do_cmd(unit, SCD_SPIN_DOWN_CMD, (char *)0, 0, rdata, &rlen);
	cd->flags &= ~SCDSPINNING;

	kdc_scd[unit].kdc_state = DC_IDLE;

	/* close channel */
	cd->openflags = 0;

	return 0;
}

void
scdstrategy(struct buf *bp)
{
	struct scd_data *cd;
	struct buf *qp;
	int s;
	int unit = scd_unit(bp->b_dev);

	cd = scd_data + unit;

	/* test validity */
#if DEBUG > 1
	printf("scd%d: DEBUG: strategy: block=%ld, bcount=%ld\n", unit, bp->b_blkno, bp->b_bcount);
#endif
	if (unit >= NSCD || bp->b_blkno < 0 || (bp->b_bcount % SCDBLKSIZE)) {
		printf("scd%d: strategy failure: blkno = %d, bcount = %d\n",
			unit, bp->b_blkno, bp->b_bcount);
		bp->b_error = EINVAL;
		bp->b_flags |= B_ERROR;
		goto bad;
	}

	/* if device invalidated (e.g. media change, door open), error */
	if (!(cd->flags & SCDVALID)) {
		printf("scd%d: media changed\n", unit);
		bp->b_error = EIO;
		goto bad;
	}

	/* read only */
	if (!(bp->b_flags & B_READ)) {
		bp->b_error = EROFS;
		goto bad;
	}
	
	/* no data to read */
	if (bp->b_bcount == 0)
		goto done;
	
	if (!(cd->flags & SCDTOC)) {
		bp->b_error = EIO;
		goto bad;
	}
	/* adjust transfer if necessary */
	if (bounds_check_with_label(bp,&cd->dlabel,1) <= 0)
		goto done;

	bp->b_pblkno = bp->b_blkno;
	bp->b_resid = 0;
	
	/* queue it */
	qp = &cd->head;
	s = splbio();
	disksort(qp,bp);
	splx(s);
	
	/* now check whether we can perform processing */
	scd_start(unit);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	bp->b_resid = bp->b_bcount;
	biodone(bp);
	return;
}

static void
scd_start(int unit)
{
	struct scd_data *cd = scd_data + unit;
	struct buf *bp, *qp = &cd->head;
	struct partition *p;
	int part;
	register s = splbio();
	
	if (cd->flags & SCDMBXBSY) {
		splx(s);
		return;
	}

	if ((bp = qp->b_actf) != 0) {
		/* block found to process, dequeue */
		qp->b_actf = bp->b_actf; /* changed from: bp->av_forw <se> */
		cd->flags |= SCDMBXBSY;
		splx(s);
	} else {
		/* nothing to do */
		splx(s);
		return;
	}

	p = cd->dlabel.d_partitions + scd_part(bp->b_dev);

	cd->mbx.unit = unit;
	cd->mbx.port = cd->iobase;
	cd->mbx.retry = 3;
	cd->mbx.bp = bp;
	cd->mbx.p_offset = p->p_offset;
	splx(s);

	scd_doread(SCD_S_BEGIN,&(cd->mbx));
	return;
}

int
scdioctl(dev_t dev, int cmd, caddr_t addr, int flags)
{
	struct scd_data *cd;
	int unit,part;
	
	unit = scd_unit(dev);
	part = scd_part(dev);
	cd = scd_data + unit;

#if DEBUG
	printf("scd%d: ioctl: cmd=0x%lx\n", unit, cmd);
#endif

	if (!(cd->flags & SCDVALID))
		return EIO;

	switch (cmd) {
	case DIOCSBAD:
		return EINVAL;
	case DIOCGDINFO:
		*(struct disklabel *)addr = cd->dlabel;
		return 0;
	case DIOCGPART:
		((struct partinfo *)addr)->disklab = &cd->dlabel;
		((struct partinfo *)addr)->part =
		    &cd->dlabel.d_partitions[0];
		return 0;
	case CDIOCPLAYTRACKS:
		return scd_playtracks(unit, (struct ioc_play_track *) addr);
	case CDIOCPLAYBLOCKS:
		return EINVAL;
	case CDIOCPLAYMSF:
		return scd_playmsf(unit, (struct ioc_play_msf *) addr);
	case CDIOCREADSUBCHANNEL:
		return scd_subchan(unit, (struct ioc_read_subchannel *) addr);
	case CDIOREADTOCHEADER:
	case CDIOREADTOCENTRYS:
	case CDIOCSETPATCH:
	case CDIOCGETVOL:
	case CDIOCSETVOL:
	case CDIOCSETMONO:
	case CDIOCSETSTERIO:
	case CDIOCSETMUTE:
	case CDIOCSETLEFT:
	case CDIOCSETRIGHT:
		return EINVAL;
	case CDIOCRESUME:
/*		return scd_resume(unit); */
	case CDIOCPAUSE:
/*		return scd_pause(unit); */
	case CDIOCSTART:
		return EINVAL;
	case CDIOCSTOP:
		return scd_stop(unit);
	case CDIOCEJECT:
		return scd_eject(unit);
	case CDIOCALLOW:
		return 0;
	default:
		printf("scd%d: unsupported ioctl (cmd=0x%lx)\n", unit, cmd);
		return ENOTTY;
	}
}

/***************************************************************
 * lower level of driver starts here
 **************************************************************/

static int
scd_playtracks(int unit, struct ioc_play_track *pt)
{
	struct scd_data *cd = scd_data + unit;
	struct ioc_play_msf msf;
	int a = pt->start_track;
	int z = pt->end_track;
	int rc, i;

	if (!(cd->flags & SCDTOC) && (rc = read_toc(unit)) != 0) {
		print_error(unit, rc);
		return EIO;
	}

#if DEBUG
	printf("scd%d: playtracks from %d:%d to %d:%d\n", unit,
		a, pt->start_index, z, pt->end_index);
#endif

	if (   a < cd->first_track
	    || a > cd->last_track
	    || a > z
	    || z > cd->last_track)
		return EINVAL;

	for (i = 0; i < 3; i++) {
		((char *)&msf.start_m)[i] = bcd2bin(cd->toc[a].start_msf[i]);
		((char *)&msf.end_m)[i] = bcd2bin(cd->toc[z+1].start_msf[i]);
	}
	return scd_playmsf(unit, &msf);
}

static int
scd_playmsf(int unit, struct ioc_play_msf *msf)
{
	struct scd_data *cd = scd_data + unit;
	unsigned char sdata[10];
	unsigned char rdata[10];
	int rlen, rc;

	sdata[0] = 0x03;
	sdata[1] = bin2bcd(msf->start_m);
	sdata[2] = bin2bcd(msf->start_s);
	sdata[3] = bin2bcd(msf->start_f);
	sdata[4] = bin2bcd(msf->end_m);
	sdata[5] = bin2bcd(msf->end_s);
	sdata[6] = bin2bcd(msf->end_f);

#if DEBUG
	printf("scd%d: playmsf: %02d:%02d:%02d -> %02d:%02d:%02d\n", unit,
		msf->start_m, msf->start_s, msf->start_f,
		msf->end_m, msf->end_s, msf->end_f);
#endif

	if ((rc = do_cmd(unit, SCD_AUDIO_PLAYBACK_CMD, sdata, 7, rdata, &rlen)) != 0) {
		print_error(unit, rc);
		return EIO;
	}
	cd->audio_status = CD_AS_PLAY_IN_PROGRESS;
	return 0;
}

static int
scd_stop(int unit)
{
	struct scd_data *cd = scd_data + unit;
	char rdata[10];
	int rlen;

	(void)do_cmd(unit, SCD_AUDIO_STOP_CMD, (char *)0, 0, rdata, &rlen);
	cd->audio_status = CD_AS_PLAY_COMPLETED;
	return 0;
}

static int
scd_eject(int unit)
{
	struct scd_data *cd = scd_data + unit;
	int port = cd->iobase;
	char rdata[10];
	int rlen;

	(void)do_cmd(unit, SCD_AUDIO_STOP_CMD, (char *)0, 0, rdata, &rlen);
	(void)do_cmd(unit, SCD_SPIN_DOWN_CMD, (char *)0, 0, rdata, &rlen);
	cd->flags &= ~(SCDSPINNING|SCDTOC);
	if (do_cmd(unit, SCD_EJECT_CMD, (char *)0, 0, rdata, &rlen) != 0)
		return EIO;
	return 0;
}

static int
scd_subchan(int unit, struct ioc_read_subchannel *sc)
{
	struct scd_data *cd = scd_data + unit;
	struct s_sony_subcode q;
	struct cd_sub_channel_info data;

#if DEBUG
	printf("scd%d: subchan af=%d, df=%d\n", unit,
		sc->address_format,
		sc->data_format);
#endif

	if (sc->address_format != CD_MSF_FORMAT)
		return EINVAL;

	if (sc->data_format != CD_CURRENT_POSITION)
		return EINVAL;

	if (read_subcode(unit, &q) != 0)
		return EIO;

	data.header.audio_status = cd->audio_status;
	data.what.position.data_format = CD_MSF_FORMAT;
	data.what.position.track_number = bcd2bin(q.track_num);
	data.what.position.reladdr.msf.unused = 0;
	data.what.position.reladdr.msf.minute = bcd2bin(q.rel_msf[0]);
	data.what.position.reladdr.msf.second = bcd2bin(q.rel_msf[1]);
	data.what.position.reladdr.msf.frame = bcd2bin(q.rel_msf[2]);
	data.what.position.absaddr.msf.unused = 0;
	data.what.position.absaddr.msf.minute = bcd2bin(q.abs_msf[0]);
	data.what.position.absaddr.msf.second = bcd2bin(q.abs_msf[1]);
	data.what.position.absaddr.msf.frame = bcd2bin(q.abs_msf[2]);

	if (copyout(&data, sc->data, min(sizeof(struct cd_sub_channel_info), sc->data_len))!=0)
		return EFAULT;
	return 0;
}

/* check to see if a Sony CD-ROM is attached to the ISA bus */

static void
get_drive_configuration(unsigned short unit,
                        unsigned char res_reg[],
                        unsigned int *res_size)
{
	unsigned port = scd_data[unit].iobase;
	int retry_count = 10000;

	outb(port + SCD_CONTROL_REG, SCD_DRIVE_RESET_BIT); /* reset drive */

	while ((retry_count-- > 0) && (!IS_ATTENTION(port)))
		DELAY(10);

	/* If I use a shorter delay, I get a timeout in get_result() */
	/* I guess there is a better way, but I don't have the manuals */
	DELAY(500000);

	(void)do_cmd(unit, SCD_REQ_DRIVE_CONFIG_CMD,
                     NULL,
                     0,
                     (unsigned char *) res_reg,
                     res_size);
	return;
}

int
scd_probe(struct isa_device *dev)
{
	struct s_sony_drive_config drive_config;
	int unit = dev->id_unit;
	unsigned int res_size;
	int i;
	static char buf[8+16+8+3];
	char *s = buf;

	scd_data[unit].flags = SCDPROBING;
	scd_data[unit].iobase = dev->id_iobase;

	bzero(&drive_config, sizeof(drive_config));
	get_drive_configuration(unit, drive_config.exec_status, &res_size);

	if (res_size < sizeof(drive_config)
		|| (drive_config.exec_status[0] & 0xf0) != 0x00)
	{
		return 0;
	}

	bcopy(drive_config.vendor_id, buf, 8);
	s = buf+8;
	while (*(s-1) == ' ')
		s--;
	*s++ = ' ';
	bcopy(drive_config.product_id, s, 16);
	s += 16;
	while (*(s-1) == ' ')
		s--;
	*s++ = ' ';
	bcopy(drive_config.product_rev_level, s, 8);
	s += 8;
	while (*(s-1) == ' ')
		s--;
	*s = 0;

	scd_data[unit].name = buf;

	if (SONY_HWC_DOUBLE_SPEED(drive_config))
		scd_data[unit].double_speed = 1;
	else
		scd_data[unit].double_speed = 0;

	return 4;
}

static int
read_subcode(int unit, struct s_sony_subcode *sc)
{
	int rlen;

	if (do_cmd(unit, SCD_REQ_SUBCODE_ADDRESS_CMD,
			(char *)0, 0, sc->exec_status, &rlen) != 0)
		return EIO;
	return 0;
}

/* State machine copied from mcd.c /Micke */

/* state machine to process read requests
 * initialize with SCD_S_BEGIN: reset state machine
 * SCD_S_WAITSTAT:  wait for ready (!busy)
 * SCD_S_WAITSPIN:  wait for drive to spin up (if not spinning)
 * SCD_S_WAITFIFO:  wait for param fifo to get ready, them exec. command.
 * SCD_S_WAITREAD:  wait for data ready, read data
 * SCD_S_WAITPARAM: wait for command result params, read them, error if bad data read.
 */

static struct scd_mbx *mbxsave;

static void
scd_doread(int state, struct scd_mbx *mbxin)
{
	struct scd_mbx *mbx = (state!=SCD_S_BEGIN) ? mbxsave : mbxin;
	int	unit = mbx->unit;
	int	port = mbx->port;
	struct	buf *bp = mbx->bp;
	struct	scd_data *cd = scd_data + unit;

	int	reg,i,k,c;
	int	blknum;
	caddr_t	addr;
	char	rdata[10];
	static char sdata[3];	/* Must be preserved between calls to this function */

loop:
	switch (state) {
	case SCD_S_BEGIN:
		mbx = mbxsave = mbxin;

	case SCD_S_BEGIN1:
		/* get status */
		mbx->count = RDELAY_WAIT;

		while (handle_attention(unit))
			;
		goto trystat;

	case SCD_S_WAITSTAT:
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITSTAT);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout. drive busy.\n",unit);
			goto harderr;
		}

trystat:
		if (IS_BUSY(port)) {
			timeout((timeout_func_t)scd_doread,
			    (caddr_t)SCD_S_WAITSTAT,hz/100); /* XXX */
			return;
		}
			
		while (handle_attention(unit))
			;
#if 0
		/* reject, if audio active */
		if (cd->status & SCDAUDIOBSY) {
			printf("scd%d: audio is active\n",unit);
			goto readerr;
		}
#endif
		mbx->sz = cd->blksize;

firstblock:
		/* for first block */
		mbx->nblk = (bp->b_bcount + (mbx->sz-1)) / mbx->sz;
		mbx->skip = 0;

nextblock:
		if (!(cd->flags & SCDVALID))
			goto changed;

		blknum 	= (bp->b_blkno / (mbx->sz/DEV_BSIZE))
			+ mbx->p_offset + mbx->skip/mbx->sz;

#if DEBUG > 1
		printf("scd%d: scd_doread: read blknum=%d\n", unit, blknum);
#endif
		/* build parameter block */
		hsg2msf(blknum, sdata);

		write_control(port, SCD_RES_RDY_CLR_BIT);
		write_control(port, SCD_PARAM_CLR_BIT);
		write_control(port, SCD_DATA_RDY_CLR_BIT);

		if (FIFOST_BIT(port, SCD_PARAM_WRITE_RDY_BIT))
			goto writeparam;

		mbx->count = 100;
		timeout((timeout_func_t)scd_doread,
			(caddr_t)SCD_S_WAITFIFO,hz/100); /* XXX */
		return;

	case SCD_S_WAITSPIN:
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITSPIN);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout waiting for drive to spin up.\n", unit);
			goto harderr;
		}
		if (!STATUS_BIT(port, SCD_RES_RDY_BIT)) {
			timeout((timeout_func_t)scd_doread,
				(caddr_t)SCD_S_WAITSPIN,hz/100); /* XXX */
			return;
		}
		write_control(port, SCD_RES_RDY_CLR_BIT);
		switch ((i = inb(port+SCD_RESULT_REG)) & 0xf0) {
		case 0x20:
			i = inb(port+SCD_RESULT_REG);
			print_error(unit, i);
			goto harderr;
		case 0x00:
			(void)inb(port+SCD_RESULT_REG);
			cd->flags |= SCDSPINNING;
			break;
		}
#if DEBUG
		printf("scd%d: DEBUG: spin up complete\n", unit);
#endif
		state = SCD_S_BEGIN1;
		goto loop;

	case SCD_S_WAITFIFO:
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITFIFO);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout. write param not ready.\n",unit);
			goto harderr;
		}
		if (!FIFOST_BIT(port, SCD_PARAM_WRITE_RDY_BIT)) {
			timeout((timeout_func_t)scd_doread,
				(caddr_t)SCD_S_WAITFIFO,hz/100); /* XXX */
			return;
		}
#if DEBUG
		printf("scd%d: mbx->count (writeparamwait) = %d(%d)\n", unit, mbx->count, 100);
#endif

writeparam:
		/* The reason this test isn't done 'till now is to make sure */
		/* that it is ok to send the SPIN_UP command below */
		if (!(cd->flags & SCDSPINNING)) {

#if DEBUG
			printf("scd%d: spinning up drive ...\n", unit);
#endif
			outb(port+SCD_CMD_REG, SCD_SPIN_UP_CMD);
			mbx->count = 300;
			timeout((timeout_func_t)scd_doread,
				(caddr_t)SCD_S_WAITSPIN,hz/100); /* XXX */
			return;
		}

		reg = port + SCD_PARAM_REG;
		/* send the read command */
		disable_intr();
		outb(reg, sdata[0]);
		outb(reg, sdata[1]);
		outb(reg, sdata[2]);
		outb(reg, 0);
		outb(reg, 0);
		outb(reg, 1);
		outb(port+SCD_CMD_REG, SCD_READ_BLKERR_STAT_CMD);
		enable_intr();

		mbx->count = RDELAY_WAITREAD;
		for (i = 0; i < 50; i++) {
			if (STATUS_BIT(port, SCD_DATA_RDY_BIT))
				goto got_data;
			DELAY(100);
		}

		timeout((timeout_func_t)scd_doread,
			(caddr_t)SCD_S_WAITREAD,hz/100); /* XXX */
		return;

	case SCD_S_WAITREAD:
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITREAD);
		if (mbx->count-- <= 0) {
			if (STATUS_BIT(port, SCD_RES_RDY_BIT))
				goto got_param;
			printf("scd%d: timeout while reading data\n",unit);
			goto readerr;
		}
		if (!STATUS_BIT(port, SCD_DATA_RDY_BIT)) {
			while (handle_attention(unit))
				;
			if (!(cd->flags & SCDVALID))
				goto changed;
			timeout((timeout_func_t)scd_doread,
				(caddr_t)SCD_S_WAITREAD,hz/100); /* XXX */
			return;
		}

#if DEBUG > 1
		printf("scd%d: mbx->count (after RDY_BIT) = %d(%d)\n", unit, mbx->count, RDELAY_WAITREAD);
#endif

got_data:
		/* data is ready */
		addr = bp->b_un.b_addr + mbx->skip;
		write_control(port, SCD_DATA_RDY_CLR_BIT);
		insb(port+SCD_READ_REG, addr, mbx->sz);

		mbx->count = 100;
		for (i = 0; i < 20; i++) {
			if (STATUS_BIT(port, SCD_RES_RDY_BIT))
				goto waitfor_param;
			DELAY(100);
		}
		goto waitfor_param;

	case SCD_S_WAITPARAM:
		untimeout((timeout_func_t)scd_doread,(caddr_t)SCD_S_WAITPARAM);
		if (mbx->count-- <= 0) {
			printf("scd%d: timeout waiting for params\n",unit);
			goto readerr;
		}

waitfor_param:
		if (!STATUS_BIT(port, SCD_RES_RDY_BIT)) {
			timeout((timeout_func_t)scd_doread,
				(caddr_t)SCD_S_WAITPARAM,hz/100); /* XXX */
			return;
		}
#if DEBUG
		if (mbx->count < 100)
			printf("scd%d: mbx->count (paramwait) = %d(%d)\n", unit, mbx->count, 100);
#endif

got_param:
		write_control(port, SCD_RES_RDY_CLR_BIT);
		switch ((i = inb(port+SCD_RESULT_REG)) & 0xf0) {
		case 0x50:
			switch (i) {
			case SONY_UNREC_CIRC_ERR:
			case SONY_UNREC_LECC_ERR:
				printf("scd%d: unrecoverable read error 0x%x\n", unit, i);
				goto harderr;
			}
			break;
		case 0x20:
			i = inb(port+SCD_RESULT_REG);
			switch (i) {
			case SONY_NOT_SPIN_ERR:
#if DEBUG
				printf("scd%d: read error: drive not spinning\n", unit);
#endif
				if (mbx->retry-- > 0) {
					state = SCD_S_BEGIN1;
					cd->flags &= ~SCDSPINNING;
					goto loop;
				}
				goto harderr;
			default:
				print_error(unit, i);
				goto readerr;
			}
		case 0x00:
			i = inb(port+SCD_RESULT_REG);
			break;
		}

		if (--mbx->nblk > 0) {
			mbx->skip += mbx->sz;
			goto nextblock;
		}

		/* return buffer */
		bp->b_resid = 0;
		biodone(bp);

		cd->flags &= ~SCDMBXBSY;
		scd_start(mbx->unit);
		return;
	}

readerr:
	if (mbx->retry-- > 0) {
		printf("scd%d: retrying ...\n",unit);
		state = SCD_S_BEGIN1;
		goto loop;
	}
harderr:
	/* invalidate the buffer */
	bp->b_error = EIO;
	bp->b_flags |= B_ERROR;
	bp->b_resid = bp->b_bcount;
	biodone(bp);

	cd->flags &= ~SCDMBXBSY;
	scd_start(mbx->unit);
	return;

changed:
	printf("scd%d: media changed\n", unit);
	goto harderr;
}

int
scdsize(dev_t dev)
{
	return -1;
}
#endif /* NSCD > 0 */
