/*-
 * Generic register and struct definitions for the BusLogic
 * MultiMaster SCSI host adapters.  Product specific probe and
 * attach routines can be found in:
 * sys/dev/buslogic/bt_isa.c	BT-54X, BT-445 cards
 * sys/dev/buslogic/bt_mca.c	BT-64X, SDC3211B, SDC3211F
 * sys/dev/buslogic/bt_pci.c	BT-946, BT-948, BT-956, BT-958 cards
 *
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 1998, 1999 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _BTREG_H_
#define _BTREG_H_

#include <sys/queue.h>

#define BT_MAXTRANSFER_SIZE	 0xffffffff	/* limited by 32bit counter */
#define BT_NSEG		32	/* The number of dma segments supported.
                                 * BT_NSEG can be maxed out at 8192 entries,
                                 * but the kernel will never need to transfer
                                 * such a large request.  To reduce the
                                 * driver's memory consumption, we reduce the
                                 * max to 32.  16 would work if all transfers
                                 * are paged alined since the kernel will only
                                 * generate at most a 64k transfer, but to
                                 * handle non-page aligned transfers, you need
                                 * 17, so we round to the next power of two
                                 * to make allocating SG space easy and
                                 * efficient.
				 */

#define ALL_TARGETS (~0)

/*
 * Control Register pp. 1-8, 1-9 (Write Only)
 */
#define	CONTROL_REG		0x00
#define		HARD_RESET	0x80	/* Hard Reset - return to POST state */
#define		SOFT_RESET	0x40	/* Soft Reset - Clears Adapter state */
#define		RESET_INTR	0x20	/* Reset/Ack Interrupt */
#define		RESET_SBUS	0x10	/* Drive SCSI bus reset signal */

/*
 * Status Register pp. 1-9, 1-10 (Read Only)
 */
#define STATUS_REG			0x00
#define		DIAG_ACTIVE		0x80	/* Performing Internal Diags */
#define		DIAG_FAIL		0x40	/* Internal Diags failed */
#define		INIT_REQUIRED		0x20	/* MBOXes need initialization */
#define		HA_READY		0x10	/* HA ready for new commands */
#define		CMD_REG_BUSY		0x08	/* HA busy with last cmd byte */
#define		DATAIN_REG_READY	0x04	/* Data-in Byte available */
#define		STATUS_REG_RSVD		0x02
#define		CMD_INVALID		0x01	/* Invalid Command detected */

/*
 * Command/Parameter Register pp. 1-10, 1-11 (Write Only)
 */
#define	COMMAND_REG			0x01

/*
 * Data in Register p. 1-11 (Read Only)
 */
#define	DATAIN_REG			0x01

/*
 * Interrupt Status Register pp. 1-12 -> 1-14 (Read Only)
 */
#define INTSTAT_REG			0x02
#define		INTR_PENDING		0x80	/* There is a pending INTR */
#define		INTSTAT_REG_RSVD	0x70
#define		SCSI_BUS_RESET		0x08	/* Bus Reset detected */
#define		CMD_COMPLETE		0x04
#define		OMB_READY		0x02	/* Outgoin Mailbox Ready */
#define		IMB_LOADED		0x01	/* Incoming Mailbox loaded */

/*
 * Definitions for the "undocumented" geometry register
 */
typedef enum {
	GEOM_NODISK,
	GEOM_64x32,
	GEOM_128x32,
	GEOM_255x32
} disk_geom_t;

#define GEOMETRY_REG			0x03
#define		DISK0_GEOMETRY		0x03
#define		DISK1_GEOMETRY		0x0c
#define		EXTENDED_TRANSLATION	0x80
#define		GEOMETRY_DISK0(g_reg) (greg & DISK0_GEOMETRY)
#define		GEOMETRY_DISK1(g_reg) ((greg & DISK1_GEOMETRY) >> 2)

#define BT_NREGS	(4)
/*
 * Opcodes for Adapter commands.
 * pp 1-18 -> 1-20
 */
typedef enum {
	BOP_TEST_CMDC_INTR	= 0x00,
	BOP_INITIALIZE_24BMBOX	= 0x01,
	BOP_START_MBOX		= 0x02,
	BOP_EXECUTE_BIOS_CMD	= 0x03,
	BOP_INQUIRE_BOARD_ID	= 0x04,
	BOP_ENABLE_OMBR_INT	= 0x05,
	BOP_SET_SEL_TIMOUT	= 0x06,
	BOP_SET_TIME_ON_BUS	= 0x07,
	BOP_SET_TIME_OFF_BUS	= 0x08,
	BOP_SET_BUS_TRANS_RATE	= 0x09,
	BOP_INQUIRE_INST_LDEVS	= 0x0A,
	BOP_INQUIRE_CONFIG	= 0x0B,
	BOP_ENABLE_TARGET_MODE	= 0x0C,
	BOP_INQUIRE_SETUP_INFO	= 0x0D,
	BOP_WRITE_LRAM		= 0x1A,
	BOP_READ_LRAM		= 0x1B,
	BOP_WRITE_CHIP_FIFO	= 0x1C,
	BOP_READ_CHIP_FIFO	= 0x1C,
	BOP_ECHO_DATA_BYTE	= 0x1F,
	BOP_ADAPTER_DIAGNOSTICS	= 0x20,
	BOP_SET_ADAPTER_OPTIONS	= 0x21,
	BOP_INQUIRE_INST_HDEVS	= 0x23,
	BOP_INQUIRE_TARG_DEVS	= 0x24,
	BOP_DISABLE_HAC_INTR	= 0x25,
	BOP_INITIALIZE_32BMBOX	= 0x81,
	BOP_EXECUTE_SCSI_CMD	= 0x83,
	BOP_INQUIRE_FW_VER_3DIG	= 0x84,
	BOP_INQUIRE_FW_VER_4DIG	= 0x85,
	BOP_INQUIRE_PCI_INFO	= 0x86,
	BOP_INQUIRE_MODEL	= 0x8B,
	BOP_TARG_SYNC_INFO	= 0x8C,
	BOP_INQUIRE_ESETUP_INFO	= 0x8D,
	BOP_ENABLE_STRICT_RR	= 0x8F,
	BOP_STORE_LRAM		= 0x90,
	BOP_FETCH_LRAM		= 0x91,
	BOP_SAVE_TO_EEPROM	= 0x92,
	BOP_UPLOAD_AUTO_SCSI	= 0x94,
	BOP_MODIFY_IO_ADDR	= 0x95,
	BOP_SET_CCB_FORMAT	= 0x96,
	BOP_FLASH_ROM_DOWNLOAD	= 0x97,
	BOP_FLASH_WRITE_ENABLE	= 0x98,
	BOP_WRITE_INQ_BUFFER	= 0x9A,
	BOP_READ_INQ_BUFFER	= 0x9B,
	BOP_FLASH_UP_DOWNLOAD	= 0xA7,
	BOP_READ_SCAM_DATA	= 0xA8,
	BOP_WRITE_SCAM_DATA	= 0xA9
} bt_op_t;

/************** Definitions of Multi-byte commands and responses ************/

typedef struct {
	u_int8_t num_mboxes;
	u_int8_t base_addr[3];
} init_24b_mbox_params_t;

typedef struct {
	u_int8_t board_type;
#define		BOARD_TYPE_NON_MCA	0x41
#define		BOARD_TYPE_MCA		0x42
	u_int8_t cust_features;
#define		FEATURES_STANDARD	0x41
	u_int8_t firmware_rev_major;
	u_int8_t firmware_rev_minor;
} board_id_data_t;

typedef struct {
	u_int8_t enable;
} enable_ombr_intr_params_t;

typedef struct {
	u_int8_t enable;
	u_int8_t reserved;
	u_int8_t timeout[2];	/* timeout in milliseconds */
} set_selto_parmas_t;

typedef struct {
	u_int8_t time;		/* time in milliseconds (2-15) */
} set_timeon_bus_params_t;

typedef struct {
	u_int8_t time;		/* time in milliseconds (2-15) */
} set_timeoff_bus_params_t;

typedef struct {
	u_int8_t rate;
} set_bus_trasfer_rate_params_t;

typedef struct {
	u_int8_t targets[8];
} installed_ldevs_data_t;

typedef struct {
	u_int8_t dma_chan;
#define		DMA_CHAN_5	0x20
#define		DMA_CHAN_6	0x40
#define		DMA_CHAN_7	0x80
	u_int8_t irq;
#define		IRQ_9		0x01
#define		IRQ_10		0x02
#define		IRQ_11		0x04
#define		IRQ_12		0x08
#define		IRQ_14		0x20
#define		IRQ_15		0x40
	u_int8_t scsi_id;
} config_data_t;

typedef struct {
	u_int8_t enable;
} target_mode_params_t;

typedef struct {
	u_int8_t offset : 4,
		 period : 3,
		 sync	: 1;
} targ_syncinfo_t;

typedef struct {
	u_int8_t	initiate_sync	: 1,
		 	parity_enable	: 1,
					: 6;

	u_int8_t	bus_transfer_rate;
	u_int8_t	time_on_bus;
	u_int8_t	time_off_bus;
	u_int8_t	num_mboxes;
	u_int8_t	mbox_base_addr[3];
	targ_syncinfo_t	low_syncinfo[8];	/* For fast and ultra, use 8C */
	u_int8_t	low_discinfo;
	u_int8_t	customer_sig;
	u_int8_t	letter_d;
	u_int8_t	ha_type;
	u_int8_t	low_wide_allowed;
	u_int8_t	low_wide_active;
	targ_syncinfo_t	high_syncinfo[8];
	u_int8_t	high_discinfo;
	u_int8_t	high_wide_allowed;
	u_int8_t	high_wide_active;
} setup_data_t;

typedef struct {
	u_int8_t phys_addr[3];
} write_adapter_lram_params_t;

typedef struct {
	u_int8_t phys_addr[3];
} read_adapter_lram_params_t;

typedef struct {
	u_int8_t phys_addr[3];
} write_chip_fifo_params_t;

typedef struct {
	u_int8_t phys_addr[3];
} read_chip_fifo_params_t;

typedef struct {
	u_int8_t length;		/* Excludes this member */
	u_int8_t low_disc_disable;
	u_int8_t low_busy_retry_disable;
	u_int8_t high_disc_disable;
	u_int8_t high_busy_retry_disable;
} set_adapter_options_params_t;

typedef struct {
	u_int8_t targets[8];
} installed_hdevs_data_t;

typedef struct {
	u_int8_t low_devs;
	u_int8_t high_devs;
} target_devs_data_t;

typedef struct {
	u_int8_t enable;
} enable_hac_interrupt_params_t;

typedef struct {
	u_int8_t num_boxes;
	u_int8_t base_addr[4];
} init_32b_mbox_params_t;

typedef u_int8_t fw_ver_3dig_data_t;

typedef u_int8_t fw_ver_4dig_data_t;

typedef struct  {
	u_int8_t offset;
	u_int8_t response_len;
} fetch_lram_params_t;

#define AUTO_SCSI_BYTE_OFFSET	64
typedef struct {
	u_int8_t	factory_sig[2];
	u_int8_t	auto_scsi_data_size;	/* 2 -> 64 bytes */
	u_int8_t	model_num[6];
	u_int8_t	adapter_ioport;
	u_int8_t	floppy_enabled	 :1,
			floppy_secondary :1,
			level_trigger	 :1,
					 :2,
			system_ram_area	 :3;
	u_int8_t	dma_channel	 :7,
			dma_autoconf	 :1;
	u_int8_t	irq_channel	 :7,
			irq_autoconf	 :1;
	u_int8_t	dma_trans_rate;
	u_int8_t	scsi_id;
	u_int8_t	low_termination	 :1,
			scsi_parity	 :1,
			high_termination :1,
			req_ack_filter	 :1,
			fast_sync	 :1,
			bus_reset	 :1,
					 :1,
			active_negation	 :1;
	u_int8_t	bus_on_delay;
	u_int8_t	bus_off_delay;
	u_int8_t	bios_enabled	 :1,
			int19h_redirect	 :1,
			extended_trans	 :1,
			removable_drives :1,
					 :1,
			morethan2disks	 :1,
			interrupt_mode	 :1,
			floptical_support:1;
	u_int8_t	low_device_enabled;
	u_int8_t	high_device_enabled;
	u_int8_t	low_wide_permitted;
	u_int8_t	high_wide_permitted;
	u_int8_t	low_fast_permitted;
	u_int8_t	high_fast_permitted;
	u_int8_t	low_sync_permitted;
	u_int8_t	high_sync_permitted;
	u_int8_t	low_disc_permitted;
	u_int8_t	high_disc_permitted;
	u_int8_t	low_send_start_unit;
	u_int8_t	high_send_start_unit;
	u_int8_t	low_ignore_in_bios_scan;
	u_int8_t	high_ignore_in_bios_scan;
	u_int8_t	pci_int_pin	 :2,
			host_ioport	 :2,
			round_robin	 :1,
			vesa_bus_over_33 :1,
			vesa_burst_write :1,
			vesa_burst_read	 :1;
	u_int8_t	low_ultra_permitted;
	u_int8_t	high_ultra_permitted;
	u_int8_t	reserved[5];
	u_int8_t	auto_scsi_max_lun;
	u_int8_t			 :1,
			scam_dominant	 :1,
			scam_enabled	 :1,
			scam_level2	 :1,
					 :4;
	u_int8_t	int13_extensions :1,
					 :1,
			cdrom_boot	 :1,
					 :2,
			multi_boot	 :1,
					 :2;
	u_int8_t	boot_target_id	 :4,
			boot_channel	 :4;
	u_int8_t	force_dev_scan	 :1,
					 :7;
	u_int8_t	low_tagged_lun_independance;
	u_int8_t	high_tagged_lun_independance;
	u_int8_t	low_renegotiate_after_cc;
	u_int8_t	high_renegotiate_after_cc;
	u_int8_t	reserverd2[10];
	u_int8_t	manufacturing_diagnotic[2];
	u_int8_t	checksum[2];
} auto_scsi_data_t;

struct bt_isa_port {
	u_int16_t addr;
	u_int8_t  probed;
	u_int8_t  bio;
};

extern struct bt_isa_port bt_isa_ports[];

#define BT_NUM_ISAPORTS 6

typedef enum {
	BIO_330		= 0,
	BIO_334		= 1,
	BIO_230		= 2,
	BIO_234		= 3,
	BIO_130		= 4,
	BIO_134		= 5,
	BIO_DISABLED	= 6,
	BIO_DISABLED2	= 7
} isa_compat_io_t;

typedef struct {
	u_int8_t io_port;
	u_int8_t irq_num;
	u_int8_t low_byte_term	:1,
		 high_byte_term	:1,
		 		:2,
		 jp1_status	:1,
		 jp2_status	:1,
		 jp3_status	:1,
		 		:1;
	u_int8_t reserved;
} pci_info_data_t;

typedef struct {
	u_int8_t ascii_model[5];	/* Fifth byte is always 0 */
} ha_model_data_t;

typedef struct {
	u_int8_t sync_rate[16];		/* Sync in 10ns units */
} target_sync_info_data_t;

typedef struct {
	u_int8_t  bus_type;
	u_int8_t  bios_addr;
	u_int16_t max_sg;
	u_int8_t  num_mboxes;
	u_int8_t  mbox_base[4];
	u_int8_t			:2,
		  sync_neg10MB		:1,
		  floppy_disable	:1,
		  floppy_secondary_port	:1,
		  burst_mode_enabled	:1,
		  level_trigger_ints	:1,
					:1;
	u_int8_t  fw_ver_bytes_2_to_4[3];
	u_int8_t  wide_bus		:1,
		  diff_bus		:1,
		  scam_capable		:1,
		  ultra_scsi		:1,
		  auto_term		:1,
		 			:3;
} esetup_info_data_t;

typedef struct {
	u_int32_t len;
	u_int32_t addr;
} bt_sg_t;

/********************** Mail Box definitions *******************************/

typedef enum {
	BMBO_FREE		= 0x0,	/* MBO intry is free */
	BMBO_START		= 0x1,	/* MBO activate entry */
	BMBO_ABORT		= 0x2	/* MBO abort entry */
} bt_mbo_action_code_t; 

typedef struct bt_mbox_out {     
	u_int32_t ccb_addr;
	u_int8_t  reserved[3];
	u_int8_t  action_code;
} bt_mbox_out_t;

typedef enum {
	BMBI_FREE		= 0x0,	/* MBI entry is free */ 
	BMBI_OK			= 0x1,	/* completed without error */
	BMBI_ABORT		= 0x2,	/* aborted ccb */
	BMBI_NOT_FOUND		= 0x3,	/* Tried to abort invalid CCB */
	BMBI_ERROR		= 0x4	/* Completed with error */
} bt_mbi_comp_code_t; 

typedef struct bt_mbox_in {      
	u_int32_t ccb_addr;    
	u_int8_t  btstat;
	u_int8_t  sdstat;
	u_int8_t  reserved;
	u_int8_t  comp_code;
} bt_mbox_in_t;

/***************** Compiled Probe Information *******************************/
struct bt_probe_info {
	int	drq;
	int	irq;
};

/****************** Hardware CCB definition *********************************/
typedef enum {
	INITIATOR_CCB		= 0x00,
	INITIATOR_SG_CCB	= 0x02,
	INITIATOR_CCB_WRESID	= 0x03,
	INITIATOR_SG_CCB_WRESID	= 0x04,
	INITIATOR_BUS_DEV_RESET = 0x81
} bt_ccb_opcode_t;

typedef enum {
	BTSTAT_NOERROR			= 0x00,
	BTSTAT_LINKED_CMD_COMPLETE	= 0x0A,
	BTSTAT_LINKED_CMD_FLAG_COMPLETE	= 0x0B,
	BTSTAT_DATAUNDERUN_ERROR	= 0x0C,
	BTSTAT_SELTIMEOUT		= 0x11,
	BTSTAT_DATARUN_ERROR		= 0x12,
	BTSTAT_UNEXPECTED_BUSFREE	= 0x13,
	BTSTAT_INVALID_PHASE		= 0x14,
	BTSTAT_INVALID_ACTION_CODE	= 0x15,
	BTSTAT_INVALID_OPCODE		= 0x16,
	BTSTAT_LINKED_CCB_LUN_MISMATCH	= 0x17,
	BTSTAT_INVALID_CCB_OR_SG_PARAM	= 0x1A,
	BTSTAT_AUTOSENSE_FAILED		= 0x1B,
	BTSTAT_TAGGED_MSG_REJECTED	= 0x1C,
	BTSTAT_UNSUPPORTED_MSG_RECEIVED	= 0x1D,
	BTSTAT_HARDWARE_FAILURE		= 0x20,
	BTSTAT_TARGET_IGNORED_ATN	= 0x21,
	BTSTAT_HA_SCSI_BUS_RESET	= 0x22,
	BTSTAT_OTHER_SCSI_BUS_RESET	= 0x23,
	BTSTAT_INVALID_RECONNECT	= 0x24,
	BTSTAT_HA_BDR			= 0x25,
	BTSTAT_ABORT_QUEUE_GENERATED	= 0x26,
	BTSTAT_HA_SOFTWARE_ERROR	= 0x27,
	BTSTAT_HA_WATCHDOG_ERROR	= 0x28,
	BTSTAT_SCSI_PERROR_DETECTED	= 0x30
} btstat_t;

struct bt_hccb {
	u_int8_t  opcode;
	u_int8_t			:3,
		  datain		:1,
		  dataout		:1,
		  wide_tag_enable	:1,	/* Wide Lun CCB format */
		  wide_tag_type		:2;	/* Wide Lun CCB format */
	u_int8_t  cmd_len;
	u_int8_t  sense_len;
	int32_t	  data_len;			/* residuals can be negative */
	u_int32_t data_addr;
	u_int8_t  reserved[2];
	u_int8_t  btstat;
	u_int8_t  sdstat;
	u_int8_t  target_id;
	u_int8_t  target_lun	:5,
		  tag_enable	:1,
		  tag_type	:2;
	u_int8_t  scsi_cdb[12];
	u_int8_t  reserved2[6];
	u_int32_t sense_addr;
};

typedef enum {
	BCCB_FREE		= 0x0,
	BCCB_ACTIVE		= 0x1,
	BCCB_DEVICE_RESET	= 0x2,
	BCCB_RELEASE_SIMQ	= 0x4
} bccb_flags_t;

struct bt_ccb {
	struct	bt_hccb		 hccb;
	SLIST_ENTRY(bt_ccb)	 links;
	u_int32_t		 flags;
	union ccb		*ccb;
	bus_dmamap_t		 dmamap;
	struct callout		 timer;
	bt_sg_t			*sg_list;
	u_int32_t		 sg_list_phys;
};

struct sg_map_node {
	bus_dmamap_t		 sg_dmamap;
	bus_addr_t		 sg_physaddr;
	bt_sg_t*		 sg_vaddr;
	SLIST_ENTRY(sg_map_node) links;
};
	
struct bt_softc {
	device_t		dev;
	struct resource		*port;
	struct resource		*irq;
	struct resource		*drq;
	void			*ih;
	struct mtx		 lock;
	struct	cam_sim		*sim;
	struct	cam_path	*path;
	bt_mbox_out_t		*cur_outbox;
	bt_mbox_in_t		*cur_inbox;
	bt_mbox_out_t		*last_outbox;
	bt_mbox_in_t		*last_inbox;
	struct	bt_ccb		*bt_ccb_array;
	SLIST_HEAD(,bt_ccb)	 free_bt_ccbs;
	LIST_HEAD(,ccb_hdr)	 pending_ccbs;
	u_int			 active_ccbs;
	u_int32_t		 bt_ccb_physbase;
	bt_mbox_in_t		*in_boxes;
	bt_mbox_out_t		*out_boxes;
	struct scsi_sense_data	*sense_buffers;
	u_int32_t		 sense_buffers_physbase;
	struct	bt_ccb		*recovery_bccb;
	u_int			 num_boxes;
	bus_dma_tag_t		 parent_dmat;	/*
						 * All dmat's derive from
						 * the dmat defined by our
						 * bus.
						 */
	bus_dma_tag_t		 buffer_dmat;	/* dmat for buffer I/O */
	bus_dma_tag_t		 mailbox_dmat;	/* dmat for our mailboxes */
	bus_dmamap_t		 mailbox_dmamap;
	bus_dma_tag_t		 ccb_dmat;	/* dmat for our ccb array */
	bus_dmamap_t		 ccb_dmamap;
	bus_dma_tag_t		 sg_dmat;	/* dmat for our sg segments */
	bus_dma_tag_t		 sense_dmat;	/* dmat for our sense buffers */
	bus_dmamap_t		 sense_dmamap;
	SLIST_HEAD(, sg_map_node) sg_maps;
	bus_addr_t		 mailbox_physbase;
	bus_addr_t		 mailbox_addrlimit;
	u_int			 num_ccbs;	/* Number of CCBs malloc'd */
	u_int			 max_ccbs;	/* Maximum allocatable CCBs */
	u_int			 max_sg;
	u_int			 scsi_id;
	u_int32_t		 extended_trans	   :1,
				 wide_bus	   :1,
				 diff_bus	   :1,
				 ultra_scsi	   :1,
				 extended_lun	   :1,
				 strict_rr	   :1,
				 tag_capable	   :1,
				 wide_lun_ccb	   :1,
				 resource_shortage :1,
				 level_trigger_ints:1,
						   :22;
	u_int16_t		 tags_permitted;
	u_int16_t		 disc_permitted;
	u_int16_t		 sync_permitted;
	u_int16_t		 fast_permitted;
	u_int16_t		 ultra_permitted;
	u_int16_t		 wide_permitted;
	u_int8_t		 init_level;
	volatile u_int8_t	 command_cmp;
	volatile u_int8_t	 latched_status;
	u_int32_t		 bios_addr;
	char			 firmware_ver[6];
	char			 model[5];
};

#define BT_TEMP_UNIT 0xFF		/* Unit for probes */
void			bt_init_softc(device_t dev,
				      struct resource *port,
				      struct resource *irq,
				      struct resource *drq);
void			bt_free_softc(device_t dev);
int			bt_port_probe(device_t dev,
				      struct bt_probe_info *info);
int			bt_probe(device_t dev);
int			bt_fetch_adapter_info(device_t dev);
int			bt_init(device_t dev); 
int			bt_attach(device_t dev);
void			bt_intr(void *arg);
int			bt_check_probed_iop(u_int ioport);
void			bt_mark_probed_bio(isa_compat_io_t port);
void			bt_mark_probed_iop(u_int ioport);
void			bt_find_probe_range(int ioport,
					    int *port_index,
					    int *max_port_index);

int			bt_iop_from_bio(isa_compat_io_t bio_index);

#define DEFAULT_CMD_TIMEOUT 100000	/* 10 sec */
int			bt_cmd(struct bt_softc *bt, bt_op_t opcode,
			       u_int8_t *params, u_int param_len,
			       u_int8_t *reply_data, u_int reply_len,
			       u_int cmd_timeout);

#define bt_name(bt)	device_get_nameunit(bt->dev)

#define bt_inb(bt, reg)				\
	bus_read_1((bt)->port, reg)

#define bt_outb(bt, reg, value)			\
	bus_write_1((bt)->port, reg, value)

#endif	/* _BT_H_ */
