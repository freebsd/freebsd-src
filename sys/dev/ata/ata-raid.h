/*-
 * Copyright (c) 2000 - 2005 Søren Schmidt <sos@FreeBSD.org>
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

/* misc defines */
#define MAX_ARRAYS      16
#define MAX_VOLUMES     4
#define MAX_DISKS       16
#define AR_PROXIMITY    2048    /* how many sectors is "close" */

#define ATA_MAGIC       "FreeBSD ATA driver RAID "

struct ata_raid_subdisk {
    struct ar_softc     *raid[MAX_VOLUMES];
    int                 disk_number[MAX_VOLUMES];
};

/*  ATA PseudoRAID Metadata */
struct ar_softc {
    int                 lun;
    u_int8_t            name[32];
    int			volume;
    u_int64_t           magic_0;
    u_int64_t           magic_1;
    int                 type;
#define AR_T_JBOD               0x0001
#define AR_T_SPAN               0x0002
#define AR_T_RAID0              0x0004
#define AR_T_RAID1              0x0008
#define AR_T_RAID01             0x0010  
#define AR_T_RAID3              0x0020
#define AR_T_RAID4              0x0040
#define AR_T_RAID5              0x0080

    int                 status;
#define AR_S_READY              0x0001
#define AR_S_DEGRADED           0x0002
#define AR_S_REBUILDING         0x0004

    int                 format;
#define AR_F_FREEBSD_RAID       0x0001
#define AR_F_ADAPTEC_RAID       0x0002
#define AR_F_HPTV2_RAID         0x0004
#define AR_F_HPTV3_RAID         0x0008
#define AR_F_INTEL_RAID         0x0010
#define AR_F_ITE_RAID           0x0020
#define AR_F_LSIV2_RAID         0x0040
#define AR_F_LSIV3_RAID         0x0080
#define AR_F_NVIDIA_RAID        0x0100
#define AR_F_PROMISE_RAID       0x0200
#define AR_F_SII_RAID           0x0400
#define AR_F_SIS_RAID           0x0800
#define AR_F_VIA_RAID           0x1000
#define AR_F_FORMAT_MASK        0x1fff

    u_int               generation;
    u_int64_t           total_sectors;
    u_int64_t           offset_sectors; /* offset from start of disk */
    u_int16_t           heads;
    u_int16_t           sectors;
    u_int32_t           cylinders;
    u_int               width;          /* array width in disks */
    u_int               interleave;     /* interleave in sectors */
    u_int               total_disks;    /* number of disks in this array */
    struct ar_disk {
	device_t        dev;
	u_int8_t        serial[16];     /* serial # of physical disk */
	u_int64_t       sectors;        /* useable sectors on this disk */
	off_t           last_lba;       /* last lba used (for performance) */
	u_int           flags;
#define AR_DF_PRESENT           0x0001  /* this HW pos has a disk present */
#define AR_DF_ASSIGNED          0x0002  /* this HW pos assigned to an array */
#define AR_DF_SPARE             0x0004  /* this HW pos is a spare */
#define AR_DF_ONLINE            0x0008  /* this HW pos is online and in use */

    } disks[MAX_DISKS];
    int                 toggle;         /* performance hack for RAID1's */
    u_int64_t           rebuild_lba;    /* rebuild progress indicator */
    struct mtx          lock;           /* metadata lock */
    struct disk         *disk;          /* disklabel/slice stuff */
    struct proc         *pid;           /* rebuilder process id */
};

/* Adaptec HostRAID Metadata */
#define ADP_LBA(dev) \
	(((struct ad_softc *)device_get_ivars(dev))->total_secs - 17)

/* note all entries are big endian */
struct adaptec_raid_conf {
    u_int32_t           magic_0;
#define ADP_MAGIC_0             0xc4650790

    u_int32_t           generation;
    u_int16_t           dummy_0;
    u_int16_t           total_configs;
    u_int16_t           dummy_1;
    u_int16_t           checksum;
    u_int32_t           dummy_2;
    u_int32_t           dummy_3;
    u_int32_t           flags;
    u_int32_t           timestamp;
    u_int32_t           dummy_4[4];
    u_int32_t           dummy_5[4];
    struct {
	u_int16_t       total_disks;
	u_int16_t       generation;
	u_int32_t       magic_0;
	u_int8_t        dummy_0;
	u_int8_t        type;
#define ADP_T_RAID0             0x00
#define ADP_T_RAID1             0x01
	u_int8_t        dummy_1;
	u_int8_t        flags;

	u_int8_t        dummy_2;
	u_int8_t        dummy_3;
	u_int8_t        dummy_4;
	u_int8_t        dummy_5;

	u_int32_t       disk_number;
	u_int32_t       dummy_6;
	u_int32_t       sectors;
	u_int16_t       stripe_shift;
	u_int16_t       dummy_7;

	u_int32_t       dummy_8[4];
	u_int8_t        name[16];
    } configs[127];
    u_int32_t           dummy_6[13];
    u_int32_t           magic_1;
#define ADP_MAGIC_1             0x9ff85009
    u_int32_t           dummy_7[3];
    u_int32_t           magic_2;
    u_int32_t           dummy_8[46];
    u_int32_t           magic_3;
#define ADP_MAGIC_3             0x4d545044
    u_int32_t           magic_4;
#define ADP_MAGIC_4             0x9ff85009
    u_int32_t           dummy_9[62];
} __packed;


/* Highpoint V2 RocketRAID Metadata */
#define HPTV2_LBA(dev)  9

struct hptv2_raid_conf {
    int8_t              filler1[32];
    u_int32_t           magic;
#define HPTV2_MAGIC_OK          0x5a7816f0
#define HPTV2_MAGIC_BAD         0x5a7816fd

    u_int32_t           magic_0;
    u_int32_t           magic_1;
    u_int32_t           order;
#define HPTV2_O_RAID0           0x01
#define HPTV2_O_RAID1           0x02
#define HPTV2_O_OK              0x04

    u_int8_t            array_width;
    u_int8_t            stripe_shift;
    u_int8_t            type;
#define HPTV2_T_RAID0           0x00
#define HPTV2_T_RAID1           0x01
#define HPTV2_T_RAID01_RAID0    0x02
#define HPTV2_T_SPAN            0x03
#define HPTV2_T_RAID_3          0x04
#define HPTV2_T_RAID_5          0x05
#define HPTV2_T_JBOD            0x06
#define HPTV2_T_RAID01_RAID1    0x07

    u_int8_t            disk_number;
    u_int32_t           total_sectors;
    u_int32_t           disk_mode;
    u_int32_t           boot_mode;
    u_int8_t            boot_disk;
    u_int8_t            boot_protect;
    u_int8_t            error_log_entries;
    u_int8_t            error_log_index;
    struct {
	u_int32_t       timestamp;
	u_int8_t        reason;
#define HPTV2_R_REMOVED         0xfe
#define HPTV2_R_BROKEN          0xff
	
	u_int8_t        disk;
	u_int8_t        status;
	u_int8_t        sectors;
	u_int32_t       lba;
    } errorlog[32];
    int8_t              filler2[16];
    u_int32_t           rebuild_lba;
    u_int8_t            dummy_1;
    u_int8_t            name_1[15];
    u_int8_t            dummy_2;
    u_int8_t            name_2[15];
    int8_t              filler3[8];
} __packed;


/* Highpoint V3 RocketRAID Metadata */
#define HPTV3_LBA(dev) \
	(((struct ad_softc *)device_get_ivars(dev))->total_secs - 11)

struct hptv3_raid_conf {
    u_int32_t           magic;
#define HPTV3_MAGIC             0x5a7816f3

    u_int32_t           magic_0;
    u_int8_t            checksum_0;
    u_int8_t            mode;
#define HPTV3_BOOT_MARK         0x01
#define HPTV3_USER_MODE         0x02
    
    u_int8_t            user_mode;
    u_int8_t            config_entries;
    struct {
	u_int32_t       total_sectors;
	u_int8_t        type;
#define HPTV3_T_SPARE           0x00
#define HPTV3_T_JBOD            0x03
#define HPTV3_T_SPAN            0x04
#define HPTV3_T_RAID0           0x05
#define HPTV3_T_RAID1           0x06
#define HPTV3_T_RAID3           0x07
#define HPTV3_T_RAID5           0x08

	u_int8_t        total_disks;
	u_int8_t        disk_number;
	u_int8_t        stripe_shift;
	u_int16_t       status;
#define HPTV3_T_NEED_REBUILD    0x01
#define HPTV3_T_RAID5_FLAG      0x02

	u_int16_t       critical_disks;
	u_int32_t       rebuild_lba;
    } __packed configs[2];
    u_int8_t            name[16];
    u_int32_t           timestamp;
    u_int8_t            description[64];
    u_int8_t            creator[16];
    u_int8_t            checksum_1;
    u_int8_t            dummy_0;
    u_int8_t            dummy_1;
    u_int8_t            flags;
#define HPTV3_T_ENABLE_TCQ      0x01
#define HPTV3_T_ENABLE_NCQ      0x02
#define HPTV3_T_ENABLE_WCACHE   0x04
#define HPTV3_T_ENABLE_RCACHE   0x08

    struct {
	u_int32_t       total_sectors;
	u_int32_t       rebuild_lba;
    } __packed configs_high[2];
    u_int32_t           filler[87];
} __packed;


/* Intel MatrixRAID Metadata */
#define INTEL_LBA(dev) \
	(((struct ad_softc *)device_get_ivars(dev))->total_secs - 3)

struct intel_raid_conf {
    u_int8_t            intel_id[24];
#define INTEL_MAGIC             "Intel Raid ISM Cfg Sig. "

    u_int8_t            version[6];
#define INTEL_VERSION_1100      "1.1.00"
#define INTEL_VERSION_1201      "1.2.01"
#define INTEL_VERSION_1202      "1.2.02"

    u_int8_t            dummy_0[2];
    u_int32_t           checksum;
    u_int32_t           config_size;
    u_int32_t           config_id;
    u_int32_t           generation;
    u_int32_t           dummy_1[2];
    u_int8_t            total_disks;
    u_int8_t            total_volumes;
    u_int8_t            dummy_2[2];
    u_int32_t           filler_0[39];
    struct {
	u_int8_t        serial[16];
	u_int32_t       sectors;
	u_int32_t       id;
	u_int32_t       flags;
#define INTEL_F_SPARE           0x01
#define INTEL_F_ASSIGNED        0x02
#define INTEL_F_DOWN            0x04
#define INTEL_F_ONLINE          0x08

	u_int32_t       filler[5];
    } __packed disk[1];
    u_int32_t           filler_1[62];
} __packed;

struct intel_raid_mapping {
    u_int8_t            name[16];
    u_int64_t           total_sectors __packed;
    u_int32_t           state;
    u_int32_t           reserved;
    u_int32_t           filler_0[20];
    u_int32_t           offset;
    u_int32_t           disk_sectors;
    u_int32_t           stripe_count;
    u_int16_t           stripe_sectors;
    u_int8_t            status;
#define INTEL_S_READY           0x00
#define INTEL_S_DISABLED        0x01
#define INTEL_S_DEGRADED        0x02
#define INTEL_S_FAILURE         0x03

    u_int8_t            type;
#define INTEL_T_RAID0           0x00
#define INTEL_T_RAID1           0x01
#define INTEL_T_RAID5           0x05

    u_int8_t            total_disks;
    u_int8_t            magic[3];
    u_int32_t           filler_1[7];
    u_int32_t           disk_idx[1];
} __packed;


/* Integrated Technology Express Metadata */
#define ITE_LBA(dev) \
	(((struct ad_softc *)device_get_ivars(dev))->total_secs - 2)

struct ite_raid_conf {
    u_int32_t           filler_1[5];
    u_int8_t            timestamp_0[8];
    u_int32_t           dummy_1;
    u_int32_t           filler_2[5];
    u_int16_t           filler_3;
    u_int8_t            ite_id[40];
#define ITE_MAGIC               "Integrated Technology Express Inc      "

    u_int16_t           filler_4;
    u_int32_t           filler_5[6];
    u_int32_t           dummy_2;
    u_int32_t           dummy_3;
    u_int32_t           filler_6[12];
    u_int32_t           dummy_4;
    u_int32_t           filler_7[5];
    u_int64_t           total_sectors __packed;
    u_int32_t           filler_8[12];
    
    u_int16_t           filler_9;
    u_int8_t            type;
#define ITE_T_RAID0             0x00
#define ITE_T_RAID1             0x01
#define ITE_T_RAID01            0x02
#define ITE_T_SPAN              0x03

    u_int8_t            filler_10;
    u_int32_t           dummy_5[8];
    u_int8_t            stripe_1kblocks;
    u_int8_t            filler_11[3];
    u_int32_t           filler_12[54];

    u_int32_t           dummy_6[4];
    u_int8_t            timestamp_1[8];
    u_int32_t           filler_13[9];
    u_int8_t            stripe_sectors;
    u_int8_t            filler_14[3];
    u_int8_t            array_width;
    u_int8_t            filler_15[3];
    u_int32_t           filler_16;
    u_int8_t            filler_17;
    u_int8_t            disk_number;
    u_int32_t           disk_sectors;
    u_int16_t           filler_18;
    u_int32_t           dummy_7[4];
    u_int32_t           filler_20[104];
} __packed;


/* LSILogic V2 MegaRAID Metadata */
#define LSIV2_LBA(dev) \
	(((struct ad_softc *)device_get_ivars(dev))->total_secs - 1)

struct lsiv2_raid_conf {
    u_int8_t            lsi_id[6];
#define LSIV2_MAGIC             "$XIDE$"

    u_int8_t            dummy_0;
    u_int8_t            flags;
    u_int16_t           version;
    u_int8_t            config_entries;
    u_int8_t            raid_count;
    u_int8_t            total_disks;
    u_int8_t            dummy_1;
    u_int16_t           dummy_2;

    union {
	struct {
	    u_int8_t    type;
#define LSIV2_T_RAID0           0x01
#define LSIV2_T_RAID1           0x02
#define LSIV2_T_SPARE           0x08

	    u_int8_t    dummy_0;
	    u_int16_t   stripe_sectors;
	    u_int8_t    array_width;
	    u_int8_t    disk_count;
	    u_int8_t    config_offset;
	    u_int8_t    dummy_1;
	    u_int8_t    flags;
#define LSIV2_R_DEGRADED        0x02

	    u_int32_t   total_sectors;
	    u_int8_t    filler[3];
	} __packed raid;
	struct {
	    u_int8_t    device;
#define LSIV2_D_MASTER          0x00
#define LSIV2_D_SLAVE           0x01
#define LSIV2_D_CHANNEL0        0x00
#define LSIV2_D_CHANNEL1        0x10
#define LSIV2_D_NONE            0xff

	    u_int8_t    dummy_0;
	    u_int32_t   disk_sectors;
	    u_int8_t    disk_number;
	    u_int8_t    raid_number;
	    u_int8_t    flags;
#define LSIV2_D_GONE            0x02

	    u_int8_t    filler[7];
	} __packed disk;
    } configs[30];
    u_int8_t            disk_number;
    u_int8_t            raid_number;
    u_int32_t           timestamp;
    u_int8_t            filler[10];
} __packed;


/* LSILogic V3 MegaRAID Metadata */
#define LSIV3_LBA(dev) \
	(((struct ad_softc *)device_get_ivars(dev))->total_secs - 4)

struct lsiv3_raid_conf {
    u_int32_t           magic_0;        /* 0xa0203200 */
    u_int32_t           filler_0[3];
    u_int8_t            magic_1[4];     /* "SATA" */
    u_int32_t           filler_1[40];
    u_int32_t           dummy_0;        /* 0x0d000003 */
    u_int32_t           filler_2[7];
    u_int32_t           dummy_1;        /* 0x0d000003 */
    u_int32_t           filler_3[70];
    u_int8_t            magic_2[8];     /* "$_ENQ$31" */
    u_int8_t            filler_4[7];
    u_int8_t            checksum_0;
    u_int8_t            filler_5[512*2];
    u_int8_t            lsi_id[6];
#define LSIV3_MAGIC             "$_IDE$"

    u_int16_t           dummy_2;        /* 0x33de for OK disk */
    u_int16_t           version;        /* 0x0131 for this version */
    u_int16_t           dummy_3;        /* 0x0440 always */
    u_int32_t           filler_6;

    struct {
	u_int16_t       stripe_pages;
	u_int8_t        type;
#define LSIV3_T_RAID0           0x00
#define LSIV3_T_RAID1           0x01

	u_int8_t        dummy_0;
	u_int8_t        total_disks;
	u_int8_t        array_width;
	u_int8_t        filler_0[10];

	u_int32_t       sectors;
	u_int16_t       dummy_1;
	u_int32_t       offset;
	u_int16_t       dummy_2;
	u_int8_t        device;
#define LSIV3_D_DEVICE          0x01
#define LSIV3_D_CHANNEL         0x10

	u_int8_t        dummy_3;
	u_int8_t        dummy_4;
	u_int8_t        dummy_5;
	u_int8_t        filler_1[16];
    } __packed raid[8];
    struct {
	u_int32_t       disk_sectors;
	u_int32_t       dummy_0;
	u_int32_t       dummy_1;
	u_int8_t        dummy_2;
	u_int8_t        dummy_3;
	u_int8_t        flags;
#define LSIV3_D_MIRROR          0x00
#define LSIV3_D_STRIPE          0xff
	u_int8_t        dummy_4;
    } __packed disk[6];
    u_int8_t            filler_7[7];
    u_int8_t            device;
    u_int32_t           timestamp;
    u_int8_t            filler_8[3];
    u_int8_t            checksum_1;
} __packed;


/* nVidia MediaShield Metadata */
#define NVIDIA_LBA(dev) \
	(((struct ad_softc *)device_get_ivars(dev))->total_secs - 2)

struct nvidia_raid_conf {
    u_int8_t            nvidia_id[8];
#define NV_MAGIC                "NVIDIA  "

    u_int32_t           config_size;
    u_int32_t           checksum;
    u_int16_t           version;
    u_int8_t            disk_number;
    u_int8_t            dummy_0;
    u_int32_t           total_sectors;
    u_int32_t           sector_size;
    u_int8_t            serial[16];
    u_int8_t            revision[4];
    u_int32_t           dummy_1;

    u_int32_t           magic_0;
#define NV_MAGIC0               0x00640044

    u_int64_t           magic_1;
    u_int64_t           magic_2;
    u_int8_t            flags;
    u_int8_t            array_width;
    u_int8_t            total_disks;
    u_int8_t            dummy_2;
    u_int16_t           type;
#define NV_T_RAID0              0x00000080
#define NV_T_RAID1              0x00000081
#define NV_T_RAID3              0x00000083
#define NV_T_RAID5              0x00000085
#define NV_T_RAID01             0x00008180
#define NV_T_SPAN               0x000000ff

    u_int16_t           dummy_3;
    u_int32_t           stripe_sectors;
    u_int32_t           stripe_bytes;
    u_int32_t           stripe_shift;
    u_int32_t           stripe_mask;
    u_int32_t           stripe_sizesectors;
    u_int32_t           stripe_sizebytes;
    u_int32_t           rebuild_lba;
    u_int32_t           dummy_4;
    u_int32_t           dummy_5;
    u_int32_t           status;
#define NV_S_BOOTABLE           0x00000001
#define NV_S_DEGRADED           0x00000002

    u_int32_t           filler[98];
} __packed;


/* Promise FastTrak Metadata */
#define PROMISE_LBA(dev) \
	(((((struct ad_softc *)device_get_ivars(dev))->total_secs / (((struct ad_softc *)device_get_ivars(dev))->heads * ((struct ad_softc *)device_get_ivars(dev))->sectors)) * ((struct ad_softc *)device_get_ivars(dev))->heads * ((struct ad_softc *)device_get_ivars(dev))->sectors) - ((struct ad_softc *)device_get_ivars(dev))->sectors)

struct promise_raid_conf {
    char                promise_id[24];
#define PR_MAGIC                "Promise Technology, Inc."

    u_int32_t           dummy_0;
    u_int64_t           magic_0;
#define PR_MAGIC0(x)            (((u_int64_t)(x.channel) << 48) | \
				((u_int64_t)(x.device != 0) << 56))
    u_int16_t           magic_1;
    u_int32_t           magic_2;
    u_int8_t            filler1[470];
    struct {
	u_int32_t       integrity;
#define PR_I_VALID              0x00000080

	u_int8_t        flags;
#define PR_F_VALID              0x00000001
#define PR_F_ONLINE             0x00000002
#define PR_F_ASSIGNED           0x00000004
#define PR_F_SPARE              0x00000008
#define PR_F_DUPLICATE          0x00000010
#define PR_F_REDIR              0x00000020
#define PR_F_DOWN               0x00000040
#define PR_F_READY              0x00000080

	u_int8_t        disk_number;
	u_int8_t        channel;
	u_int8_t        device;
	u_int64_t       magic_0 __packed;
	u_int32_t       disk_offset;
	u_int32_t       disk_sectors;
	u_int32_t       rebuild_lba;
	u_int16_t       generation;
	u_int8_t        status;
#define PR_S_VALID              0x01
#define PR_S_ONLINE             0x02
#define PR_S_INITED             0x04
#define PR_S_READY              0x08
#define PR_S_DEGRADED           0x10
#define PR_S_MARKED             0x20
#define PR_S_FUNCTIONAL         0x80

	u_int8_t        type;
#define PR_T_RAID0              0x00
#define PR_T_RAID1              0x01
#define PR_T_RAID3              0x02
#define PR_T_RAID5              0x04
#define PR_T_SPAN               0x08
#define PR_T_JBOD               0x10

	u_int8_t        total_disks;
	u_int8_t        stripe_shift;
	u_int8_t        array_width;
	u_int8_t        array_number;
	u_int32_t       total_sectors;
	u_int16_t       cylinders;
	u_int8_t        heads;
	u_int8_t        sectors;
	u_int64_t       magic_1 __packed;
	struct {
	    u_int8_t    flags;
	    u_int8_t    dummy_0;
	    u_int8_t    channel;
	    u_int8_t    device;
	    u_int64_t   magic_0 __packed;
	} disk[8];
    } raid;
    int32_t             filler2[346];
    u_int32_t           checksum;
} __packed;


/* Silicon Image Medley Metadata */
#define SII_LBA(dev) \
	( ((struct ad_softc *)device_get_ivars(dev))->total_secs - 1)

struct sii_raid_conf {
    u_int16_t   	ata_params_00_53[54];
    u_int64_t   	total_sectors;
    u_int16_t   	ata_params_58_79[70];
    u_int16_t   	dummy_0;
    u_int16_t   	dummy_1;
    u_int32_t   	controller_pci_id;
    u_int16_t   	version_minor;
    u_int16_t   	version_major;
    u_int8_t    	timestamp[6];
    u_int16_t   	stripe_sectors;
    u_int16_t   	dummy_2;
    u_int8_t    	disk_number;
    u_int8_t    	type;
#define SII_T_RAID0             0x00
#define SII_T_RAID1             0x01
#define SII_T_RAID01            0x02
#define SII_T_SPARE             0x03

    u_int8_t    	raid0_disks;
    u_int8_t    	raid0_ident;
    u_int8_t    	raid1_disks;
    u_int8_t    	raid1_ident;
    u_int64_t   	rebuild_lba;
    u_int32_t   	generation;
    u_int8_t    	status;
#define SII_S_READY             0x01
    
    u_int8_t    	base_raid1_position;
    u_int8_t    	base_raid0_position;
    u_int8_t    	position;
    u_int16_t   	dummy_3;
    u_int8_t    	name[16];
    u_int16_t   	checksum_0;
    int8_t      	filler1[190];
    u_int16_t   	checksum_1;
} __packed;


/* Silicon Integrated Systems RAID Metadata */
#define SIS_LBA(dev) \
	( ((struct ad_softc *)device_get_ivars(dev))->total_secs - 16)

struct sis_raid_conf {
    u_int16_t   	magic;
#define SIS_MAGIC               0x0010

    u_int8_t		disks;
#define SIS_D_MASTER          	0xf0
#define SIS_D_MIRROR          	0x0f

    u_int8_t		type_total_disks;
#define SIS_D_MASK          	0x0f
#define SIS_T_MASK              0xf0
#define SIS_T_JBOD              0x10
#define SIS_T_RAID0         	0x20
#define SIS_T_RAID1             0x30

    u_int32_t   	dummy_0;
    u_int32_t   	controller_pci_id;
    u_int16_t   	stripe_sectors;
    u_int16_t   	dummy_1;
    u_int32_t   	timestamp;
    u_int8_t		model[40];
    u_int8_t		disk_number;
    u_int8_t		dummy_2[3];
    int8_t      	filler1[448];
} __packed;


/* VIA Tech V-RAID Metadata */
#define VIA_LBA(dev) \
	( ((struct ad_softc *)device_get_ivars(dev))->total_secs - 1)

struct via_raid_conf {
    u_int16_t   	magic;
#define VIA_MAGIC               0xaa55

    u_int8_t    	dummy_0;
    u_int8_t    	type;
#define VIA_T_MASK              0x7e
#define VIA_T_BOOTABLE          0x01
#define VIA_T_RAID0             0x04
#define VIA_T_RAID1             0x0c
#define VIA_T_RAID01            0x4c
#define VIA_T_RAID5             0x2c
#define VIA_T_SPAN              0x44
#define VIA_T_UNKNOWN           0x80

    u_int8_t    	disk_index;
#define VIA_D_MASK		0x0f
#define VIA_D_DEGRADED		0x10
#define VIA_D_HIGH_IDX		0x20

    u_int8_t    	stripe_layout;
#define VIA_L_DISKS             0x07
#define VIA_L_MASK              0xf0
#define VIA_L_SHIFT       	4

    u_int64_t   	disk_sectors;
    u_int32_t   	disk_id;
    u_int32_t   	disks[8];
    u_int8_t    	checksum;
    u_int8_t    	filler_1[461];
} __packed;
