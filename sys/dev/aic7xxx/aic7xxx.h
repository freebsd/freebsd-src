/*
 * Interface to the generic driver for the aic7xxx based adaptec
 * SCSI controllers.  This is used to implement product specific
 * probe and attach routines.
 *
 * Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999 Justin T. Gibbs.
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
 * Where this Software is combined with software released under the terms of 
 * the GNU Public License ("GPL") and the terms of the GPL would require the 
 * combined work to also be released under the terms of the GPL, the terms
 * and conditions of this License will apply in addition to those of the
 * GPL with the exception of any terms or conditions of this License that
 * conflict with, or are expressly prohibited by, the GPL.
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
 *	$Id: aic7xxx.h,v 1.5.2.1 1999/03/07 00:40:47 gibbs Exp $
 */

#ifndef _AIC7XXX_H_
#define _AIC7XXX_H_

#include "ahc.h"                /* for NAHC from config */
#include "opt_aic7xxx.h"	/* for config options */

#include <pci/pcivar.h>		/* for pcici_t */

#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif

/*
 * The maximum transfer per S/G segment.
 */
#define AHC_MAXTRANSFER_SIZE	 0x00ffffff	/* limited by 24bit counter */

/*
 * The number of dma segments supported.  The current implementation limits
 * us to 255 S/G entries (this may change to be unlimited at some point).
 * To reduce the driver's memory consumption, we further limit the number
 * supported to be sufficient to handle the largest mapping supported by
 * the kernel, MAXPHYS.  Assuming the transfer is as fragmented as possible
 * and unaligned, this turns out to be the number of paged sized transfers
 * in MAXPHYS plus an extra element to handle any unaligned residual.
 */
#define AHC_NSEG (MIN(btoc(MAXPHYS) + 1, 255))

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

extern u_long ahc_unit;

struct ahc_dma_seg {
	u_int32_t	addr;
	u_int32_t	len;
};

typedef enum {
	AHC_NONE	= 0x0000,
	AHC_CHIPID_MASK	= 0x00FF,
	AHC_AIC7770	= 0x0001,
	AHC_AIC7850	= 0x0002,
	AHC_AIC7860	= 0x0003,
	AHC_AIC7870	= 0x0004,
	AHC_AIC7880	= 0x0005,
	AHC_AIC7890	= 0x0006,
	AHC_AIC7895	= 0x0007,
	AHC_AIC7896	= 0x0008,
	AHC_VL		= 0x0100,	/* Bus type VL */
	AHC_EISA	= 0x0200,	/* Bus type EISA */
	AHC_PCI		= 0x0400,	/* Bus type PCI */
} ahc_chip;

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
	AHC_AIC7770_FE	= AHC_FENONE,	
	AHC_AIC7850_FE	= AHC_FENONE|AHC_SPIOCAP,
	AHC_AIC7860_FE	= AHC_ULTRA|AHC_SPIOCAP,
	AHC_AIC7870_FE	= AHC_FENONE,
	AHC_AIC7880_FE	= AHC_ULTRA,
	AHC_AIC7890_FE	= AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA2|AHC_QUEUE_REGS
			  |AHC_SG_PRELOAD|AHC_MULTI_TID|AHC_HS_MAILBOX,
	AHC_AIC7895_FE	= AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA,
	AHC_AIC7896_FE	= AHC_MORE_SRAM|AHC_CMD_CHAN|AHC_ULTRA2|AHC_QUEUE_REGS
			  |AHC_SG_PRELOAD|AHC_MULTI_TID|AHC_HS_MAILBOX,
} ahc_feature;

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
} ahc_flag;

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
	SCB_ABORT		= 0x1000,
	SCB_QUEUED_MSG		= 0x2000,
	SCB_ACTIVE		= 0x4000,
	SCB_TARGET_IMMEDIATE	= 0x8000
} scb_flag;

/*
 * The driver keeps up to MAX_SCB scb structures per card in memory.  The SCB
 * consists of a "hardware SCB" mirroring the fields availible on the card
 * and additional information the kernel stores for each transaction.
 */
struct hardware_scb {
/*0*/   u_int8_t  control;
/*1*/	u_int8_t  tcl;		/* 4/1/3 bits */
/*2*/	u_int8_t  status;
/*3*/	u_int8_t  SG_count;
/*4*/	u_int32_t SG_pointer;
/*8*/	u_int8_t  residual_SG_count;
/*9*/	u_int8_t  residual_data_count[3];
/*12*/	u_int32_t data;
/*16*/	u_int32_t datalen;		/* Really only three bytes, but its
					 * faster to treat it as a long on
					 * a quad boundary.
					 */
/*20*/	u_int32_t cmdpointer;
/*24*/	u_int8_t  cmdlen;
/*25*/	u_int8_t  tag;			/* Index into our kernel SCB array.
					 * Also used as the tag for tagged I/O
					 */
/*26*/	u_int8_t  next;			/* Used for threading SCBs in the
					 * "Waiting for Selection" and
					 * "Disconnected SCB" lists down
					 * in the sequencer.
					 */
/*27*/	u_int8_t  scsirate;		/* Value for SCSIRATE register */
/*28*/	u_int8_t  scsioffset;		/* Value for SCSIOFFSET register */
/*29*/	u_int8_t  spare[3];		/*
					 * Spare space available on
					 * all controller types.
					 */
/*32*/	u_int8_t  cmdstore[16];		/*
					 * CDB storage for controllers
					 * supporting 64 byte SCBs.
					 */
/*48*/	u_int32_t cmdstore_busaddr;	/*
					 * Address of command store for
					 * 32byte SCB adapters
					 */
/*48*/	u_int8_t  spare_64[12];	/*
					 * Pad to 64 bytes.
					 */
};

struct scb {
	struct	hardware_scb	*hscb;
	SLIST_ENTRY(scb)	 links;	 /* for chaining */
	union ccb		*ccb;	 /* the ccb for this cmd */
	scb_flag		 flags;
	bus_dmamap_t		 dmamap;
	struct	ahc_dma_seg 	*sg_list;
	bus_addr_t		 sg_list_phys;
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
	u_int8_t initiator_channel;
	u_int8_t targ_id;	/* Target ID we were selected at */
	u_int8_t identify;	/* Identify message */
	u_int8_t bytes[21];
	u_int8_t cmd_valid;
	u_int8_t pad[7];
};

/*
 * Per lun target mode state including accept TIO CCB
 * and immediate notify CCB pools.
 */
struct tmode_lstate {
	struct ccb_hdr_slist accept_tios;
	struct ccb_hdr_slist immed_notifies;
};

#define AHC_TRANS_CUR		0x01	/* Modify current neogtiation status */
#define AHC_TRANS_ACTIVE	0x03	/* Assume this is the active target */
#define AHC_TRANS_GOAL		0x04	/* Modify negotiation goal */
#define AHC_TRANS_USER		0x08	/* Modify user negotiation settings */

struct ahc_transinfo {
	u_int8_t width;
	u_int8_t period;
	u_int8_t offset;
};

struct ahc_initiator_tinfo {
	u_int8_t scsirate;
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
	struct tmode_lstate*		enabled_luns[8];
	struct ahc_initiator_tinfo	transinfo[16];

	/*
	 * Per initiator state bitmasks.
	 */
	u_int16_t		 ultraenb;	/* Using ultra sync rate  */
	u_int16_t	 	 discenable;	/* Disconnection allowed  */
	u_int16_t		 tagenable;	/* Tagged Queuing allowed */
};

/*
 * Define the format of the aic7XX0 SEEPROM registers (16 bits).
 */

struct seeprom_config {
/*
 * SCSI ID Configuration Flags
 */
	u_int16_t device_flags[16];	/* words 0-15 */
#define		CFXFER		0x0007	/* synchronous transfer rate */
#define		CFSYNCH		0x0008	/* enable synchronous transfer */
#define		CFDISC		0x0010	/* enable disconnection */
#define		CFWIDEB		0x0020	/* wide bus device */
#define		CFSYNCHISULTRA	0x0040	/* CFSYNCH is an ultra offset (2940AU)*/
/*		UNUSED		0x0080	*/
#define		CFSTART		0x0100	/* send start unit SCSI command */
#define		CFINCBIOS	0x0200	/* include in BIOS scan */
#define		CFRNFOUND	0x0400	/* report even if not found */
#define		CFMULTILUN	0x0800	/* Probe multiple luns in BIOS scan */
/*		UNUSED		0xf000	*/

/*
 * BIOS Control Bits
 */
	u_int16_t bios_control;		/* word 16 */
#define		CFSUPREM	0x0001	/* support all removeable drives */
#define		CFSUPREMB	0x0002	/* support removeable drives for boot only */
#define		CFBIOSEN	0x0004	/* BIOS enabled */
/*		UNUSED		0x0008	*/
#define		CFSM2DRV	0x0010	/* support more than two drives */
#define		CF284XEXTEND	0x0020	/* extended translation (284x cards) */	
/*		UNUSED		0x0060	*/
#define		CFEXTEND	0x0080	/* extended translation enabled */
/*		UNUSED		0xff00	*/

/*
 * Host Adapter Control Bits
 */
	u_int16_t adapter_control;	/* word 17 */	
#define		CFAUTOTERM	0x0001	/* Perform Auto termination */
#define		CFULTRAEN	0x0002	/* Ultra SCSI speed enable */
#define		CF284XSELTO     0x0003	/* Selection timeout (284x cards) */
#define		CF284XFIFO      0x000C	/* FIFO Threshold (284x cards) */
#define		CFSTERM		0x0004	/* SCSI low byte termination */
#define		CFWSTERM	0x0008	/* SCSI high byte termination */
#define		CFSPARITY	0x0010	/* SCSI parity */
#define		CF284XSTERM     0x0020	/* SCSI low byte term (284x cards) */	
#define		CFRESETB	0x0040	/* reset SCSI bus at boot */
#define		CFCHNLBPRIMARY	0x0100	/* aic7895 probe B channel first */
#define		CFSEAUTOTERM	0x0400	/* aic7890 Perform SE Auto Termination*/
#define		CFLVDSTERM	0x0800	/* aic7890 LVD Termination */
/*		UNUSED		0xf080	*/

/*
 * Bus Release, Host Adapter ID
 */
	u_int16_t brtime_id;		/* word 18 */
#define		CFSCSIID	0x000f	/* host adapter SCSI ID */
/*		UNUSED		0x00f0	*/
#define		CFBRTIME	0xff00	/* bus release time */

/*
 * Maximum targets
 */
	u_int16_t max_targets;		/* word 19 */	
#define		CFMAXTARG	0x00ff	/* maximum targets */
/*		UNUSED		0xff00	*/
	u_int16_t res_1[11];		/* words 20-30 */
	u_int16_t checksum;		/* word 31 */
};

struct ahc_syncrate {
	int sxfr_ultra2;
	int sxfr;
	/* Rates in Ultra mode have bit 8 of sxfr set */
#define		ULTRA_SXFR 0x100
	u_int8_t period; /* Period to send to SCSI target */
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
	u_int8_t	numscbs;
	u_int8_t	maxhscbs;	/* Number of SCBs on the card */
	u_int8_t	init_level;	/*
					 * How far we've initialized
					 * this structure.
					 */
};

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
	ahc_flag		 flags;

	/* Values to store in the SEQCTL register for pause and unpause */
	u_int8_t		 unpause;
	u_int8_t		 pause;

	/* Command Queues */
	u_int8_t		 qoutfifonext;
	u_int8_t		 qinfifonext;
	u_int8_t		*qoutfifo;
	u_int8_t		*qinfifo;

	/*
	 * 256 byte array storing the SCBID of outstanding
	 * untagged SCBs indexed by TCL.
	 */	
	u_int8_t		 *untagged_scbs;

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
	u_int8_t		 our_id;
	u_int8_t		 our_id_b;

	/* Targets that need negotiation messages */
	u_int16_t		 targ_msg_req;

	/*
	 * PCI error detection and data for running the
	 * PCI error interrupt handler.
	 */
	int			 unsolicited_ints;
	pcici_t			 pci_config_id;

	/*
	 * Target incoming command FIFO.
	 */
	struct target_cmd	*targetcmds;
	u_int8_t		 tqinfifonext;

	/*
	 * Incoming and outgoing message handling.
	 */
	u_int8_t		 send_msg_perror;
	ahc_msg_type		 msg_type;
	u_int8_t		 msgout_buf[8];	/* Message we are sending */
	u_int8_t		 msgin_buf[8];	/* Message we are receiving */
	u_int			 msgout_len;	/* Length of message to send */
	u_int			 msgout_index;	/* Current index in msgout */
	u_int			 msgin_index;	/* Current index in msgin */

	bus_dma_tag_t		 parent_dmat;
	bus_dma_tag_t		 shared_data_dmat;
	bus_dmamap_t		 shared_data_dmamap;
	bus_addr_t		 shared_data_busaddr;

	/* Number of enabled target mode device on this card */
	u_int			 enabled_luns;

	/* Initialization level of this data structure */
	u_int			 init_level;
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

char *ahc_name(struct ahc_softc *ahc);

struct ahc_softc*
	ahc_alloc(int unit, u_int32_t io_base, vm_offset_t maddr, 
		  bus_dma_tag_t parent_dmat, ahc_chip chip,
		  ahc_feature features, ahc_flag flags,
		  struct scb_data *scb_data);
int	ahc_reset(struct ahc_softc *ahc);
void	ahc_free(struct ahc_softc *);
int	ahc_probe_scbs(struct ahc_softc *);
int	ahc_init(struct ahc_softc *);
int	ahc_attach(struct ahc_softc *);
void	ahc_intr(void *arg);

#define ahc_inb(ahc, port)				\
	bus_space_read_1((ahc)->tag, (ahc)->bsh, port)

#define ahc_outb(ahc, port, value)			\
	bus_space_write_1((ahc)->tag, (ahc)->bsh, port, value)

#define ahc_outsb(ahc, port, valp, count)		\
	bus_space_write_multi_1((ahc)->tag, (ahc)->bsh, port, valp, count)

#endif  /* _AIC7XXX_H_ */
