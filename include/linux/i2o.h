/*
 * I2O kernel space accessible structures/APIs
 * 
 * (c) Copyright 1999, 2000 Red Hat Software
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License 
 * as published by the Free Software Foundation; either version 
 * 2 of the License, or (at your option) any later version.  
 * 
 *************************************************************************
 *
 * This header file defined the I2O APIs/structures for use by 
 * the I2O kernel modules.
 *
 */

#ifndef _I2O_H
#define _I2O_H

#ifdef __KERNEL__ /* This file to be included by kernel only */

#include <linux/i2o-dev.h>

/* How many different OSM's are we allowing */
#define MAX_I2O_MODULES		64

/* How many OSMs can register themselves for device status updates? */
#define I2O_MAX_MANAGERS	4

#include <asm/semaphore.h>	/* Needed for MUTEX init macros */
#include <linux/config.h>
#include <linux/notifier.h>
#include <asm/atomic.h>

/*
 *	Message structures
 */
struct i2o_message
{
	u8	version_offset;
	u8	flags;
	u16	size;
	u32	target_tid:12;
	u32	init_tid:12;
	u32	function:8;
	u32	initiator_context;
	/* List follows */
};

/*
 *	Each I2O device entity has one or more of these. There is one
 *	per device.
 */
struct i2o_device
{
	i2o_lct_entry lct_data;		/* Device LCT information */
	u32 flags;
	int i2oversion;			/* I2O version supported. Actually
					 * there should be high and low
					 * version */

	struct proc_dir_entry *proc_entry;	/* /proc dir */

	/* Primary user */
	struct i2o_handler *owner;

	/* Management users */
	struct i2o_handler *managers[I2O_MAX_MANAGERS];
	int num_managers;

	struct i2o_controller *controller;	/* Controlling IOP */
	struct i2o_device *next;	/* Chain */
	struct i2o_device *prev;
	char dev_name[8];		/* linux /dev name if available */
};

/*
 *	Resource data for each PCI I2O controller
 */
struct i2o_pci
{
	int		irq;
	int		short_req:1;	/* Use small block sizes        */
	int		dpt:1;		/* Don't quiesce                */
	int		promise:1;	/* Promise controller		*/
#ifdef CONFIG_MTRR
	int		mtrr_reg0;
	int		mtrr_reg1;
#endif
};

/*
 * Transport types supported by I2O stack
 */
#define I2O_TYPE_PCI		0x01		/* PCI I2O controller */


/*
 * Each I2O controller has one of these objects
 */
struct i2o_controller
{
	struct pci_dev *pdev;		/* PCI device */

	char name[16];
	int unit;
	int type;
	int enabled;

	struct notifier_block *event_notifer;	/* Events */
	atomic_t users;
	struct i2o_device *devices;		/* I2O device chain */
	struct i2o_controller *next;		/* Controller chain */
	unsigned long post_port;		/* Inbout port address */
	unsigned long reply_port;		/* Outbound port address */
	unsigned long irq_mask;			/* Interrupt register address */

	/* Dynamic LCT related data */
	struct semaphore lct_sem;
	int lct_pid;
	int lct_running;

	i2o_status_block *status_block;		/* IOP status block */
	i2o_lct *lct;				/* Logical Config Table */
	i2o_lct *dlct;				/* Temp LCT */
	i2o_hrt *hrt;				/* HW Resource Table */

	unsigned long mem_offset;		/* MFA offset */
	unsigned long mem_phys;			/* MFA physical */

	int battery:1;				/* Has a battery backup */
	int io_alloc:1;				/* An I/O resource was allocated */
	int mem_alloc:1;			/* A memory resource was allocated */

	struct resource io_resource;		/* I/O resource allocated to the IOP */
	struct resource mem_resource;		/* Mem resource allocated to the IOP */

	struct proc_dir_entry *proc_entry;	/* /proc dir */

	union {					/* Bus information */
		struct i2o_pci pci;
	} bus;

	/* Bus specific destructor */
	void (*destructor)(struct i2o_controller *);

	/* Bus specific attach/detach */
	int (*bind)(struct i2o_controller *, struct i2o_device *);

	/* Bus specific initiator */
	int (*unbind)(struct i2o_controller *, struct i2o_device *);

	/* Bus specific enable/disable */
	void (*bus_enable)(struct i2o_controller *);
	void (*bus_disable)(struct i2o_controller *);

	void *page_frame;			/* Message buffers */
	dma_addr_t page_frame_map;		/* Cache map */
};

/*
 * OSM resgistration block
 *
 * Each OSM creates at least one of these and registers it with the
 * I2O core through i2o_register_handler.  An OSM may want to
 * register more than one if it wants a fast path to a reply
 * handler by having a separate initiator context for each 
 * class function.
 */
struct i2o_handler
{
	/* Message reply handler */
	void (*reply)(struct i2o_handler *, struct i2o_controller *,
		      struct i2o_message *);

	/* New device notification handler */
	void (*new_dev_notify)(struct i2o_controller *, struct i2o_device *);

	/* Device deltion handler */
	void (*dev_del_notify)(struct i2o_controller *, struct i2o_device *);

	/* Reboot notification handler */
	void (*reboot_notify)(void);

	char *name;		/* OSM name */
	int context;		/* Low 8 bits of the transaction info */
	u32 class;		/* I2O classes that this driver handles */
	/* User data follows */
};

#ifdef MODULE
/*
 * Used by bus specific modules to communicate with the core
 *
 * This is needed because the bus modules cannot make direct
 * calls to the core as this results in the i2o_bus_specific_module
 * being dependent on the core, not the otherway around.
 * In that case, a 'modprobe i2o_lan' loads i2o_core & i2o_lan,
 * but _not_ i2o_pci...which makes the whole thing pretty useless :)
 *
 */
struct i2o_core_func_table
{
	int	(*install)(struct i2o_controller *);
	int	(*activate)(struct i2o_controller *);
	struct i2o_controller *(*find)(int);
	void	(*unlock)(struct i2o_controller *);
	void	(*run_queue)(struct i2o_controller * c);
	int	(*delete)(struct i2o_controller *);
};
#endif /* MODULE */

/*
 * I2O System table entry
 *
 * The system table contains information about all the IOPs in the
 * system.  It is sent to all IOPs so that they can create peer2peer
 * connections between them.
 */
struct i2o_sys_tbl_entry
{
	u16	org_id;
	u16	reserved1;
	u32	iop_id:12;
	u32	reserved2:20;
	u16	seg_num:12;
	u16	i2o_version:4;
	u8	iop_state;
	u8	msg_type;
	u16	frame_size;
	u16	reserved3;
	u32	last_changed;
	u32	iop_capabilities;
	u32	inbound_low;
	u32	inbound_high;
};

struct i2o_sys_tbl
{
	u8	num_entries;
	u8	version;
	u16	reserved1;
	u32	change_ind;
	u32	reserved2;
	u32	reserved3;
	struct i2o_sys_tbl_entry iops[0];
};

/*
 *	Messenger inlines
 */
static inline u32 I2O_POST_READ32(struct i2o_controller *c)
{
	return readl(c->post_port);
}

static inline void I2O_POST_WRITE32(struct i2o_controller *c, u32 val)
{
	writel(val, c->post_port);
}


static inline u32 I2O_REPLY_READ32(struct i2o_controller *c)
{
	return readl(c->reply_port);
}

static inline void I2O_REPLY_WRITE32(struct i2o_controller *c, u32 val)
{
	writel(val, c->reply_port);
}


static inline u32 I2O_IRQ_READ32(struct i2o_controller *c)
{
	return readl(c->irq_mask);
}

static inline void I2O_IRQ_WRITE32(struct i2o_controller *c, u32 val)
{
	writel(val, c->irq_mask);
}


static inline void i2o_post_message(struct i2o_controller *c, u32 m)
{
	/* The second line isnt spurious - thats forcing PCI posting */
	I2O_POST_WRITE32(c, m);
	(void) I2O_IRQ_READ32(c);
}

static inline void i2o_flush_reply(struct i2o_controller *c, u32 m)
{
	I2O_REPLY_WRITE32(c, m);
}

/*
 *	Endian handling wrapped into the macro - keeps the core code
 *	cleaner.
 */
 
#define i2o_raw_writel(val, mem)	__raw_writel(cpu_to_le32(val), mem)

extern struct i2o_controller *i2o_find_controller(int);
extern void i2o_unlock_controller(struct i2o_controller *);
extern struct i2o_controller *i2o_controller_chain;
extern int i2o_num_controllers;
extern int i2o_status_get(struct i2o_controller *);

extern int i2o_install_handler(struct i2o_handler *);
extern int i2o_remove_handler(struct i2o_handler *);

extern int i2o_claim_device(struct i2o_device *, struct i2o_handler *);
extern int i2o_release_device(struct i2o_device *, struct i2o_handler *);
extern int i2o_device_notify_on(struct i2o_device *, struct i2o_handler *);
extern int i2o_device_notify_off(struct i2o_device *,
				 struct i2o_handler *);

extern int i2o_post_this(struct i2o_controller *, u32 *, int);
extern int i2o_post_wait(struct i2o_controller *, u32 *, int, int);
extern int i2o_post_wait_mem(struct i2o_controller *, u32 *, int, int,
			     void *, void *);

extern int i2o_query_scalar(struct i2o_controller *, int, int, int, void *,
			    int);
extern int i2o_set_scalar(struct i2o_controller *, int, int, int, void *,
			  int);
extern int i2o_query_table(int, struct i2o_controller *, int, int, int,
			   void *, int, void *, int);
extern int i2o_clear_table(struct i2o_controller *, int, int);
extern int i2o_row_add_table(struct i2o_controller *, int, int, int,
			     void *, int);
extern int i2o_issue_params(int, struct i2o_controller *, int, void *, int,
			    void *, int);

extern int i2o_event_register(struct i2o_controller *, u32, u32, u32, u32);
extern int i2o_event_ack(struct i2o_controller *, u32 *);

extern void i2o_report_status(const char *, const char *, u32 *);
extern void i2o_dump_message(u32 *);
extern const char *i2o_get_class_name(int);

extern int i2o_install_controller(struct i2o_controller *);
extern int i2o_activate_controller(struct i2o_controller *);
extern void i2o_run_queue(struct i2o_controller *);
extern int i2o_delete_controller(struct i2o_controller *);

/*
 *	Cache strategies
 */
 
 
/*	The NULL strategy leaves everything up to the controller. This tends to be a
 *	pessimal but functional choice.
 */
#define CACHE_NULL		0
/*	Prefetch data when reading. We continually attempt to load the next 32 sectors
 *	into the controller cache. 
 */
#define CACHE_PREFETCH		1
/*	Prefetch data when reading. We sometimes attempt to load the next 32 sectors
 *	into the controller cache. When an I/O is less <= 8K we assume its probably
 *	not sequential and don't prefetch (default)
 */
#define CACHE_SMARTFETCH	2
/*	Data is written to the cache and then out on to the disk. The I/O must be
 *	physically on the medium before the write is acknowledged (default without
 *	NVRAM)
 */
#define CACHE_WRITETHROUGH	17
/*	Data is written to the cache and then out on to the disk. The controller
 *	is permitted to write back the cache any way it wants. (default if battery
 *	backed NVRAM is present). It can be useful to set this for swap regardless of
 *	battery state.
 */
#define CACHE_WRITEBACK		18
/*	Optimise for under powered controllers, especially on RAID1 and RAID0. We
 *	write large I/O's directly to disk bypassing the cache to avoid the extra
 *	memory copy hits. Small writes are writeback cached
 */
#define CACHE_SMARTBACK		19
/*	Optimise for under powered controllers, especially on RAID1 and RAID0. We
 *	write large I/O's directly to disk bypassing the cache to avoid the extra
 *	memory copy hits. Small writes are writethrough cached. Suitable for devices
 *	lacking battery backup
 */
#define CACHE_SMARTTHROUGH	20

/*
 *	Ioctl structures
 */
 

#define 	BLKI2OGRSTRAT	_IOR('2', 1, int) 
#define 	BLKI2OGWSTRAT	_IOR('2', 2, int) 
#define 	BLKI2OSRSTRAT	_IOW('2', 3, int) 
#define 	BLKI2OSWSTRAT	_IOW('2', 4, int) 




/*
 *	I2O Function codes
 */

/*
 *	Executive Class
 */
#define	I2O_CMD_ADAPTER_ASSIGN		0xB3
#define	I2O_CMD_ADAPTER_READ		0xB2
#define	I2O_CMD_ADAPTER_RELEASE		0xB5
#define	I2O_CMD_BIOS_INFO_SET		0xA5
#define	I2O_CMD_BOOT_DEVICE_SET		0xA7
#define	I2O_CMD_CONFIG_VALIDATE		0xBB
#define	I2O_CMD_CONN_SETUP		0xCA
#define	I2O_CMD_DDM_DESTROY		0xB1
#define	I2O_CMD_DDM_ENABLE		0xD5
#define	I2O_CMD_DDM_QUIESCE		0xC7
#define	I2O_CMD_DDM_RESET		0xD9
#define	I2O_CMD_DDM_SUSPEND		0xAF
#define	I2O_CMD_DEVICE_ASSIGN		0xB7
#define	I2O_CMD_DEVICE_RELEASE		0xB9
#define	I2O_CMD_HRT_GET			0xA8
#define	I2O_CMD_ADAPTER_CLEAR		0xBE
#define	I2O_CMD_ADAPTER_CONNECT		0xC9
#define	I2O_CMD_ADAPTER_RESET		0xBD
#define	I2O_CMD_LCT_NOTIFY		0xA2
#define	I2O_CMD_OUTBOUND_INIT		0xA1
#define	I2O_CMD_PATH_ENABLE		0xD3
#define	I2O_CMD_PATH_QUIESCE		0xC5
#define	I2O_CMD_PATH_RESET		0xD7
#define	I2O_CMD_STATIC_MF_CREATE	0xDD
#define	I2O_CMD_STATIC_MF_RELEASE	0xDF
#define	I2O_CMD_STATUS_GET		0xA0
#define	I2O_CMD_SW_DOWNLOAD		0xA9
#define	I2O_CMD_SW_UPLOAD		0xAB
#define	I2O_CMD_SW_REMOVE		0xAD
#define	I2O_CMD_SYS_ENABLE		0xD1
#define	I2O_CMD_SYS_MODIFY		0xC1
#define	I2O_CMD_SYS_QUIESCE		0xC3
#define	I2O_CMD_SYS_TAB_SET		0xA3

/*
 * Utility Class
 */
#define I2O_CMD_UTIL_NOP		0x00
#define I2O_CMD_UTIL_ABORT		0x01
#define I2O_CMD_UTIL_CLAIM		0x09
#define I2O_CMD_UTIL_RELEASE		0x0B
#define I2O_CMD_UTIL_PARAMS_GET		0x06
#define I2O_CMD_UTIL_PARAMS_SET		0x05
#define I2O_CMD_UTIL_EVT_REGISTER	0x13
#define I2O_CMD_UTIL_EVT_ACK		0x14
#define I2O_CMD_UTIL_CONFIG_DIALOG	0x10
#define I2O_CMD_UTIL_DEVICE_RESERVE	0x0D
#define I2O_CMD_UTIL_DEVICE_RELEASE	0x0F
#define I2O_CMD_UTIL_LOCK		0x17
#define I2O_CMD_UTIL_LOCK_RELEASE	0x19
#define I2O_CMD_UTIL_REPLY_FAULT_NOTIFY	0x15

/*
 * SCSI Host Bus Adapter Class
 */
#define I2O_CMD_SCSI_EXEC		0x81
#define I2O_CMD_SCSI_ABORT		0x83
#define I2O_CMD_SCSI_BUSRESET		0x27

/*
 * Random Block Storage Class
 */
#define I2O_CMD_BLOCK_READ		0x30
#define I2O_CMD_BLOCK_WRITE		0x31
#define I2O_CMD_BLOCK_CFLUSH		0x37
#define I2O_CMD_BLOCK_MLOCK		0x49
#define I2O_CMD_BLOCK_MUNLOCK		0x4B
#define I2O_CMD_BLOCK_MMOUNT		0x41
#define I2O_CMD_BLOCK_MEJECT		0x43
#define I2O_CMD_BLOCK_POWER		0x70

#define I2O_PRIVATE_MSG			0xFF

/* Command status values  */

#define I2O_CMD_IN_PROGRESS	0x01
#define I2O_CMD_REJECTED	0x02
#define I2O_CMD_FAILED		0x03
#define I2O_CMD_COMPLETED	0x04

/* I2O API function return values */

#define I2O_RTN_NO_ERROR			0
#define I2O_RTN_NOT_INIT			1
#define I2O_RTN_FREE_Q_EMPTY			2
#define I2O_RTN_TCB_ERROR			3
#define I2O_RTN_TRANSACTION_ERROR		4
#define I2O_RTN_ADAPTER_ALREADY_INIT		5
#define I2O_RTN_MALLOC_ERROR			6
#define I2O_RTN_ADPTR_NOT_REGISTERED		7
#define I2O_RTN_MSG_REPLY_TIMEOUT		8
#define I2O_RTN_NO_STATUS			9
#define I2O_RTN_NO_FIRM_VER			10
#define	I2O_RTN_NO_LINK_SPEED			11

/* Reply message status defines for all messages */

#define I2O_REPLY_STATUS_SUCCESS                    	0x00
#define I2O_REPLY_STATUS_ABORT_DIRTY                	0x01
#define I2O_REPLY_STATUS_ABORT_NO_DATA_TRANSFER     	0x02
#define	I2O_REPLY_STATUS_ABORT_PARTIAL_TRANSFER		0x03
#define	I2O_REPLY_STATUS_ERROR_DIRTY			0x04
#define	I2O_REPLY_STATUS_ERROR_NO_DATA_TRANSFER		0x05
#define	I2O_REPLY_STATUS_ERROR_PARTIAL_TRANSFER		0x06
#define	I2O_REPLY_STATUS_PROCESS_ABORT_DIRTY		0x08
#define	I2O_REPLY_STATUS_PROCESS_ABORT_NO_DATA_TRANSFER	0x09
#define	I2O_REPLY_STATUS_PROCESS_ABORT_PARTIAL_TRANSFER	0x0A
#define	I2O_REPLY_STATUS_TRANSACTION_ERROR		0x0B
#define	I2O_REPLY_STATUS_PROGRESS_REPORT		0x80

/* Status codes and Error Information for Parameter functions */

#define I2O_PARAMS_STATUS_SUCCESS		0x00
#define I2O_PARAMS_STATUS_BAD_KEY_ABORT		0x01
#define I2O_PARAMS_STATUS_BAD_KEY_CONTINUE   	0x02
#define I2O_PARAMS_STATUS_BUFFER_FULL		0x03
#define I2O_PARAMS_STATUS_BUFFER_TOO_SMALL	0x04
#define I2O_PARAMS_STATUS_FIELD_UNREADABLE	0x05
#define I2O_PARAMS_STATUS_FIELD_UNWRITEABLE	0x06
#define I2O_PARAMS_STATUS_INSUFFICIENT_FIELDS	0x07
#define I2O_PARAMS_STATUS_INVALID_GROUP_ID	0x08
#define I2O_PARAMS_STATUS_INVALID_OPERATION	0x09
#define I2O_PARAMS_STATUS_NO_KEY_FIELD		0x0A
#define I2O_PARAMS_STATUS_NO_SUCH_FIELD		0x0B
#define I2O_PARAMS_STATUS_NON_DYNAMIC_GROUP	0x0C
#define I2O_PARAMS_STATUS_OPERATION_ERROR	0x0D
#define I2O_PARAMS_STATUS_SCALAR_ERROR		0x0E
#define I2O_PARAMS_STATUS_TABLE_ERROR		0x0F
#define I2O_PARAMS_STATUS_WRONG_GROUP_TYPE	0x10

/* DetailedStatusCode defines for Executive, DDM, Util and Transaction error
 * messages: Table 3-2 Detailed Status Codes.*/

#define I2O_DSC_SUCCESS                        0x0000
#define I2O_DSC_BAD_KEY                        0x0002
#define I2O_DSC_TCL_ERROR                      0x0003
#define I2O_DSC_REPLY_BUFFER_FULL              0x0004
#define I2O_DSC_NO_SUCH_PAGE                   0x0005
#define I2O_DSC_INSUFFICIENT_RESOURCE_SOFT     0x0006
#define I2O_DSC_INSUFFICIENT_RESOURCE_HARD     0x0007
#define I2O_DSC_CHAIN_BUFFER_TOO_LARGE         0x0009
#define I2O_DSC_UNSUPPORTED_FUNCTION           0x000A
#define I2O_DSC_DEVICE_LOCKED                  0x000B
#define I2O_DSC_DEVICE_RESET                   0x000C
#define I2O_DSC_INAPPROPRIATE_FUNCTION         0x000D
#define I2O_DSC_INVALID_INITIATOR_ADDRESS      0x000E
#define I2O_DSC_INVALID_MESSAGE_FLAGS          0x000F
#define I2O_DSC_INVALID_OFFSET                 0x0010
#define I2O_DSC_INVALID_PARAMETER              0x0011
#define I2O_DSC_INVALID_REQUEST                0x0012
#define I2O_DSC_INVALID_TARGET_ADDRESS         0x0013
#define I2O_DSC_MESSAGE_TOO_LARGE              0x0014
#define I2O_DSC_MESSAGE_TOO_SMALL              0x0015
#define I2O_DSC_MISSING_PARAMETER              0x0016
#define I2O_DSC_TIMEOUT                        0x0017
#define I2O_DSC_UNKNOWN_ERROR                  0x0018
#define I2O_DSC_UNKNOWN_FUNCTION               0x0019
#define I2O_DSC_UNSUPPORTED_VERSION            0x001A
#define I2O_DSC_DEVICE_BUSY                    0x001B
#define I2O_DSC_DEVICE_NOT_AVAILABLE           0x001C

/* FailureStatusCodes, Table 3-3 Message Failure Codes */

#define I2O_FSC_TRANSPORT_SERVICE_SUSPENDED             0x81
#define I2O_FSC_TRANSPORT_SERVICE_TERMINATED            0x82
#define I2O_FSC_TRANSPORT_CONGESTION                    0x83
#define I2O_FSC_TRANSPORT_FAILURE                       0x84
#define I2O_FSC_TRANSPORT_STATE_ERROR                   0x85
#define I2O_FSC_TRANSPORT_TIME_OUT                      0x86
#define I2O_FSC_TRANSPORT_ROUTING_FAILURE               0x87
#define I2O_FSC_TRANSPORT_INVALID_VERSION               0x88
#define I2O_FSC_TRANSPORT_INVALID_OFFSET                0x89
#define I2O_FSC_TRANSPORT_INVALID_MSG_FLAGS             0x8A
#define I2O_FSC_TRANSPORT_FRAME_TOO_SMALL               0x8B
#define I2O_FSC_TRANSPORT_FRAME_TOO_LARGE               0x8C
#define I2O_FSC_TRANSPORT_INVALID_TARGET_ID             0x8D
#define I2O_FSC_TRANSPORT_INVALID_INITIATOR_ID          0x8E
#define I2O_FSC_TRANSPORT_INVALID_INITIATOR_CONTEXT     0x8F
#define I2O_FSC_TRANSPORT_UNKNOWN_FAILURE               0xFF

/* Device Claim Types */
#define	I2O_CLAIM_PRIMARY					0x01000000
#define	I2O_CLAIM_MANAGEMENT					0x02000000
#define	I2O_CLAIM_AUTHORIZED					0x03000000
#define	I2O_CLAIM_SECONDARY					0x04000000

/* Message header defines for VersionOffset */
#define I2OVER15	0x0001
#define I2OVER20	0x0002

/* Default is 1.5, FIXME: Need support for both 1.5 and 2.0 */
#define I2OVERSION	I2OVER15

#define SGL_OFFSET_0    I2OVERSION
#define SGL_OFFSET_4    (0x0040 | I2OVERSION)
#define SGL_OFFSET_5    (0x0050 | I2OVERSION)
#define SGL_OFFSET_6    (0x0060 | I2OVERSION)
#define SGL_OFFSET_7    (0x0070 | I2OVERSION)
#define SGL_OFFSET_8    (0x0080 | I2OVERSION)
#define SGL_OFFSET_9    (0x0090 | I2OVERSION)
#define SGL_OFFSET_10   (0x00A0 | I2OVERSION)

#define TRL_OFFSET_5    (0x0050 | I2OVERSION)
#define TRL_OFFSET_6    (0x0060 | I2OVERSION)

/* Transaction Reply Lists (TRL) Control Word structure */
#define TRL_SINGLE_FIXED_LENGTH		0x00
#define TRL_SINGLE_VARIABLE_LENGTH	0x40
#define TRL_MULTIPLE_FIXED_LENGTH	0x80


 /* msg header defines for MsgFlags */
#define MSG_STATIC	0x0100
#define MSG_64BIT_CNTXT	0x0200
#define MSG_MULTI_TRANS	0x1000
#define MSG_FAIL	0x2000
#define MSG_FINAL	0x4000
#define MSG_REPLY	0x8000

 /* minimum size msg */
#define THREE_WORD_MSG_SIZE	0x00030000
#define FOUR_WORD_MSG_SIZE	0x00040000
#define FIVE_WORD_MSG_SIZE	0x00050000
#define SIX_WORD_MSG_SIZE	0x00060000
#define SEVEN_WORD_MSG_SIZE	0x00070000
#define EIGHT_WORD_MSG_SIZE	0x00080000
#define NINE_WORD_MSG_SIZE	0x00090000
#define TEN_WORD_MSG_SIZE	0x000A0000
#define ELEVEN_WORD_MSG_SIZE	0x000B0000
#define I2O_MESSAGE_SIZE(x)	((x)<<16)


/* Special TID Assignments */

#define ADAPTER_TID		0
#define HOST_TID		1

#define MSG_FRAME_SIZE		64	/* i2o_scsi assumes >= 32 */
#define NMBR_MSG_FRAMES		128

#define MSG_POOL_SIZE		(MSG_FRAME_SIZE*NMBR_MSG_FRAMES*sizeof(u32))

#define I2O_POST_WAIT_OK	0
#define I2O_POST_WAIT_TIMEOUT	-ETIMEDOUT

#endif /* __KERNEL__ */
#endif /* _I2O_H */
