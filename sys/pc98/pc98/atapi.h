/*
 * Device-independent level for ATAPI drivers.
 *
 * Copyright (C) 1995 Cronyx Ltd.
 * Author Serge Vakulenko, <vak@cronyx.ru>
 *
 * This software is distributed with NO WARRANTIES, not even the implied
 * warranties for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 * Authors grant any other persons or organizations permission to use
 * or modify this software as long as this message is kept with the software,
 * all derivative works or modified versions.
 *
 * Version 1.9, Thu Oct 12 15:53:50 MSK 1995
 */

/*
 * Disk Controller ATAPI register definitions.
 */
#ifdef PC98
#define AR_DATA         0x0             /* RW - data register (16 bits) */
#define AR_ERROR        0x2             /*  R - error register */
#define AR_FEATURES     0x2             /*  W - features */
#define AR_IREASON      0x4             /* RW - interrupt reason */
#define AR_TAG          0x6             /*    - reserved for SAM TAG byte */
#define AR_CNTLO        0x8             /* RW - byte count, low byte */
#define AR_CNTHI        0xa             /* RW - byte count, high byte */
#define AR_DRIVE        0xc             /* RW - drive select */
#define AR_COMMAND      0xe             /*  W - command register */
#define AR_STATUS       0xe             /*  R - immediate status */
#else
#define AR_DATA         0x0             /* RW - data register (16 bits) */
#define AR_ERROR        0x1             /*  R - error register */
#define AR_FEATURES     0x1             /*  W - features */
#define AR_IREASON      0x2             /* RW - interrupt reason */
#define AR_TAG          0x3             /*    - reserved for SAM TAG byte */
#define AR_CNTLO        0x4             /* RW - byte count, low byte */
#define AR_CNTHI        0x5             /* RW - byte count, high byte */
#define AR_DRIVE        0x6             /* RW - drive select */
#define AR_COMMAND      0x7             /*  W - command register */
#define AR_STATUS       0x7             /*  R - immediate status */
#endif

/*
 * Status register bits
 */
#define ARS_CHECK       0x01            /* error occured, see sense key/code */
					/* bit 0x02 reserved */
#define ARS_CORR        0x04            /* correctable error occured */
#define ARS_DRQ         0x08            /* data request / ireason valid */
#define ARS_DSC         0x10            /* immediate operation completed */
#define ARS_DF          0x20            /* drive fault */
#define ARS_DRDY        0x40            /* ready to get command */
#define ARS_BSY         0x80            /* registers busy */
					/* for overlap mode only: */
#define ARS_SERVICE     0x10            /* service is requested */
#define ARS_DMARDY      0x20            /* ready to start a DMA transfer */
#define ARS_BITS        "\20\010busy\7ready\6fault\5opdone\4drq\3corr\1check"

/*
 * Error register bits
 */
#define AER_ILI         0x01            /* illegal length indication */
#define AER_EOM         0x02            /* end of media detected */
#define AER_ABRT        0x04            /* command aborted */
#define AER_MCR         0x08            /* media change requested */
#define AER_SKEY        0xf0            /* sense key mask */
#define AER_SK_NO_SENSE         0x00    /* no specific sense key info */
#define AER_SK_RECOVERED_ERROR  0x10    /* command succeeded, data recovered */
#define AER_SK_NOT_READY        0x20    /* no access to drive */
#define AER_SK_MEDIUM_ERROR     0x30    /* non-recovered data error */
#define AER_SK_HARDWARE_ERROR   0x40    /* non-recoverable hardware failure */
#define AER_SK_ILLEGAL_REQUEST  0x50    /* invalid command parameter(s) */
#define AER_SK_UNIT_ATTENTION   0x60    /* media changed */
#define AER_SK_DATA_PROTECT     0x70    /* reading read-protected sector */
#define AER_SK_ABORTED_COMMAND  0xb0    /* command aborted, try again */
#define AER_SK_MISCOMPARE       0xe0    /* data did not match the medium */
#define AER_BITS        "\20\4mchg\3abort\2eom\1ili"

/*
 * Feature register bits
 */
#define ARF_DMA         0x01            /* transfer data via DMA */
#define ARF_OVERLAP     0x02            /* release the bus until completion */

/*
 * Interrupt reason register bits
 */
#define ARI_CMD         0x01            /* command(1) or data(0) */
#define ARI_IN          0x02            /* transfer to(1) or from(0) the host */
#define ARI_RELEASE     0x04            /* bus released until completion */

/*
 * Drive register values
 */
#define ARD_DRIVE0      0xa0            /* drive 0 selected */
#define ARD_DRIVE1      0xb0            /* drive 1 selected */

/*
 * ATA commands
 */
#define ATAPIC_IDENTIFY         0xa1    /* get drive parameters */
#define ATAPIC_PACKET           0xa0    /* execute packet command */

/*
 * Mandatory packet commands
 */
#define ATAPI_TEST_UNIT_READY   0x00    /* check if the device is ready */
#define ATAPI_REQUEST_SENSE     0x03    /* get sense data */
#define ATAPI_START_STOP        0x1b    /* start/stop the media */
#define ATAPI_PREVENT_ALLOW     0x1e    /* prevent/allow media removal */
#define ATAPI_READ_CAPACITY     0x25    /* get volume capacity */
#define ATAPI_READ_BIG          0x28    /* read data */
#define ATAPI_WRITE_BIG		0x2a    /* write data */
#define ATAPI_READ_TOC          0x43    /* get table of contents */
#define ATAPI_READ_SUBCHANNEL   0x42    /* get subchannel info */
#define ATAPI_MODE_SELECT_BIG   0x55    /* set device parameters */
#define ATAPI_MODE_SENSE        0x5a    /* get device parameters */
#define ATAPI_PLAY_CD           0xb4    /* universal play command */
#define ATAPI_MECH_STATUS       0xbd    /* get changer mechanism status */
#define ATAPI_LOAD_UNLOAD       0xa6    /* changer control command */

/*
 * Optional packet commands
 */
#define ATAPI_PLAY_MSF          0x47    /* play by MSF address */
#define ATAPI_PAUSE             0x4b    /* stop/start audio operation */

/*
 * Nonstandard packet commands
 */
#define ATAPI_PLAY_TRACK        0x48    /* play by track number */
#define ATAPI_PLAY_BIG          0xa5    /* play by logical block address */

/*
 * Drive parameter information
 */
struct atapi_params {
	unsigned        cmdsz : 2;      /* packet command size */
#define AT_PSIZE_12     0               /* 12 bytes */
#define AT_PSIZE_16     1               /* 16 bytes */
	unsigned : 3;
	unsigned        drqtype : 2;    /* DRQ type */
#define AT_DRQT_MPROC   0               /* microprocessor DRQ - 3 msec delay */
#define AT_DRQT_INTR    1               /* interrupt DRQ - 10 msec delay */
#define AT_DRQT_ACCEL   2               /* accelerated DRQ - 50 usec delay */
	unsigned        removable : 1;  /* device is removable */
	unsigned        devtype : 5;    /* device type */
#define AT_TYPE_DIRECT  0               /* direct-access (magnetic disk) */
#define AT_TYPE_TAPE    1               /* streaming tape (QIC-121 model) */
#define AT_TYPE_CDROM   5               /* CD-ROM device */
#define AT_TYPE_OPTICAL 7               /* optical disk */
	unsigned : 1;
	unsigned        proto : 2;      /* command protocol */
#define AT_PROTO_ATAPI  2
	short reserved1[9];
	char            serial[20];     /* serial number - optional */
	short reserved2[3];
	char            revision[8];    /* firmware revision */
	char            model[40];      /* model name */
	short reserved3[2];
	u_char          vendor_cap;     /* vendor unique capabilities */
	unsigned        dmaflag : 1;    /* DMA supported */
	unsigned        lbaflag : 1;    /* LBA supported - always 1 */
	unsigned        iordydis : 1;   /* IORDY can be disabled */
	unsigned        iordyflag : 1;  /* IORDY supported */
	unsigned : 1;
	unsigned        ovlapflag : 1;  /* overlap operation supported */
	unsigned : 1;
	unsigned        idmaflag : 1;   /* interleaved DMA supported */
	short reserved4;
	u_short         pio_timing;     /* PIO cycle timing */
	u_short         dma_timing;     /* DMA cycle timing */
	u_short         flags;
#define AT_FLAG_54_58   1               /* words 54-58 valid */
#define AT_FLAG_64_70   2               /* words 64-70 valid */
	short reserved5[8];
	u_char          swdma_flag;     /* singleword DMA mode supported */
	u_char          swdma_active;   /* singleword DMA mode active */
	u_char          mwdma_flag;     /* multiword DMA mode supported */
	u_char          mwdma_active;   /* multiword DMA mode active */
	u_char          apio_flag;      /* advanced PIO mode supported */
	u_char reserved6;
	u_short         mwdma_min;      /* min. M/W DMA time per word (ns) */
	u_short         mwdma_dflt;     /* recommended M/W DMA time (ns) - optional */
	u_short         pio_nfctl_min;  /* min. PIO cycle time w/o flow ctl - optional */
	u_short         pio_iordy_min;  /* min. PIO c/t with IORDY flow ctl - optional */
	short reserved7[2];
	u_short         rls_ovlap;      /* release time (us) for overlap cmd - optional */
	u_short         rls_service;    /* release time (us) for service cmd - optional */
};

/*
 * ATAPI operation result structure
 */
struct atapires {
	u_char code;                    /* result code */
#define RES_OK          0               /* i/o done */
#define RES_ERR         1               /* i/o finished with error */
#define RES_NOTRDY      2               /* controller not ready */
#define RES_NODRQ       3               /* no data request */
#define RES_INVDIR      4               /* invalid bus phase direction */
#define RES_OVERRUN     5               /* data overrun */
#define RES_UNDERRUN    6               /* data underrun */
	u_char status;                  /* status register contents */
	u_char error;                   /* error register contents */
};

struct atapidrv {                       /* delayed attach info */
	int ctlr;                       /* IDE controller, 0/1 */
	int unit;                       /* drive unit, 0/1 */
	int port;                       /* controller base port */
	int attached;                   /* the drive is attached */
};

struct buf;
struct wcd;
typedef void atapi_callback_t(struct wcd *, struct buf *, int, struct atapires);

struct atapicmd {                       /* ATAPI command block */
	struct atapicmd *next;          /* next command in queue */
	int              busy;          /* busy flag */
	u_char           cmd[16];       /* command and args */
	int              unit;          /* drive unit number */
	int              count;         /* byte count, >0 - read, <0 - write */
	char            *addr;          /* data to transfer */
	atapi_callback_t *callback;     /* call when done */
	void            *cbarg1;        /* callback arg 1 */
	void            *cbarg2;        /* callback arg 1 */
	struct atapires  result;        /* resulting error code */
};

struct atapi {                          /* ATAPI controller data */
	u_short          port;          /* i/o port base */
	u_char           ctrlr;         /* physical controller number */
	u_char           debug : 1;     /* trace enable flag */
	u_char           cmd16 : 1;     /* 16-byte command flag */
	u_char           intrcmd : 1;   /* interrupt before cmd flag */
	u_char           slow : 1;      /* slow reaction device */
	u_char           attached[2];   /* units are attached to subdrivers */
	struct atapi_params *params[2]; /* params for units 0,1 */
	struct atapicmd *queue;         /* queue of commands to perform */
	struct atapicmd *tail;          /* tail of queue */
	struct atapicmd *free;          /* queue of free command blocks */
	struct atapicmd  cmdrq[16];     /* pool of command requests */
};

#ifdef KERNEL
struct atapi;

extern struct atapidrv atapi_drvtab[4]; /* delayed attach info */
extern int atapi_ndrv;                  /* the number of potential drives */
extern struct atapi *atapi_tab;         /* the table of atapi controllers */

#ifndef ATAPI_STATIC
#   define atapi_start             (*atapi_start_ptr)
#   define atapi_intr              (*atapi_intr_ptr)
#   define atapi_debug             (*atapi_debug_ptr)
#   define atapi_request_wait      (*atapi_request_wait_ptr)
#   define atapi_request_callback  (*atapi_request_callback_ptr)
#   define atapi_request_immediate (*atapi_request_immediate_ptr)
#endif

#ifndef ATAPI_MODULE
int atapi_attach (int ctlr, int unit, int port);
#endif

/*
 * These "functions" are declared with archaic `extern's because they are
 * actually pointers in the !ATAPI_STATIC case.
 */
extern int atapi_start (int ctrlr);
extern int atapi_intr (int ctrlr);
extern void atapi_debug (struct atapi *ata, int on);
extern struct atapires atapi_request_wait (struct atapi *ata, int unit,
	u_char cmd, u_char a1, u_char a2, u_char a3, u_char a4,
	u_char a5, u_char a6, u_char a7, u_char a8, u_char a9,
	u_char a10, u_char a11, u_char a12, u_char a13, u_char a14, u_char a15,
	char *addr, int count);
extern void atapi_request_callback (struct atapi *ata, int unit,
	u_char cmd, u_char a1, u_char a2, u_char a3, u_char a4,
	u_char a5, u_char a6, u_char a7, u_char a8, u_char a9,
	u_char a10, u_char a11, u_char a12, u_char a13, u_char a14, u_char a15,
	char *addr, int count, atapi_callback_t *done, void *x, void *y);
extern struct atapires atapi_request_immediate (struct atapi *ata, int unit,
	u_char cmd, u_char a1, u_char a2, u_char a3, u_char a4,
	u_char a5, u_char a6, u_char a7, u_char a8, u_char a9,
	u_char a10, u_char a11, u_char a12, u_char a13, u_char a14, u_char a15,
	char *addr, int count);
#endif
