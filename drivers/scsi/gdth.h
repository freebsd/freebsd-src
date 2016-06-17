#ifndef _GDTH_H
#define _GDTH_H

/*
 * Header file for the GDT Disk Array/Storage RAID controllers driver for Linux
 * 
 * gdth.h Copyright (C) 1995-02 ICP vortex, an Intel company, Achim Leubner
 * See gdth.c for further informations and 
 * below for supported controller types
 *
 * <achim.leubner@intel.com>
 *
 * $Id: gdth.h,v 1.46 2002/02/05 09:39:53 achim Exp $
 */

#include <linux/version.h>
#include <linux/types.h>

#ifndef NULL
#define NULL 0
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

/* defines, macros */

/* driver version */
#define GDTH_VERSION_STR        "2.05"
#define GDTH_VERSION            2
#define GDTH_SUBVERSION         5

/* protocol version */
#define PROTOCOL_VERSION        1

/* OEM IDs */
#define OEM_ID_ICP	0x941c
#define OEM_ID_INTEL	0x8000

/* controller classes */
#define GDT_ISA         0x01                    /* ISA controller */
#define GDT_EISA        0x02                    /* EISA controller */
#define GDT_PCI         0x03                    /* PCI controller */
#define GDT_PCINEW      0x04                    /* new PCI controller */
#define GDT_PCIMPR      0x05                    /* PCI MPR controller */
/* GDT_EISA, controller subtypes EISA */
#define GDT3_ID         0x0130941c              /* GDT3000/3020 */
#define GDT3A_ID        0x0230941c              /* GDT3000A/3020A/3050A */
#define GDT3B_ID        0x0330941c              /* GDT3000B/3010A */
/* GDT_ISA */
#define GDT2_ID         0x0120941c              /* GDT2000/2020 */

/* vendor ID, device IDs (PCI) */
/* these defines should already exist in <linux/pci.h> */
#ifndef PCI_VENDOR_ID_VORTEX
#define PCI_VENDOR_ID_VORTEX            0x1119  /* PCI controller vendor ID */
#endif
#ifndef PCI_VENDOR_ID_INTEL
#define PCI_VENDOR_ID_INTEL             0x8086  
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDT60x0
/* GDT_PCI */
#define PCI_DEVICE_ID_VORTEX_GDT60x0    0       /* GDT6000/6020/6050 */
#define PCI_DEVICE_ID_VORTEX_GDT6000B   1       /* GDT6000B/6010 */
/* GDT_PCINEW */
#define PCI_DEVICE_ID_VORTEX_GDT6x10    2       /* GDT6110/6510 */
#define PCI_DEVICE_ID_VORTEX_GDT6x20    3       /* GDT6120/6520 */
#define PCI_DEVICE_ID_VORTEX_GDT6530    4       /* GDT6530 */
#define PCI_DEVICE_ID_VORTEX_GDT6550    5       /* GDT6550 */
/* GDT_PCINEW, wide/ultra SCSI controllers */
#define PCI_DEVICE_ID_VORTEX_GDT6x17    6       /* GDT6117/6517 */
#define PCI_DEVICE_ID_VORTEX_GDT6x27    7       /* GDT6127/6527 */
#define PCI_DEVICE_ID_VORTEX_GDT6537    8       /* GDT6537 */
#define PCI_DEVICE_ID_VORTEX_GDT6557    9       /* GDT6557/6557-ECC */
/* GDT_PCINEW, wide SCSI controllers */
#define PCI_DEVICE_ID_VORTEX_GDT6x15    10      /* GDT6115/6515 */
#define PCI_DEVICE_ID_VORTEX_GDT6x25    11      /* GDT6125/6525 */
#define PCI_DEVICE_ID_VORTEX_GDT6535    12      /* GDT6535 */
#define PCI_DEVICE_ID_VORTEX_GDT6555    13      /* GDT6555/6555-ECC */
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDT6x17RP
/* GDT_MPR, RP series, wide/ultra SCSI */
#define PCI_DEVICE_ID_VORTEX_GDT6x17RP  0x100   /* GDT6117RP/GDT6517RP */
#define PCI_DEVICE_ID_VORTEX_GDT6x27RP  0x101   /* GDT6127RP/GDT6527RP */
#define PCI_DEVICE_ID_VORTEX_GDT6537RP  0x102   /* GDT6537RP */
#define PCI_DEVICE_ID_VORTEX_GDT6557RP  0x103   /* GDT6557RP */
/* GDT_MPR, RP series, narrow/ultra SCSI */
#define PCI_DEVICE_ID_VORTEX_GDT6x11RP  0x104   /* GDT6111RP/GDT6511RP */
#define PCI_DEVICE_ID_VORTEX_GDT6x21RP  0x105   /* GDT6121RP/GDT6521RP */
#endif
#ifndef PCI_DEVICE_ID_VORTEX_GDT6x17RD
/* GDT_MPR, RD series, wide/ultra SCSI */
#define PCI_DEVICE_ID_VORTEX_GDT6x17RD  0x110   /* GDT6117RD/GDT6517RD */
#define PCI_DEVICE_ID_VORTEX_GDT6x27RD  0x111   /* GDT6127RD/GDT6527RD */
#define PCI_DEVICE_ID_VORTEX_GDT6537RD  0x112   /* GDT6537RD */
#define PCI_DEVICE_ID_VORTEX_GDT6557RD  0x113   /* GDT6557RD */
/* GDT_MPR, RD series, narrow/ultra SCSI */
#define PCI_DEVICE_ID_VORTEX_GDT6x11RD  0x114   /* GDT6111RD/GDT6511RD */
#define PCI_DEVICE_ID_VORTEX_GDT6x21RD  0x115   /* GDT6121RD/GDT6521RD */
/* GDT_MPR, RD series, wide/ultra2 SCSI */
#define PCI_DEVICE_ID_VORTEX_GDT6x18RD  0x118   /* GDT6118RD/GDT6518RD/
                                                   GDT6618RD */
#define PCI_DEVICE_ID_VORTEX_GDT6x28RD  0x119   /* GDT6128RD/GDT6528RD/
                                                   GDT6628RD */
#define PCI_DEVICE_ID_VORTEX_GDT6x38RD  0x11A   /* GDT6538RD/GDT6638RD */
#define PCI_DEVICE_ID_VORTEX_GDT6x58RD  0x11B   /* GDT6558RD/GDT6658RD */
/* GDT_MPR, RN series (64-bit PCI), wide/ultra2 SCSI */
#define PCI_DEVICE_ID_VORTEX_GDT7x18RN  0x168   /* GDT7118RN/GDT7518RN/
                                                   GDT7618RN */
#define PCI_DEVICE_ID_VORTEX_GDT7x28RN  0x169   /* GDT7128RN/GDT7528RN/
                                                   GDT7628RN */
#define PCI_DEVICE_ID_VORTEX_GDT7x38RN  0x16A   /* GDT7538RN/GDT7638RN */
#define PCI_DEVICE_ID_VORTEX_GDT7x58RN  0x16B   /* GDT7558RN/GDT7658RN */
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDT6x19RD
/* GDT_MPR, RD series, Fibre Channel */
#define PCI_DEVICE_ID_VORTEX_GDT6x19RD  0x210   /* GDT6519RD/GDT6619RD */
#define PCI_DEVICE_ID_VORTEX_GDT6x29RD  0x211   /* GDT6529RD/GDT6629RD */
/* GDT_MPR, RN series (64-bit PCI), Fibre Channel */
#define PCI_DEVICE_ID_VORTEX_GDT7x19RN  0x260   /* GDT7519RN/GDT7619RN */
#define PCI_DEVICE_ID_VORTEX_GDT7x29RN  0x261   /* GDT7529RN/GDT7629RN */
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDTMAXRP
/* GDT_MPR, last device ID */
#define PCI_DEVICE_ID_VORTEX_GDTMAXRP   0x2ff   
#endif

#ifndef PCI_DEVICE_ID_VORTEX_GDTNEWRX
/* new GDT Rx Controller */
#define PCI_DEVICE_ID_VORTEX_GDTNEWRX	0x300
#endif
	
#ifndef PCI_DEVICE_ID_INTEL_SRC
/* Intel Storage RAID Controller */
#define PCI_DEVICE_ID_INTEL_SRC		0x600
#endif

/* limits */
#define GDTH_SCRATCH    PAGE_SIZE               /* 4KB scratch buffer */
#define GDTH_SCRATCH_ORD 0                      /* order 0 means 1 page */
#define GDTH_MAXCMDS    124
#define GDTH_MAXC_P_L   16                      /* max. cmds per lun */
#define GDTH_MAX_RAW    2                       /* max. cmds per raw device */
#define MAXOFFSETS      128
#define MAXHA           16
#define MAXID           127
#define MAXLUN          8
#define MAXBUS          6
#define MAX_HDRIVES     100                     /* max. host drive count */
#define MAX_LDRIVES     255                     /* max. log. drive count */
#define MAX_EVENTS      100                     /* event buffer count */
#define MAX_RES_ARGS    40                      /* device reservation, 
                                                   must be a multiple of 4 */
#define MAXCYLS         1024
#define HEADS           64
#define SECS            32                      /* mapping 64*32 */
#define MEDHEADS        127
#define MEDSECS         63                      /* mapping 127*63 */
#define BIGHEADS        255
#define BIGSECS         63                      /* mapping 255*63 */

/* special command ptr. */
#define UNUSED_CMND     ((Scsi_Cmnd *)-1)
#define INTERNAL_CMND   ((Scsi_Cmnd *)-2)
#define SCREEN_CMND     ((Scsi_Cmnd *)-3)
#define SPECIAL_SCP(p)  (p==UNUSED_CMND || p==INTERNAL_CMND || p==SCREEN_CMND)

/* controller services */
#define SCSIRAWSERVICE  3
#define CACHESERVICE    9
#define SCREENSERVICE   11

/* screenservice defines */
#define MSG_INV_HANDLE  -1                      /* special message handle */
#define MSGLEN          16                      /* size of message text */
#define MSG_SIZE        34                      /* size of message structure */
#define MSG_REQUEST     0                       /* async. event: message */

/* cacheservice defines */
#define SECTOR_SIZE     0x200                   /* always 512 bytes per sec. */

/* DPMEM constants */
#define DPMEM_MAGIC     0xC0FFEE11
#define IC_HEADER_BYTES 48
#define IC_QUEUE_BYTES  4
#define DPMEM_COMMAND_OFFSET    IC_HEADER_BYTES+IC_QUEUE_BYTES*MAXOFFSETS

/* cluster_type constants */
#define CLUSTER_DRIVE         1
#define CLUSTER_MOUNTED       2
#define CLUSTER_RESERVED      4
#define CLUSTER_RESERVE_STATE (CLUSTER_DRIVE|CLUSTER_MOUNTED|CLUSTER_RESERVED)

/* commands for all services, cache service */
#define GDT_INIT        0                       /* service initialization */
#define GDT_READ        1                       /* read command */
#define GDT_WRITE       2                       /* write command */
#define GDT_INFO        3                       /* information about devices */
#define GDT_FLUSH       4                       /* flush dirty cache buffers */
#define GDT_IOCTL       5                       /* ioctl command */
#define GDT_DEVTYPE     9                       /* additional information */
#define GDT_MOUNT       10                      /* mount cache device */
#define GDT_UNMOUNT     11                      /* unmount cache device */
#define GDT_SET_FEAT    12                      /* set feat. (scatter/gather) */
#define GDT_GET_FEAT    13                      /* get features */
#define GDT_WRITE_THR   16                      /* write through */
#define GDT_READ_THR    17                      /* read through */
#define GDT_EXT_INFO    18                      /* extended info */
#define GDT_RESET       19                      /* controller reset */
#define GDT_RESERVE_DRV 20                      /* reserve host drive */
#define GDT_RELEASE_DRV 21                      /* release host drive */
#define GDT_CLUST_INFO  22                      /* cluster info */
#define GDT_RW_ATTRIBS  23                      /* R/W attribs (write thru,..)*/
#define GDT_CLUST_RESET 24                      /* releases the cluster drives*/
#define GDT_FREEZE_IO   25                      /* freezes all IOs */
#define GDT_UNFREEZE_IO 26                      /* unfreezes all IOs */

/* raw service commands */
#define GDT_RESERVE     14                      /* reserve dev. to raw serv. */
#define GDT_RELEASE     15                      /* release device */
#define GDT_RESERVE_ALL 16                      /* reserve all devices */
#define GDT_RELEASE_ALL 17                      /* release all devices */
#define GDT_RESET_BUS   18                      /* reset bus */
#define GDT_SCAN_START  19                      /* start device scan */
#define GDT_SCAN_END    20                      /* stop device scan */  

/* screen service commands */
#define GDT_REALTIME    3                       /* realtime clock to screens. */

/* IOCTL command defines */
#define SCSI_DR_INFO    0x00                    /* SCSI drive info */                   
#define SCSI_CHAN_CNT   0x05                    /* SCSI channel count */   
#define SCSI_DR_LIST    0x06                    /* SCSI drive list */
#define SCSI_DEF_CNT    0x15                    /* grown/primary defects */
#define DSK_STATISTICS  0x4b                    /* SCSI disk statistics */
#define IOCHAN_DESC     0x5d                    /* description of IO channel */
#define IOCHAN_RAW_DESC 0x5e                    /* description of raw IO chn. */
#define L_CTRL_PATTERN  0x20000000L             /* SCSI IOCTL mask */
#define ARRAY_INFO      0x12                    /* array drive info */
#define ARRAY_DRV_LIST  0x0f                    /* array drive list */
#define ARRAY_DRV_LIST2 0x34                    /* array drive list (new) */
#define LA_CTRL_PATTERN 0x10000000L             /* array IOCTL mask */
#define CACHE_DRV_CNT   0x01                    /* cache drive count */
#define CACHE_DRV_LIST  0x02                    /* cache drive list */
#define CACHE_INFO      0x04                    /* cache info */
#define CACHE_CONFIG    0x05                    /* cache configuration */
#define CACHE_DRV_INFO  0x07                    /* cache drive info */
#define BOARD_FEATURES  0x15                    /* controller features */
#define BOARD_INFO      0x28                    /* controller info */
#define HOST_GET        0x10001L                /* get host drive list */
#define IO_CHANNEL      0x00020000L             /* default IO channel */
#define INVALID_CHANNEL 0x0000ffffL             /* invalid channel */

/* service errors */
#define S_OK            1                       /* no error */
#define S_GENERR        6                       /* general error */
#define S_BSY           7                       /* controller busy */
#define S_CACHE_UNKNOWN 12                      /* cache serv.: drive unknown */
#define S_RAW_SCSI      12                      /* raw serv.: target error */
#define S_RAW_ILL       0xff                    /* raw serv.: illegal */
#define S_CACHE_RESERV	-24			/* cache: reserv. conflict */	

/* timeout values */
#define INIT_RETRIES    100000                  /* 100000 * 1ms = 100s */
#define INIT_TIMEOUT    100000                  /* 100000 * 1ms = 100s */
#define POLL_TIMEOUT    10000                   /* 10000 * 1ms = 10s */

/* priorities */
#define DEFAULT_PRI     0x20
#define IOCTL_PRI       0x10
#define HIGH_PRI        0x08

/* data directions */
#define GDTH_DATA_IN    0x01000000L             /* data from target */
#define GDTH_DATA_OUT   0x00000000L             /* data to target */

/* BMIC registers (EISA controllers) */
#define ID0REG          0x0c80                  /* board ID */
#define EINTENABREG     0x0c89                  /* interrupt enable */
#define SEMA0REG        0x0c8a                  /* command semaphore */
#define SEMA1REG        0x0c8b                  /* status semaphore */
#define LDOORREG        0x0c8d                  /* local doorbell */
#define EDENABREG       0x0c8e                  /* EISA system doorbell enab. */
#define EDOORREG        0x0c8f                  /* EISA system doorbell */
#define MAILBOXREG      0x0c90                  /* mailbox reg. (16 bytes) */
#define EISAREG         0x0cc0                  /* EISA configuration */

/* other defines */
#define LINUX_OS        8                       /* used for cache optim. */
#define SCATTER_GATHER  1                       /* s/g feature */
#define GDTH_MAXSG      32                      /* max. s/g elements */
#define SECS32          0x1f                    /* round capacity */
#define BIOS_ID_OFFS    0x10                    /* offset contr-ID in ISABIOS */
#define LOCALBOARD      0                       /* board node always 0 */
#define ASYNCINDEX      0                       /* cmd index async. event */
#define SPEZINDEX       1                       /* cmd index unknown service */
#define GDT_WR_THROUGH  0x100                   /* WRITE_THROUGH supported */


/* typedefs */
typedef u32     ulong32;
#define PACKED  __attribute__((packed))

/* screenservice message */
typedef struct {                               
    ulong32     msg_handle;                     /* message handle */
    ulong32     msg_len;                        /* size of message */
    ulong32     msg_alen;                       /* answer length */
    unchar      msg_answer;                     /* answer flag */
    unchar      msg_ext;                        /* more messages */
    unchar      msg_reserved[2];
    char        msg_text[MSGLEN+2];             /* the message text */
} PACKED gdth_msg_str;

/* IOCTL data structures */
/* SCSI drive info */
typedef struct {
    unchar      vendor[8];                      /* vendor string */
    unchar      product[16];                    /* product string */
    unchar      revision[4];                    /* revision */
    ulong32     sy_rate;                        /* current rate for sync. tr. */
    ulong32     sy_max_rate;                    /* max. rate for sync. tr. */
    ulong32     no_ldrive;                      /* belongs to this log. drv.*/
    ulong32     blkcnt;                         /* number of blocks */
    ushort      blksize;                        /* size of block in bytes */
    unchar      available;                      /* flag: access is available */
    unchar      init;                           /* medium is initialized */
    unchar      devtype;                        /* SCSI devicetype */
    unchar      rm_medium;                      /* medium is removable */
    unchar      wp_medium;                      /* medium is write protected */
    unchar      ansi;                           /* SCSI I/II or III? */
    unchar      protocol;                       /* same as ansi */
    unchar      sync;                           /* flag: sync. transfer enab. */
    unchar      disc;                           /* flag: disconnect enabled */
    unchar      queueing;                       /* flag: command queing enab. */
    unchar      cached;                         /* flag: caching enabled */
    unchar      target_id;                      /* target ID of device */
    unchar      lun;                            /* LUN id of device */
    unchar      orphan;                         /* flag: drive fragment */
    ulong32     last_error;                     /* sense key or drive state */
    ulong32     last_result;                    /* result of last command */
    ulong32     check_errors;                   /* err. in last surface check */
    unchar      percent;                        /* progress for surface check */
    unchar      last_check;                     /* IOCTRL operation */
    unchar      res[2];
    ulong32     flags;                          /* from 1.19/2.19: raw reserv.*/
    unchar      multi_bus;                      /* multi bus dev? (fibre ch.) */
    unchar      mb_status;                      /* status: available? */
    unchar      res2[2];
    unchar      mb_alt_status;                  /* status on second bus */
    unchar      mb_alt_bid;                     /* number of second bus */
    unchar      mb_alt_tid;                     /* target id on second bus */
    unchar      res3;
    unchar      fc_flag;                        /* from 1.22/2.22: info valid?*/
    unchar      res4;
    ushort      fc_frame_size;                  /* frame size (bytes) */
    char        wwn[8];                         /* world wide name */
} PACKED gdth_diskinfo_str;

/* get SCSI channel count  */
typedef struct {
    ulong32     channel_no;                     /* number of channel */
    ulong32     drive_cnt;                      /* drive count */
    unchar      siop_id;                        /* SCSI processor ID */
    unchar      siop_state;                     /* SCSI processor state */ 
} PACKED gdth_getch_str;

/* get SCSI drive numbers */
typedef struct {
    ulong32     sc_no;                          /* SCSI channel */
    ulong32     sc_cnt;                         /* sc_list[] elements */
    ulong32     sc_list[MAXID];                 /* minor device numbers */
} PACKED gdth_drlist_str;

/* get grown/primary defect count */
typedef struct {
    unchar      sddc_type;                      /* 0x08: grown, 0x10: prim. */
    unchar      sddc_format;                    /* list entry format */
    unchar      sddc_len;                       /* list entry length */
    unchar      sddc_res;
    ulong32     sddc_cnt;                       /* entry count */
} PACKED gdth_defcnt_str;

/* disk statistics */
typedef struct {
    ulong32     bid;                            /* SCSI channel */
    ulong32     first;                          /* first SCSI disk */
    ulong32     entries;                        /* number of elements */
    ulong32     count;                          /* (R) number of init. el. */
    ulong32     mon_time;                       /* time stamp */
    struct {
        unchar  tid;                            /* target ID */
        unchar  lun;                            /* LUN */
        unchar  res[2];
        ulong32 blk_size;                       /* block size in bytes */
        ulong32 rd_count;                       /* bytes read */
        ulong32 wr_count;                       /* bytes written */
        ulong32 rd_blk_count;                   /* blocks read */
        ulong32 wr_blk_count;                   /* blocks written */
        ulong32 retries;                        /* retries */
        ulong32 reassigns;                      /* reassigns */
    } PACKED list[1];
} PACKED gdth_dskstat_str;

/* IO channel header */
typedef struct {
    ulong32     version;                        /* version (-1UL: newest) */
    unchar      list_entries;                   /* list entry count */
    unchar      first_chan;                     /* first channel number */
    unchar      last_chan;                      /* last channel number */
    unchar      chan_count;                     /* (R) channel count */
    ulong32     list_offset;                    /* offset of list[0] */
} PACKED gdth_iochan_header;

/* get IO channel description */
typedef struct {
    gdth_iochan_header  hdr;
    struct {
        ulong32         address;                /* channel address */
        unchar          type;                   /* type (SCSI, FCAL) */
        unchar          local_no;               /* local number */
        ushort          features;               /* channel features */
    } PACKED list[MAXBUS];
} PACKED gdth_iochan_str;

/* get raw IO channel description */
typedef struct {
    gdth_iochan_header  hdr;
    struct {
        unchar      proc_id;                    /* processor id */
        unchar      proc_defect;                /* defect ? */
        unchar      reserved[2];
    } PACKED list[MAXBUS];
} PACKED gdth_raw_iochan_str;

/* array drive component */
typedef struct {
    ulong32     al_controller;                  /* controller ID */
    unchar      al_cache_drive;                 /* cache drive number */
    unchar      al_status;                      /* cache drive state */
    unchar      al_res[2];     
} PACKED gdth_arraycomp_str;

/* array drive information */
typedef struct {
    unchar      ai_type;                        /* array type (RAID0,4,5) */
    unchar      ai_cache_drive_cnt;             /* active cachedrives */
    unchar      ai_state;                       /* array drive state */
    unchar      ai_master_cd;                   /* master cachedrive */
    ulong32     ai_master_controller;           /* ID of master controller */
    ulong32     ai_size;                        /* user capacity [sectors] */
    ulong32     ai_striping_size;               /* striping size [sectors] */
    ulong32     ai_secsize;                     /* sector size [bytes] */
    ulong32     ai_err_info;                    /* failed cache drive */
    unchar      ai_name[8];                     /* name of the array drive */
    unchar      ai_controller_cnt;              /* number of controllers */
    unchar      ai_removable;                   /* flag: removable */
    unchar      ai_write_protected;             /* flag: write protected */
    unchar      ai_devtype;                     /* type: always direct access */
    gdth_arraycomp_str  ai_drives[35];          /* drive components: */
    unchar      ai_drive_entries;               /* number of drive components */
    unchar      ai_protected;                   /* protection flag */
    unchar      ai_verify_state;                /* state of a parity verify */
    unchar      ai_ext_state;                   /* extended array drive state */
    unchar      ai_expand_state;                /* array expand state (>=2.18)*/
    unchar      ai_reserved[3];
} PACKED gdth_arrayinf_str;

/* get array drive list */
typedef struct {
    ulong32     controller_no;                  /* controller no. */
    unchar      cd_handle;                      /* master cachedrive */
    unchar      is_arrayd;                      /* Flag: is array drive? */
    unchar      is_master;                      /* Flag: is array master? */
    unchar      is_parity;                      /* Flag: is parity drive? */
    unchar      is_hotfix;                      /* Flag: is hotfix drive? */
    unchar      res[3];
} PACKED gdth_alist_str;

typedef struct {
    ulong32     entries_avail;                  /* allocated entries */
    ulong32     entries_init;                   /* returned entries */
    ulong32     first_entry;                    /* first entry number */
    ulong32     list_offset;                    /* offset of following list */
    gdth_alist_str list[1];                     /* list */
} PACKED gdth_arcdl_str;

/* cache info/config IOCTL */
typedef struct {
    ulong32     version;                        /* firmware version */
    ushort      state;                          /* cache state (on/off) */
    ushort      strategy;                       /* cache strategy */
    ushort      write_back;                     /* write back state (on/off) */
    ushort      block_size;                     /* cache block size */
} PACKED gdth_cpar_str;

typedef struct {
    ulong32     csize;                          /* cache size */
    ulong32     read_cnt;                       /* read/write counter */
    ulong32     write_cnt;
    ulong32     tr_hits;                        /* hits */
    ulong32     sec_hits;
    ulong32     sec_miss;                       /* misses */
} PACKED gdth_cstat_str;

typedef struct {
    gdth_cpar_str   cpar;
    gdth_cstat_str  cstat;
} PACKED gdth_cinfo_str;

/* cache drive info */
typedef struct {
    unchar      cd_name[8];                     /* cache drive name */
    ulong32     cd_devtype;                     /* SCSI devicetype */
    ulong32     cd_ldcnt;                       /* number of log. drives */
    ulong32     cd_last_error;                  /* last error */
    unchar      cd_initialized;                 /* drive is initialized */
    unchar      cd_removable;                   /* media is removable */
    unchar      cd_write_protected;             /* write protected */
    unchar      cd_flags;                       /* Pool Hot Fix? */
    ulong32     ld_blkcnt;                      /* number of blocks */
    ulong32     ld_blksize;                     /* blocksize */
    ulong32     ld_dcnt;                        /* number of disks */
    ulong32     ld_slave;                       /* log. drive index */
    ulong32     ld_dtype;                       /* type of logical drive */
    ulong32     ld_last_error;                  /* last error */
    unchar      ld_name[8];                     /* log. drive name */
    unchar      ld_error;                       /* error */
} PACKED gdth_cdrinfo_str;

/* board features */
typedef struct {
    unchar      chaining;                       /* Chaining supported */
    unchar      striping;                       /* Striping (RAID-0) supp. */
    unchar      mirroring;                      /* Mirroring (RAID-1) supp. */
    unchar      raid;                           /* RAID-4/5/10 supported */
} PACKED gdth_bfeat_str;

/* board info IOCTL */
typedef struct {
    ulong32     ser_no;                         /* serial no. */
    unchar      oem_id[2];                      /* OEM ID */
    ushort      ep_flags;                       /* eprom flags */
    ulong32     proc_id;                        /* processor ID */
    ulong32     memsize;                        /* memory size (bytes) */
    unchar      mem_banks;                      /* memory banks */
    unchar      chan_type;                      /* channel type */
    unchar      chan_count;                     /* channel count */
    unchar      rdongle_pres;                   /* dongle present? */
    ulong32     epr_fw_ver;                     /* (eprom) firmware version */
    ulong32     upd_fw_ver;                     /* (update) firmware version */
    ulong32     upd_revision;                   /* update revision */
    char        type_string[16];                /* controller name */
    char        raid_string[16];                /* RAID firmware name */
    unchar      update_pres;                    /* update present? */
    unchar      xor_pres;                       /* XOR engine present? */
    unchar      prom_type;                      /* ROM type (eprom/flash) */
    unchar      prom_count;                     /* number of ROM devices */
    ulong32     dup_pres;                       /* duplexing module present? */
    ulong32     chan_pres;                      /* number of expansion chn. */
    ulong32     mem_pres;                       /* memory expansion inst. ? */
    unchar      ft_bus_system;                  /* fault bus supported? */
    unchar      subtype_valid;                  /* board_subtype valid? */
    unchar      board_subtype;                  /* subtype/hardware level */
    unchar      ramparity_pres;                 /* RAM parity check hardware? */
} PACKED gdth_binfo_str; 

/* get host drive info */
typedef struct {
    char        name[8];                        /* host drive name */
    ulong32     size;                           /* size (sectors) */
    unchar      host_drive;                     /* host drive number */
    unchar      log_drive;                      /* log. drive (master) */
    unchar      reserved;
    unchar      rw_attribs;                     /* r/w attribs */
    ulong32     start_sec;                      /* start sector */
} PACKED gdth_hentry_str;

typedef struct {
    ulong32     entries;                        /* entry count */
    ulong32     offset;                         /* offset of entries */
    unchar      secs_p_head;                    /* sectors/head */
    unchar      heads_p_cyl;                    /* heads/cylinder */
    unchar      reserved;
    unchar      clust_drvtype;                  /* cluster drive type */
    ulong32     location;                       /* controller number */
    gdth_hentry_str entry[MAX_HDRIVES];         /* entries */
} PACKED gdth_hget_str;    

/* scatter/gather element */
typedef struct {
    ulong32     sg_ptr;                         /* address */
    ulong32     sg_len;                         /* length */
} PACKED gdth_sg_str;

/* command structure */
typedef struct {
    ulong32     BoardNode;                      /* board node (always 0) */
    ulong32     CommandIndex;                   /* command number */
    ushort      OpCode;                         /* the command (READ,..) */
    union {
        struct {
            ushort      DeviceNo;               /* number of cache drive */
            ulong32     BlockNo;                /* block number */
            ulong32     BlockCnt;               /* block count */
            ulong32     DestAddr;               /* dest. addr. (if s/g: -1) */
            ulong32     sg_canz;                /* s/g element count */
            gdth_sg_str sg_lst[GDTH_MAXSG];     /* s/g list */
        } PACKED cache;                         /* cache service cmd. str. */
        struct {
            ushort      param_size;             /* size of p_param buffer */
            ulong32     subfunc;                /* IOCTL function */
            ulong32     channel;                /* device */
            ulong32     p_param;                /* buffer */
        } PACKED ioctl;                         /* IOCTL command structure */
        struct {
            ushort      reserved;
            union {
                struct {
                    ulong32  msg_handle;        /* message handle */
                    ulong32  msg_addr;          /* message buffer address */
                } PACKED msg;
                unchar       data[12];          /* buffer for rtc data, ... */
            } su;
        } PACKED screen;                        /* screen service cmd. str. */
        struct {
            ushort      reserved;
            ulong32     direction;              /* data direction */
            ulong32     mdisc_time;             /* disc. time (0: no timeout)*/
            ulong32     mcon_time;              /* connect time(0: no to.) */
            ulong32     sdata;                  /* dest. addr. (if s/g: -1) */
            ulong32     sdlen;                  /* data length (bytes) */
            ulong32     clen;                   /* SCSI cmd. length(6,10,12) */
            unchar      cmd[12];                /* SCSI command */
            unchar      target;                 /* target ID */
            unchar      lun;                    /* LUN */
            unchar      bus;                    /* SCSI bus number */
            unchar      priority;               /* only 0 used */
            ulong32     sense_len;              /* sense data length */
            ulong32     sense_data;             /* sense data addr. */
            ulong32     link_p;                 /* linked cmds (not supp.) */
            ulong32     sg_ranz;                /* s/g element count */
            gdth_sg_str sg_lst[GDTH_MAXSG];     /* s/g list */
        } PACKED raw;                           /* raw service cmd. struct. */
    } u;
    /* additional variables */
    unchar      Service;                        /* controller service */
    ushort      Status;                         /* command result */
    ulong32     Info;                           /* additional information */
    Scsi_Cmnd   *RequestBuffer;                 /* request buffer */
} PACKED gdth_cmd_str;

/* controller event structure */
#define ES_ASYNC    1
#define ES_DRIVER   2
#define ES_TEST     3
#define ES_SYNC     4
typedef struct {
    ushort                  size;               /* size of structure */
    union {
        char                stream[16];
        struct {
            ushort          ionode;
            ushort          service;
            ulong32         index;
        } PACKED driver;
        struct {
            ushort          ionode;
            ushort          service;
            ushort          status;
            ulong32         info;
            unchar          scsi_coord[3];
        } PACKED async;
        struct {
            ushort          ionode;
            ushort          service;
            ushort          status;
            ulong32         info;
            ushort          hostdrive;
            unchar          scsi_coord[3];
            unchar          sense_key;
        } PACKED sync;
        struct {
            ulong32         l1, l2, l3, l4;
        } PACKED test;
    } eu;
    ulong32                 severity;
    unchar                  event_string[256];          
} PACKED gdth_evt_data;

typedef struct {
    ulong32         first_stamp;
    ulong32         last_stamp;
    ushort          same_count;
    ushort          event_source;
    ushort          event_idx;
    unchar          application;
    unchar          reserved;
    gdth_evt_data   event_data;
} PACKED gdth_evt_str;


/* DPRAM structures */

/* interface area ISA/PCI */
typedef struct {
    unchar              S_Cmd_Indx;             /* special command */
    unchar volatile     S_Status;               /* status special command */
    ushort              reserved1;
    ulong32             S_Info[4];              /* add. info special command */
    unchar volatile     Sema0;                  /* command semaphore */
    unchar              reserved2[3];
    unchar              Cmd_Index;              /* command number */
    unchar              reserved3[3];
    ushort volatile     Status;                 /* command status */
    ushort              Service;                /* service(for async.events) */
    ulong32             Info[2];                /* additional info */
    struct {
        ushort          offset;                 /* command offs. in the DPRAM*/
        ushort          serv_id;                /* service */
    } PACKED comm_queue[MAXOFFSETS];            /* command queue */
    ulong32             bios_reserved[2];
    unchar              gdt_dpr_cmd[1];         /* commands */
} PACKED gdt_dpr_if;

/* SRAM structure PCI controllers */
typedef struct {
    ulong32     magic;                          /* controller ID from BIOS */
    ushort      need_deinit;                    /* switch betw. BIOS/driver */
    unchar      switch_support;                 /* see need_deinit */
    unchar      padding[9];
    unchar      os_used[16];                    /* OS code per service */
    unchar      unused[28];
    unchar      fw_magic;                       /* contr. ID from firmware */
} PACKED gdt_pci_sram;

/* SRAM structure EISA controllers (but NOT GDT3000/3020) */
typedef struct {
    unchar      os_used[16];                    /* OS code per service */
    ushort      need_deinit;                    /* switch betw. BIOS/driver */
    unchar      switch_support;                 /* see need_deinit */
    unchar      padding;
} PACKED gdt_eisa_sram;


/* DPRAM ISA controllers */
typedef struct {
    union {
        struct {
            unchar      bios_used[0x3c00-32];   /* 15KB - 32Bytes BIOS */
            ulong32     magic;                  /* controller (EISA) ID */
            ushort      need_deinit;            /* switch betw. BIOS/driver */
            unchar      switch_support;         /* see need_deinit */
            unchar      padding[9];
            unchar      os_used[16];            /* OS code per service */
        } PACKED dp_sram;
        unchar          bios_area[0x4000];      /* 16KB reserved for BIOS */
    } bu;
    union {
        gdt_dpr_if      ic;                     /* interface area */
        unchar          if_area[0x3000];        /* 12KB for interface */
    } u;
    struct {
        unchar          memlock;                /* write protection DPRAM */
        unchar          event;                  /* release event */
        unchar          irqen;                  /* board interrupts enable */
        unchar          irqdel;                 /* acknowledge board int. */
        unchar volatile Sema1;                  /* status semaphore */
        unchar          rq;                     /* IRQ/DRQ configuration */
    } PACKED io;
} PACKED gdt2_dpram_str;

/* DPRAM PCI controllers */
typedef struct {
    union {
        gdt_dpr_if      ic;                     /* interface area */
        unchar          if_area[0xff0-sizeof(gdt_pci_sram)];
    } u;
    gdt_pci_sram        gdt6sr;                 /* SRAM structure */
    struct {
        unchar          unused0[1];
        unchar volatile Sema1;                  /* command semaphore */
        unchar          unused1[3];
        unchar          irqen;                  /* board interrupts enable */
        unchar          unused2[2];
        unchar          event;                  /* release event */
        unchar          unused3[3];
        unchar          irqdel;                 /* acknowledge board int. */
        unchar          unused4[3];
    } PACKED io;
} PACKED gdt6_dpram_str;

/* PLX register structure (new PCI controllers) */
typedef struct {
    unchar              cfg_reg;        /* DPRAM cfg.(2:below 1MB,0:anywhere)*/
    unchar              unused1[0x3f];
    unchar volatile     sema0_reg;              /* command semaphore */
    unchar volatile     sema1_reg;              /* status semaphore */
    unchar              unused2[2];
    ushort volatile     status;                 /* command status */
    ushort              service;                /* service */
    ulong32             info[2];                /* additional info */
    unchar              unused3[0x10];
    unchar              ldoor_reg;              /* PCI to local doorbell */
    unchar              unused4[3];
    unchar volatile     edoor_reg;              /* local to PCI doorbell */
    unchar              unused5[3];
    unchar              control0;               /* control0 register(unused) */
    unchar              control1;               /* board interrupts enable */
    unchar              unused6[0x16];
} PACKED gdt6c_plx_regs;

/* DPRAM new PCI controllers */
typedef struct {
    union {
        gdt_dpr_if      ic;                     /* interface area */
        unchar          if_area[0x4000-sizeof(gdt_pci_sram)];
    } u;
    gdt_pci_sram        gdt6sr;                 /* SRAM structure */
} PACKED gdt6c_dpram_str;

/* i960 register structure (PCI MPR controllers) */
typedef struct {
    unchar              unused1[16];
    unchar volatile     sema0_reg;              /* command semaphore */
    unchar              unused2;
    unchar volatile     sema1_reg;              /* status semaphore */
    unchar              unused3;
    ushort volatile     status;                 /* command status */
    ushort              service;                /* service */
    ulong32             info[2];                /* additional info */
    unchar              ldoor_reg;              /* PCI to local doorbell */
    unchar              unused4[11];
    unchar volatile     edoor_reg;              /* local to PCI doorbell */
    unchar              unused5[7];
    unchar              edoor_en_reg;           /* board interrupts enable */
    unchar              unused6[27];
    ulong32             unused7[939];         
    ulong32             severity;       
    char                evt_str[256];           /* event string */
} PACKED gdt6m_i960_regs;

/* DPRAM PCI MPR controllers */
typedef struct {
    gdt6m_i960_regs     i960r;                  /* 4KB i960 registers */
    union {
        gdt_dpr_if      ic;                     /* interface area */
        unchar          if_area[0x3000-sizeof(gdt_pci_sram)];
    } u;
    gdt_pci_sram        gdt6sr;                 /* SRAM structure */
} PACKED gdt6m_dpram_str;


/* PCI resources */
typedef struct {
#if LINUX_VERSION_CODE >= 0x02015C
    struct pci_dev      *pdev;
#endif
    ushort              vendor_id;              /* vendor (ICP, Intel, ..) */
    ushort              device_id;              /* device ID (0,..,9) */
    ushort              subdevice_id;           /* sub device ID */
    unchar              bus;                    /* PCI bus */
    unchar              device_fn;              /* PCI device/function no. */
    ulong               dpmem;                  /* DPRAM address */
    ulong               io;                     /* IO address */
    ulong               io_mm;                  /* IO address mem. mapped */
    unchar              irq;                    /* IRQ */
} gdth_pci_str;


/* controller information structure */
typedef struct {
    ushort              oem_id;                 /* OEM */
    ushort              type;                   /* controller class */
    ushort              raw_feat;               /* feat. raw service (s/g,..) */
    ulong32             stype;                  /* subtype (PCI: device ID) */
    ushort              subdevice_id;           /* sub device ID (PCI) */
    ushort              fw_vers;                /* firmware version */
    ushort              cache_feat;             /* feat. cache serv. (s/g,..) */
    ushort              bmic;                   /* BMIC address (EISA) */
    void                *brd;                   /* DPRAM address */
    ulong32             brd_phys;               /* slot number/BIOS address */
    gdt6c_plx_regs      *plx;                   /* PLX regs (new PCI contr.) */
    gdth_cmd_str        *pccb;                  /* address command structure */
    char                *pscratch;              /* scratch (DMA) buffer */
    unchar              scratch_busy;           /* in use? */
    unchar              scan_mode;              /* current scan mode */
    unchar              irq;                    /* IRQ */
    unchar              drq;                    /* DRQ (ISA controllers) */
    ushort              status;                 /* command status */
    ushort              service;                /* service/firmware ver./.. */
    ulong32             info;
    ulong32             info2;                  /* additional info */
    Scsi_Cmnd           *req_first;             /* top of request queue */
    struct {
        unchar          present;                /* Flag: host drive present? */
        unchar          is_logdrv;              /* Flag: log. drive (master)? */
        unchar          is_arraydrv;            /* Flag: array drive? */
        unchar          is_master;              /* Flag: array drive master? */
        unchar          is_parity;              /* Flag: parity drive? */
        unchar          is_hotfix;              /* Flag: hotfix drive? */
        unchar          master_no;              /* number of master drive */
        unchar          lock;                   /* drive locked? (hot plug) */
        unchar          heads;                  /* mapping */
        unchar          secs;
        ushort          devtype;                /* further information */
        ulong32         size;                   /* capacity */
        unchar          ldr_no;                 /* log. drive no. */
        unchar          rw_attribs;             /* r/w attributes */
        unchar          cluster_type;           /* cluster properties */
        unchar          media_changed;          /* Flag:MOUNT/UNMOUNT occured */
        ulong32         start_sec;              /* start sector */
    } hdr[MAX_LDRIVES];                         /* host drives */
    struct {
        unchar          lock;                   /* channel locked? (hot plug) */
        unchar          pdev_cnt;               /* physical device count */
        unchar          local_no;               /* local channel number */
        unchar          io_cnt[MAXID];          /* current IO count */
        ulong32         address;                /* channel address */
        ulong32         id_list[MAXID];         /* IDs of the phys. devices */
    } raw[MAXBUS];                              /* SCSI channels */
    struct {
        Scsi_Cmnd       *cmnd;                  /* pending request */
        ushort          service;                /* service */
    } cmd_tab[GDTH_MAXCMDS];                    /* table of pend. requests */
    unchar              bus_cnt;                /* SCSI bus count */
    unchar              tid_cnt;                /* Target ID count */
    unchar              bus_id[MAXBUS];         /* IOP IDs */
    unchar              virt_bus;               /* number of virtual bus */
    unchar              more_proc;              /* more /proc info supported */
    ushort              cmd_cnt;                /* command count in DPRAM */
    ushort              cmd_len;                /* length of actual command */
    ushort              cmd_offs_dpmem;         /* actual offset in DPRAM */
    ushort              ic_all_size;            /* sizeof DPRAM interf. area */
    gdth_cpar_str       cpar;                   /* controller cache par. */
    gdth_bfeat_str      bfeat;                  /* controller features */
    gdth_binfo_str      binfo;                  /* controller info */
    gdth_evt_data       dvr;                    /* event structure */
#if LINUX_VERSION_CODE >= 0x02015F
    spinlock_t          smp_lock;
#endif
} gdth_ha_str;

/* structure for scsi_register(), SCSI bus != 0 */
typedef struct {
    ushort      hanum;
    ushort      busnum;
} gdth_num_str;

/* structure for scsi_register() */
typedef struct {
    gdth_num_str        numext;                 /* must be the first element */
    gdth_ha_str         haext;
    gdth_cmd_str        cmdext;
} gdth_ext_str;


/* INQUIRY data format */
typedef struct {
    unchar      type_qual;
    unchar      modif_rmb;
    unchar      version;
    unchar      resp_aenc;
    unchar      add_length;
    unchar      reserved1;
    unchar      reserved2;
    unchar      misc;
    unchar      vendor[8];
    unchar      product[16];
    unchar      revision[4];
} PACKED gdth_inq_data;

/* READ_CAPACITY data format */
typedef struct {
    ulong32     last_block_no;
    ulong32     block_length;
} PACKED gdth_rdcap_data;

/* REQUEST_SENSE data format */
typedef struct {
    unchar      errorcode;
    unchar      segno;
    unchar      key;
    ulong32     info;
    unchar      add_length;
    ulong32     cmd_info;
    unchar      adsc;
    unchar      adsq;
    unchar      fruc;
    unchar      key_spec[3];
} PACKED gdth_sense_data;

/* MODE_SENSE data format */
typedef struct {
    struct {
        unchar  data_length;
        unchar  med_type;
        unchar  dev_par;
        unchar  bd_length;
    } PACKED hd;
    struct {
        unchar  dens_code;
        unchar  block_count[3];
        unchar  reserved;
        unchar  block_length[3];
    } PACKED bd;
} PACKED gdth_modep_data;

/* stack frame */
typedef struct {
    ulong       b[10];                          /* 32/64 bit compiler ! */
} PACKED gdth_stackframe;


/* function prototyping */

int gdth_detect(Scsi_Host_Template *);
int gdth_release(struct Scsi_Host *);
int gdth_queuecommand(Scsi_Cmnd *,void (*done)(Scsi_Cmnd *));
int gdth_abort(Scsi_Cmnd *);
#if LINUX_VERSION_CODE >= 0x010346
int gdth_reset(Scsi_Cmnd *, unsigned int reset_flags);
#else
int gdth_reset(Scsi_Cmnd *);
#endif
const char *gdth_info(struct Scsi_Host *);

#if LINUX_VERSION_CODE >= 0x020322
int gdth_bios_param(Disk *,kdev_t,int *);
int gdth_proc_info(char *,char **,off_t,int,int,int);
int gdth_eh_abort(Scsi_Cmnd *scp);
int gdth_eh_device_reset(Scsi_Cmnd *scp);
int gdth_eh_bus_reset(Scsi_Cmnd *scp);
int gdth_eh_host_reset(Scsi_Cmnd *scp);
#define GDTH { proc_name:       "gdth",                          \
               proc_info:       gdth_proc_info,                  \
               name:            "GDT SCSI Disk Array Controller",\
               detect:          gdth_detect,                     \
               release:         gdth_release,                    \
               info:            gdth_info,                       \
               command:         NULL,                            \
               queuecommand:    gdth_queuecommand,               \
               eh_abort_handler: gdth_eh_abort,                  \
               eh_device_reset_handler: gdth_eh_device_reset,    \
               eh_bus_reset_handler: gdth_eh_bus_reset,          \
               eh_host_reset_handler: gdth_eh_host_reset,        \
               abort:           gdth_abort,                      \
               reset:           gdth_reset,                      \
               bios_param:      gdth_bios_param,                 \
               can_queue:       GDTH_MAXCMDS,                    \
               this_id:         -1,                              \
               sg_tablesize:    GDTH_MAXSG,                      \
               cmd_per_lun:     GDTH_MAXC_P_L,                   \
               present:         0,                               \
               unchecked_isa_dma: 1,                             \
               use_clustering:  ENABLE_CLUSTERING,               \
               use_new_eh_code: 1       /* use new error code */ }    

#elif LINUX_VERSION_CODE >= 0x02015F
int gdth_bios_param(Disk *,kdev_t,int *);
extern struct proc_dir_entry proc_scsi_gdth;
int gdth_proc_info(char *,char **,off_t,int,int,int);
int gdth_eh_abort(Scsi_Cmnd *scp);
int gdth_eh_device_reset(Scsi_Cmnd *scp);
int gdth_eh_bus_reset(Scsi_Cmnd *scp);
int gdth_eh_host_reset(Scsi_Cmnd *scp);
#define GDTH { proc_dir:        &proc_scsi_gdth,                 \
               proc_info:       gdth_proc_info,                  \
               name:            "GDT SCSI Disk Array Controller",\
               detect:          gdth_detect,                     \
               release:         gdth_release,                    \
               info:            gdth_info,                       \
               command:         NULL,                            \
               queuecommand:    gdth_queuecommand,               \
               eh_abort_handler: gdth_eh_abort,                  \
               eh_device_reset_handler: gdth_eh_device_reset,    \
               eh_bus_reset_handler: gdth_eh_bus_reset,          \
               eh_host_reset_handler: gdth_eh_host_reset,        \
               abort:           gdth_abort,                      \
               reset:           gdth_reset,                      \
               bios_param:      gdth_bios_param,                 \
               can_queue:       GDTH_MAXCMDS,                    \
               this_id:         -1,                              \
               sg_tablesize:    GDTH_MAXSG,                      \
               cmd_per_lun:     GDTH_MAXC_P_L,                   \
               present:         0,                               \
               unchecked_isa_dma: 1,                             \
               use_clustering:  ENABLE_CLUSTERING,               \
               use_new_eh_code: 1       /* use new error code */ }    

#elif LINUX_VERSION_CODE >= 0x010300
int gdth_bios_param(Disk *,kdev_t,int *);
extern struct proc_dir_entry proc_scsi_gdth;
int gdth_proc_info(char *,char **,off_t,int,int,int);
#define GDTH { NULL, NULL,                              \
                   &proc_scsi_gdth,                     \
                   gdth_proc_info,                      \
                   "GDT SCSI Disk Array Controller",    \
                   gdth_detect,                         \
                   gdth_release,                        \
                   gdth_info,                           \
                   NULL,                                \
                   gdth_queuecommand,                   \
                   gdth_abort,                          \
                   gdth_reset,                          \
                   NULL,                                \
                   gdth_bios_param,                     \
                   GDTH_MAXCMDS,                        \
                   -1,                                  \
                   GDTH_MAXSG,                          \
                   GDTH_MAXC_P_L,                       \
                   0,                                   \
                   1,                                   \
                   ENABLE_CLUSTERING}

#else
int gdth_bios_param(Disk *,int,int *);
#define GDTH { NULL, NULL,                              \
                   "GDT SCSI Disk Array Controller",    \
                   gdth_detect,                         \
                   gdth_release,                        \
                   gdth_info,                           \
                   NULL,                                \
                   gdth_queuecommand,                   \
                   gdth_abort,                          \
                   gdth_reset,                          \
                   NULL,                                \
                   gdth_bios_param,                     \
                   GDTH_MAXCMDS,                        \
                   -1,                                  \
                   GDTH_MAXSG,                          \
                   GDTH_MAXC_P_L,                       \
                   0,                                   \
                   1,                                   \
                   ENABLE_CLUSTERING}
#endif

#endif

