/*
 * Generic register and struct definitions for the Adaptech 154x/164x
 * SCSI host adapters. Product specific probe and attach routines can
 * be found in:
 *      <fill in list here>
 *
 * Derived from bt.c written by:
 *
 * Copyright (c) 1998 Justin T. Gibbs.
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

#ifndef _AHAREG_H_
#define _AHAREG_H_

#include <sys/queue.h>
#include <cam/scsi/scsi_all.h>

#define AHA_MAXTRANSFER_SIZE	 0xffffff	/* limited by 24bit counter */
#define AHA_NSEG		17	/* The number of dma segments 
					 * supported. */
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
 * Definitions for the "undocumented" geometry register, we just need
 * its location.
 */
#define GEOMETRY_REG			0x03

#define AHA_NREGS	(4)

/*
 * Opcodes for Adapter commands.
 */
typedef enum {
	AOP_NOP			= 0x00,
	AOP_INITIALIZE_MBOX	= 0x01,
	AOP_START_MBOX		= 0x02,
	AOP_EXECUTE_BIOS_CMD	= 0x03,
	AOP_INQUIRE_BOARD_ID	= 0x04,
	AOP_ENABLE_OMBR_INT	= 0x05,
	AOP_SET_SEL_TIMOUT	= 0x06,
	AOP_SET_TIME_ON_BUS	= 0x07,
	AOP_SET_TIME_OFF_BUS	= 0x08,
	AOP_SET_BUS_TRANS_RATE	= 0x09,
	AOP_INQUIRE_INST_LDEVS	= 0x0A,
	AOP_INQUIRE_CONFIG	= 0x0B,
	AOP_ENABLE_TARGET_MODE	= 0x0C,
	AOP_INQUIRE_SETUP_INFO	= 0x0D,
	AOP_WRITE_LRAM		= 0x1A,
	AOP_READ_LRAM		= 0x1B,
	AOP_WRITE_CHIP_FIFO	= 0x1C,
	AOP_READ_CHIP_FIFO	= 0x1D,
	AOP_ECHO_DATA_BYTE	= 0x1F,
	AOP_ADAPTER_DIAGNOSTICS	= 0x20,
	AOP_SET_ADAPTER_OPTIONS	= 0x21,
	AOP_SET_EEPROM          = 0x22,
	AOP_RETURN_EEPROM	= 0x23,
	AOP_ENABLE_SHADOW_RAM	= 0x24,
	AOP_INIT_BIOS_MBOX	= 0x25,
	AOP_SET_BIOS_BANK_1	= 0x26,
	AOP_SET_BIOS_BANK_2	= 0x27,
	AOP_RETURN_EXT_BIOS_INFO= 0x28,
	AOP_MBOX_IF_ENABLE	= 0x29,
	AOP_SCSI_TERM_STATUS	= 0x2C,
	AOP_INQUIRE_SCAM_DEV	= 0x2D,
	AOP_SCSI_DEV_TABLE	= 0x2E,
	AOP_SCAM_OP		= 0x2F,
	AOP_START_BIOS_CMD	= 0x82,
	AOP_INQUIRE_ESETUP_INFO	= 0x8D
} aha_op_t;

/************** Definitions of Multi-byte commands and responses ************/

struct	aha_extbios
{
	uint8_t flags;			/* Bit 3 == 1 extended bios enabled */
	uint8_t mailboxlock;		/* mail box lock code to unlock it */
};

typedef struct {
	uint8_t num_mboxes;
	uint8_t base_addr[3];
} init_24b_mbox_params_t;

typedef struct {
	uint8_t board_type;
/* These values are mostly from the aha-1540CP technical reference, but */
/* with other values from the old aha1542.c driver. The values from the */
/* aha-1540CP technical manual are used where conflicts arise */
#define		BOARD_1540_16HEAD_BIOS	0x00
#define		BOARD_1540_64HEAD_BIOS	0x30
#define		BOARD_1542		0x41	/* aha-1540/1542 w/64-h bios */
#define		BOARD_1640		0x42	/* aha-1640 */
#define		BOARD_1740		0x43	/* aha-1740A/1742A/1744 */
#define		BOARD_1542C		0x44	/* aha-1542C */
#define		BOARD_1542CF		0x45	/* aha-1542CF */
#define		BOARD_1542CP		0x46	/* aha-1542CP, plug and play */
	uint8_t cust_features;
#define		FEATURES_STANDARD	0x30
	uint8_t firmware_rev_major;
	uint8_t firmware_rev_minor;
} board_id_data_t;

typedef struct {
	uint8_t dma_chan;
#define		DMA_CHAN_5	0x20
#define		DMA_CHAN_6	0x40
#define		DMA_CHAN_7	0x80
	uint8_t irq;
#define		IRQ_9		0x01
#define		IRQ_10		0x02
#define		IRQ_11		0x04
#define		IRQ_12		0x08
#define		IRQ_14		0x20
#define		IRQ_15		0x40
	uint8_t scsi_id;
} config_data_t;

typedef struct {
	uint8_t enable;
} target_mode_params_t;

typedef struct {
	uint8_t offset : 4,
		 period : 3,
		 sync	: 1;
} targ_syncinfo_t;

typedef struct {
	uint8_t	initiate_sync	: 1,
		 	parity_enable	: 1,
					: 6;

	uint8_t	bus_transfer_rate;
	uint8_t	time_on_bus;
	uint8_t	time_off_bus;
	uint8_t	num_mboxes;
	uint8_t	mbox_base_addr[3];
	targ_syncinfo_t	syncinfo[8];
	uint8_t	discinfo;
	uint8_t	customer_sig[20];
	uint8_t	auto_retry;
	uint8_t	board_switches;
	uint8_t	firmware_cksum[2];
	uint8_t	bios_mbox_addr[3];
} setup_data_t;

struct aha_isa_port {
	uint16_t addr;
	uint8_t  bio;	/* board IO offset */
};

#define AHA_NUM_ISAPORTS 6

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
	uint8_t sync_rate[16];		/* Sync in 10ns units */
} target_sync_info_data_t;

typedef struct {
	uint8_t len[3];
	uint8_t addr[3];
} aha_sg_t;

/********************** Mail Box definitions *******************************/

typedef enum {
	AMBO_FREE		= 0x0,	/* MBO intry is free */
	AMBO_START		= 0x1,	/* MBO activate entry */
	AMBO_ABORT		= 0x2	/* MBO abort entry */
} aha_mbo_action_code_t; 

typedef struct aha_mbox_out {
	uint8_t  action_code;
	uint8_t  ccb_addr[3];
} aha_mbox_out_t;

typedef enum {
	AMBI_FREE		= 0x0,	/* MBI entry is free */ 
	AMBI_OK			= 0x1,	/* completed without error */
	AMBI_ABORT		= 0x2,	/* aborted ccb */
	AMBI_NOT_FOUND		= 0x3,	/* Tried to abort invalid CCB */
	AMBI_ERROR		= 0x4	/* Completed with error */
} aha_mbi_comp_code_t; 

typedef struct aha_mbox_in {      
	uint8_t  comp_code;
	uint8_t  ccb_addr[3];
} aha_mbox_in_t;

/****************** Hardware CCB definition *********************************/
typedef enum {
	INITIATOR_CCB		= 0x00,
	INITIATOR_SG_CCB	= 0x02,
	INITIATOR_CCB_WRESID	= 0x03,
	INITIATOR_SG_CCB_WRESID	= 0x04,
	INITIATOR_BUS_DEV_RESET = 0x81
} aha_ccb_opcode_t;

typedef enum {
	AHASTAT_NOERROR			= 0x00,
	AHASTAT_SELTIMEOUT		= 0x11,
	AHASTAT_DATARUN_ERROR		= 0x12,
	AHASTAT_UNEXPECTED_BUSFREE	= 0x13,
	AHASTAT_INVALID_PHASE		= 0x14,
	AHASTAT_INVALID_ACTION_CODE	= 0x15,
	AHASTAT_INVALID_OPCODE		= 0x16,
	AHASTAT_LINKED_CCB_LUN_MISMATCH	= 0x17,
	AHASTAT_INVALID_CCB_OR_SG_PARAM	= 0x1A,
	AHASTAT_HA_SCSI_BUS_RESET	= 0x22,	/* stolen from bt */
	AHASTAT_HA_BDR			= 0x25	/* Stolen from bt */
} ahastat_t;

struct aha_hccb {
	uint8_t  opcode;			/* 0 */
	uint8_t  lun		: 3,		/* 1 */
		  datain	: 1,
		  dataout	: 1,
		  target	: 3;
	uint8_t  cmd_len;			/* 2 */
	uint8_t  sense_len;			/* 3 */
	uint8_t  data_len[3];			/* 4 */
	uint8_t  data_addr[3];			/* 7 */
	uint8_t  link_ptr[3];			/* 10 */
	uint8_t  link_id;			/* 13 */
	uint8_t  ahastat;			/* 14 */
	uint8_t  sdstat;			/* 15 */
	uint8_t  reserved1;			/* 16 */
	uint8_t  reserved2;			/* 17 */
	uint8_t  scsi_cdb[16];			/* 18 */
	uint8_t  sense_data[SSD_FULL_SIZE];
};

typedef enum {
	ACCB_FREE		= 0x0,
	ACCB_ACTIVE		= 0x1,
	ACCB_DEVICE_RESET	= 0x2,
	ACCB_RELEASE_SIMQ	= 0x4
} accb_flags_t;

struct aha_ccb {
	struct	aha_hccb	 hccb;		/* hccb assumed to be at 0 */
	SLIST_ENTRY(aha_ccb)	 links;
	uint32_t		 flags;
	union ccb		*ccb;
	bus_dmamap_t		 dmamap;
	aha_sg_t		*sg_list;
	uint32_t		 sg_list_phys;
};

struct sg_map_node {
	bus_dmamap_t		 sg_dmamap;
	bus_addr_t		 sg_physaddr;
	aha_sg_t*		 sg_vaddr;
	SLIST_ENTRY(sg_map_node) links;
};
	
struct aha_softc {
	bus_space_tag_t		 tag;
	bus_space_handle_t	 bsh;
	struct	cam_sim		*sim;
	struct	cam_path	*path;
	aha_mbox_out_t		*cur_outbox;
	aha_mbox_in_t		*cur_inbox;
	aha_mbox_out_t		*last_outbox;
	aha_mbox_in_t		*last_inbox;
	struct	aha_ccb		*aha_ccb_array;
	SLIST_HEAD(,aha_ccb)	 free_aha_ccbs;
	LIST_HEAD(,ccb_hdr)	 pending_ccbs;
	u_int			 active_ccbs;
	uint32_t		 aha_ccb_physbase;
	aha_ccb_opcode_t	 ccb_sg_opcode;
	aha_ccb_opcode_t	 ccb_ccb_opcode;
	aha_mbox_in_t		*in_boxes;
	aha_mbox_out_t		*out_boxes;
	struct scsi_sense_data	*sense_buffers;
	uint32_t		 sense_buffers_physbase;
	struct	aha_ccb		*recovery_accb;
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
	bus_dma_tag_t		 sg_dmat;	/* dmat for our sg maps */
	SLIST_HEAD(, sg_map_node) sg_maps;
	bus_addr_t		 mailbox_physbase;
	u_int			 num_ccbs;	/* Number of CCBs malloc'd */
	u_int			 max_ccbs;	/* Maximum allocatable CCBs */
	u_int			 max_sg;
	u_int			 unit;
	u_int			 scsi_id;
	uint32_t		 extended_trans	  :1,
				 diff_bus	  :1,
				 extended_lun	  :1,
				 strict_rr	  :1,
				 tag_capable	  :1,
				 resource_shortage:1,
						  :26;
	uint16_t		 disc_permitted;
	uint16_t		 sync_permitted;
	uint8_t			 init_level;
	volatile uint8_t	 command_cmp;
	volatile uint8_t	 latched_status;
	uint32_t		 bios_addr;
	uint8_t			 fw_major;
	uint8_t			 fw_minor;
	char			 model[32];
	uint8_t			 boardid;
	struct resource		*irq;
	int			 irqrid;
	struct resource		*port;
	int			 portrid;
	struct resource		*drq;
	int			 drqrid;
	void			**ih;
};

extern struct aha_softc *aha_softcs[];	/* XXX Config should handle this */
extern u_long aha_unit;

#define AHA_TEMP_UNIT 0xFF		/* Unit for probes */
struct aha_softc*	aha_alloc(int, bus_space_tag_t, bus_space_handle_t);
int			aha_attach(struct aha_softc *);
int			aha_cmd(struct aha_softc *, aha_op_t, uint8_t *,
			    u_int, uint8_t *, u_int, u_int);
int			aha_detach(struct aha_softc *);
int			aha_fetch_adapter_info(struct aha_softc *);
void			aha_find_probe_range(int, int *, int *);
void			aha_free(struct aha_softc *);
int			aha_init(struct aha_softc *); 
void			aha_intr(void *);
int			aha_iop_from_bio(isa_compat_io_t);
char *			aha_name(struct aha_softc *);
int			aha_probe(struct aha_softc *);

#define DEFAULT_CMD_TIMEOUT 10000	/* 1 sec */

#define aha_inb(aha, port)				\
	bus_space_read_1((aha)->tag, (aha)->bsh, port)

#define aha_outb(aha, port, value)			\
	bus_space_write_1((aha)->tag, (aha)->bsh, port, value)

#define ADP0100_PNP		0x00019004	/* ADP0100 */
#define AHA1540_PNP		0x40159004	/* ADP1540 */
#define AHA1542_PNP		0x42159004	/* ADP1542 */
#define AHA1542_PNPCOMPAT	0xA000D040	/* PNP00A0 */
#define ICU0091_PNP		0X91005AA4	/* ICU0091 */

#endif	/* _AHA_H_ */
