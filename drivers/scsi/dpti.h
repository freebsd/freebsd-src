/***************************************************************************
                          dpti.h  -  description
                             -------------------
    begin                : Thu Sep 7 2000
    copyright            : (C) 2001 by Adaptec
    email                : deanna_bonds@adaptec.com

    See README.dpti for history, notes, license info, and credits
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef _DPT_H
#define _DPT_H

#ifndef LINUX_VERSION_CODE
#include <linux/version.h>
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,00)
#define MAX_TO_IOP_MESSAGES   (210)
#else
#define MAX_TO_IOP_MESSAGES   (255)
#endif
#define MAX_FROM_IOP_MESSAGES (255)


/*
 * SCSI interface function Prototypes
 */

static int adpt_proc_info(char *buffer, char **start, off_t offset,
		  int length, int inode, int inout);
static int adpt_detect(Scsi_Host_Template * sht);
static int adpt_queue(Scsi_Cmnd * cmd, void (*cmdcomplete) (Scsi_Cmnd *));
static int adpt_abort(Scsi_Cmnd * cmd);
static int adpt_reset(Scsi_Cmnd* cmd);
static int adpt_release(struct Scsi_Host *host);

static const char *adpt_info(struct Scsi_Host *pSHost);
static int adpt_bios_param(Disk * disk, kdev_t dev, int geom[]);

static int adpt_bus_reset(Scsi_Cmnd* cmd);
static int adpt_device_reset(Scsi_Cmnd* cmd);


/*
 * Scsi_Host_Template (see hosts.h) 
 */

#define DPT_DRIVER_NAME	"Adaptec I2O RAID"

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,3,00)
#define DPT_I2O { \
	proc_info: adpt_proc_info,					\
	detect: adpt_detect,						\
	release: adpt_release,						\
	info: adpt_info,						\
	queuecommand: adpt_queue,					\
	eh_abort_handler: adpt_abort,					\
	eh_device_reset_handler: adpt_device_reset,			\
	eh_bus_reset_handler: adpt_bus_reset,				\
	eh_host_reset_handler: adpt_reset,				\
	bios_param: adpt_bios_param,					\
	can_queue: MAX_TO_IOP_MESSAGES ,/* max simultaneous cmds      */\
	this_id: 7,			/* scsi id of host adapter    */\
	sg_tablesize: 0,		/* max scatter-gather cmds    */\
	cmd_per_lun: 256,		/* cmds per lun (linked cmds) */\
	use_clustering: ENABLE_CLUSTERING,				\
	use_new_eh_code: 1						\
}

#else				/* KERNEL_VERSION > 2.2.16 */

#define DPT_I2O { \
	proc_info: adpt_proc_info,					\
	detect: adpt_detect,						\
	release: adpt_release,						\
	info: adpt_info,						\
	queuecommand: adpt_queue,					\
	eh_abort_handler: adpt_abort,					\
	eh_device_reset_handler: adpt_device_reset,			\
	eh_bus_reset_handler: adpt_bus_reset,				\
	eh_host_reset_handler: adpt_reset,				\
	bios_param: adpt_bios_param,					\
	can_queue: MAX_TO_IOP_MESSAGES,	/* max simultaneous cmds      */\
	this_id: 7,			/* scsi id of host adapter    */\
	sg_tablesize: 0,		/* max scatter-gather cmds    */\
	cmd_per_lun: 256,		/* cmds per lun (linked cmds) */\
	use_clustering: ENABLE_CLUSTERING,				\
	use_new_eh_code: 1,						\
	proc_name: "dpt_i2o"	/* this is the name of our proc node*/	\
}
#endif

#ifndef HOSTS_C

#include "dpt/sys_info.h"
#include <linux/wait.h>
#include "dpt/dpti_i2o.h"
#include "dpt/dpti_ioctl.h"

#define DPT_I2O_VERSION "2.4 Build 5"
#define DPT_VERSION     2
#define DPT_REVISION    '4'
#define DPT_SUBREVISION '5'
#define DPT_BETA	""
#define DPT_MONTH      8 
#define DPT_DAY        7
#define DPT_YEAR        (2001-1980)

#define DPT_DRIVER	"dpt_i2o"
#define DPTI_I2O_MAJOR	(151)
#define DPT_ORGANIZATION_ID (0x1B)        /* For Private Messages */
#define DPTI_MAX_HBA	(16)
#define MAX_CHANNEL     (5)	// Maximum Channel # Supported
#define MAX_ID        	(128)	// Maximum Target ID Supported

/* Sizes in 4 byte words */
#define REPLY_FRAME_SIZE  (17)
#define MAX_MESSAGE_SIZE  (128)
#define SG_LIST_ELEMENTS  (56)

#define EMPTY_QUEUE           0xffffffff
#define I2O_INTERRUPT_PENDING_B   (0x08)

#define PCI_DPT_VENDOR_ID         (0x1044)	// DPT PCI Vendor ID
#define PCI_DPT_DEVICE_ID         (0xA501)	// DPT PCI I2O Device ID
#define PCI_DPT_RAPTOR_DEVICE_ID  (0xA511)	

//#define REBOOT_NOTIFIER 1
/* Debugging macro from Linux Device Drivers - Rubini */
#undef PDEBUG
#ifdef DEBUG
//TODO add debug level switch
#  define PDEBUG(fmt, args...)  printk(KERN_DEBUG "dpti: " fmt, ##args)
#  define PDEBUGV(fmt, args...) printk(KERN_DEBUG "dpti: " fmt, ##args)
#else
# define PDEBUG(fmt, args...) /* not debugging: nothing */
# define PDEBUGV(fmt, args...) /* not debugging: nothing */
#endif

#define PERROR(fmt, args...) printk(KERN_ERR fmt, ##args)
#define PWARN(fmt, args...) printk(KERN_WARNING fmt, ##args)
#define PINFO(fmt, args...) printk(KERN_INFO fmt, ##args)
#define PCRIT(fmt, args...) printk(KERN_CRIT fmt, ##args)

#define SHUTDOWN_SIGS	(sigmask(SIGKILL)|sigmask(SIGINT)|sigmask(SIGTERM))

// Command timeouts
#define FOREVER			(0)
#define TMOUT_INQUIRY 		(20)
#define TMOUT_FLUSH		(360/45)
#define TMOUT_ABORT		(30)
#define TMOUT_SCSI		(300)
#define TMOUT_IOPRESET		(360)
#define TMOUT_GETSTATUS		(15)
#define TMOUT_INITOUTBOUND	(15)
#define TMOUT_LCT		(360)


#define I2O_SCSI_DEVICE_DSC_MASK                0x00FF

#define I2O_DETAIL_STATUS_UNSUPPORTED_FUNCTION  0x000A

#define I2O_SCSI_DSC_MASK                   0xFF00
#define I2O_SCSI_DSC_SUCCESS                0x0000
#define I2O_SCSI_DSC_REQUEST_ABORTED        0x0200
#define I2O_SCSI_DSC_UNABLE_TO_ABORT        0x0300
#define I2O_SCSI_DSC_COMPLETE_WITH_ERROR    0x0400
#define I2O_SCSI_DSC_ADAPTER_BUSY           0x0500
#define I2O_SCSI_DSC_REQUEST_INVALID        0x0600
#define I2O_SCSI_DSC_PATH_INVALID           0x0700
#define I2O_SCSI_DSC_DEVICE_NOT_PRESENT     0x0800
#define I2O_SCSI_DSC_UNABLE_TO_TERMINATE    0x0900
#define I2O_SCSI_DSC_SELECTION_TIMEOUT      0x0A00
#define I2O_SCSI_DSC_COMMAND_TIMEOUT        0x0B00
#define I2O_SCSI_DSC_MR_MESSAGE_RECEIVED    0x0D00
#define I2O_SCSI_DSC_SCSI_BUS_RESET         0x0E00
#define I2O_SCSI_DSC_PARITY_ERROR_FAILURE   0x0F00
#define I2O_SCSI_DSC_AUTOSENSE_FAILED       0x1000
#define I2O_SCSI_DSC_NO_ADAPTER             0x1100
#define I2O_SCSI_DSC_DATA_OVERRUN           0x1200
#define I2O_SCSI_DSC_UNEXPECTED_BUS_FREE    0x1300
#define I2O_SCSI_DSC_SEQUENCE_FAILURE       0x1400
#define I2O_SCSI_DSC_REQUEST_LENGTH_ERROR   0x1500
#define I2O_SCSI_DSC_PROVIDE_FAILURE        0x1600
#define I2O_SCSI_DSC_BDR_MESSAGE_SENT       0x1700
#define I2O_SCSI_DSC_REQUEST_TERMINATED     0x1800
#define I2O_SCSI_DSC_IDE_MESSAGE_SENT       0x3300
#define I2O_SCSI_DSC_RESOURCE_UNAVAILABLE   0x3400
#define I2O_SCSI_DSC_UNACKNOWLEDGED_EVENT   0x3500
#define I2O_SCSI_DSC_MESSAGE_RECEIVED       0x3600
#define I2O_SCSI_DSC_INVALID_CDB            0x3700
#define I2O_SCSI_DSC_LUN_INVALID            0x3800
#define I2O_SCSI_DSC_SCSI_TID_INVALID       0x3900
#define I2O_SCSI_DSC_FUNCTION_UNAVAILABLE   0x3A00
#define I2O_SCSI_DSC_NO_NEXUS               0x3B00
#define I2O_SCSI_DSC_SCSI_IID_INVALID       0x3C00
#define I2O_SCSI_DSC_CDB_RECEIVED           0x3D00
#define I2O_SCSI_DSC_LUN_ALREADY_ENABLED    0x3E00
#define I2O_SCSI_DSC_BUS_BUSY               0x3F00
#define I2O_SCSI_DSC_QUEUE_FROZEN           0x4000


#ifndef TRUE
#define TRUE                  1
#define FALSE                 0
#endif

#define HBA_FLAGS_INSTALLED_B       0x00000001	// Adapter Was Installed
#define HBA_FLAGS_BLINKLED_B        0x00000002	// Adapter In Blink LED State
#define HBA_FLAGS_IN_RESET	0x00000040	/* in reset */
#define HBA_HOSTRESET_FAILED	0x00000080	/* adpt_resethost failed */


// Device state flags
#define DPTI_DEV_ONLINE    0x00
#define DPTI_DEV_UNSCANNED 0x01
#define DPTI_DEV_RESET	   0x02
#define DPTI_DEV_OFFLINE   0x04


struct adpt_device {
	struct adpt_device* next_lun;
	u32	flags;
	u32	type;
	u32	capacity;
	u32	block_size;
	u8	scsi_channel;
	u8	scsi_id;
	u8 	scsi_lun;
	u8	state;
	u16	tid;
	struct i2o_device* pI2o_dev;
	Scsi_Device *pScsi_dev;
};

struct adpt_channel {
	struct adpt_device* device[MAX_ID];	/* used as an array of 128 scsi ids */
	u8	scsi_id;
	u8	type;
	u16	tid;
	u32	state;
	struct i2o_device* pI2o_dev;
};

// HBA state flags
#define DPTI_STATE_RESET	(0x01)
#define DPTI_STATE_IOCTL	(0x02)

typedef struct _adpt_hba {
	struct _adpt_hba *next;
	struct pci_dev *pDev;
	struct Scsi_Host *host;
	u32 state;
	spinlock_t state_lock;
	int unit;
	int host_no;		/* SCSI host number */
	u8 initialized;
	u8 in_use;		/* is the management node open*/

	char name[32];
	char detail[55];

	ulong base_addr_virt;
	ulong msg_addr_virt;
	ulong base_addr_phys;
	ulong  post_port;
	ulong  reply_port;
	ulong  irq_mask;
	u16  post_count;
	u32  post_fifo_size;
	u32  reply_fifo_size;
	u32* reply_pool;
	u32  sg_tablesize;	// Scatter/Gather List Size.       
	u8  top_scsi_channel;
	u8  top_scsi_id;
	u8  top_scsi_lun;

	i2o_status_block* status_block;
	i2o_hrt* hrt;
	i2o_lct* lct;
	uint lct_size;
	struct i2o_device* devices;
	struct adpt_channel channel[MAX_CHANNEL];
	struct proc_dir_entry* proc_entry;	/* /proc dir */

	ulong FwDebugBuffer_P;	// Virtual Address Of FW Debug Buffer
	u32   FwDebugBufferSize;	// FW Debug Buffer Size In Bytes
	ulong FwDebugStrLength_P;	// Virtual Addr Of FW Debug String Len
	ulong FwDebugFlags_P;	// Virtual Address Of FW Debug Flags 
	ulong FwDebugBLEDflag_P;	// Virtual Addr Of FW Debug BLED
	ulong FwDebugBLEDvalue_P;	// Virtual Addr Of FW Debug BLED
	u32 FwDebugFlags;
} adpt_hba;

struct sg_simple_element {
   u32  flag_count;
   u32 addr_bus;
}; 

/*
 * Function Prototypes
 */

static void adpt_i2o_sys_shutdown(void);
static int adpt_init(void);
static int adpt_i2o_build_sys_table(void);
static void adpt_isr(int irq, void *dev_id, struct pt_regs *regs);
#ifdef REBOOT_NOTIFIER
static int adpt_reboot_event(struct notifier_block *n, ulong code, void *p);
#endif

static void adpt_i2o_report_hba_unit(adpt_hba* pHba, struct i2o_device *d);
static int adpt_i2o_query_scalar(adpt_hba* pHba, int tid, 
			int group, int field, void *buf, int buflen);
#ifdef DEBUG
static const char *adpt_i2o_get_class_name(int class);
#endif
static int adpt_i2o_issue_params(int cmd, adpt_hba* pHba, int tid, 
		  void *opblk, int oplen, void *resblk, int reslen);
static int adpt_i2o_post_wait(adpt_hba* pHba, u32* msg, int len, int timeout);
static int adpt_i2o_lct_get(adpt_hba* pHba);
static int adpt_i2o_parse_lct(adpt_hba* pHba);
static int adpt_i2o_activate_hba(adpt_hba* pHba);
static int adpt_i2o_enable_hba(adpt_hba* pHba);
static int adpt_i2o_install_device(adpt_hba* pHba, struct i2o_device *d);
static s32 adpt_i2o_post_this(adpt_hba* pHba, u32* data, int len);
static s32 adpt_i2o_quiesce_hba(adpt_hba* pHba);
static s32 adpt_i2o_status_get(adpt_hba* pHba);
static s32 adpt_i2o_init_outbound_q(adpt_hba* pHba);
static s32 adpt_i2o_hrt_get(adpt_hba* pHba);
static s32 adpt_scsi_to_i2o(adpt_hba* pHba, Scsi_Cmnd* cmd, struct adpt_device* dptdevice);
static s32 adpt_i2o_to_scsi(ulong reply, Scsi_Cmnd* cmd);
static s32 adpt_scsi_register(adpt_hba* pHba,Scsi_Host_Template * sht);
static s32 adpt_hba_reset(adpt_hba* pHba);
static s32 adpt_i2o_reset_hba(adpt_hba* pHba);
static s32 adpt_rescan(adpt_hba* pHba);
static s32 adpt_i2o_reparse_lct(adpt_hba* pHba);
static s32 adpt_send_nop(adpt_hba*pHba,u32 m);
static void adpt_i2o_delete_hba(adpt_hba* pHba);
static void adpt_select_queue_depths(struct Scsi_Host *host, Scsi_Device * devicelist);
static void adpt_inquiry(adpt_hba* pHba);
static void adpt_fail_posted_scbs(adpt_hba* pHba);
static struct adpt_device* adpt_find_device(adpt_hba* pHba, u32 chan, u32 id, u32 lun);
static int adpt_install_hba(Scsi_Host_Template* sht, struct pci_dev* pDev) ;
static int adpt_i2o_online_hba(adpt_hba* pHba);
static void adpt_i2o_post_wait_complete(u32, int);
static int adpt_i2o_systab_send(adpt_hba* pHba);

static int adpt_ioctl(struct inode *inode, struct file *file, uint cmd, ulong arg);
static int adpt_open(struct inode *inode, struct file *file);
static int adpt_close(struct inode *inode, struct file *file);


#ifdef UARTDELAY
static void adpt_delay(int millisec);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,4,0)
static struct pci_dev* adpt_pci_find_device(uint vendor, struct pci_dev* from);
#endif

#if defined __ia64__ 
static void adpt_ia64_info(sysInfo_S* si);
#endif
#if defined __sparc__ 
static void adpt_sparc_info(sysInfo_S* si);
#endif
#if defined __alpha__ 
static void adpt_sparc_info(sysInfo_S* si);
#endif
#if defined __i386__
static void adpt_i386_info(sysInfo_S* si);
#endif

#define PRINT_BUFFER_SIZE     512

#define HBA_FLAGS_DBG_FLAGS_MASK         0xffff0000	// Mask for debug flags
#define HBA_FLAGS_DBG_KERNEL_PRINT_B     0x00010000	// Kernel Debugger Print
#define HBA_FLAGS_DBG_FW_PRINT_B         0x00020000	// Firmware Debugger Print
#define HBA_FLAGS_DBG_FUNCTION_ENTRY_B   0x00040000	// Function Entry Point
#define HBA_FLAGS_DBG_FUNCTION_EXIT_B    0x00080000	// Function Exit
#define HBA_FLAGS_DBG_ERROR_B            0x00100000	// Error Conditions
#define HBA_FLAGS_DBG_INIT_B             0x00200000	// Init Prints
#define HBA_FLAGS_DBG_OS_COMMANDS_B      0x00400000	// OS Command Info
#define HBA_FLAGS_DBG_SCAN_B             0x00800000	// Device Scan

#define FW_DEBUG_STR_LENGTH_OFFSET 0
#define FW_DEBUG_FLAGS_OFFSET      4
#define FW_DEBUG_BLED_OFFSET       8

#define FW_DEBUG_FLAGS_NO_HEADERS_B    0x01
#endif				/* !HOSTS_C */
#endif				/* _DPT_H */
