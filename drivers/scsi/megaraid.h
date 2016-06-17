#ifndef __MEGARAID_H__
#define __MEGARAID_H__

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

/*
 * For state flag. Do not use LSB(8 bits) which are
 * reserved for storing info about channels.
 */
#define IN_ISR		  	0x80000000L
#define IN_ABORT		0x40000000L
#define IN_RESET		0x20000000L
#define IN_QUEUE		0x10000000L
#define BOARD_QUARTZ	0x08000000L
#define BOARD_40LD	   	0x04000000L
#define BOARD_64BIT		0x02000000L

#define SCB_FREE     0x0
#define SCB_ACTIVE   0x1
#define SCB_WAITQ    0x2
#define SCB_ISSUED   0x3
#define SCB_COMPLETE 0x4
#define SCB_ABORTED  0x5
#define SCB_RESET    0x6

#define M_RD_CRLFSTR 			"\n"
#define M_RD_IOCTL_CMD			0x80
#define M_RD_IOCTL_CMD_NEW		0x81
#define M_RD_DRIVER_IOCTL_INTERFACE	0x82

#define MEGARAID_VERSION "v1.18k (Release Date: Thu Aug 28 10:05:11 EDT 2003)\n"


#define MEGARAID_IOCTL_VERSION 	114

/* Methods */
#define GET_DRIVER_INFO 		0x1

#define MEGA_CMD_TIMEOUT		10

/* Feel free to fiddle with these.. max values are:
   SGLIST     0..26
   COMMANDS   0..253
   CMDPERLUN  0..63
*/

#define MAX_SGLIST	      	0x1A
#define MAX_COMMANDS	    	127
#define MAX_CMD_PER_LUN	 	63
#define MAX_FIRMWARE_STATUS     46

#define MAX_LOGICAL_DRIVES      8
#define MAX_CHANNEL	     	5
#define MAX_TARGET	      	15
#define MAX_PHYSICAL_DRIVES     MAX_CHANNEL*MAX_TARGET

#define INQUIRY_DATA_SIZE       0x24
#define MAX_CDB_LEN	     		0x0A
#define MAX_REQ_SENSE_LEN       0x20

#define INTR_VALID	      	0x40

/* Direction Macros for MBOX Data direction */
#define TO_DEVICE		0x0
#define FROM_DEVICE		0x1
#define FROMTO_DEVICE		0x2

/* Mailbox commands */
#define MEGA_MBOXCMD_LREAD		0x01
#define MEGA_MBOXCMD_LWRITE		0x02
#define MEGA_MBOXCMD_LREAD64		0xA7
#define MEGA_MBOXCMD_LWRITE64		0xA8
#define MEGA_MBOXCMD_PASSTHRU		0x03
#define MEGA_MBOXCMD_EXTPASSTHRU	0xE3
#define MEGA_MBOXCMD_PASSTHRU64		0xC3
#define MEGA_MBOXCMD_ADAPTERINQ		0x05


/* Offsets into Mailbox */
#define COMMAND_PORT       	0x00
#define COMMAND_ID_PORT    	0x01
#define SG_LIST_PORT0      	0x08
#define SG_LIST_PORT1      	0x09
#define SG_LIST_PORT2      	0x0a
#define SG_LIST_PORT3      	0x0b
#define SG_ELEMENT_PORT    	0x0d
#define NO_FIRED_PORT      	0x0f

/* I/O Port offsets */
#define I_CMD_PORT	 	0x00
#define I_ACK_PORT	 	0x00
#define I_TOGGLE_PORT      	0x01
#define INTR_PORT	  	0x0a

#define MAILBOX_SIZE       	(sizeof(mega_mailbox)-16)
#define MBOX_BUSY_PORT     	0x00
#define MBOX_PORT0	 	0x04
#define MBOX_PORT1	 	0x05
#define MBOX_PORT2	 	0x06
#define MBOX_PORT3	 	0x07
#define ENABLE_MBOX_REGION 	0x0B

/* I/O Port Values */
#define ISSUE_BYTE	 	0x10
#define ACK_BYTE	   	0x08
#define ENABLE_INTR_BYTE   	0xc0
#define DISABLE_INTR_BYTE  	0x00
#define VALID_INTR_BYTE    	0x40
#define MBOX_BUSY_BYTE     	0x10
#define ENABLE_MBOX_BYTE   	0x00

/* Setup some port macros here */
#define WRITE_MAILBOX(base,offset,value)   	*(base+offset)=value
#define READ_MAILBOX(base,offset)		*(base+offset)

#define WRITE_PORT(base,offset,value)      	outb_p(value,base+offset)
#define READ_PORT(base,offset)	     		inb_p(base+offset)

#define ISSUE_COMMAND(base)	WRITE_PORT(base,I_CMD_PORT,ISSUE_BYTE)
#define CLEAR_INTR(base)	WRITE_PORT(base,I_ACK_PORT,ACK_BYTE)
#define ENABLE_INTR(base)	WRITE_PORT(base,I_TOGGLE_PORT,ENABLE_INTR_BYTE)
#define DISABLE_INTR(base)	WRITE_PORT(base,I_TOGGLE_PORT,DISABLE_INTR_BYTE)

#ifndef PCI_VENDOR_ID_LSI_LOGIC
#define PCI_VENDOR_ID_LSI_LOGIC		0x1000
#endif

/* Define AMI's PCI codes */
#ifndef PCI_VENDOR_ID_AMI
#define PCI_VENDOR_ID_AMI		0x101E
#endif

#ifndef PCI_DEVICE_ID_AMI_MEGARAID
#define PCI_DEVICE_ID_AMI_MEGARAID	0x9010
#endif

#ifndef PCI_DEVICE_ID_AMI_MEGARAID2
#define PCI_DEVICE_ID_AMI_MEGARAID2	0x9060
#endif

#ifndef PCI_DEVICE_ID_AMI_MEGARAID3
#define PCI_DEVICE_ID_AMI_MEGARAID3	0x1960
#endif

#define PCI_VENDOR_ID_DISCOVERY		0x1028
#define PCI_DEVICE_ID_DISCOVERY		0x000E

#define PCI_VENDOR_ID_PERC4_DI_YSTONE	0x1028
#define PCI_DEVICE_ID_PERC4_DI_YSTONE	0x000F

#define PCI_VENDOR_ID_PERC4_QC_VERDE	0x1000
#define PCI_DEVICE_ID_PERC4_QC_VERDE	0x0407

/* Special Adapter Commands */
#define FW_FIRE_WRITE   	0x2C
#define FW_FIRE_FLASH   	0x2D

#define FC_NEW_CONFIG	   		0xA1
#define DCMD_FC_CMD			0xA1
#define DCMD_FC_PROCEED	     		0x02
#define DCMD_DELETE_LOGDRV	  	0x03
#define DCMD_FC_READ_NVRAM_CONFIG   	0x04
#define DCMD_FC_READ_NVRAM_CONFIG_64   	0xC0
#define DCMD_FC_READ_FINAL_CONFIG   	0x05
#define DCMD_GET_DISK_CONFIG		0x06
#define DCMD_GET_DISK_CONFIG_64		0xC2
#define DCMD_CHANGE_LDNO	    	0x07
#define DCMD_COMPACT_CONFIG	 	0x08
#define DCMD_DELETE_DRIVEGROUP      	0x09
#define DCMD_GET_LOOPID_INFO		0x0A
#define DCMD_CHANGE_LOOPID	  	0x0B
#define DCMD_GET_NUM_SCSI_CHANS     	0x0C
#define DCMD_WRITE_CONFIG	   	0x0D
#define DCMD_WRITE_CONFIG_64   		0xC1

#define NC_SUBOP_PRODUCT_INFO       	0x0E
#define NC_SUBOP_ENQUIRY3	   	0x0F
#define ENQ3_GET_SOLICITED_NOTIFY_ONLY  0x01
#define ENQ3_GET_SOLICITED_FULL	 	0x02
#define ENQ3_GET_UNSOLICITED	    	0x03

#define PCI_CONF_BASE_ADDR_OFFSET  	0x10
#define PCI_CONF_IRQ_OFFSET		0x3c
#define PCI_CONF_AMISIG	    		0xa0
#define PCI_CONF_AMISIG64		0xa4

/* Sub-System Vendor ID sorted on alphabetical order*/
#define LSI_SUBSYS_ID                  0x1000
#define	AMI_SUBSYS_ID			0x101E
#define DELL_SUBSYS_ID			0x1028
#define	HP_SUBSYS_ID			0x103C
#define	INTEL_SUBSYS_ID			0x8086

#define AMI_SIGNATURE	      		0x3344
#define AMI_SIGNATURE_471	  	0xCCCC
#define AMI_64BIT_SIGNATURE		0x0299

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,1,0)	/*0x20100 */
#define MEGARAID \
  { NULL,			      	/* Next				*/\
    NULL,			        /* Usage Count Pointer		*/\
    NULL,			       	/* proc Directory Entry		*/\
    megaraid_proc_info,		 	/* proc Info Function		*/\
    "MegaRAID",			 	/* Driver Name			*/\
    megaraid_detect,		    	/* Detect Host Adapter		*/\
    megaraid_release,		   	/* Release Host Adapter		*/\
    megaraid_info,		      	/* Driver Info Function		*/\
    megaraid_command,		   	/* Command Function		*/\
    megaraid_queue,		     	/* Queue Command Function	*/\
    megaraid_abort,		     	/* Abort Command Function	*/\
    megaraid_reset,		     	/* Reset Command Function	*/\
    NULL,			       	/* Slave Attach Function	*/\
    megaraid_biosparam,		 	/* Disk BIOS Parameters		*/\
    MAX_COMMANDS,		       	/* # of cmds that can be\
					outstanding at any time		*/\
    7,				  	/* HBA Target ID		*/\
    MAX_SGLIST,			 	/* Scatter/Gather Table Size	*/\
    MAX_CMD_PER_LUN,		    	/* SCSI Commands per LUN	*/\
    0,				  	/* Present			*/\
    0,				  	/* Default Unchecked ISA DMA	*/\
    ENABLE_CLUSTERING }			/* Enable Clustering		*/
#else
#define MEGARAID \
  {\
    name:	    	"MegaRAID",		/* Driver Name			*/\
    proc_info:		megaraid_proc_info,     /* /proc driver info		*/\
    detect:		megaraid_detect,	/* Detect Host Adapter		*/\
    release:	  	megaraid_release,	/* Release Host Adapter		*/\
    info:	     	megaraid_info,	   	/* Driver Info Function		*/\
    command:	  	megaraid_command,	/* Command Function		*/\
    queuecommand:  	megaraid_queue,		/* Queue Command Function	*/\
    abort:	    	megaraid_abort,	  	/* Abort Command Function	*/\
    reset:	    	megaraid_reset,	  	/* Reset Command Function	*/\
    bios_param:     	megaraid_biosparam, 	/* Disk BIOS Parameters		*/\
    can_queue:		MAX_COMMANDS,	    	/* Can Queue			*/\
    this_id:	  	7,		       	/* HBA Target ID		*/\
    sg_tablesize:   	MAX_SGLIST,	  	/* Scatter/Gather Table Size	*/\
    cmd_per_lun:    	MAX_CMD_PER_LUN,	/* SCSI Commands per LUN	*/\
    present:	  	0,		       	/* Present			*/\
    unchecked_isa_dma:	0,		       	/* Default Unchecked ISA DMA	*/\
    use_clustering:   	ENABLE_CLUSTERING, 	/* Enable Clustering		*/\
	highmem_io:		1, /* enable HIGHMEM I/O */ \
  }
#endif

/***********************************************************************
 * Structure Declarations for the Firmware supporting 40 Logical Drives
 * and 256 Physical Drives.
 ***********************************************************************/

#define FC_MAX_LOGICAL_DRIVES       	40
#define FC_MAX_LOG_DEVICES	  	FC_MAX_LOGICAL_DRIVES
#define FC_MAX_SPAN_DEPTH	   	8
#define FC_MAX_ROW_SIZE	     		32

#define FC_MAX_CHANNELS	     		16
#define FC_MAX_TARGETS_PER_CHANNEL  	16
#define FC_MAX_PHYSICAL_DEVICES     	256

/********************************************
 * PRODUCT_INFO
 ********************************************/

#define SIG_40LOG_32STR_8SPN  0x00282008

/*
 * Utilities declare this strcture size as 1024 bytes. So more fields can
 * be added in future.
 */

struct MRaidProductInfo {
	u32 DataSize;		/* current size in bytes (not including resvd) */
	u32 ConfigSignature;
	/* Current value is 0x00282008
	 * 0x28=MAX_LOGICAL_DRIVES,
	 * 0x20=Number of stripes and
	 * 0x08=Number of spans */
	u8 FwVer[16];		/* printable ASCI string */
	u8 BiosVer[16];		/* printable ASCI string */
	u8 ProductName[80];	/* printable ASCI string */

	u8 MaxConcCmds;		/* Max. concurrent commands supported */
	u8 SCSIChanPresent;	/* Number of SCSI Channels detected */
	u8 FCLoopPresent;	/* Number of Fibre Loops detected */
	u8 memType;		/* EDO, FPM, SDRAM etc */

	u32 signature;
	u16 DramSize;		/* In terms of MB */
	u16 subSystemID;

	u16 subSystemVendorID;
	u8 numNotifyCounters;
	u8 pad1k[889];		/* 135 + 889 resvd = 1024 total size */
} __attribute__ ((packed));
typedef struct MRaidProductInfo megaRaidProductInfo;

/********************************************
 * Standard ENQUIRY
 ********************************************/
struct FC_ADP_INFO {
	u8 MaxConcCmds;		/* Max. concurrent commands supported. */
	u8 RbldRate;		/* Rebuild Rate. Varies from 0%-100% */
	u8 MaxTargPerChan;	/* Max. Targets supported per chan. */
	u8 ChanPresent;		/* No. of Chans present on this adapter. */
	u8 FwVer[4];		/* Firmware version. */
	u16 AgeOfFlash;		/* No. of times FW has been downloaded. */
	u8 ChipSetValue;	/* Contents of 0xC0000832 */
	u8 DramSize;		/* In terms of MB */
	u8 CacheFlushInterval;	/* In terms of Seconds */
	u8 BiosVersion[4];
	u8 BoardType;
	u8 sense_alert;
	u8 write_config_count;	/* Increase with evry configuration change */
	u8 drive_inserted_count;/* Increase with every drive inserted */
	u8 inserted_drive;	/* Channel: Id of inserted drive */
	u8 battery_status;
	/*
	   BIT 0 : battery module missing
	   BIT 1 : VBAD
	   BIT 2 : temp high
	   BIT 3 : battery pack missing
	   BIT 4,5 : 00 - charge complete
	   01 - fast charge in prog
	   10 - fast charge fail
	   11 - undefined
	   BIt 6 : counter > 1000
	   Bit 7 : undefined
	 */
	u8 dec_fault_bus_info;	/* was resvd */
} __attribute__ ((packed));

struct FC_LDRV_INFO {
	u8 NumLDrv;		/* No. of Log. Drvs configured. */
	u8 recon_state[FC_MAX_LOGICAL_DRIVES / 8];
	/* bit field for State of reconstruct */
	u16 LDrvOpStatus[FC_MAX_LOGICAL_DRIVES / 8];
	/* bit field Status of Long Operations. */

	u32 LDrvSize[FC_MAX_LOGICAL_DRIVES];	/* Size of each log. Drv. */
	u8 LDrvProp[FC_MAX_LOGICAL_DRIVES];
	u8 LDrvState[FC_MAX_LOGICAL_DRIVES];	/* State of Logical Drives. */
} __attribute__ ((packed));

#define PREVSTAT_MASK   0xf0
#define CURRSTAT_MASK   0x0f

struct FC_PDRV_INFO {
	u8 PDrvState[FC_MAX_PHYSICAL_DEVICES];	/* State of Phys Drvs. */
} __attribute__ ((packed));

struct FC_AdapterInq {
	struct FC_ADP_INFO AdpInfo;
	struct FC_LDRV_INFO LogdrvInfo;
	struct FC_PDRV_INFO PhysdrvInfo;
} __attribute__ ((packed));

typedef struct FC_AdapterInq mega_RAIDINQ_FC;

/********************************************
 * NOTIFICATION
 ********************************************/

#define MAX_NOTIFY_SIZE     0x80
#define CUR_NOTIFY_SIZE     sizeof(struct MegaRAID_Notify)

/*
 * Utilities declare this strcture size as ?? bytes. So more fields can
 * be added in future.
 */
struct MegaRAID_Notify {
	u32 globalCounter;	/* Any change increments this counter */

	u8 paramCounter;	/* Indicates any params changed  */
	u8 paramId;		/* Param modified - defined below */
	u16 paramVal;		/* New val of last param modified */

	u8 writeConfigCounter;	/* write config occurred */
	u8 writeConfigRsvd[3];

	u8 ldrvOpCounter;	/* Indicates ldrv op started/completed */
	u8 ldrvOpId;		/* ldrv num */
	u8 ldrvOpCmd;		/* ldrv operation - defined below */
	u8 ldrvOpStatus;	/* status of the operation */

	u8 ldrvStateCounter;	/* Indicates change of ldrv state */
	u8 ldrvStateId;		/* ldrv num */
	u8 ldrvStateNew;	/* New state */
	u8 ldrvStateOld;	/* old state */

	u8 pdrvStateCounter;	/* Indicates change of ldrv state */
	u8 pdrvStateId;		/* pdrv id */
	u8 pdrvStateNew;	/* New state */
	u8 pdrvStateOld;	/* old state */

	u8 pdrvFmtCounter;	/* Indicates pdrv format started/over */
	u8 pdrvFmtId;		/* pdrv id */
	u8 pdrvFmtVal;		/* format started/over */
	u8 pdrvFmtRsvd;

	u8 targXferCounter;	/* Indicates SCSI-2 Xfer rate change */
	u8 targXferId;		/* pdrv Id  */
	u8 targXferVal;		/* new Xfer params of last pdrv */
	u8 targXferRsvd;

	u8 fcLoopIdChgCounter;	/* Indicates loopid changed */
	u8 fcLoopIdPdrvId;	/* pdrv id */
	u8 fcLoopId0;		/* loopid on fc loop 0 */
	u8 fcLoopId1;		/* loopid on fc loop 1 */

	u8 fcLoopStateCounter;	/* Indicates loop state changed */
	u8 fcLoopState0;	/* state of fc loop 0 */
	u8 fcLoopState1;	/* state of fc loop 1 */
	u8 fcLoopStateRsvd;
} __attribute__ ((packed));

/********************************************
 * PARAM IDs in Notify struct
 ********************************************/
#define PARAM_RBLD_RATE		 0x01
    /*--------------------------------------
     * Param val =
     *      byte 0: new rbld rate
     *--------------------------------------*/
#define PARAM_CACHE_FLUSH_INTERVAL      0x02
    /*--------------------------------------
     * Param val =
     *      byte 0: new cache flush interval
     *--------------------------------------*/
#define PARAM_SENSE_ALERT	       0x03
    /*--------------------------------------
     * Param val =
     *      byte 0: last pdrv id causing chkcond
     *--------------------------------------*/
#define PARAM_DRIVE_INSERTED	    0x04
    /*--------------------------------------
     * Param val =
     *      byte 0: last pdrv id inserted
     *--------------------------------------*/
#define PARAM_BATTERY_STATUS	    0x05
    /*--------------------------------------
     * Param val =
     *      byte 0: battery status
     *--------------------------------------*/

/********************************************
 * Ldrv operation cmd in Notify struct
 ********************************************/
#define LDRV_CMD_CHKCONSISTANCY	 0x01
#define LDRV_CMD_INITIALIZE	 0x02
#define LDRV_CMD_RECONSTRUCTION	 0x03

/********************************************
 * Ldrv operation status in Notify struct
 ********************************************/
#define LDRV_OP_SUCCESS		 0x00
#define LDRV_OP_FAILED		 0x01
#define LDRV_OP_ABORTED		 0x02
#define LDRV_OP_CORRECTED	 0x03
#define LDRV_OP_STARTED		 0x04

/********************************************
 * Raid Logical drive states.
 ********************************************/
#define     RDRV_OFFLINE	0
#define     RDRV_DEGRADED	1
#define     RDRV_OPTIMAL	2
#define     RDRV_DELETED	3

/*******************************************
 * Physical drive states.
 *******************************************/
#define     PDRV_UNCNF		0
#define     PDRV_ONLINE		3
#define     PDRV_FAILED		4
#define     PDRV_RBLD		5

/*******************************************
 * Formal val in Notify struct
 *******************************************/
#define PDRV_FMT_START		0x01
#define PDRV_FMT_OVER		0x02

/********************************************
 * FC Loop State in Notify Struct
 ********************************************/
#define ENQ_FCLOOP_FAILED	0
#define ENQ_FCLOOP_ACTIVE	1
#define ENQ_FCLOOP_TRANSIENT	2

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
#define M_RD_DMA_TYPE_NONE	      	0xFFFF
#define M_RD_PTHRU_WITH_BULK_DATA   	0x0001
#define M_RD_PTHRU_WITH_SGLIST	  	0x0002
#define M_RD_BULK_DATA_ONLY	     	0x0004
#define M_RD_SGLIST_ONLY		0x0008
#define M_RD_EPTHRU_WITH_BULK_DATA   	0x0010
#endif
/********************************************
 * ENQUIRY3
 ********************************************/
/*
 * Utilities declare this strcture size as 1024 bytes. So more fields can
 * be added in future.
 */
struct MegaRAID_Enquiry3 {
	u32 dataSize;		/* current size in bytes (not including resvd) */

	struct MegaRAID_Notify notify;

	u8 notifyRsvd[MAX_NOTIFY_SIZE - CUR_NOTIFY_SIZE];

	u8 rbldRate;		/* Rebuild rate (0% - 100%) */
	u8 cacheFlushInterval;	/* In terms of Seconds */
	u8 senseAlert;
	u8 driveInsertedCount;	/* drive insertion count */

	u8 batteryStatus;
	u8 numLDrv;		/* No. of Log Drives configured */
	u8 reconState[FC_MAX_LOGICAL_DRIVES / 8];	/* State of reconstruct */
	u16 lDrvOpStatus[FC_MAX_LOGICAL_DRIVES / 8];	/* log. Drv Status */

	u32 lDrvSize[FC_MAX_LOGICAL_DRIVES];	/* Size of each log. Drv */
	u8 lDrvProp[FC_MAX_LOGICAL_DRIVES];
	u8 lDrvState[FC_MAX_LOGICAL_DRIVES];	/* State of Logical Drives */
	u8 pDrvState[FC_MAX_PHYSICAL_DEVICES];	/* State of Phys. Drvs. */
	u16 physDrvFormat[FC_MAX_PHYSICAL_DEVICES / 16];

	u8 targXfer[80];	/* phys device transfer rate */
	u8 pad1k[263];		/* 761 + 263reserved = 1024 bytes total size */
} __attribute__ ((packed));
typedef struct MegaRAID_Enquiry3 mega_Enquiry3;

/* Structures */
typedef struct _mega_ADP_INFO {
	u8 MaxConcCmds;
	u8 RbldRate;
	u8 MaxTargPerChan;
	u8 ChanPresent;
	u8 FwVer[4];
	u16 AgeOfFlash;
	u8 ChipSetValue;
	u8 DramSize;
	u8 CacheFlushInterval;
	u8 BiosVer[4];
	u8 resvd[7];
} mega_ADP_INFO;

typedef struct _mega_LDRV_INFO {
	u8 NumLDrv;
	u8 resvd[3];
	u32 LDrvSize[MAX_LOGICAL_DRIVES];
	u8 LDrvProp[MAX_LOGICAL_DRIVES];
	u8 LDrvState[MAX_LOGICAL_DRIVES];
} mega_LDRV_INFO;

typedef struct _mega_PDRV_INFO {
	u8 PDrvState[MAX_PHYSICAL_DRIVES];
	u8 resvd;
} mega_PDRV_INFO;

/* RAID inquiry: Mailbox command 0x5*/
typedef struct _mega_RAIDINQ {
	mega_ADP_INFO AdpInfo;
	mega_LDRV_INFO LogdrvInfo;
	mega_PDRV_INFO PhysdrvInfo;
} mega_RAIDINQ;

/* Passthrough command: Mailbox command 0x3*/
typedef struct mega_passthru {
	u8 timeout:3;		/* 0=6sec/1=60sec/2=10min/3=3hrs */
	u8 ars:1;
	u8 reserved:3;
	u8 islogical:1;
	u8 logdrv;		/* if islogical == 1 */
	u8 channel;		/* if islogical == 0 */
	u8 target;		/* if islogical == 0 */
	u8 queuetag;		/* unused */
	u8 queueaction;		/* unused */
	u8 cdb[MAX_CDB_LEN];
	u8 cdblen;
	u8 reqsenselen;
	u8 reqsensearea[MAX_REQ_SENSE_LEN];
	u8 numsgelements;
	u8 scsistatus;
	u32 dataxferaddr;
	u32 dataxferlen;
} mega_passthru;

/*
 * Extended passthru: support CDB > 10 bytes
 */
typedef struct {
	u8 timeout:3;		/* 0=6sec/1=60sec/2=10min/3=3hrs */
	u8 ars:1;
	u8 rsvd1:1;
	u8 cd_rom:1;
	u8 rsvd2:1;
	u8 islogical:1;

	u8 logdrv;		/* if islogical == 1 */
	u8 channel;		/* if islogical == 0 */
	u8 target;		/* if islogical == 0 */

	u8 queuetag;		/* unused */
	u8 queueaction;		/* unused */

	u8 cdblen;
	u8 rsvd3;
	u8 cdb[16];

	u8 numsgelements;
	u8 status;
	u8 reqsenselen;
	u8 reqsensearea[MAX_REQ_SENSE_LEN];
	u8 rsvd4;

	u32 dataxferaddr;
	u32 dataxferlen;
}mega_ext_passthru;

struct _mega_mailbox {
	/* 0x0 */ u8 cmd;
	/* 0x1 */ u8 cmdid;
	/* 0x2 */ u16 numsectors;
	/* 0x4 */ u32 lba;
	/* 0x8 */ u32 xferaddr;
	/* 0xC */ u8 logdrv;
	/* 0xD */ u8 numsgelements;
	/* 0xE */ u8 resvd;
	/* 0xF */ u8 busy;
	/* 0x10 */ u8 numstatus;
	/* 0x11 */ u8 status;
	/* 0x12 */ u8 completed[46];
	volatile u8 mraid_poll;
	volatile u8 mraid_ack;
	u8 pad[16];		/* for alignment purposes */
} __attribute__ ((packed));
typedef struct _mega_mailbox mega_mailbox;

typedef struct {
	u32 xferSegment_lo;
	u32 xferSegment_hi;
	mega_mailbox mailbox;
} mega_mailbox64;

typedef struct _mega_ioctl_mbox {
	/* 0x0 */ u8 cmd;
	/* 0x1 */ u8 cmdid;
	/* 0x2 */ u8 channel;
	/* 0x3 */ u8 param;
	/* 0x4 */ u8 pad[4];
	/* 0x8 */ u32 xferaddr;
	/* 0xC */ u8 logdrv;
	/* 0xD */ u8 numsgelements;
	/* 0xE */ u8 resvd;
	/* 0xF */ u8 busy;
	/* 0x10 */ u8 numstatus;
	/* 0x11 */ u8 status;
	/* 0x12 */ u8 completed[46];
	u8 mraid_poll;
	u8 mraid_ack;
	u8 malign[16];
} mega_ioctl_mbox;

typedef struct _mega_64sglist32 {
	u64 address;
	u32 length;
} __attribute__ ((packed)) mega_64sglist;

typedef struct _mega_sglist {
	u32 address;
	u32 length;
} mega_sglist;

/* Queued command data */
typedef struct _mega_scb mega_scb;

struct _mega_scb {
	int idx;
	u32 state;
	u32 isrcount;
	u8 mboxData[16];
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	u32 dma_type;
	dma_addr_t dma_h_bulkdata;	/*Dma handle for bulk data transfter */
	u32 dma_direction;	/*Dma direction */
	dma_addr_t dma_h_sgdata;	/*Dma handle for the sglist structure */
	dma_addr_t dma_h_sglist[MAX_SGLIST];	/*Dma handle for all SGL elements */
	u8 sglist_count;
	dma_addr_t dma_sghandle64;
	dma_addr_t dma_passthruhandle64;
	dma_addr_t dma_ext_passthruhandle64;
	dma_addr_t dma_bounce_buffer;
	u8 *bounce_buffer;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	mega_passthru *pthru;
	mega_ext_passthru *epthru;
#else
	mega_passthru pthru;
	mega_ext_passthru epthru;
#endif

	Scsi_Cmnd *SCpnt;
	mega_sglist *sgList;
	mega_64sglist *sg64List;
	struct semaphore ioctl_sem;
	void *buff_ptr;
	u32 iDataSize;
	mega_scb *next;
};

/* internal locking by the queue manipulting routines */
#define INTERNAL_LOCK   0
/* external locking by the queue manipulting routines */
#define EXTERNAL_LOCK   1
#define NO_LOCK		2
#define INTR_ENB	0	/* do not disable interrupt while manipulating */
#define INTR_DIS	1	/* disable interrupt while manipulating */

#define NVIRT_CHAN		4	/* # of virtual channels to represent 60 logical
							drives */

/* Per-controller data */
typedef struct _mega_host_config {
	u8 numldrv;
	u32 flag;

#ifdef __LP64__
	u64 base;
#else
	u32 base;
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
	dma_addr_t dma_handle64, adjdmahandle64;
	struct pci_dev *dev;
#endif

	mega_scb *qFreeH;
	mega_scb *qFreeT;
	spinlock_t lock_free;

	mega_scb *qPendingH;
	mega_scb *qPendingT;
	spinlock_t lock_pend;

	Scsi_Cmnd *qCompletedH;
	Scsi_Cmnd *qCompletedT;
	spinlock_t lock_scsicmd;

	u32 qFcnt;
	u32 qPcnt;
	u32 qCcnt;

	unsigned long nReads[FC_MAX_LOGICAL_DRIVES];
	unsigned long nReadBlocks[FC_MAX_LOGICAL_DRIVES];
	unsigned long nWrites[FC_MAX_LOGICAL_DRIVES];
	unsigned long nWriteBlocks[FC_MAX_LOGICAL_DRIVES];
	unsigned long nInterrupts;
	/* Host adapter parameters */
	u8 fwVer[7];
	u8 biosVer[7];

	struct Scsi_Host *host;

	volatile mega_mailbox64 *mbox64;	/* ptr to beginning of 64-bit mailbox */
	volatile mega_mailbox *mbox;	/* ptr to beginning of standard mailbox */

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,4,0)
/* ptr to beginning of standard mailbox */
	volatile mega_mailbox64 *mailbox64ptr;
#else
	volatile mega_mailbox64 mailbox64;
#endif

	volatile u8 mega_buffer[2 * 1024L];
	volatile megaRaidProductInfo productInfo;

	u8 max_cmds;
	mega_scb scbList[MAX_COMMANDS];

#define PROCBUFSIZE 4096
	char procbuf[PROCBUFSIZE];
	int procidx;
	struct proc_dir_entry *controller_proc_dir_entry;
	struct proc_dir_entry *proc_read, *proc_stat, *proc_status, *proc_mbox;
	int		support_ext_cdb;

	u8		boot_ldrv_enabled;	/* boot from logical drive */
	u8		boot_ldrv;			/* boot logical drive */
	u8		boot_pdrv_enabled;	/* boot from physical drive */
	u8		boot_pdrv_ch;		/* boot physical drive channel */
	u8		boot_pdrv_tgt;		/* boot physical drive target */

	int		support_random_del;	/* Do we support random deletion of logdrvs */
	int		read_ldidmap;	/* set after logical drive deltion. The logical
								drive number must be read from the map */
	int		quiescent;	/* a stage reached when delete logical drive needs to
						   be done. Stop sending requests to the hba till
						   delete operation is completed */

	mega_scb	*int_qh;	/* commands are queued in the internal queue */
	mega_scb	*int_qt;	/* while the hba is quiescent */
	int			int_qlen;
	char		logdrv_chan[MAX_CHANNEL+NVIRT_CHAN]; /* logical drive are on
														 what channels. */
	int			mega_ch_class;
} mega_host_config;

typedef struct _driver_info {
	int size;
	ulong version;
} mega_driver_info;

/*
 * User ioctl structure.
 * This structure will be used for Traditional Method ioctl interface
 * commands (M_RD_IOCTL_CMD),Alternate Buffer Method (M_RD_IOCTL_CMD_NEW) 
 * ioctl commands and the Driver ioctls(M_RD_DRIVER_IOCTL_INTERFACE).
 * The Driver ioctl interface handles the commands at
 * the driver level, without being sent to the card.
 */
#define MEGADEVIOC      0x84

/* system call imposed limit. Change accordingly */
#define IOCTL_MAX_DATALEN       4096

#pragma pack(1)
struct uioctl_t {
	u32 inlen;
	u32 outlen;
	union {
		u8 fca[16];
		struct {
			u8 opcode;
			u8 subopcode;
			u16 adapno;
#if BITS_PER_LONG == 32
			u8 *buffer;
			u8 pad[4];
#endif
#if BITS_PER_LONG == 64
			u8 *buffer;
#endif
			u32 length;
		} fcs;
	} ui;
	u8 mbox[18];		/* 16 bytes + 2 status bytes */
	mega_passthru pthru;
#if BITS_PER_LONG == 32
	char *data;		/* buffer <= 4096 for 0x80 commands */
	char pad[4];
#endif
#if BITS_PER_LONG == 64
	char *data;
#endif
};
#pragma pack()

/*
 * struct mcontroller is used to pass information about the controllers in the
 * system. Its upto the application how to use the information. We are passing
 * as much info about the cards as possible and useful. Before issuing the
 * call to find information about the cards, the applicaiton needs to issue a
 * ioctl first to find out the number of controllers in the system.
 */
#define MAX_CONTROLLERS 32

struct mcontroller {
	u64 base;
	u8 irq;
	u8 numldrv;
	u8 pcibus;
	u16 pcidev;
	u8 pcifun;
	u16 pciid;
	u16 pcivendor;
	u8 pcislot;
	u32 uid;
};

struct mbox_passthru {
	u8 cmd;
	u8 cmdid;
	u16 pad1;
	u32 pad2;
	u32 dataxferaddr;
	u8 pad3;
	u8 pad4;
	u8 rsvd;
	u8 mboxbusy;
	u8 nstatus;
	u8 status;
};

/*
 * Defines for Driver IOCTL interface, Op-code:M_RD_DRIVER_IOCTL_INTERFACE
 */
#define MEGAIOC_MAGIC  	'm'

/* Mega IOCTL command */
#define MEGAIOCCMD     	_IOWR(MEGAIOC_MAGIC, 0, struct uioctl_t)

#define MEGAIOC_QNADAP		'm'	/* Query # of adapters */
#define MEGAIOC_QDRVRVER	'e'	/* Query driver version */
#define MEGAIOC_QADAPINFO   	'g'	/* Query adapter information */
#define MKADAP(adapno)	  	(MEGAIOC_MAGIC << 8 | (adapno) )
#define GETADAP(mkadap)	 	( (mkadap) ^ MEGAIOC_MAGIC << 8 )

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)	/*0x20300 */
extern struct proc_dir_entry proc_scsi_megaraid;
#endif

/* For Host Re-Ordering */
#define MAX_CONTROLLERS	32

struct mega_hbas {
	int is_bios_enabled;
	mega_host_config *hostdata_addr;
};

#define		IS_BIOS_ENABLED		0x62
#define		GET_BIOS		0x01
#define		CHNL_CLASS		0xA9
#define		GET_CHNL_CLASS	0x00
#define		SET_CHNL_CLASS	0x01
#define		CH_RAID			0x01
#define		CH_SCSI			0x00


#define BIOS_PVT_DATA		0x40
#define GET_BIOS_PVT_DATA	0x00

#pragma pack(1)
struct private_bios_data {
	u8		geometry:4;		/*
							 * bits 0-3 - BIOS geometry
							 * 0x0001 - 1GB
							 * 0x0010 - 2GB
							 * 0x1000 - 8GB
							 * Others values are invalid
							 */
	u8		unused:4;		/* bits 4-7 are unused */
	u8		boot_drv;		/*
							 * logical/physical drive set as boot drive
							 * 0..7 - for 8LD cards
							 * 0..39 - for 40LD cards
							 */
	u8		rsvd[12];
	u16		cksum;			/* 0-(sum of first 13 bytes of this structure) */
};
#pragma pack()

/*
 * Command for random deletion of logical drives
 */
#define	FC_DEL_LOGDRV		0xA4	/* f/w command */
#define	OP_SUP_DEL_LOGDRV	0x2A	/* is feature supported */
#define OP_GET_LDID_MAP		0x18	/* get logdrv id and logdrv number map */
#define OP_DEL_LOGDRV		0x1C	/* delete logical drive */

/*================================================================
 *
 *                    Function prototypes
 *
 *================================================================
 */
const char *megaraid_info (struct Scsi_Host *);
int megaraid_detect (Scsi_Host_Template *);
int megaraid_release (struct Scsi_Host *);
int megaraid_command (Scsi_Cmnd *);
int megaraid_abort (Scsi_Cmnd *);
int megaraid_reset (Scsi_Cmnd *, unsigned int);
int megaraid_queue (Scsi_Cmnd *, void (*done) (Scsi_Cmnd *));
int megaraid_biosparam (Disk *, kdev_t, int *);
int megaraid_proc_info (char *buffer, char **start, off_t offset,
			int length, int hostno, int inout);

static int megaIssueCmd (mega_host_config * megaCfg, u_char * mboxData,
			 mega_scb * scb, int intr);
static int mega_build_sglist (mega_host_config * megaCfg, mega_scb * scb,
			      u32 * buffer, u32 * length);
static int mega_busyWaitMbox (mega_host_config *);
static int mega_runpendq (mega_host_config *);
static void mega_rundoneq (mega_host_config *);
static void mega_cmd_done (mega_host_config *, mega_scb *, int);
static inline void mega_freeSgList (mega_host_config * megaCfg);
static void mega_Convert8ldTo40ld (mega_RAIDINQ * inquiry,
				   mega_Enquiry3 * enquiry3,
				   megaRaidProductInfo * productInfo);

static int megaraid_reboot_notify (struct notifier_block *,
				   unsigned long, void *);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,0)  
static mega_scb *mega_ioctl (mega_host_config * megaCfg, Scsi_Cmnd * SCpnt);
static void mega_build_kernel_sg (char *barea, ulong xfersize, mega_scb * pScb,
			   mega_ioctl_mbox * mbox);
#endif

static int megadev_open (struct inode *, struct file *);
static int megadev_ioctl_entry (struct inode *, struct file *,
				unsigned int, unsigned long);
static int megadev_ioctl (struct inode *, struct file *,
			  unsigned int, unsigned long);
static mega_scb *megadev_doioctl (mega_host_config *, Scsi_Cmnd *);
static int megadev_close (struct inode *, struct file *);
static void megadev_ioctl_done (Scsi_Cmnd *);
static int mega_init_scb (mega_host_config *);
static void enq_scb_freelist (mega_host_config *, mega_scb *,
			      int lock, int intr);

static int mega_is_bios_enabled (mega_host_config *);
static void mega_reorder_hosts (void);
static void mega_swap_hosts (struct Scsi_Host *, struct Scsi_Host *);

static void mega_create_proc_entry (int index, struct proc_dir_entry *);
static int mega_support_ext_cdb(mega_host_config *);
static mega_passthru* mega_prepare_passthru(mega_host_config *, mega_scb *,
		Scsi_Cmnd *, int, int);
static mega_ext_passthru* mega_prepare_extpassthru(mega_host_config *,
		mega_scb *, Scsi_Cmnd *, int, int);
static void mega_enum_raid_scsi(mega_host_config *);
static int mega_partsize(Disk *, kdev_t, int *);
static void mega_get_boot_drv(mega_host_config *);
static int mega_get_ldrv_num(mega_host_config *, Scsi_Cmnd *, int);
static int mega_support_random_del(mega_host_config *);
static int mega_del_logdrv(mega_host_config *, int);
static int mega_do_del_logdrv(mega_host_config *, int);

#endif

/* vi: set ts=4: */
