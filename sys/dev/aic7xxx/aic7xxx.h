/*
 * Interface to the generic driver for the aic7xxx based adaptec
 * SCSI controllers.  This is used to implement product specific
 * probe and attach routines.
 *
 * Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999, 2000 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU Public License ("GPL").
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

#ifndef _AIC7XXX_H_
#define _AIC7XXX_H_

#include "opt_aic7xxx.h"	/* for config options */
#include "aic7xxx_reg.h"

#include <sys/bus.h>		/* For device_t */

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

/*
 * The maximum number of supported targets.
 */
#define AHC_NUM_TARGETS 16

/*
 * The maximum number of supported luns.
 * Although the identify message only supports 64 luns in SPI3, you
 * can have 2^64 luns when information unit transfers are enabled.
 * The max we can do sanely given the 8bit nature of the RISC engine
 * on these chips is 256.
 */
#define AHC_NUM_LUNS 256

/*
 * The maximum transfer per S/G segment.
 */
#define AHC_MAXTRANSFER_SIZE	 0x00ffffff	/* limited by 24bit counter */

/*
 * The number of dma segments supported.  The sequencer can handle any number
 * of physically contiguous S/G entrys.  To reduce the driver's memory
 * consumption, we limit the number supported to be sufficient to handle
 * the largest mapping supported by the kernel, MAXPHYS.  Assuming the
 * transfer is as fragmented as possible and unaligned, this turns out to
 * be the number of paged sized transfers in MAXPHYS plus an extra element
 * to handle any unaligned residual.  The sequencer fetches SG elements
 * in 128 byte chucks, so make the number per-transaction a nice multiple
 * of 16 (8 byte S/G elements).
 */
/* XXX Worth the space??? */
#define AHC_NSEG (roundup(btoc(MAXPHYS) + 1, 16))

#define AHC_SCB_MAX	255	/*
				 * Up to 255 SCBs on some types of aic7xxx
				 * based boards.  The aic7870 have 16 internal
				 * SCBs, but external SRAM bumps this to 255.
				 * The aic7770 family have only 4, and the 
				 * aic7850 has only 3.
				 */

#define AHC_TMODE_CMDS	256    /*
				* Ring Buffer of incoming target commands.
				* We allocate 256 to simplify the logic
				* in the sequencer by using the natural
				* wrap point of an 8bit counter.
				*/

/*
 * The aic7xxx chips only support a 24bit length.  We use the top
 * byte of the length to store additional address bits as well
 * as an indicator if this is the last SG segment in a transfer.
 * This gives us an addressable range of 512GB on machines with
 * 64bit PCI or with chips that can support dual address cycles
 * on 32bit PCI busses.
 */
struct ahc_dma_seg {
	uint32_t	addr;
	uint32_t	len;
#define	AHC_DMA_LAST_SEG	0x80000000
#define	AHC_SG_HIGH_ADDR_MASK	0x7F000000
#define	AHC_SG_LEN_MASK		0x00FFFFFF
};

/* The chip order is from least sophisticated to most sophisticated */
typedef enum {
	AHC_NONE	= 0x0000,
	AHC_CHIPID_MASK	= 0x00FF,
	AHC_AIC7770	= 0x0001,
	AHC_AIC7850	= 0x0002,
	AHC_AIC7855	= 0x0003,
	AHC_AIC7859	= 0x0004,
	AHC_AIC7860	= 0x0005,
	AHC_AIC7870	= 0x0006,
	AHC_AIC7880	= 0x0007,
	AHC_AIC7895	= 0x0008,
	AHC_AIC7890	= 0x0009,
	AHC_AIC7896	= 0x000a,
	AHC_AIC7892	= 0x000b,
	AHC_AIC7899	= 0x000c,
	AHC_VL		= 0x0100,	/* Bus type VL */
	AHC_EISA	= 0x0200,	/* Bus type EISA */
	AHC_PCI		= 0x0400,	/* Bus type PCI */
	AHC_BUS_MASK	= 0x0F00
} ahc_chip;

extern char *ahc_chip_names[];

typedef enum {
	AHC_FENONE	= 0x0000,
	AHC_ULTRA	= 0x0001,	/* Supports 20MHz Transfers */
	AHC_ULTRA2	= 0x0002,	/* Supports 40MHz Transfers */
	AHC_WIDE  	= 0x0004,	/* Wide Channel */
	AHC_TWIN	= 0x0008,	/* Twin Channel */
	AHC_MORE_SRAM	= 0x0010,	/* 80 bytes instead of 64 */
	AHC_CMD_CHAN	= 0x0020,	/* Has a Command DMA Channel */
	AHC_QUEUE_REGS	= 0x0040,	/* Has Queue management registers */
	AHC_SG_PRELOAD	= 0x0080,	/* Can perform auto-SG preload */
	AHC_SPIOCAP	= 0x0100,	/* Has a Serial Port I/O Cap Register */
	AHC_MULTI_TID	= 0x0200,	/* Has bitmask of TIDs for select-in */
	AHC_HS_MAILBOX	= 0x0400,	/* Has HS_MAILBOX register */
	AHC_DT		= 0x0800,	/* Double Transition transfers */
	AHC_NEW_TERMCTL	= 0x1000,
	AHC_MULTI_FUNC	= 0x2000,	/* Multi-Function Twin Channel Device */
	AHC_LARGE_SCBS	= 0x4000,	/* 64byte SCBs */
	AHC_AIC7770_FE	= AHC_FENONE,
	AHC_AIC7850_FE	= AHC_SPIOCAP,
	AHC_AIC7855_FE	= AHC_AIC7850_FE,
	AHC_AIC7859_FE	= AHC_AIC7850_FE|AHC_ULTRA,
	AHC_AIC7860_FE	= AHC_AIC7859_FE,
	AHC_AIC7870_FE	= AHC_FENONE,
	AHC_AIC7880_FE	= AHC_ULTRA,
	AHC_AIC7890_FE	= AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA2|AHC_QUEUE_REGS
			  |AHC_SG_PRELOAD|AHC_MULTI_TID|AHC_HS_MAILBOX
			  |AHC_NEW_TERMCTL|AHC_LARGE_SCBS,
	AHC_AIC7892_FE	= AHC_AIC7890_FE|AHC_DT,
	AHC_AIC7895_FE	= AHC_AIC7880_FE|AHC_MORE_SRAM
			  |AHC_CMD_CHAN|AHC_MULTI_FUNC|AHC_LARGE_SCBS,
	AHC_AIC7895C_FE	= AHC_AIC7895_FE|AHC_MULTI_TID,
	AHC_AIC7896_FE	= AHC_AIC7890_FE|AHC_MULTI_FUNC,
	AHC_AIC7899_FE	= AHC_AIC7892_FE|AHC_MULTI_FUNC
} ahc_feature;

typedef enum {
	AHC_BUGNONE		= 0x00,
	/*
	 * On all chips prior to the U2 product line,
	 * the WIDEODD S/G segment feature does not
	 * work during scsi->HostBus transfers.
	 */
	AHC_TMODE_WIDEODD_BUG	= 0x01,
	/*
	 * On the aic7890/91 Rev 0 chips, the autoflush
	 * feature does not work.  A manual flush of
	 * the DMA FIFO is required.
	 */
	AHC_AUTOFLUSH_BUG	= 0x02
} ahc_bug;

typedef enum {
	AHC_FNONE		= 0x000,
	AHC_PAGESCBS		= 0x001,/* Enable SCB paging */
	AHC_CHANNEL_B_PRIMARY	= 0x002,/*
					 * On twin channel adapters, probe
					 * channel B first since it is the
					 * primary bus.
					 */
	AHC_USEDEFAULTS		= 0x004,/*
					 * For cards without an seeprom
					 * or a BIOS to initialize the chip's
					 * SRAM, we use the default target
					 * settings.
					 */
	AHC_SHARED_SRAM		= 0x010,
	AHC_LARGE_SEEPROM	= 0x020,/* Uses C56_66 not C46 */
	AHC_RESET_BUS_A		= 0x040,
	AHC_RESET_BUS_B		= 0x080,
	AHC_EXTENDED_TRANS_A	= 0x100,
	AHC_EXTENDED_TRANS_B	= 0x200,
	AHC_TERM_ENB_A		= 0x400,
	AHC_TERM_ENB_B		= 0x800,
	AHC_INITIATORMODE	= 0x1000,/*
					  * Allow initiator operations on
					  * this controller.
					  */
	AHC_TARGETMODE		= 0x2000,/*
					  * Allow target operations on this
					  * controller.
					  */
	AHC_NEWEEPROM_FMT	= 0x4000,
	AHC_RESOURCE_SHORTAGE	= 0x8000,
	AHC_TQINFIFO_BLOCKED	= 0x10000,/* Blocked waiting for ATIOs */
	AHC_INT50_SPEEDFLEX	= 0x20000,/*
					   * Internal 50pin connector
					   * sits behind an aic3860
					   */
	AHC_SCB_BTT		= 0x40000 /*
					   * The busy targets table is
					   * stored in SCB space rather
					   * than SRAM.
					   */
} ahc_flag;

struct ahc_probe_config {
	const char	*description;
	char		 channel;
	char		 channel_b;
	ahc_chip	 chip;
	ahc_feature	 features;
	ahc_bug		 bugs;
	ahc_flag	 flags;
};

typedef enum {
	SCB_FREE		= 0x0000,
	SCB_OTHERTCL_TIMEOUT	= 0x0002,/*
					  * Another device was active
					  * during the first timeout for
					  * this SCB so we gave ourselves
					  * an additional timeout period
					  * in case it was hogging the
					  * bus.
				          */
	SCB_DEVICE_RESET	= 0x0004,
	SCB_SENSE		= 0x0008,
	SCB_RECOVERY_SCB	= 0x0040,
	SCB_NEGOTIATE		= 0x0080,
	SCB_ABORT		= 0x1000,
	SCB_QUEUED_MSG		= 0x2000,
	SCB_ACTIVE		= 0x4000,
	SCB_TARGET_IMMEDIATE	= 0x8000
} scb_flag;

/*
 * The driver keeps up to MAX_SCB scb structures per card in memory.  The SCB
 * consists of a "hardware SCB" mirroring the fields availible on the card
 * and additional information the kernel stores for each transaction.
 *
 * To minimize space utilization, a portion of the hardware scb stores
 * different data during different portions of a SCSI transaction.
 * As initialized by the host driver for the initiator role, this area
 * contains the SCSI cdb (or pointer to the  cdb) to be executed.  After
 * the cdb has been presented to the target, this area serves to store
 * residual transfer information and the SCSI status byte.
 * For the target role, the contents of this area do not change, but
 * still serve a different purpose than for the initiator role.  See
 * struct target_data for details.
 */

struct status_pkt {
	uint32_t	residual_datacnt;
	uint32_t	residual_sg_ptr;
	uint8_t	scsi_status;
};

struct target_data {
	uint8_t	target_phases;
	uint8_t	data_phase;
	uint8_t	scsi_status;
	uint8_t	initiator_tag;
};

struct hardware_scb {
/*0*/   uint8_t  control;
/*1*/	uint8_t  scsiid;	/* what to load in the SCSIID register */
/*2*/	uint8_t  lun;
/*3*/	uint8_t  cdb_len;
/*4*/	union {
		/*
		 * 12 bytes of cdb information only
		 * used on chips with 32byte SCBs.
		 */
		uint8_t	cdb[12];
		uint32_t	cdb_ptr;
		struct		status_pkt status;
		struct		target_data tdata;
	} shared_data;
/*
 * A word about residuals.  The scb is presented to the sequencer with
 * the dataptr and datacnt fields initialized to the contents of the
 * first S/G element to transfer.  The sgptr field is initialized to
 * the bus address for the S/G element that follows the first in the
 * in core S/G array or'ed with the SG_FULL_RESID flag.  Sgptr may point
 * to an invalid S/G entry for this transfer.  If no transfer is to occur,
 * sgptr is set to SG_LIST_NULL.  The SG_FULL_RESID flag insures that
 * the residual will be correctly noted even if no data transfers occur.
 * Once the data phase is entered, the residual sgptr and datacnt are
 * loaded from the sgptr and the datacnt fields.  After each S/G element's
 * dataptr and length are loaded into the hardware, the residual sgptr
 * is advanced.  After each S/G element is expired, its datacnt field
 * is checked to see if the LAST_SEG flag is set.  If so, SG_LIST_NULL
 * is set in the residual sg ptr and the transfer is considered complete.
 * If the sequencer determines that three is a residual in the tranfer,
 * it will set the SG_RESID_VALID flag in sgptr and dma the scb back into
 * host memory.  To sumarize:
 *
 * Sequencer:
 *	o A residual has occurred if SG_FULL_RESID is set in sgptr,
 *	  or residual_sgptr does not have SG_LIST_NULL set.
 *
 *	o We are transfering the last segment if residual_datacnt has
 *	  the SG_LAST_SEG flag set.
 *
 * Host:
 *	o A residual has occurred if a completed scb has the
 *	  SG_RESID_VALID flag set.
 *
 *	o residual_sgptr and sgptr refer to the "next" sg entry
 *	  and so may point beyond the last valid sg entry for the
 *	  transfer.
 */ 
/*16*/	uint32_t dataptr;
/*20*/	uint32_t datacnt;		/*
					 * The highest address byte is
					 * really the 5th. byte in the
					 * dataptr.
					 */
/*24*/	uint32_t sgptr;
#define SG_PTR_MASK	0xFFFFFFF8
/*28*/	uint8_t  tag;			/* Index into our kernel SCB array.
					 * Also used as the tag for tagged I/O
					 */
/*29*/	uint8_t  scsirate;		/* Value for SCSIRATE register */
/*30*/	uint8_t  scsioffset;		/* Value for SCSIOFFSET register */
/*31*/	uint8_t  next;			/* Used for threading SCBs in the
					 * "Waiting for Selection" and
					 * "Disconnected SCB" lists down
					 * in the sequencer.
					 */
/*32*/	uint8_t  cdb32[32];		/*
					 * CDB storage for controllers
					 * supporting 64 byte SCBs.
					 */
};

struct scb {
	struct	hardware_scb	*hscb;
	union {
		SLIST_ENTRY(scb)	 sle;
		TAILQ_ENTRY(scb)	 tqe;
	} links;
	union ccb		*ccb;	 /* the ccb for this cmd */
	scb_flag		 flags;
	bus_dmamap_t		 dmamap;
	struct	ahc_dma_seg 	*sg_list;
	bus_addr_t		 sg_list_phys;
	bus_addr_t		 cdb32_busaddr;
	u_int			 sg_count;/* How full ahc_dma_seg is */
};

/*
 * Connection desciptor for select-in requests in target mode.
 * The first byte is the connecting target, followed by identify
 * message and optional tag information, terminated by 0xFF.  The
 * remainder is the command to execute.  The cmd_valid byte is on
 * an 8 byte boundary to simplify setting it on aic7880 hardware
 * which only has limited direct access to the DMA FIFO.
 */
struct target_cmd {
	uint8_t scsiid;
	uint8_t identify;	/* Identify message */
	uint8_t bytes[22];
	uint8_t cmd_valid;
	uint8_t pad[7];
};

/*
 * Number of events we can buffer up if we run out
 * of immediate notify ccbs.
 */
#define AHC_TMODE_EVENT_BUFFER_SIZE 8
struct ahc_tmode_event {
	uint8_t initiator_id;
	uint8_t event_type;	/* MSG type or EVENT_TYPE_BUS_RESET */
#define	EVENT_TYPE_BUS_RESET 0xFF
	uint8_t event_arg;
};

/*
 * Per lun target mode state including accept TIO CCB
 * and immediate notify CCB pools.
 */
struct tmode_lstate {
	struct cam_path *path;
	struct ccb_hdr_slist accept_tios;
	struct ccb_hdr_slist immed_notifies;
	struct ahc_tmode_event event_buffer[AHC_TMODE_EVENT_BUFFER_SIZE];
	uint8_t event_r_idx;
	uint8_t event_w_idx;
};

#define AHC_TRANS_CUR		0x01	/* Modify current neogtiation status */
#define AHC_TRANS_ACTIVE	0x03	/* Assume this is the active target */
#define AHC_TRANS_GOAL		0x04	/* Modify negotiation goal */
#define AHC_TRANS_USER		0x08	/* Modify user negotiation settings */

struct ahc_transinfo {
	uint8_t protocol_version;
	uint8_t transport_version;
	uint8_t width;
	uint8_t period;
	uint8_t offset;
	uint8_t ppr_options;
};

struct ahc_initiator_tinfo {
	uint8_t scsirate;
	struct ahc_transinfo current;
	struct ahc_transinfo goal;
	struct ahc_transinfo user;
};

/*
 * Per target mode enabled target state.  Esentially just an array of
 * pointers to lun target state as well as sync/wide negotiation information
 * for each initiator<->target mapping (including the mapping for when we
 * are the initiator).
 */
struct tmode_tstate {
	struct tmode_lstate*		enabled_luns[64];
	struct ahc_initiator_tinfo	transinfo[16];

	/*
	 * Per initiator state bitmasks.
	 */
	uint16_t		 ultraenb;	/* Using ultra sync rate  */
	uint16_t	 	 discenable;	/* Disconnection allowed  */
	uint16_t		 tagenable;	/* Tagged Queuing allowed */
};

/*
 * Define the format of the aic7XXX SEEPROM registers (16 bits).
 */

struct seeprom_config {
/*
 * SCSI ID Configuration Flags
 */
	uint16_t device_flags[16];	/* words 0-15 */
#define		CFXFER		0x0007	/* synchronous transfer rate */
#define		CFSYNCH		0x0008	/* enable synchronous transfer */
#define		CFDISC		0x0010	/* enable disconnection */
#define		CFWIDEB		0x0020	/* wide bus device */
#define		CFSYNCHISULTRA	0x0040	/* CFSYNCH is an ultra offset (2940AU)*/
#define		CFSYNCSINGLE	0x0080	/* Single-Transition signalling */
#define		CFSTART		0x0100	/* send start unit SCSI command */
#define		CFINCBIOS	0x0200	/* include in BIOS scan */
#define		CFRNFOUND	0x0400	/* report even if not found */
#define		CFMULTILUNDEV	0x0800	/* Probe multiple luns in BIOS scan */
#define		CFWBCACHEENB	0x4000	/* Enable W-Behind Cache on disks */
#define		CFWBCACHENOP	0xc000	/* Don't touch W-Behind Cache */

/*
 * BIOS Control Bits
 */
	uint16_t bios_control;		/* word 16 */
#define		CFSUPREM	0x0001	/* support all removeable drives */
#define		CFSUPREMB	0x0002	/* support removeable boot drives */
#define		CFBIOSEN	0x0004	/* BIOS enabled */
/*		UNUSED		0x0008	*/
#define		CFSM2DRV	0x0010	/* support more than two drives */
#define		CF284XEXTEND	0x0020	/* extended translation (284x cards) */	
#define		CFSTPWLEVEL	0x0010	/* Termination level control */
#define		CFEXTEND	0x0080	/* extended translation enabled */
#define		CFSCAMEN	0x0100	/* SCAM enable */
/*		UNUSED		0xff00	*/

/*
 * Host Adapter Control Bits
 */
	uint16_t adapter_control;	/* word 17 */	
#define		CFAUTOTERM	0x0001	/* Perform Auto termination */
#define		CFULTRAEN	0x0002	/* Ultra SCSI speed enable */
#define		CF284XSELTO     0x0003	/* Selection timeout (284x cards) */
#define		CF284XFIFO      0x000C	/* FIFO Threshold (284x cards) */
#define		CFSTERM		0x0004	/* SCSI low byte termination */
#define		CFWSTERM	0x0008	/* SCSI high byte termination */
#define		CFSPARITY	0x0010	/* SCSI parity */
#define		CF284XSTERM     0x0020	/* SCSI low byte term (284x cards) */	
#define		CFMULTILUN	0x0020	/* SCSI low byte term (284x cards) */	
#define		CFRESETB	0x0040	/* reset SCSI bus at boot */
#define		CFCLUSTERENB	0x0080	/* Cluster Enable */
#define		CFCHNLBPRIMARY	0x0100	/* aic7895 probe B channel first */
#define		CFSEAUTOTERM	0x0400	/* Ultra2 Perform secondary Auto Term*/
#define		CFSELOWTERM	0x0800	/* Ultra2 secondary low term */
#define		CFSEHIGHTERM	0x1000	/* Ultra2 secondary high term */
#define		CFDOMAINVAL	0x4000	/* Perform Domain Validation*/

/*
 * Bus Release, Host Adapter ID
 */
	uint16_t brtime_id;		/* word 18 */
#define		CFSCSIID	0x000f	/* host adapter SCSI ID */
/*		UNUSED		0x00f0	*/
#define		CFBRTIME	0xff00	/* bus release time */

/*
 * Maximum targets
 */
	uint16_t max_targets;		/* word 19 */	
#define		CFMAXTARG	0x00ff	/* maximum targets */
#define		CFBOOTLUN	0x0f00	/* Lun to boot from */
#define		CFBOOTID	0xf000	/* Target to boot from */
	uint16_t res_1[10];		/* words 20-29 */
	uint16_t signature;		/* Signature == 0x250 */
#define		CFSIGNATURE	0x250
	uint16_t checksum;		/* word 31 */
};

struct ahc_syncrate {
	u_int sxfr_u2;
	u_int sxfr;
	/* Rates in Ultra mode have bit 8 of sxfr set */
#define		ULTRA_SXFR 0x100
#define		ST_SXFR	   0x010	/* Rate Single Transition Only */
#define		DT_SXFR	   0x040	/* Rate Double Transition Only */
	uint8_t period; /* Period to send to SCSI target */
	char *rate;
};

typedef enum {
	MSG_TYPE_NONE			= 0x00,
	MSG_TYPE_INITIATOR_MSGOUT	= 0x01,
	MSG_TYPE_INITIATOR_MSGIN	= 0x02,
	MSG_TYPE_TARGET_MSGOUT		= 0x03,
	MSG_TYPE_TARGET_MSGIN		= 0x04
} ahc_msg_type;

struct sg_map_node {
	bus_dmamap_t		 sg_dmamap;
	bus_addr_t		 sg_physaddr;
	struct ahc_dma_seg*	 sg_vaddr;
	SLIST_ENTRY(sg_map_node) links;
};
	
struct scb_data {
	struct	hardware_scb	*hscbs;	    /* Array of hardware SCBs */
	struct	scb *scbarray;		    /* Array of kernel SCBs */
	SLIST_HEAD(, scb) free_scbs;	/*
					 * Pool of SCBs ready to be assigned
					 * commands to execute.
					 */
	struct	scsi_sense_data *sense; /* Per SCB sense data */

	/*
	 * "Bus" addresses of our data structures.
	 */
	bus_dma_tag_t	 hscb_dmat;	/* dmat for our hardware SCB array */
	bus_dmamap_t	 hscb_dmamap;
	bus_addr_t	 hscb_busaddr;
	bus_dma_tag_t	 sense_dmat;
	bus_dmamap_t	 sense_dmamap;
	bus_addr_t	 sense_busaddr;
	bus_dma_tag_t	 sg_dmat;	/* dmat for our sg segments */
	SLIST_HEAD(, sg_map_node) sg_maps;
	uint8_t	numscbs;
	uint8_t	maxhscbs;	/* Number of SCBs on the card */
	uint8_t	init_level;	/*
					 * How far we've initialized
					 * this structure.
					 */
};

TAILQ_HEAD(scb_tailq, scb);

struct ahc_softc {
	bus_space_tag_t		 tag;
	bus_space_handle_t	 bsh;
	bus_dma_tag_t		 buffer_dmat;	/* dmat for buffer I/O */
	struct scb_data		*scb_data;

	/*
	 * CCBs that have been send to the controller
	 */
	LIST_HEAD(, ccb_hdr)	 pending_ccbs;

	/*
	 * Counting lock for deferring the release of additional
	 * untagged transactions from the untagged_queues.  When
	 * the lock is decremented to 0, all queues in the
	 * untagged_queues array are run.
	 */
	u_int			 untagged_queue_lock;

	/*
	 * Per-target queue of untagged-transactions.  The
	 * transaction at the head of the queue is the
	 * currently pending untagged transaction for the
	 * target.  The driver only allows a single untagged
	 * transaction per target.
	 */
	struct scb_tailq	 untagged_queues[16];

	/*
	 * Target mode related state kept on a per enabled lun basis.
	 * Targets that are not enabled will have null entries.
	 * As an initiator, we keep one target entry for our initiator
	 * ID to store our sync/wide transfer settings.
	 */
	struct tmode_tstate*	 enabled_targets[16];

	/*
	 * The black hole device responsible for handling requests for
	 * disabled luns on enabled targets.
	 */
	struct tmode_lstate*	 black_hole;

	/*
	 * Device instance currently on the bus awaiting a continue TIO
	 * for a command that was not given the disconnect priveledge.
	 */
	struct tmode_lstate*	 pending_device;

	/*
	 * Card characteristics
	 */
	ahc_chip		 chip;
	ahc_feature		 features;
	ahc_bug			 bugs;
	ahc_flag		 flags;

	/* Values to store in the SEQCTL register for pause and unpause */
	uint8_t		 unpause;
	uint8_t		 pause;

	/* Command Queues */
	uint8_t		 qoutfifonext;
	uint8_t		 qinfifonext;
	uint8_t		*qoutfifo;
	uint8_t		*qinfifo;

	/*
	 * Hooks into the XPT.
	 */
	struct	cam_sim		*sim;
	struct	cam_sim		*sim_b;
	struct	cam_path	*path;
	struct	cam_path	*path_b;

	int			 unit;

	/* Channel Names ('A', 'B', etc.) */
	char			 channel;
	char			 channel_b;

	/* Initiator Bus ID */
	uint8_t		 our_id;
	uint8_t		 our_id_b;

	/* Targets that need negotiation messages */
	uint16_t		 targ_msg_req;

	/*
	 * PCI error detection and data for running the
	 * PCI error interrupt handler.
	 */
	int			 unsolicited_ints;
	device_t		 device;

	/*
	 * Target incoming command FIFO.
	 */
	struct target_cmd	*targetcmds;
	uint8_t		 tqinfifonext;

	/*
	 * Incoming and outgoing message handling.
	 */
	uint8_t		 send_msg_perror;
	ahc_msg_type		 msg_type;
	uint8_t		 msgout_buf[8];	/* Message we are sending */
	uint8_t		 msgin_buf[8];	/* Message we are receiving */
	u_int			 msgout_len;	/* Length of message to send */
	u_int			 msgout_index;	/* Current index in msgout */
	u_int			 msgin_index;	/* Current index in msgin */

	int			 regs_res_type;
	int			 regs_res_id;
	int			 irq_res_type;
	struct resource		*regs;
	struct resource		*irq;
	void			*ih;
	bus_dma_tag_t		 parent_dmat;
	bus_dma_tag_t		 shared_data_dmat;
	bus_dmamap_t		 shared_data_dmamap;
	bus_addr_t		 shared_data_busaddr;
	bus_addr_t		 dma_bug_buf;

	/* Number of enabled target mode device on this card */
	u_int			 enabled_luns;

	/* Initialization level of this data structure */
	u_int			 init_level;

	uint16_t	 	 user_discenable;/* Disconnection allowed  */
	uint16_t		 user_tagenable;/* Tagged Queuing allowed */
};

struct full_ahc_softc {
	struct ahc_softc softc;
	struct scb_data  scb_data_storage;
};

/* #define AHC_DEBUG */
#ifdef AHC_DEBUG
/* Different debugging levels used when AHC_DEBUG is defined */
#define AHC_SHOWMISC	0x0001
#define AHC_SHOWCMDS	0x0002
#define AHC_SHOWSCBS	0x0004
#define AHC_SHOWABORTS	0x0008
#define AHC_SHOWSENSE	0x0010
#define AHC_SHOWSCBCNT	0x0020

extern int ahc_debug; /* Initialized in i386/scsi/aic7xxx.c */
#endif

#define ahc_inb(ahc, port)				\
	bus_space_read_1((ahc)->tag, (ahc)->bsh, port)

#define ahc_outb(ahc, port, value)			\
	bus_space_write_1((ahc)->tag, (ahc)->bsh, port, value)

#define ahc_outsb(ahc, port, valp, count)		\
	bus_space_write_multi_1((ahc)->tag, (ahc)->bsh, port, valp, count)

#define ahc_insb(ahc, port, valp, count)		\
	bus_space_read_multi_1((ahc)->tag, (ahc)->bsh, port, valp, count)

char *ahc_name(struct ahc_softc *ahc);

void	ahc_init_probe_config(struct ahc_probe_config *config);
struct ahc_softc*
	ahc_alloc(device_t dev, struct resource *regs, int regs_type,
		  int regs_id, bus_dma_tag_t parent_dmat, 
		  struct ahc_probe_config *config, struct scb_data *scb_data);
int	ahc_reset(struct ahc_softc *ahc);
void	ahc_free(struct ahc_softc *);
int	ahc_probe_scbs(struct ahc_softc *);
int	ahc_init(struct ahc_softc *);
int	ahc_attach(struct ahc_softc *);
void	ahc_intr(void *arg);
static __inline int  sequencer_paused(struct ahc_softc *ahc);
static __inline void ahc_pause_bug_fix(struct ahc_softc *ahc);
static __inline void pause_sequencer(struct ahc_softc *ahc);
static __inline void unpause_sequencer(struct ahc_softc *ahc);

static __inline void
ahc_pause_bug_fix(struct ahc_softc *ahc)
{
	/*
	 * Clear the CIOBUS stretch signal by reading a register that will
	 * set this signal and deassert it.  Without this workaround, if
	 * the chip is paused, by an interrupt or manual pause, while
	 * accessing scb ram, then accesses to certain registers will hang
	 * the system (infinite pci retries).
	 */
	if ((ahc->features & AHC_ULTRA2) != 0)
		(void)ahc_inb(ahc, CCSCBCTL);
}

static __inline int
sequencer_paused(struct ahc_softc *ahc)
{
	return ((ahc_inb(ahc, HCNTRL) & PAUSE) != 0);
}

static __inline void
pause_sequencer(struct ahc_softc *ahc)
{
	ahc_outb(ahc, HCNTRL, ahc->pause);

	/*
	 * Since the sequencer can disable pausing in a critical section, we
	 * must loop until it actually stops.
	 */
	while (sequencer_paused(ahc) == 0)
		;

	ahc_pause_bug_fix(ahc);
}

static __inline void
unpause_sequencer(struct ahc_softc *ahc)
{
	if ((ahc_inb(ahc, INTSTAT) & (SCSIINT | SEQINT | BRKADRINT)) == 0)
		ahc_outb(ahc, HCNTRL, ahc->unpause);
}

#endif  /* _AIC7XXX_H_ */
