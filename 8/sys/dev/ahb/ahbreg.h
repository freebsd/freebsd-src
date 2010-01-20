/*-
 * Hardware structure definitions for the Adaptec 174X CAM SCSI device driver.
 *
 * Copyright (c) 1998 Justin T. Gibbs
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
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

/* Resource Constatns */
#define	AHB_NECB	64
#define AHB_NSEG	32

/* AHA1740 EISA ID, IO port range size, and offset from slot base */
#define EISA_DEVICE_ID_ADAPTEC_1740  0x04900000
#define	AHB_EISA_IOSIZE 0x100
#define	AHB_EISA_SLOT_OFFSET 0xc00

/* AHA1740 EISA board control registers (Offset from slot base) */
#define	EBCTRL				0x084
#define		CDEN			0x01

/*
 * AHA1740 EISA board mode registers (Offset from slot base)
 */
#define PORTADDR			0x0C0
#define		PORTADDR_ENHANCED	0x80

#define BIOSADDR			0x0C1

#define	INTDEF				0x0C2
#define		INT9			0x00
#define		INT10			0x01
#define		INT11			0x02
#define		INT12			0x03
#define		INT14			0x05
#define		INT15			0x06
#define		INTLEVEL		0x08
#define		INTEN			0x10

#define	SCSIDEF				0x0C3
#define		HSCSIID			0x0F	/* our SCSI ID */
#define		RSTBUS			0x10

#define	BUSDEF				0x0C4
#define		B0uS			0x00	/* give up bus immediatly */
#define		B4uS			0x01	/* delay 4uSec. */
#define		B8uS			0x02	/* delay 8uSec. */

#define	RESV0				0x0C5

#define	RESV1				0x0C6
#define		EXTENDED_TRANS		0x01

#define	RESV2				0x0C7

/*
 * AHA1740 ENHANCED mode mailbox control regs (Offset from slot base)
 */
#define MBOXOUT0			0x0D0
#define MBOXOUT1			0x0D1
#define MBOXOUT2			0x0D2
#define MBOXOUT3			0x0D3

#define	ATTN				0x0D4
#define		ATTN_TARGMASK		0x0F
#define		ATTN_IMMED		0x10
#define		ATTN_STARTECB		0x40
#define		ATTN_ABORTECB		0x50
#define		ATTN_TARG_RESET		0x80

#define	CONTROL				0x0D5
#define		CNTRL_SET_HRDY		0x20
#define		CNTRL_CLRINT		0x40
#define		CNTRL_HARD_RST		0x80

#define	INTSTAT				0x0D6
#define		INTSTAT_TARGET_MASK	0x0F
#define		INTSTAT_MASK		0xF0
#define		INTSTAT_ECB_OK		0x10	/* ECB Completed w/out error */
#define		INTSTAT_ECB_CMPWRETRY	0x50	/* ECB Completed w/retries */
#define		INTSTAT_HW_ERR		0x70	/* Adapter Hardware Failure */
#define		INTSTAT_IMMED_OK	0xA0	/* Immediate command complete */
#define		INTSTAT_ECB_CMPWERR	0xC0	/* ECB Completed w/error */
#define		INTSTAT_AEN_OCCURED	0xD0	/* Async Event Notification */
#define		INTSTAT_IMMED_ERR	0xE0	/* Immediate command failed */

#define HOSTSTAT			0x0D7
#define		HOSTSTAT_MBOX_EMPTY	0x04
#define		HOSTSTAT_INTPEND	0x02
#define		HOSTSTAT_BUSY		0x01


#define	MBOXIN0				0x0D8
#define	MBOXIN1				0x0D9
#define	MBOXIN2				0x0DA
#define	MBOXIN3				0x0DB

#define STATUS2				0x0DC
#define	STATUS2_HOST_READY		0x01

typedef enum {
	IMMED_RESET		  = 0x000080,
	IMMED_DEVICE_CLEAR_QUEUE  = 0x000480,
	IMMED_ADAPTER_CLEAR_QUEUE = 0x000880,
	IMMED_RESUME		  = 0x200090
} immed_cmd;

struct ecb_status {
	/* Status Flags */
	u_int16_t 	no_error      :1, /* Completed with no error */
			data_underrun :1,
				      :1,
			ha_queue_full :1,
			spec_check    :1,
			data_overrun  :1,
			chain_halted  :1,
			intr_issued   :1,
			status_avail  :1, /* status bytes 14-31 are valid */
			sense_stored  :1,
				      :1,
			init_requied  :1,
			major_error   :1,
				      :1,
			extended_ca   :1,
				      :1;
	/* Host Status */
	u_int8_t	ha_status;
	u_int8_t	scsi_status;
	int32_t		resid_count;
	u_int32_t	resid_addr;
	u_int16_t	addit_status;
	u_int8_t	sense_len;
	u_int8_t	unused[9];
	u_int8_t	cdb[6];
};

typedef enum {
	HS_OK			= 0x00,
	HS_CMD_ABORTED_HOST	= 0x04,
	HS_CMD_ABORTED_ADAPTER	= 0x05,
	HS_FIRMWARE_LOAD_REQ	= 0x08,
	HS_TARGET_NOT_ASSIGNED	= 0x0A,
	HS_SEL_TIMEOUT		= 0x11,
	HS_DATA_RUN_ERR		= 0x12,
	HS_UNEXPECTED_BUSFREE	= 0x13,
	HS_INVALID_PHASE	= 0x14,
	HS_INVALID_OPCODE	= 0x16,
	HS_INVALID_CMD_LINK	= 0x17,
	HS_INVALID_ECB_PARAM	= 0x18,
	HS_DUP_TCB_RECEIVED	= 0x19,
	HS_REQUEST_SENSE_FAILED	= 0x1A,
	HS_TAG_MSG_REJECTED	= 0x1C,
	HS_HARDWARE_ERR		= 0x20,
	HS_ATN_TARGET_FAILED	= 0x21,
	HS_SCSI_RESET_ADAPTER	= 0x22,
	HS_SCSI_RESET_INCOMING	= 0x23,
	HS_PROGRAM_CKSUM_ERROR	= 0x80
} host_status;

typedef enum {
	ECBOP_NOP		 = 0x00,
	ECBOP_INITIATOR_SCSI_CMD = 0x01,
	ECBOP_RUN_DIAGNOSTICS	 = 0x05,
	ECBOP_INITIALIZE_SCSI	 = 0x06, /* Set syncrate/disc/parity */
	ECBOP_READ_SENSE	 = 0x08,
	ECBOP_DOWNLOAD_FIRMWARE  = 0x09,
	ECBOP_READ_HA_INQDATA	 = 0x0a,
	ECBOP_TARGET_SCSI_CMD	 = 0x10
} ecb_op;

struct ha_inquiry_data {
	struct	  scsi_inquiry_data scsi_data;
	u_int8_t  release_date[8];
	u_int8_t  release_time[8];
	u_int16_t firmware_cksum;
	u_int16_t reserved;
	u_int16_t target_data[16];
};

struct hardware_ecb {
	u_int16_t	opcode;
	u_int16_t	flag_word1;
#define	FW1_LINKED_CMD		0x0001
#define FW1_DISABLE_INTR	0x0080
#define FW1_SUPPRESS_URUN_ERR	0x0400
#define	FW1_SG_ECB		0x1000
#define FW1_ERR_STATUS_BLK_ONLY	0x4000
#define FW1_AUTO_REQUEST_SENSE	0x8000
	u_int16_t	flag_word2;
#define FW2_LUN_MASK		0x0007
#define FW2_TAG_ENB		0x0008
#define FW2_TAG_TYPE		0x0030
#define FW2_TAG_TYPE_SHIFT	4
#define FW2_DISABLE_DISC	0x0040
#define FW2_CHECK_DATA_DIR	0x0100
#define FW2_DATA_DIR_IN		0x0200
#define FW2_SUPRESS_TRANSFER	0x0400
#define FW2_CALC_CKSUM		0x0800
#define FW2_RECOVERY_ECB	0x4000
#define FW2_NO_RETRY_ON_BUSY	0x8000
	u_int16_t	reserved;
	u_int32_t	data_ptr;
	u_int32_t	data_len;
	u_int32_t	status_ptr;
	u_int32_t	link_ptr;
	u_int32_t	reserved2;
	u_int32_t	sense_ptr;
	u_int8_t	sense_len;
	u_int8_t	cdb_len;
	u_int16_t	cksum;
	u_int8_t	cdb[12];
};

typedef struct {
	u_int32_t addr;
	u_int32_t len;
} ahb_sg_t;

typedef enum {
	ECB_FREE		= 0x0,
	ECB_ACTIVE		= 0x1,
	ECB_DEVICE_RESET	= 0x2,
	ECB_SCSIBUS_RESET	= 0x4,
	ECB_RELEASE_SIMQ	= 0x8
} ecb_state;

struct ecb {
	struct hardware_ecb	 hecb;
	struct ecb_status	 status;
	struct scsi_sense_data	 sense;	
	ahb_sg_t		 sg_list[AHB_NSEG];
	SLIST_ENTRY(ecb)	 links;
	ecb_state		 state;
	union ccb		*ccb;
	bus_dmamap_t		 dmamap;
};

struct ahb_softc {
	device_t		 dev;
	bus_space_tag_t		 tag;
	bus_space_handle_t	 bsh;
	struct	cam_sim		*sim;
	struct	cam_path	*path;
	SLIST_HEAD(,ecb)	 free_ecbs;
	LIST_HEAD(,ccb_hdr)	 pending_ccbs;
	struct ecb		*ecb_array;
	u_int32_t		 ecb_physbase;
	bus_dma_tag_t		 buffer_dmat;	/* dmat for buffer I/O */
	bus_dma_tag_t		 ecb_dmat;	/* dmat for our ecb array */
	bus_dmamap_t		 ecb_dmamap;
	volatile u_int32_t	 immed_cmd;
	struct	ecb		*immed_ecb;
	struct	ha_inquiry_data	*ha_inq_data;
	u_int32_t		 ha_inq_physbase;
	u_long			 unit;
	u_int			 init_level;
	u_int			 scsi_id;
	u_int			 num_ecbs;
	u_int			 extended_trans;
	u_int8_t		 disc_permitted;
	u_int8_t		 tags_permitted;
};
