/*-
 * Copyright (c) 2000 Søren Schmidt
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

struct ar_softc {
    int lun;
    int32_t magic_0;
    int32_t magic_1;
    int flags;
#define	AR_F_RAID_0	0x0001		/* STRIPE */
#define	AR_F_RAID_1	0x0002		/* MIRROR */
#define	AR_F_SPAN	0x0004		/* SPAN */
#define	AR_F_CONF_DONE	0x0008
    
    int num_subdisks;
    struct ad_softc *subdisk[8];
    int num_mirrordisks;
    struct ad_softc *mirrordisk[8];
    int interleave;
    int last_disk;
    int32_t last_lba[8][2];

    u_int16_t heads;
    u_int16_t sectors;
    u_int32_t cylinders;
    u_int32_t total_secs;
    int reserved;			/* sectors that are NOT to be used */
    int offset;				/* offset from start of disk */

    struct disk                 disk;	/* disklabel/slice stuff */
    dev_t                       dev;	/* device place holder */

};

struct ar_buf {
    struct buf bp;
    struct buf *org;
    int drive;
    struct ar_buf *mirror;
    int done;
};

struct highpoint_raid_conf {
    int8_t		filler1[32];
    u_int32_t		magic;			/* 0x20 */
#define HPT_MAGIC_OK		0x5a7816f0
#define HPT_MAGIC_BAD		0x5a7816fd

    u_int32_t		magic_0;
    u_int32_t		magic_1;
    u_int32_t		order;
#define HPT_O_MIRROR		0x01
#define HPT_O_STRIPE		0x02
#define HPT_O_OK		0x04

    u_int8_t		raid_disks;
    u_int8_t		raid0_shift;
    u_int8_t		type;
#define HPT_T_RAID_0		0x00
#define HPT_T_RAID_1		0x01
#define HPT_T_RAID_01_RAID_0	0x02
#define HPT_T_SPAN		0x03
#define HPT_T_RAID_3		0x04
#define HPT_T_RAID_5		0x05
#define HPT_T_SINGLEDISK	0x06
#define HPT_T_RAID_01_RAID_1	0x07

    u_int8_t		disk_number;
    u_int32_t		total_secs;
    u_int32_t		disk_mode;
    u_int32_t		boot_mode;
    u_int8_t		boot_disk;
    u_int8_t		boot_protect;
    u_int8_t		error_log_entries;
    u_int8_t		error_log_index;
    struct {
	u_int32_t	timestamp;
	u_int8_t	reason;
#define	HPT_R_REMOVED		0xfe
#define	HPT_R_BROKEN		0xff
	
	u_int8_t	disk;
	u_int8_t	status;
	u_int8_t	sectors;
	u_int32_t	lba;
    } errorlog[32];
    int8_t		filler2[60];
};

struct promise_raid_conf {
    char		promise_id[24];
#define PR_MAGIC	"Promise Technology, Inc."

    int32_t		dummy_0;
    int32_t		magic_0;
    int32_t		dummy_1;
    int32_t		magic_1;
    int16_t		dummy_2;
    int8_t		filler1[470];
    struct {
	int32_t	flags;				/* 0x200 */
#define PR_F_CONFED		0x00000080

	int8_t		dummy_0;
	int8_t		disk_number;
	int8_t		channel;
	int8_t		device;
	int32_t		magic_0;
	int32_t		dummy_1;
	int32_t		dummy_2;		/* 0x210 */
	int32_t		disk_secs;
	int32_t		dummy_3;
	int16_t		dummy_4;
	int8_t		status;
#define	PR_S_DEFINED		0x01
#define	PR_S_ONLINE		0x02
#define	PR_S_OFFLINE		0x10

	int8_t		type;
#define	PR_T_STRIPE		0x00
#define	PR_T_MIRROR		0x01
#define	PR_T_STRIPE_MIRROR	0x04
#define	PR_T_SPAN		0x08

	u_int8_t	total_disks;		/* 0x220 */
	u_int8_t	raid0_shift;
	u_int8_t	raid0_disks;
	u_int8_t	array_number;
	u_int32_t	total_secs;
	u_int16_t	cylinders;
	u_int8_t	heads;
	u_int8_t	sectors;
	int32_t		magic_1;
	int32_t		dummy_5;		/* 0x230 */
	struct {
	    int16_t	dummy_0;
	    int8_t	channel;
	    int8_t	device;
	    int32_t	magic_0;
	    int32_t	disk_number;
	} disk[8];
    } raid;
    int32_t		filler2[346];
    uint32_t		checksum;
};

int ar_probe(struct ad_softc *);

