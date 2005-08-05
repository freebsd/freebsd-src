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

#ifndef _SYS_ATA_H_
#define _SYS_ATA_H_

#include <sys/ioccom.h>

/* ATA/ATAPI device parameters */
struct ata_params {
/*000*/ u_int16_t       config;         /* configuration info */
#define ATA_PROTO_MASK                  0x8003
#define ATA_PROTO_ATAPI                 0x8000
#define ATA_PROTO_ATAPI_12              0x8000
#define ATA_PROTO_ATAPI_16              0x8001
#define ATA_ATAPI_TYPE_MASK             0x1f00
#define ATA_ATAPI_TYPE_DIRECT           0x0000  /* disk/floppy */
#define ATA_ATAPI_TYPE_TAPE             0x0100  /* streaming tape */
#define ATA_ATAPI_TYPE_CDROM            0x0500  /* CD-ROM device */
#define ATA_ATAPI_TYPE_OPTICAL          0x0700  /* optical disk */
#define ATA_DRQ_MASK                    0x0060
#define ATA_DRQ_SLOW                    0x0000  /* cpu 3 ms delay */
#define ATA_DRQ_INTR                    0x0020  /* interrupt 10 ms delay */
#define ATA_DRQ_FAST                    0x0040  /* accel 50 us delay */

/*001*/ u_int16_t       cylinders;              /* # of cylinders */
	u_int16_t       reserved2;
/*003*/ u_int16_t       heads;                  /* # heads */
	u_int16_t       obsolete4;
	u_int16_t       obsolete5;
/*006*/ u_int16_t       sectors;                /* # sectors/track */
/*007*/ u_int16_t       vendor7[3];
/*010*/ u_int8_t        serial[20];             /* serial number */
/*020*/ u_int16_t       retired20;
	u_int16_t       retired21;
	u_int16_t       obsolete22;
/*023*/ u_int8_t        revision[8];            /* firmware revision */
/*027*/ u_int8_t        model[40];              /* model name */
/*047*/ u_int16_t       sectors_intr;           /* sectors per interrupt */
/*048*/ u_int16_t       usedmovsd;              /* double word read/write? */
/*049*/ u_int16_t       capabilities1;
#define ATA_SUPPORT_DMA                 0x0100
#define ATA_SUPPORT_LBA                 0x0200
#define ATA_SUPPORT_OVERLAP             0x4000

/*050*/ u_int16_t       capabilities2;
/*051*/ u_int16_t       retired_piomode;        /* PIO modes 0-2 */
#define ATA_RETIRED_PIO_MASK            0x0300

/*052*/ u_int16_t       retired_dmamode;        /* DMA modes */
#define ATA_RETIRED_DMA_MASK            0x0003

/*053*/ u_int16_t       atavalid;               /* fields valid */
#define ATA_FLAG_54_58                  0x0001  /* words 54-58 valid */
#define ATA_FLAG_64_70                  0x0002  /* words 64-70 valid */
#define ATA_FLAG_88                     0x0004  /* word 88 valid */

/*054*/ u_int16_t       current_cylinders;
/*055*/ u_int16_t       current_heads;
/*056*/ u_int16_t       current_sectors;
/*057*/ u_int16_t       current_size_1;
/*058*/ u_int16_t       current_size_2;
/*059*/ u_int16_t       multi;
#define ATA_MULTI_VALID                 0x0100

/*060*/ u_int16_t       lba_size_1;
	u_int16_t       lba_size_2;
	u_int16_t       obsolete62;
/*063*/ u_int16_t       mwdmamodes;             /* multiword DMA modes */
/*064*/ u_int16_t       apiomodes;              /* advanced PIO modes */

/*065*/ u_int16_t       mwdmamin;               /* min. M/W DMA time/word ns */
/*066*/ u_int16_t       mwdmarec;               /* rec. M/W DMA time ns */
/*067*/ u_int16_t       pioblind;               /* min. PIO cycle w/o flow */
/*068*/ u_int16_t       pioiordy;               /* min. PIO cycle IORDY flow */
	u_int16_t       reserved69;
	u_int16_t       reserved70;
/*071*/ u_int16_t       rlsovlap;               /* rel time (us) for overlap */
/*072*/ u_int16_t       rlsservice;             /* rel time (us) for service */
	u_int16_t       reserved73;
	u_int16_t       reserved74;
/*075*/ u_int16_t       queue;
#define ATA_QUEUE_LEN(x)                ((x) & 0x001f)

	u_int16_t       satacapabilities;
#define ATA_SATA_GEN1                   0x0002
#define ATA_SATA_GEN2                   0x0004
#define ATA_SUPPORT_NCQ                 0x0100
#define ATA_SUPPORT_IFPWRMNGTRCV        0x0200

	u_int16_t       reserved77;
	u_int16_t       satasupport;
#define ATA_SUPPORT_NONZERO             0x0002
#define ATA_SUPPORT_AUTOACTIVATE        0x0004
#define ATA_SUPPORT_IFPWRMNGT           0x0008
#define ATA_SUPPORT_INORDERDATA         0x0010
	u_int16_t       sataenabled;

/*080*/ u_int16_t       version_major;
/*081*/ u_int16_t       version_minor;

	struct {
/*082/085*/ u_int16_t   command1;
#define ATA_SUPPORT_SMART               0x0001
#define ATA_SUPPORT_SECURITY            0x0002
#define ATA_SUPPORT_REMOVABLE           0x0004
#define ATA_SUPPORT_POWERMGT            0x0008
#define ATA_SUPPORT_PACKET              0x0010
#define ATA_SUPPORT_WRITECACHE          0x0020
#define ATA_SUPPORT_LOOKAHEAD           0x0040
#define ATA_SUPPORT_RELEASEIRQ          0x0080
#define ATA_SUPPORT_SERVICEIRQ          0x0100
#define ATA_SUPPORT_RESET               0x0200
#define ATA_SUPPORT_PROTECTED           0x0400
#define ATA_SUPPORT_WRITEBUFFER         0x1000
#define ATA_SUPPORT_READBUFFER          0x2000
#define ATA_SUPPORT_NOP                 0x4000

/*083/086*/ u_int16_t   command2;
#define ATA_SUPPORT_MICROCODE           0x0001
#define ATA_SUPPORT_QUEUED              0x0002
#define ATA_SUPPORT_CFA                 0x0004
#define ATA_SUPPORT_APM                 0x0008
#define ATA_SUPPORT_NOTIFY              0x0010
#define ATA_SUPPORT_STANDBY             0x0020
#define ATA_SUPPORT_SPINUP              0x0040
#define ATA_SUPPORT_MAXSECURITY         0x0100
#define ATA_SUPPORT_AUTOACOUSTIC        0x0200
#define ATA_SUPPORT_ADDRESS48           0x0400
#define ATA_SUPPORT_OVERLAY             0x0800
#define ATA_SUPPORT_FLUSHCACHE          0x1000
#define ATA_SUPPORT_FLUSHCACHE48        0x2000

/*084/087*/ u_int16_t   extension;
	} __packed support, enabled;

/*088*/ u_int16_t       udmamodes;              /* UltraDMA modes */
/*089*/ u_int16_t       erase_time;
/*090*/ u_int16_t       enhanced_erase_time;
/*091*/ u_int16_t       apm_value;
/*092*/ u_int16_t       master_passwd_revision;
/*093*/ u_int16_t       hwres;
#define ATA_CABLE_ID                    0x2000

/*094*/ u_int16_t       acoustic;
#define ATA_ACOUSTIC_CURRENT(x)         ((x) & 0x00ff)
#define ATA_ACOUSTIC_VENDOR(x)          (((x) & 0xff00) >> 8)

/*095*/ u_int16_t       stream_min_req_size;
/*096*/ u_int16_t       stream_transfer_time;
/*097*/ u_int16_t       stream_access_latency;
/*098*/ u_int32_t       stream_granularity;
/*100*/ u_int16_t       lba_size48_1;
	u_int16_t       lba_size48_2;
	u_int16_t       lba_size48_3;
	u_int16_t       lba_size48_4;
	u_int16_t       reserved104[23];
/*127*/ u_int16_t       removable_status;
/*128*/ u_int16_t       security_status;
	u_int16_t       reserved129[31];
/*160*/ u_int16_t       cfa_powermode1;
	u_int16_t       reserved161[15];
/*176*/ u_int16_t       media_serial[30];
	u_int16_t       reserved206[49];
/*255*/ u_int16_t       integrity;
} __packed;


/* ATA transfer modes */
#define ATA_MODE_MASK           0x0f
#define ATA_DMA_MASK            0xf0
#define ATA_PIO                 0x00
#define ATA_PIO0                0x08
#define ATA_PIO1                0x09
#define ATA_PIO2                0x0a
#define ATA_PIO3                0x0b
#define ATA_PIO4                0x0c
#define ATA_PIO_MAX             0x0f
#define ATA_DMA                 0x10
#define ATA_WDMA0               0x20
#define ATA_WDMA1               0x21
#define ATA_WDMA2               0x22
#define ATA_UDMA0               0x40
#define ATA_UDMA1               0x41
#define ATA_UDMA2               0x42
#define ATA_UDMA3               0x43
#define ATA_UDMA4               0x44
#define ATA_UDMA5               0x45
#define ATA_UDMA6               0x46
#define ATA_SA150               0x47
#define ATA_DMA_MAX             0x4f


/* ATA commands */
#define ATA_NOP                         0x00    /* NOP */
#define         ATA_NF_FLUSHQUEUE       0x00    /* flush queued cmd's */
#define         ATA_NF_AUTOPOLL         0x01    /* start autopoll function */
#define ATA_DEVICE_RESET                0x08    /* reset device */
#define ATA_READ                        0x20    /* read */
#define ATA_READ48                      0x24    /* read 48bit LBA */
#define ATA_READ_DMA48                  0x25    /* read DMA 48bit LBA */
#define ATA_READ_DMA_QUEUED48           0x26    /* read DMA QUEUED 48bit LBA */
#define ATA_READ_MUL48                  0x29    /* read multi 48bit LBA */
#define ATA_WRITE                       0x30    /* write */
#define ATA_WRITE48                     0x34    /* write 48bit LBA */
#define ATA_WRITE_DMA48                 0x35    /* write DMA 48bit LBA */
#define ATA_WRITE_DMA_QUEUED48          0x36    /* write DMA QUEUED 48bit LBA*/
#define ATA_WRITE_MUL48                 0x39    /* write multi 48bit LBA */
#define ATA_READ_FPDMA_QUEUED           0x60    /* read DMA NCQ */
#define ATA_WRITE_FPDMA_QUEUED          0x61    /* write DMA NCQ */
#define ATA_SEEK                        0x70    /* seek */
#define ATA_PACKET_CMD                  0xa0    /* packet command */
#define ATA_ATAPI_IDENTIFY              0xa1    /* get ATAPI params*/
#define ATA_SERVICE                     0xa2    /* service command */
#define ATA_CFA_ERASE                   0xc0    /* CFA erase */
#define ATA_READ_MUL                    0xc4    /* read multi */
#define ATA_WRITE_MUL                   0xc5    /* write multi */
#define ATA_SET_MULTI                   0xc6    /* set multi size */
#define ATA_READ_DMA_QUEUED             0xc7    /* read DMA QUEUED */
#define ATA_READ_DMA                    0xc8    /* read DMA */
#define ATA_WRITE_DMA                   0xca    /* write DMA */
#define ATA_WRITE_DMA_QUEUED            0xcc    /* write DMA QUEUED */
#define ATA_STANDBY_IMMEDIATE           0xe0    /* standby immediate */
#define ATA_IDLE_IMMEDIATE              0xe1    /* idle immediate */
#define ATA_STANDBY_CMD                 0xe2    /* standby */
#define ATA_IDLE_CMD                    0xe3    /* idle */
#define ATA_READ_BUFFER                 0xe4    /* read buffer */
#define ATA_SLEEP                       0xe6    /* sleep */
#define ATA_FLUSHCACHE                  0xe7    /* flush cache to disk */
#define ATA_FLUSHCACHE48                0xea    /* flush cache to disk */
#define ATA_ATA_IDENTIFY                0xec    /* get ATA params */
#define ATA_SETFEATURES                 0xef    /* features command */
#define         ATA_SF_SETXFER          0x03    /* set transfer mode */
#define         ATA_SF_ENAB_WCACHE      0x02    /* enable write cache */
#define         ATA_SF_DIS_WCACHE       0x82    /* disable write cache */
#define         ATA_SF_ENAB_RCACHE      0xaa    /* enable readahead cache */
#define         ATA_SF_DIS_RCACHE       0x55    /* disable readahead cache */
#define         ATA_SF_ENAB_RELIRQ      0x5d    /* enable release interrupt */
#define         ATA_SF_DIS_RELIRQ       0xdd    /* disable release interrupt */
#define         ATA_SF_ENAB_SRVIRQ      0x5e    /* enable service interrupt */
#define         ATA_SF_DIS_SRVIRQ       0xde    /* disable service interrupt */
#define ATA_SECURITY_FREEE_LOCK         0xf5    /* freeze security config */
#define ATA_READ_NATIVE_MAX_ADDDRESS    0xf8    /* read native max address */
#define ATA_SET_MAX_ADDRESS             0xf9    /* set max address */


/* ATAPI commands */
#define ATAPI_TEST_UNIT_READY           0x00    /* check if device is ready */
#define ATAPI_REZERO                    0x01    /* rewind */
#define ATAPI_REQUEST_SENSE             0x03    /* get sense data */
#define ATAPI_FORMAT                    0x04    /* format unit */
#define ATAPI_READ                      0x08    /* read data */
#define ATAPI_WRITE                     0x0a    /* write data */
#define ATAPI_WEOF                      0x10    /* write filemark */
#define         ATAPI_WF_WRITE          0x01
#define ATAPI_SPACE                     0x11    /* space command */
#define         ATAPI_SP_FM             0x01
#define         ATAPI_SP_EOD            0x03
#define ATAPI_MODE_SELECT               0x15    /* mode select */
#define ATAPI_ERASE                     0x19    /* erase */
#define ATAPI_MODE_SENSE                0x1a    /* mode sense */
#define ATAPI_START_STOP                0x1b    /* start/stop unit */
#define         ATAPI_SS_LOAD           0x01
#define         ATAPI_SS_RETENSION      0x02
#define         ATAPI_SS_EJECT          0x04
#define ATAPI_PREVENT_ALLOW             0x1e    /* media removal */
#define ATAPI_READ_FORMAT_CAPACITIES    0x23    /* get format capacities */
#define ATAPI_READ_CAPACITY             0x25    /* get volume capacity */
#define ATAPI_READ_BIG                  0x28    /* read data */
#define ATAPI_WRITE_BIG                 0x2a    /* write data */
#define ATAPI_LOCATE                    0x2b    /* locate to position */
#define ATAPI_READ_POSITION             0x34    /* read position */
#define ATAPI_SYNCHRONIZE_CACHE         0x35    /* flush buf, close channel */
#define ATAPI_WRITE_BUFFER              0x3b    /* write device buffer */
#define ATAPI_READ_BUFFER               0x3c    /* read device buffer */
#define ATAPI_READ_SUBCHANNEL           0x42    /* get subchannel info */
#define ATAPI_READ_TOC                  0x43    /* get table of contents */
#define ATAPI_PLAY_10                   0x45    /* play by lba */
#define ATAPI_PLAY_MSF                  0x47    /* play by MSF address */
#define ATAPI_PLAY_TRACK                0x48    /* play by track number */
#define ATAPI_PAUSE                     0x4b    /* pause audio operation */
#define ATAPI_READ_DISK_INFO            0x51    /* get disk info structure */
#define ATAPI_READ_TRACK_INFO           0x52    /* get track info structure */
#define ATAPI_RESERVE_TRACK             0x53    /* reserve track */
#define ATAPI_SEND_OPC_INFO             0x54    /* send OPC structurek */
#define ATAPI_MODE_SELECT_BIG           0x55    /* set device parameters */
#define ATAPI_REPAIR_TRACK              0x58    /* repair track */
#define ATAPI_READ_MASTER_CUE           0x59    /* read master CUE info */
#define ATAPI_MODE_SENSE_BIG            0x5a    /* get device parameters */
#define ATAPI_CLOSE_TRACK               0x5b    /* close track/session */
#define ATAPI_READ_BUFFER_CAPACITY      0x5c    /* get buffer capicity */
#define ATAPI_SEND_CUE_SHEET            0x5d    /* send CUE sheet */
#define ATAPI_BLANK                     0xa1    /* blank the media */
#define ATAPI_SEND_KEY                  0xa3    /* send DVD key structure */
#define ATAPI_REPORT_KEY                0xa4    /* get DVD key structure */
#define ATAPI_PLAY_12                   0xa5    /* play by lba */
#define ATAPI_LOAD_UNLOAD               0xa6    /* changer control command */
#define ATAPI_READ_STRUCTURE            0xad    /* get DVD structure */
#define ATAPI_PLAY_CD                   0xb4    /* universal play command */
#define ATAPI_SET_SPEED                 0xbb    /* set drive speed */
#define ATAPI_MECH_STATUS               0xbd    /* get changer status */
#define ATAPI_READ_CD                   0xbe    /* read data */
#define ATAPI_POLL_DSC                  0xff    /* poll DSC status bit */


struct ata_ioc_devices {
    int                 channel;
    char                name[2][32];
    struct ata_params   params[2];
};

/* pr channel ATA ioctl calls */
#define IOCATAGMAXCHANNEL       _IOR('a',  1, int)
#define IOCATAREINIT            _IOW('a',  2, int)
#define IOCATAATTACH            _IOW('a',  3, int)
#define IOCATADETACH            _IOW('a',  4, int)
#define IOCATADEVICES           _IOWR('a',  5, struct ata_ioc_devices)

struct ata_ioc_request {
    union {
	struct {
	    u_int8_t            command;
	    u_int8_t            feature;
	    u_int64_t           lba;
	    u_int16_t           count;
	} ata;
	struct {
	    char                ccb[16];
	} atapi;
    } u;
    caddr_t             data;
    int                 count;
    int                 flags;
#define ATA_CMD_CONTROL                 0x01
#define ATA_CMD_READ                    0x02
#define ATA_CMD_WRITE                   0x04
#define ATA_CMD_ATAPI                   0x08

    int                 timeout;
    int                 error;
};

/* pr device ATA ioctl calls */
#define IOCATAREQUEST           _IOWR('a', 100, struct ata_ioc_request)
#define IOCATAGPARM             _IOR('a', 101, struct ata_params)
#define IOCATAGMODE             _IOR('a', 102, int)
#define IOCATASMODE             _IOW('a', 103, int)


struct ata_ioc_raid_config {
	    int                 lun;
	    int                 type;
#define AR_JBOD                         0x0001
#define AR_SPAN                         0x0002
#define AR_RAID0                        0x0004
#define AR_RAID1                        0x0008
#define AR_RAID01                       0x0010
#define AR_RAID3                        0x0020
#define AR_RAID4                        0x0040
#define AR_RAID5                        0x0080

	    int                 interleave;
	    int                 status;
#define AR_READY                        1
#define AR_DEGRADED                     2
#define AR_REBUILDING                   4

	    int                 progress;
	    int                 total_disks;
	    int                 disks[16];
};

/* ATA RAID ioctl calls */
#define IOCATARAIDCREATE        _IOW('a', 200, struct ata_ioc_raid_config)
#define IOCATARAIDDELETE        _IOW('a', 201, int)
#define IOCATARAIDSTATUS        _IOWR('a', 202, struct ata_ioc_raid_config)
#define IOCATARAIDADDSPARE      _IOW('a', 203, struct ata_ioc_raid_config)
#define IOCATARAIDREBUILD       _IOW('a', 204, int)

#endif /* _SYS_ATA_H_ */
