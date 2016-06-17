#ifndef _ASM_IRQ_H
#define _ASM_IRQ_H

#include <linux/config.h>
#ifdef __KERNEL__
#include <asm/hardirq.h>

/*
 * How many IRQ's for S390 ?!?
 */
#define __MAX_SUBCHANNELS 65536
#define NR_IRQS           __MAX_SUBCHANNELS
#define NR_CHPIDS 256

#define LPM_ANYPATH 0xff /* doesn't really belong here, Ingo? */

#define INVALID_STORAGE_AREA ((void *)(-1 - 0x3FFF ))

/*
 * path management control word
 */
typedef struct {
      __u32 intparm;      /* interruption parameter */
      __u32 qf   : 1;     /* qdio facility */
      __u32 res0 : 1;     /* reserved zeros */
      __u32 isc  : 3;     /* interruption sublass */
      __u32 res5 : 3;     /* reserved zeros */
      __u32 ena  : 1;     /* enabled */
      __u32 lm   : 2;     /* limit mode */
      __u32 mme  : 2;     /* measurement-mode enable */
      __u32 mp   : 1;     /* multipath mode */
      __u32 tf   : 1;     /* timing facility */
      __u32 dnv  : 1;     /* device number valid */
      __u32 dev  : 16;    /* device number */
      __u8  lpm;          /* logical path mask */
      __u8  pnom;         /* path not operational mask */
      __u8  lpum;         /* last path used mask */
      __u8  pim;          /* path installed mask */
      __u16 mbi;          /* measurement-block index */
      __u8  pom;          /* path operational mask */
      __u8  pam;          /* path available mask */
      __u8  chpid[8];     /* CHPID 0-7 (if available) */
      __u32 unused1 : 8;  /* reserved zeros */
      __u32 st      : 3;  /* subchannel type */
      __u32 unused2 : 20; /* reserved zeros */
      __u32 csense  : 1;  /* concurrent sense; can be enabled ...*/
                          /*  ... per MSCH, however, if facility */
                          /*  ... is not installed, this results */
                          /*  ... in an operand exception.       */
   } __attribute__ ((packed)) pmcw_t;

#endif /* __KERNEL__ */
/*
 * subchannel status word
 */
typedef struct {
      __u32 key  : 4; /* subchannel key */
      __u32 sctl : 1; /* suspend control */
      __u32 eswf : 1; /* ESW format */
      __u32 cc   : 2; /* deferred condition code */
      __u32 fmt  : 1; /* format */
      __u32 pfch : 1; /* prefetch */
      __u32 isic : 1; /* initial-status interruption control */
      __u32 alcc : 1; /* address-limit checking control */
      __u32 ssi  : 1; /* supress-suspended interruption */
      __u32 zcc  : 1; /* zero condition code */
      __u32 ectl : 1; /* extended control */
      __u32 pno  : 1;     /* path not operational */
      __u32 res  : 1;     /* reserved */
      __u32 fctl : 3;     /* function control */
      __u32 actl : 7;     /* activity control */
      __u32 stctl : 5;    /* status control */
      __u32 cpa;          /* channel program address */
      __u32 dstat : 8;    /* device status */
      __u32 cstat : 8;    /* subchannel status */
      __u32 count : 16;   /* residual count */
   } __attribute__ ((packed)) scsw_t;

#define SCSW_FCTL_CLEAR_FUNC     0x1
#define SCSW_FCTL_HALT_FUNC      0x2
#define SCSW_FCTL_START_FUNC     0x4

#define SCSW_ACTL_SUSPENDED      0x1
#define SCSW_ACTL_DEVACT         0x2
#define SCSW_ACTL_SCHACT         0x4
#define SCSW_ACTL_CLEAR_PEND     0x8
#define SCSW_ACTL_HALT_PEND      0x10
#define SCSW_ACTL_START_PEND     0x20
#define SCSW_ACTL_RESUME_PEND    0x40

#define SCSW_STCTL_STATUS_PEND   0x1
#define SCSW_STCTL_SEC_STATUS    0x2
#define SCSW_STCTL_PRIM_STATUS   0x4
#define SCSW_STCTL_INTER_STATUS  0x8
#define SCSW_STCTL_ALERT_STATUS  0x10

#define DEV_STAT_ATTENTION       0x80
#define DEV_STAT_STAT_MOD        0x40
#define DEV_STAT_CU_END          0x20
#define DEV_STAT_BUSY            0x10
#define DEV_STAT_CHN_END         0x08
#define DEV_STAT_DEV_END         0x04
#define DEV_STAT_UNIT_CHECK      0x02
#define DEV_STAT_UNIT_EXCEP      0x01

#define SCHN_STAT_PCI            0x80
#define SCHN_STAT_INCORR_LEN     0x40
#define SCHN_STAT_PROG_CHECK     0x20
#define SCHN_STAT_PROT_CHECK     0x10
#define SCHN_STAT_CHN_DATA_CHK   0x08
#define SCHN_STAT_CHN_CTRL_CHK   0x04
#define SCHN_STAT_INTF_CTRL_CHK  0x02
#define SCHN_STAT_CHAIN_CHECK    0x01

/*
 * architectured values for first sense byte
 */
#define SNS0_CMD_REJECT         0x80
#define SNS_CMD_REJECT          SNS0_CMD_REJECT
#define SNS0_INTERVENTION_REQ   0x40
#define SNS0_BUS_OUT_CHECK      0x20
#define SNS0_EQUIPMENT_CHECK    0x10
#define SNS0_DATA_CHECK         0x08
#define SNS0_OVERRUN            0x04
/*                              0x02 reserved */
#define SNS0_INCOMPL_DOMAIN     0x01

/*
 * architectured values for second sense byte
 */
#define SNS1_PERM_ERR           0x80
#define SNS1_INV_TRACK_FORMAT   0x40
#define SNS1_EOC                0x20
#define SNS1_MESSAGE_TO_OPER    0x10
#define SNS1_NO_REC_FOUND       0x08
#define SNS1_FILE_PROTECTED     0x04
#define SNS1_WRITE_INHIBITED    0x02
#define SNS1_INPRECISE_END      0x01

/*
 * architectured values for third sense byte
 */
#define SNS2_REQ_INH_WRITE      0x80
#define SNS2_CORRECTABLE        0x40
#define SNS2_FIRST_LOG_ERR      0x20
#define SNS2_ENV_DATA_PRESENT   0x10
/*                              0x08 reserved */
#define SNS2_INPRECISE_END      0x04
/*                              0x02 reserved */
/*                              0x01 reserved */

#ifdef __KERNEL__
/*
 * subchannel information block
 */
typedef struct {
      pmcw_t pmcw;             /* path management control word */
      scsw_t scsw;             /* subchannel status word */
      __u8 mda[12];            /* model dependent area */
   } __attribute__ ((packed,aligned(4))) schib_t;
#endif /* __KERNEL__ */

typedef struct {
      __u8  cmd_code;/* command code */
      __u8  flags;   /* flags, like IDA adressing, etc. */
      __u16 count;   /* byte count */
      __u32 cda;     /* data address */
   } __attribute__ ((packed,aligned(8))) ccw1_t;

#define CCW_FLAG_DC             0x80
#define CCW_FLAG_CC             0x40
#define CCW_FLAG_SLI            0x20
#define CCW_FLAG_SKIP           0x10
#define CCW_FLAG_PCI            0x08
#define CCW_FLAG_IDA            0x04
#define CCW_FLAG_SUSPEND        0x02

#define CCW_CMD_READ_IPL        0x02
#define CCW_CMD_NOOP            0x03
#define CCW_CMD_BASIC_SENSE     0x04
#define CCW_CMD_TIC             0x08
#define CCW_CMD_SENSE_PGID      0x34
#define CCW_CMD_SUSPEND_RECONN  0x5B
#define CCW_CMD_RDC             0x64
#define CCW_CMD_SET_PGID        0xAF
#define CCW_CMD_SENSE_ID        0xE4
#define CCW_CMD_DCTL            0xF3

#ifdef __KERNEL__
#define SENSE_MAX_COUNT         0x20

/*
 * architectured values for first sense byte
 */
#define SNS0_CMD_REJECT         0x80
#define SNS_CMD_REJECT          SNS0_CMD_REJECT
#define SNS0_INTERVENTION_REQ   0x40
#define SNS0_BUS_OUT_CHECK      0x20
#define SNS0_EQUIPMENT_CHECK    0x10
#define SNS0_DATA_CHECK         0x08
#define SNS0_OVERRUN            0x04

/*
 * operation request block
 */
typedef struct {
      __u32 intparm;  /* interruption parameter */
      __u32 key  : 4; /* flags, like key, suspend control, etc. */
      __u32 spnd : 1; /* suspend control */
      __u32 res1 : 1; /* reserved */
      __u32 mod  : 1; /* modification control */
      __u32 sync : 1; /* synchronize control */
      __u32 fmt  : 1; /* format control */
      __u32 pfch : 1; /* prefetch control */
      __u32 isic : 1; /* initial-status-interruption control */
      __u32 alcc : 1; /* address-limit-checking control */
      __u32 ssic : 1; /* suppress-suspended-interr. control */
      __u32 res2 : 1; /* reserved */
      __u32 c64  : 1; /* IDAW/QDIO 64 bit control  */
      __u32 i2k  : 1; /* IDAW 2/4kB block size control */
      __u32 lpm  : 8; /* logical path mask */
      __u32 ils  : 1; /* incorrect length */
      __u32 zero : 6; /* reserved zeros */
      __u32 orbx : 1; /* ORB extension control */
      __u32 cpa;      /* channel program address */
   }  __attribute__ ((packed,aligned(4))) orb_t;

#endif /* __KERNEL__ */
typedef struct {
      __u32 res0  : 4;  /* reserved */
      __u32 pvrf  : 1;  /* path-verification-required flag */
      __u32 cpt   : 1;  /* channel-path timeout */
      __u32 fsavf : 1;  /* Failing storage address validity flag */
      __u32 cons  : 1;  /* concurrent-sense */
      __u32 res8  : 2;  /* reserved */
      __u32 scnt  : 6;  /* sense count if cons == 1 */
      __u32 res16 : 16; /* reserved */
   } __attribute__ ((packed)) erw_t;

/*
 * subchannel logout area
 */
typedef struct {
      __u32 res0  : 1;  /* reserved */
      __u32 esf   : 7;  /* extended status flags */
      __u32 lpum  : 8;  /* last path used mask */
      __u32 res16 : 1;  /* reserved */
      __u32 fvf   : 5;  /* field-validity flags */
      __u32 sacc  : 2;  /* storage access code */
      __u32 termc : 2;  /* termination code */
      __u32 devsc : 1;  /* device-status check */
      __u32 serr  : 1;  /* secondary error */
      __u32 ioerr : 1;  /* i/o-error alert */
      __u32 seqc  : 3;  /* sequence code */
   } __attribute__ ((packed)) sublog_t ;

/*
 * Format 0 Extended Status Word (ESW)
 */
typedef struct {
      sublog_t sublog;    /* subchannel logout */
      erw_t    erw;       /* extended report word */
      __u32    faddr;     /* failing address */
      __u32    zeros[2];  /* 2 fullwords of zeros */
   } __attribute__ ((packed)) esw0_t;

/*
 * Format 1 Extended Status Word (ESW)
 */
typedef struct {
      __u8  zero0;    /* reserved zeros */
      __u8  lpum;     /* last path used mask */
      __u16 zero16;   /* reserved zeros */
      erw_t erw;      /* extended report word */
      __u32 zeros[3]; /* 2 fullwords of zeros */
   } __attribute__ ((packed)) esw1_t;

/*
 * Format 2 Extended Status Word (ESW)
 */
typedef struct {
      __u8  zero0;    /* reserved zeros */
      __u8  lpum;     /* last path used mask */
      __u16 dcti;     /* device-connect-time interval */
      erw_t erw;      /* extended report word */
      __u32 zeros[3]; /* 2 fullwords of zeros */
   } __attribute__ ((packed)) esw2_t;

/*
 * Format 3 Extended Status Word (ESW)
 */
typedef struct {
      __u8  zero0;    /* reserved zeros */
      __u8  lpum;     /* last path used mask */
      __u16 res;      /* reserved */
      erw_t erw;      /* extended report word */
      __u32 zeros[3]; /* 2 fullwords of zeros */
   } __attribute__ ((packed)) esw3_t;

typedef union {
      esw0_t esw0;
      esw1_t esw1;
      esw2_t esw2;
      esw3_t esw3;
   } __attribute__ ((packed)) esw_t;

/*
 * interruption response block
 */
typedef struct {
      scsw_t scsw;             /* subchannel status word */
      esw_t  esw;              /* extended status word */
      __u8   ecw[32];          /* extended control word */
   } __attribute__ ((packed,aligned(4))) irb_t;
#ifdef __KERNEL__

/*
 * TPI info structure
 */
typedef struct {
	__u32 reserved1  : 16;   /* reserved 0x00000001 */
	__u32 irq        : 16;   /* aka. subchannel number */
	__u32 intparm;           /* interruption parameter */
	__u32 adapter_IO : 1;
	__u32 reserved2  : 1;
	__u32 isc        : 3;
	__u32 reserved3  : 12;
	__u32 int_type   : 3;
	__u32 reserved4  : 12;
   } __attribute__ ((packed)) tpi_info_t;


//
// command information word  (CIW) layout
//
typedef struct _ciw {
   __u32        et       :  2; // entry type
   __u32        reserved :  2; // reserved
   __u32        ct       :  4; // command type
   __u32        cmd      :  8; // command
   __u32        count    : 16; // count
   } __attribute__ ((packed)) ciw_t;

#define CIW_TYPE_RCD    0x0    // read configuration data
#define CIW_TYPE_SII    0x1    // set interface identifier
#define CIW_TYPE_RNI    0x2    // read node identifier

#define MAX_CIWS 8
//
// sense-id response buffer layout
//
typedef struct {
  /* common part */
      __u8           reserved;     /* always 0x'FF' */
      __u16          cu_type;      /* control unit type */
      __u8           cu_model;     /* control unit model */
      __u16          dev_type;     /* device type */
      __u8           dev_model;    /* device model */
      __u8           unused;       /* padding byte */
  /* extended part */
      ciw_t    ciw[MAX_CIWS];      /* variable # of CIWs */
   }  __attribute__ ((packed,aligned(4))) senseid_t;

/*
 * where we put the ssd info
 */
typedef struct _ssd_info {
	__u8   valid:1;
	__u8   type:7;          /* subchannel type */
	__u8   chpid[8];        /* chpids */
	__u16  fla[8];          /* full link addresses */
} __attribute__ ((packed)) ssd_info_t;

/*
 * area for store event information
 */
typedef struct chsc_area_t {
	struct {
		/* word 0 */
		__u16 command_code1;
		__u16 command_code2;
		union {
			struct {
				/* word 1 */
				__u32 reserved1;
				/* word 2 */
				__u32 reserved2;
			} __attribute__ ((packed,aligned(8))) sei_req;
			struct {
				/* word 1 */
				__u16 reserved1;
				__u16 f_sch;     /* first subchannel */
				/* word 2 */
				__u16 reserved2;
				__u16 l_sch;    /* last subchannel */
			} __attribute__ ((packed,aligned(8))) ssd_req;
		} request_block_data;
		/* word 3 */
		__u32 reserved3;
	} __attribute__ ((packed,aligned(8))) request_block;
	struct {
		/* word 0 */
		__u16 length;
		__u16 response_code;
		/* word 1 */
		__u32 reserved1;
		union {
			struct {
				/* word 2 */
				__u8  flags;
				__u8  vf;         /* validity flags */
				__u8  rs;         /* reporting source */
				__u8  cc;         /* content code */
				/* word 3 */
				__u16 fla;        /* full link address */
				__u16 rsid;       /* reporting source id */
				/* word 4 */
				__u32 reserved2;
				/* word 5 */
				__u32 reserved3;
				/* word 6-102 */
				__u32 ccdf[96];   /* content-code dependent field */
			} __attribute__ ((packed,aligned(8))) sei_res;
			struct {
				/* word 2 */
				__u8 sch_valid : 1;
				__u8 dev_valid : 1;
				__u8 st        : 3; /* subchannel type */
				__u8 zeroes    : 3;
				__u8  unit_addr;  /* unit address */
				__u16 devno;      /* device number */
				/* word 3 */
				__u8 path_mask;  
				__u8 fla_valid_mask;
				__u16 sch;        /* subchannel */
				/* words 4-5 */
				__u8 chpid[8];    /* chpids 0-7 */
				/* words 6-9 */
				__u16 fla[8];     /* full link addresses 0-7 */
				/* words 10-102 */
				__u32 padding[92];
			} __attribute__ ((packed,aligned(8))) ssd_res;
		} response_block_data;
	} __attribute__ ((packed,aligned(8))) response_block;
} __attribute__ ((packed,aligned(PAGE_SIZE))) chsc_area_t;

#endif /* __KERNEL__ */
/*
 * sense data
 */
typedef struct {
      __u8          res[32];   /* reserved   */
      __u8          data[32];  /* sense data */
   } __attribute__ ((packed)) sense_t;

/*
 * device status area, to be provided by the device driver
 *  when calling request_irq() as parameter "dev_id", later
 *  tied to the "action" control block.
 *
 * Note : No data area must be added after union ii or the
 *         effective devstat size calculation will fail !
 */
typedef struct {
     __u16         devno;    /* device number, aka. "cuu" from irb */
     unsigned long intparm;  /* interrupt parameter */
     __u8          cstat;    /* channel status - accumulated */
     __u8          dstat;    /* device status - accumulated */
     __u8          lpum;     /* last path used mask from irb */
     __u8          unused;   /* not used - reserved */
     unsigned int  flag;     /* flag : see below */
     __u32         cpa;      /* CCW address from irb at primary status */
     __u32         rescnt;   /* res. count from irb at primary status */
     __u32         scnt;     /* sense count, if DEVSTAT_FLAG_SENSE_AVAIL */
     union {
        irb_t   irb;         /* interruption response block */
        sense_t sense;       /* sense information */
        } ii;                /* interrupt information */
  } devstat_t;

#define DEVSTAT_FLAG_SENSE_AVAIL   0x00000001
#define DEVSTAT_NOT_OPER           0x00000002
#define DEVSTAT_START_FUNCTION     0x00000004
#define DEVSTAT_HALT_FUNCTION      0x00000008
#define DEVSTAT_STATUS_PENDING     0x00000010
#define DEVSTAT_REVALIDATE         0x00000020
#define DEVSTAT_DEVICE_GONE        0x00000040
#define DEVSTAT_DEVICE_OWNED       0x00000080
#define DEVSTAT_CLEAR_FUNCTION     0x00000100
#define DEVSTAT_PCI                0x00000200
#define DEVSTAT_SUSPENDED          0x00000400
#define DEVSTAT_UNKNOWN_DEV        0x00000800
#define DEVSTAT_UNFRIENDLY_DEV     0x00001000
#define DEVSTAT_NOT_ACC            0x00002000
#define DEVSTAT_NOT_ACC_ERR        0x00004000
#define DEVSTAT_FINAL_STATUS       0x80000000

#define DEVINFO_NOT_OPER           DEVSTAT_NOT_OPER
#define DEVINFO_UNKNOWN_DEV        DEVSTAT_UNKNOWN_DEV
#define DEVINFO_DEVICE_OWNED       DEVSTAT_DEVICE_OWNED
#define DEVINFO_QDIO_CAPABLE       0x40000000
#define DEVINFO_UNFRIENDLY_DEV     DEVSTAT_UNFRIENDLY_DEV

#define INTPARM_STATUS_PENDING     0xFFFFFFFF
#ifdef __KERNEL__

#define IO_INTERRUPT_TYPE          0 /* I/O interrupt type */

typedef  void (* io_handler_func1_t) ( int             irq,
                                       devstat_t      *devstat,
                                       struct pt_regs *rgs);

typedef  void (* io_handler_func_t) ( int             irq,
                                      void           *devstat,
                                      struct pt_regs *rgs);

typedef  void ( * not_oper_handler_func_t)( int irq,
                                            int status );

typedef  int  (* adapter_int_handler_t)( __u32 intparm );

typedef struct {
	io_handler_func_t         handler;  /* interrupt handler routine */
	const char               *name;     /* device name */
	devstat_t                *dev_id;   /* device status block */
   } irq_desc_t;

typedef struct {
	__u8  state1    :  2;   /* path state value 1 */
	__u8  state2    :  2;   /* path state value 2 */
	__u8  state3    :  1;   /* path state value 3 */
	__u8  resvd     :  3;   /* reserved */
	} __attribute__ ((packed)) path_state_t;

typedef struct {
   union {
		__u8         fc;   /* SPID function code */
		path_state_t ps;   /* SNID path state */
	} inf;
	__u32 cpu_addr  : 16;   /* CPU address */
	__u32 cpu_id    : 24;   /* CPU identification */
	__u32 cpu_model : 16;   /* CPU model */
	__u32 tod_high;         /* high word TOD clock */
	} __attribute__ ((packed)) pgid_t;

#define SPID_FUNC_SINGLE_PATH      0x00
#define SPID_FUNC_MULTI_PATH       0x80
#define SPID_FUNC_ESTABLISH        0x00
#define SPID_FUNC_RESIGN           0x40
#define SPID_FUNC_DISBAND          0x20

#define SNID_STATE1_RESET          0
#define SNID_STATE1_UNGROUPED      2
#define SNID_STATE1_GROUPED        3

#define SNID_STATE2_NOT_RESVD      0
#define SNID_STATE2_RESVD_ELSE     2
#define SNID_STATE2_RESVD_SELF     3

#define SNID_STATE3_MULTI_PATH     1
#define SNID_STATE3_SINGLE_PATH    0

/*
 * Flags used as input parameters for do_IO()
 */
#define DOIO_EARLY_NOTIFICATION  0x0001 /* allow for I/O completion ... */
                                        /* ... notification after ... */
                                        /* ... primary interrupt status */
#define DOIO_RETURN_CHAN_END     DOIO_EARLY_NOTIFICATION
#define DOIO_VALID_LPM           0x0002 /* LPM input parameter is valid */
#define DOIO_WAIT_FOR_INTERRUPT  0x0004 /* wait synchronously for interrupt */
#define DOIO_REPORT_ALL          0x0008 /* report all interrupt conditions */
#define DOIO_ALLOW_SUSPEND       0x0010 /* allow for channel prog. suspend */
#define DOIO_DENY_PREFETCH       0x0020 /* don't allow for CCW prefetch */
#define DOIO_SUPPRESS_INTER      0x0040 /* suppress intermediate inter. */
                                        /* ... for suspended CCWs */
#define DOIO_TIMEOUT             0x0080 /* 3 secs. timeout for sync. I/O */
#define DOIO_DONT_CALL_INTHDLR   0x0100 /* don't call interrupt handler */
#define DOIO_USE_DIAG98          0x0400 /* use DIAG98 instead of SSCH */

/*
 * do_IO()
 *
 *  Start a S/390 channel program. When the interrupt arrives, the
 *  IRQ handler is called, either immediately, delayed (dev-end missing,
 *  or sense required) or never (no IRQ handler registered -
 *  should never occur, as the IRQ (subchannel ID) should be
 *  disabled if no handler is present. Depending on the action
 *  taken, do_IO() returns :  0      - Success
 *                           -EIO    - Status pending
 *                                        see : action->dev_id->cstat
 *                                              action->dev_id->dstat
 *                           -EBUSY  - Device busy
 *                           -ENODEV - Device not operational
 */
int do_IO( int            irq,          /* IRQ aka. subchannel number */
           ccw1_t        *cpa,          /* logical channel program address */
           unsigned long  intparm,      /* interruption parameter */
           __u8           lpm,          /* logical path mask */
           unsigned long  flag);        /* flags : see above */

int start_IO( int           irq,       /* IRQ aka. subchannel number */
              ccw1_t       *cpa,       /* logical channel program address */
              unsigned long  intparm,   /* interruption parameter */
              __u8          lpm,       /* logical path mask */
              unsigned int  flag);     /* flags : see above */

void do_crw_pending( void  );	         /* CRW handler */

int resume_IO( int irq);               /* IRQ aka. subchannel number */

int halt_IO( int           irq,         /* IRQ aka. subchannel number */
             unsigned long intparm,     /* dummy intparm */
             unsigned long flag);       /* possible DOIO_WAIT_FOR_INTERRUPT */

int clear_IO( int           irq,         /* IRQ aka. subchannel number */
              unsigned long intparm,     /* dummy intparm */
              unsigned long flag);       /* possible DOIO_WAIT_FOR_INTERRUPT */

int process_IRQ( struct pt_regs regs,
                 unsigned int   irq,
                 unsigned int   intparm);


int enable_cpu_sync_isc ( int irq );
int disable_cpu_sync_isc( int irq );

typedef struct {
     int          irq;                  /* irq, aka. subchannel */
     __u16        devno;                /* device number */
     unsigned int status;               /* device status */
     senseid_t    sid_data;             /* senseID data */
     } s390_dev_info_t;

int get_dev_info( int irq, s390_dev_info_t *);   /* to be eliminated - don't use */

int get_dev_info_by_irq  ( int irq, s390_dev_info_t *pdi);
int get_dev_info_by_devno( __u16 devno, s390_dev_info_t *pdi);

int          get_irq_by_devno( __u16 devno );
unsigned int get_devno_by_irq( int irq );

int get_irq_first( void );
int get_irq_next ( int irq );

int read_dev_chars( int irq, void **buffer, int length );
int read_conf_data( int irq, void **buffer, int *length, __u8 lpm );

int  s390_DevicePathVerification( int irq, __u8 domask );

int s390_trigger_resense(int irq);

int s390_request_irq_special( int                      irq,
                              io_handler_func_t        io_handler,
                              not_oper_handler_func_t  not_oper_handler,
                              unsigned long            irqflags,
                              const char              *devname,
                              void                    *dev_id);

extern int set_cons_dev(int irq);
extern int wait_cons_dev(int irq);
extern schib_t *s390_get_schib( int irq );

extern int s390_register_adapter_interrupt(adapter_int_handler_t handler);
extern int s390_unregister_adapter_interrupt(adapter_int_handler_t handler);

/*
 * Some S390 specific IO instructions as inline
 */

extern __inline__ int stsch(int irq, volatile schib_t *addr)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   stsch 0(%2)\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode)
		: "d" (irq | 0x10000), "a" (addr)
		: "cc", "1" );
        return ccode;
}

extern __inline__ int msch(int irq, volatile schib_t *addr)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   msch  0(%2)\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode)
		: "d" (irq | 0x10000L), "a" (addr)
                : "cc", "1" );
        return ccode;
}

extern __inline__ int msch_err(int irq, volatile schib_t *addr)
{
        int ccode;

        __asm__ __volatile__(
                "    lr   1,%1\n"
                "    msch 0(%2)\n"
                "0:  ipm  %0\n"
                "    srl  %0,28\n"
                "1:\n"
#ifdef CONFIG_ARCH_S390X
                ".section .fixup,\"ax\"\n"
                "2:  l    %0,%3\n"
                "    jg   1b\n"
                ".previous\n"
                ".section __ex_table,\"a\"\n"
                "   .align 8\n"
                "   .quad 0b,2b\n"
                ".previous"
#else
                ".section .fixup,\"ax\"\n"
                "2:  l    %0,%3\n"
                "    bras 1,3f\n"
                "    .long 1b\n"
                "3:  l    1,0(1)\n"
                "    br   1\n"
                ".previous\n"
                ".section __ex_table,\"a\"\n"
                "   .align 4\n"
                "   .long 0b,2b\n"
                ".previous"
#endif
                : "=d" (ccode)
                : "d" (irq | 0x10000L), "a" (addr), "i" (__LC_PGM_ILC)
                : "cc", "1" );
        return ccode;
}

extern __inline__ int tsch(int irq, volatile irb_t *addr)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   tsch  0(%2)\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "d" (irq | 0x10000L), "a" (addr)
                : "cc", "1" );
        return ccode;
}

extern __inline__ int tpi( volatile tpi_info_t *addr)
{
        int ccode;

        __asm__ __volatile__(
                "   tpi   0(%1)\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "a" (addr)
                : "cc", "1" );
        return ccode;
}

extern __inline__ int ssch(int irq, volatile orb_t *addr)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   ssch  0(%2)\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "d" (irq | 0x10000L), "a" (addr)
                : "cc", "1" );
        return ccode;
}

extern __inline__ int diag98(int irq, volatile orb_t *addr)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   lgr   0,%2\n"    /* orb in 0 */
		"   lghi  2,12\n"    /* function code 0x0c */
		"   diag  2,0,152\n" /* diag98 instead of ssch,
					result in gpr 1 */
                "   ipm   %0\n"      /* usual cc evaluation. cc=3 will be
				        reported as not operational */
                "   srl   %0,28"
                : "=d" (ccode) 
		: "d" (irq | 0x10000L), "a" (addr)
                : "cc", "0", "1", "2");
        return ccode;
}

extern __inline__ int rsch(int irq)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   rsch\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "d" (irq | 0x10000L)
                : "cc", "1" );
        return ccode;
}

extern __inline__ int csch(int irq)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   csch\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "d" (irq | 0x10000L)
                : "cc", "1" );
        return ccode;
}

extern __inline__ int hsch(int irq)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   hsch\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "d" (irq | 0x10000L)
                : "cc", "1" );
        return ccode;
}

extern __inline__ int xsch(int irq)
{
	int ccode;
	
	__asm__ __volatile__(
                "   lr    1,%1\n"
                "   .insn rre,0xb2760000,%1,0\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "d" (irq | 0x10000L)
                : "cc", "1" );
	return ccode;
}

extern __inline__ int iac( void)
{
        int ccode;

        __asm__ __volatile__(
                "   iac   1\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) : : "cc", "1" );
        return ccode;
}

extern __inline__ int rchp(int chpid)
{
        int ccode;

        __asm__ __volatile__(
                "   lr    1,%1\n"
                "   rchp\n"
                "   ipm   %0\n"
                "   srl   %0,28"
                : "=d" (ccode) 
		: "d" (chpid)
                : "cc", "1" );
        return ccode;
}

typedef struct {
     __u16 vrdcdvno : 16;   /* device number (input) */
     __u16 vrdclen  : 16;   /* data block length (input) */
     __u32 vrdcvcla : 8;    /* virtual device class (output) */
     __u32 vrdcvtyp : 8;    /* virtual device type (output) */
     __u32 vrdcvsta : 8;    /* virtual device status (output) */
     __u32 vrdcvfla : 8;    /* virtual device flags (output) */
     __u32 vrdcrccl : 8;    /* real device class (output) */
     __u32 vrdccrty : 8;    /* real device type (output) */
     __u32 vrdccrmd : 8;    /* real device model (output) */
     __u32 vrdccrft : 8;    /* real device feature (output) */
     } __attribute__ ((packed,aligned(4))) diag210_t;

void VM_virtual_device_info( __u16      devno,   /* device number */
                             senseid_t *ps );    /* ptr to senseID data */

extern int diag210( diag210_t * addr);

extern __inline__ int chsc( chsc_area_t * chsc_area)
{
	int cc;
	
	__asm__ __volatile__ (
	        ".insn	rre,0xb25f0000,%1,0	\n\t"
		"ipm	%0	\n\t"
		"srl	%0,28	\n\t"
		: "=d" (cc) 
		: "d" (chsc_area) 
		: "cc" );
	
	return cc;
}

/*
 * Various low-level irq details needed by irq.c, process.c,
 * time.c, io_apic.c and smp.c
 *
 * Interrupt entry/exit code at both C and assembly level
 */

void mask_irq(unsigned int irq);
void unmask_irq(unsigned int irq);

#define MAX_IRQ_SOURCES 128

extern spinlock_t irq_controller_lock;

#ifdef CONFIG_SMP

#include <asm/atomic.h>

static inline void irq_enter(int cpu, unsigned int irq)
{
        hardirq_enter(cpu);
        while (atomic_read(&global_irq_lock) != 0) {
                eieio();
        }
}

static inline void irq_exit(int cpu, unsigned int irq)
{
        hardirq_exit(cpu);
        release_irqlock(cpu);
}


#else

#define irq_enter(cpu, irq)     (++local_irq_count(cpu))
#define irq_exit(cpu, irq)      (--local_irq_count(cpu))

#endif

#define __STR(x) #x
#define STR(x) __STR(x)

#ifdef CONFIG_SMP

/*
 *      SMP has a few special interrupts for IPI messages
 */

#endif /* CONFIG_SMP */

/*
 * x86 profiling function, SMP safe. We might want to do this in
 * assembly totally?
 */
extern char _stext;
static inline void s390_do_profile (unsigned long addr)
{
        if (prof_buffer && current->pid) {
#ifndef CONFIG_ARCH_S390X
                addr &= 0x7fffffff;
#endif
                addr -= (unsigned long) &_stext;
                addr >>= prof_shift;
                /*
                 * Don't ignore out-of-bounds EIP values silently,
                 * put them into the last histogram slot, so if
                 * present, they will show up as a sharp peak.
                 */
                if (addr > prof_len-1)
                        addr = prof_len-1;
                atomic_inc((atomic_t *)&prof_buffer[addr]);
        }
}

#include <asm/s390io.h>

#define get_irq_lock(irq) &ioinfo[irq]->irq_lock

#define s390irq_spin_lock(irq) \
        spin_lock(&(ioinfo[irq]->irq_lock))

#define s390irq_spin_unlock(irq) \
        spin_unlock(&(ioinfo[irq]->irq_lock))

#define s390irq_spin_lock_irqsave(irq,flags) \
        spin_lock_irqsave(&(ioinfo[irq]->irq_lock), flags)
#define s390irq_spin_unlock_irqrestore(irq,flags) \
        spin_unlock_irqrestore(&(ioinfo[irq]->irq_lock), flags)

#define touch_nmi_watchdog() do { } while(0)

#endif /* __KERNEL__ */
#endif

