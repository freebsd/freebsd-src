/*-
 * Copyright (c) 2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
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

struct ar_disk {
    struct ata_device	*device;
    u_int64_t		disk_sectors;	/* sectors on this disk */
    off_t		last_lba;	/* last lba used */
    int			flags;
#define AR_DF_PRESENT		0x00000001
#define AR_DF_ASSIGNED		0x00000002
#define AR_DF_SPARE		0x00000004
#define AR_DF_ONLINE		0x00000008
};

#define MAX_ARRAYS	16
#define MAX_DISKS	16
#define AR_PROXIMITY	2048

struct ar_softc {
    int			lun;
    int32_t		magic_0;	/* ident for this array */
    int32_t		magic_1;	/* ident for this array */
    int			flags;
#define AR_F_RAID0		0x0001	/* STRIPE */
#define AR_F_RAID1		0x0002	/* MIRROR */
#define AR_F_SPAN		0x0004	/* SPAN */
#define AR_F_READY		0x0100
#define AR_F_DEGRADED		0x0200
#define AR_F_REBUILDING		0x0400
#define AR_F_PROMISE_RAID	0x1000
#define AR_F_HIGHPOINT_RAID	0x2000
    
    int			total_disks;	/* number of disks in this array */
    int			generation;	/* generation of this array */
    struct ar_disk	disks[MAX_DISKS+1]; /* ptr to each disk in array */
    int			width;		/* array width in disks */
    u_int16_t		heads;
    u_int16_t		sectors;
    u_int32_t		cylinders;
    u_int64_t		total_sectors;
    int			interleave;	/* interleave in bytes */
    int			reserved;	/* sectors that are NOT to be used */
    int			offset;		/* offset from start of disk */
    u_int64_t		lock_start;	/* start of locked area for rebuild */
    u_int64_t		lock_end;	/* end of locked area for rebuild */
    struct		disk disk;	/* disklabel/slice stuff */
    dev_t		dev;		/* device place holder */
};

struct ar_buf {
    struct bio		bp;
    struct bio		*org;	
    struct ar_buf	*mirror;
    int			drive;
    int			flags;
#define AB_F_DONE		0x01
};

#define HPT_LBA			9

struct highpoint_raid_conf {
    int8_t		filler1[32];
    u_int32_t		magic;			/* 0x20 */
#define HPT_MAGIC_OK		0x5a7816f0
#define HPT_MAGIC_BAD		0x5a7816fd

    u_int32_t		magic_0;
    u_int32_t		magic_1;
    u_int32_t		order;
#define HPT_O_DOWN		0x00
#define HPT_O_RAID01DEGRADED	0x01
#define HPT_O_RAID01DST		0x02
#define HPT_O_RAID01SRC		0x03
#define HPT_O_READY		0x04

    u_int8_t		array_width;
    u_int8_t		stripe_shift;
    u_int8_t		type;
#define HPT_T_RAID0		0x00
#define HPT_T_RAID1		0x01
#define HPT_T_RAID01_RAID0	0x02
#define HPT_T_SPAN		0x03
#define HPT_T_RAID_3		0x04
#define HPT_T_RAID_5		0x05
#define HPT_T_SINGLEDISK	0x06
#define HPT_T_RAID01_RAID1	0x07

    u_int8_t		disk_number;
    u_int32_t		total_sectors;
    u_int32_t		disk_mode;
    u_int32_t		boot_mode;
    u_int8_t		boot_disk;
    u_int8_t		boot_protect;
    u_int8_t		error_log_entries;
    u_int8_t		error_log_index;
    struct {
	u_int32_t	timestamp;
	u_int8_t	reason;
#define HPT_R_REMOVED		0xfe
#define HPT_R_BROKEN		0xff
	
	u_int8_t	disk;
	u_int8_t	status;
	u_int8_t	sectors;
	u_int32_t	lba;
    } errorlog[32];
    int8_t		filler2[60];
} __attribute__((packed));


#define PR_LBA(adp) \
	(((adp->total_secs / (adp->heads * adp->sectors)) * \
	  adp->heads * adp->sectors) - adp->sectors)

struct promise_raid_conf {
    char		promise_id[24];
#define PR_MAGIC	"Promise Technology, Inc."

    u_int32_t		dummy_0;
    u_int64_t		magic_0;
#define PR_MAGIC0(x)	((u_int64_t)x.device->channel->unit << 48) | \
			((u_int64_t)(x.device->unit != 0) << 56)
    u_int16_t		magic_1;
    u_int32_t		magic_2;
    u_int8_t		filler1[470];
    struct {
	u_int32_t	integrity;		/* 0x200 */
#define PR_I_VALID		0x00000080

	u_int8_t	flags;
#define PR_F_VALID		0x00000001
#define PR_F_ONLINE		0x00000002
#define PR_F_ASSIGNED		0x00000004
#define PR_F_SPARE		0x00000008
#define PR_F_DUPLICATE		0x00000010
#define PR_F_REDIR		0x00000020
#define PR_F_DOWN		0x00000040
#define PR_F_READY		0x00000080

	u_int8_t	disk_number;
	u_int8_t	channel;
	u_int8_t	device;
	u_int64_t	magic_0;
	u_int32_t	disk_offset;		/* 0x210 */
	u_int32_t	disk_sectors;
	u_int32_t	rebuild_lba;
	u_int16_t	generation;
	u_int8_t	status;
#define PR_S_VALID		0x01
#define PR_S_ONLINE		0x02
#define PR_S_INITED		0x04
#define PR_S_READY		0x08
#define PR_S_DEGRADED		0x10
#define PR_S_MARKED		0x20
#define PR_S_FUNCTIONAL		0x80

	u_int8_t	type;
#define PR_T_RAID0		0x00
#define PR_T_RAID1		0x01
#define PR_T_RAID3		0x02
#define PR_T_RAID5		0x04
#define PR_T_SPAN		0x08

	u_int8_t	total_disks;		/* 0x220 */
	u_int8_t	stripe_shift;
	u_int8_t	array_width;
	u_int8_t	array_number;
	u_int32_t	total_sectors;
	u_int16_t	cylinders;
	u_int8_t	heads;
	u_int8_t	sectors;
	int64_t		magic_1;
	struct {				/* 0x240 */
	    u_int8_t	flags;
	    u_int8_t	dummy_0;
	    u_int8_t	channel;
	    u_int8_t	device;
	    u_int64_t	magic_0;
	} disk[8];
    } raid;
    int32_t		filler2[346];
    u_int32_t		checksum;
} __attribute__((packed));

int ata_raiddisk_probe(struct ad_softc *);
int ata_raiddisk_attach(struct ad_softc *);
int ata_raiddisk_detach(struct ad_softc *);
void ata_raid_attach(void);
int ata_raid_rebuild(int);

