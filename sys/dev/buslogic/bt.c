/*
 * Generic driver for the BusLogic MultiMaster SCSI host adapters
 * Product specific probe and attach routines can be found in:
 * i386/isa/bt_isa.c	BT-54X, BT-445 cards
 * i386/eisa/bt_eisa.c	BT-74x, BT-75x cards
 * pci/bt_pci.c		BT-946, BT-948, BT-956, BT-958 cards
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
 *      $Id$
 */

 /*
  * Special thanks to Leonard N. Zubkoff for writing such a complete and
  * well documented Mylex/BusLogic MultiMaster driver for Linux.  Support
  * in this driver for the wide range of MultiMaster controllers and
  * firmware revisions, with their otherwise undocumented quirks, would not
  * have been possible without his efforts.
  */

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
 
/*
 * XXX It appears that BusLogic PCI adapters go out to lunch if you 
 *     attempt to perform memory mapped I/O.
 */
#if 0
#include "pci.h"
#if NPCI > 0
#include <machine/bus_memio.h>
#endif
#endif
#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/clock.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_message.h>

#include <vm/vm.h>
#include <vm/pmap.h>
 
#include <dev/buslogic/btreg.h>

struct bt_softc *bt_softcs[NBT];

/* MailBox Management functions */
static __inline void	btnextinbox(struct bt_softc *bt);
static __inline void	btnextoutbox(struct bt_softc *bt);

static __inline void
btnextinbox(struct bt_softc *bt)
{
	if (bt->cur_inbox == bt->last_inbox)
		bt->cur_inbox = bt->in_boxes;
	else
		bt->cur_inbox++;
}

static __inline void
btnextoutbox(struct bt_softc *bt)
{
	if (bt->cur_outbox == bt->last_outbox)
		bt->cur_outbox = bt->out_boxes;
	else
		bt->cur_outbox++;
}

/* CCB Mangement functions */
static __inline u_int32_t		btccbvtop(struct bt_softc *bt,
						  struct bt_ccb *bccb);
static __inline struct bt_ccb*		btccbptov(struct bt_softc *bt,
						  u_int32_t ccb_addr);
static __inline u_int32_t		btsensepaddr(struct bt_softc *bt,
						     struct bt_ccb *bccb);
static __inline struct scsi_sense_data* btsensevaddr(struct bt_softc *bt,
						     struct bt_ccb *bccb);

static __inline u_int32_t
btccbvtop(struct bt_softc *bt, struct bt_ccb *bccb)
{
	return (bt->bt_ccb_physbase
	      + (u_int32_t)((caddr_t)bccb - (caddr_t)bt->bt_ccb_array));
}

static __inline struct bt_ccb *
btccbptov(struct bt_softc *bt, u_int32_t ccb_addr)
{
	return (bt->bt_ccb_array +
	        ((struct bt_ccb*)ccb_addr-(struct bt_ccb*)bt->bt_ccb_physbase));
}

static __inline u_int32_t
btsensepaddr(struct bt_softc *bt, struct bt_ccb *bccb)
{
	u_int index;

	index = (u_int)(bccb - bt->bt_ccb_array);
	return (bt->sense_buffers_physbase
		+ (index * sizeof(struct scsi_sense_data)));
}

static __inline struct scsi_sense_data *
btsensevaddr(struct bt_softc *bt, struct bt_ccb *bccb)
{
	u_int index;

	index = (u_int)(bccb - bt->bt_ccb_array);
	return (bt->sense_buffers + index);
}

static __inline struct bt_ccb*	btgetccb(struct bt_softc *bt);
static __inline void		btfreeccb(struct bt_softc *bt,
					  struct bt_ccb *bccb);
static void		btallocccbs(struct bt_softc *bt);
static bus_dmamap_callback_t btexecuteccb;
static void		btdone(struct bt_softc *bt, struct bt_ccb *bccb,
			       bt_mbi_comp_code_t comp_code);

/* Host adapter command functions */
static int	btreset(struct bt_softc* bt, int hard_reset);

/* Initialization functions */
static int			btinitmboxes(struct bt_softc *bt);
static bus_dmamap_callback_t	btmapmboxes;
static bus_dmamap_callback_t	btmapccbs;
static bus_dmamap_callback_t	btmapsgs;

/* Transfer Negotiation Functions */
static void btfetchtransinfo(struct bt_softc *bt,
			     struct ccb_trans_settings *cts);

/* CAM SIM entry points */
#define ccb_bccb_ptr spriv_ptr0
#define ccb_bt_ptr spriv_ptr1
static void	btaction(struct cam_sim *sim, union ccb *ccb);
static void	btpoll(struct cam_sim *sim);

/* Our timeout handler */
timeout_t bttimeout;

u_long bt_unit = 0;

/*
 * XXX
 * Do our own re-probe protection until a configuration
 * manager can do it for us.  This ensures that we don't
 * reprobe a card already found by the EISA or PCI probes.
 */
struct bt_isa_port bt_isa_ports[] =
{
	{ 0x330, 0 },
	{ 0x334, 0 },
	{ 0x230, 0 },
	{ 0x234, 0 },
	{ 0x130, 0 },
	{ 0x134, 0 }
};

/* Exported functions */
struct bt_softc *
bt_alloc(int unit, bus_space_tag_t tag, bus_space_handle_t bsh)
{
	struct  bt_softc *bt;  
	int	i;

	if (unit != BT_TEMP_UNIT) {
		if (unit >= NBT) {
			printf("bt: unit number (%d) too high\n", unit);
			return NULL;
		}

		/*
		 * Allocate a storage area for us
		 */
		if (bt_softcs[unit]) {    
			printf("bt%d: memory already allocated\n", unit);
			return NULL;    
		}
	}

	bt = malloc(sizeof(struct bt_softc), M_DEVBUF, M_NOWAIT);
	if (!bt) {
		printf("bt%d: cannot malloc!\n", unit);
		return NULL;    
	}
	bzero(bt, sizeof(struct bt_softc));
	SLIST_INIT(&bt->free_bt_ccbs);
	LIST_INIT(&bt->pending_ccbs);
	SLIST_INIT(&bt->sg_maps);
	bt->unit = unit;
	bt->tag = tag;
	bt->bsh = bsh;

	if (bt->unit != BT_TEMP_UNIT) {
		bt_softcs[unit] = bt;
	}
	return (bt);
}

void
bt_free(struct bt_softc *bt)
{
	switch (bt->init_level) {
	default:
	case 11:
		bus_dmamap_unload(bt->sense_dmat, bt->sense_dmamap);
	case 10:
		bus_dmamem_free(bt->sense_dmat, bt->sense_buffers,
				bt->sense_dmamap);
	case 9:
		bus_dma_tag_destroy(bt->sense_dmat);
	case 8:
	{
		struct sg_map_node *sg_map;

		while ((sg_map = SLIST_FIRST(&bt->sg_maps))!= NULL) {
			SLIST_REMOVE_HEAD(&bt->sg_maps, links);
			bus_dmamap_unload(bt->sg_dmat,
					  sg_map->sg_dmamap);
			bus_dmamem_free(bt->sg_dmat, sg_map->sg_vaddr,
					sg_map->sg_dmamap);
			free(sg_map, M_DEVBUF);
		}
		bus_dma_tag_destroy(bt->sg_dmat);
	}
	case 7:
		bus_dmamap_unload(bt->ccb_dmat, bt->ccb_dmamap);
	case 6:
		bus_dmamem_free(bt->ccb_dmat, bt->bt_ccb_array,
				bt->ccb_dmamap);
		bus_dmamap_destroy(bt->ccb_dmat, bt->ccb_dmamap);
	case 5:
		bus_dma_tag_destroy(bt->ccb_dmat);
	case 4:
		bus_dmamap_unload(bt->mailbox_dmat, bt->mailbox_dmamap);
	case 3:
		bus_dmamem_free(bt->mailbox_dmat, bt->in_boxes,
				bt->mailbox_dmamap);
		bus_dmamap_destroy(bt->mailbox_dmat, bt->mailbox_dmamap);
	case 2:
		bus_dma_tag_destroy(bt->buffer_dmat);
	case 1:
		bus_dma_tag_destroy(bt->mailbox_dmat);
	case 0:
		break;
	}
	if (bt->unit != BT_TEMP_UNIT) {
		bt_softcs[bt->unit] = NULL;
	}
	free(bt, M_DEVBUF);
}

/*
 * Probe the adapter and verify that the card is a BusLogic.
 */
int
bt_probe(struct bt_softc* bt)
{
	esetup_info_data_t esetup_info;
	u_int	 status;
	u_int	 intstat;
	u_int	 geometry;
	int	 error;
	u_int8_t param;

	/*
	 * See if the three I/O ports look reasonable.
	 * Touch the minimal number of registers in the
	 * failure case.
	 */
	status = bt_inb(bt, STATUS_REG);
	if ((status == 0)
	 || (status & (DIAG_ACTIVE|CMD_REG_BUSY|
		       STATUS_REG_RSVD|CMD_INVALID)) != 0) {
		if (bootverbose)
			printf("%s: Failed Status Reg Test - %x\n", bt_name(bt),
			       status);
		return (ENXIO);
	}

	intstat = bt_inb(bt, INTSTAT_REG);
	if ((intstat & INTSTAT_REG_RSVD) != 0) {
		printf("%s: Failed Intstat Reg Test\n", bt_name(bt));
		return (ENXIO);
	}

	geometry = bt_inb(bt, GEOMETRY_REG);
	if (geometry == 0xFF) {
		if (bootverbose)
			printf("%s: Failed Geometry Reg Test\n", bt_name(bt));
		return (ENXIO);
	}

	/*
	 * Looking good so far.  Final test is to reset the
	 * adapter and attempt to fetch the extended setup
	 * information.  This should filter out all 1542 cards.
	 */
	if ((error = btreset(bt, /*hard_reset*/TRUE)) != 0) {
		if (bootverbose)
			printf("%s: Failed Reset\n", bt_name(bt));
		return (ENXIO);
	}
	
	param = sizeof(esetup_info);
	error = bt_cmd(bt, BOP_INQUIRE_ESETUP_INFO, &param, /*parmlen*/1,
		       (u_int8_t*)&esetup_info, sizeof(esetup_info),
		       DEFAULT_CMD_TIMEOUT);
	if (error != 0) {
		return (ENXIO);
	}

	return (0);
}

/*
 * Pull the boards setup information and record it in our softc.
 */
int
bt_fetch_adapter_info(struct bt_softc *bt)
{
	board_id_data_t	board_id;
	esetup_info_data_t esetup_info;
	config_data_t config_data;
	int	 error;
	u_int8_t length_param;

	/* First record the firmware version */
	error = bt_cmd(bt, BOP_INQUIRE_BOARD_ID, NULL, /*parmlen*/0,
		       (u_int8_t*)&board_id, sizeof(board_id),
		       DEFAULT_CMD_TIMEOUT);
	if (error != 0) {
		printf("%s: bt_fetch_adapter_info - Failed Get Board Info\n",
		       bt_name(bt));
		return (error);
	}
	bt->firmware_ver[0] = board_id.firmware_rev_major;
	bt->firmware_ver[1] = '.';
	bt->firmware_ver[2] = board_id.firmware_rev_minor;
	bt->firmware_ver[3] = '\0';
		
	/*
	 * Depending on the firmware major and minor version,
	 * we may be able to fetch additional minor version info.
	 */
	if (bt->firmware_ver[0] > '0') {
		
		error = bt_cmd(bt, BOP_INQUIRE_FW_VER_3DIG, NULL, /*parmlen*/0,
			       (u_int8_t*)&bt->firmware_ver[3], 1,
			       DEFAULT_CMD_TIMEOUT);
		if (error != 0) {
			printf("%s: bt_fetch_adapter_info - Failed Get "
			       "Firmware 3rd Digit\n", bt_name(bt));
			return (error);
		}
		if (bt->firmware_ver[3] == ' ')
			bt->firmware_ver[3] = '\0';
		bt->firmware_ver[4] = '\0';
	}

	if (strcmp(bt->firmware_ver, "3.3") >= 0) {

		error = bt_cmd(bt, BOP_INQUIRE_FW_VER_4DIG, NULL, /*parmlen*/0,
			       (u_int8_t*)&bt->firmware_ver[4], 1,
			       DEFAULT_CMD_TIMEOUT);
		if (error != 0) {
			printf("%s: bt_fetch_adapter_info - Failed Get "
			       "Firmware 4th Digit\n", bt_name(bt));
			return (error);
		}
		if (bt->firmware_ver[4] == ' ')
			bt->firmware_ver[4] = '\0';
		bt->firmware_ver[5] = '\0';
	}

	/*
	 * Some boards do not handle the "recently documented"
	 * Inquire Board Model Number command correctly or do not give
	 * exact information.  Use the Firmware and Extended Setup
	 * information in these cases to come up with the right answer.
	 * The major firmware revision number indicates:
	 *
	 * 	5.xx	BusLogic "W" Series Host Adapters:
	 *		BT-948/958/958D
	 *	4.xx	BusLogic "C" Series Host Adapters:
	 *		BT-946C/956C/956CD/747C/757C/757CD/445C/545C/540CF
	 *	3.xx	BusLogic "S" Series Host Adapters:
	 *		BT-747S/747D/757S/757D/445S/545S/542D
	 *		BT-542B/742A (revision H)
	 *	2.xx	BusLogic "A" Series Host Adapters:
	 *		BT-542B/742A (revision G and below)
	 *	0.xx	AMI FastDisk VLB/EISA BusLogic Clone Host Adapter
	 */
	length_param = sizeof(esetup_info);
	error = bt_cmd(bt, BOP_INQUIRE_ESETUP_INFO, &length_param, /*parmlen*/1,
		       (u_int8_t*)&esetup_info, sizeof(esetup_info),
		       DEFAULT_CMD_TIMEOUT);
	if (error != 0) {
		return (error);
	}
	
  	bt->bios_addr = esetup_info.bios_addr << 12;

	if (esetup_info.bus_type == 'A'
	 && bt->firmware_ver[0] == '2') {
		strcpy(bt->model, "542B");
	} else if (esetup_info.bus_type == 'E'
		&& (strncmp(bt->firmware_ver, "2.1", 3) == 0
		 || strncmp(bt->firmware_ver, "2.20", 4) == 0)) {
		strcpy(bt->model, "742A");
	} else if (esetup_info.bus_type == 'E'
		&& bt->firmware_ver[0] == '0') {
		/* AMI FastDisk EISA Series 441 0.x */
		strcpy(bt->model, "747A");
	} else {
		ha_model_data_t model_data;
		int i;

		length_param = sizeof(model_data);
		error = bt_cmd(bt, BOP_INQUIRE_MODEL, &length_param, 1,
			       (u_int8_t*)&model_data, sizeof(model_data),
			       DEFAULT_CMD_TIMEOUT);
		if (error != 0) {
			printf("%s: bt_fetch_adapter_info - Failed Inquire "
			       "Model Number\n", bt_name(bt));
			return (error);
		}
		for (i = 0; i < sizeof(model_data.ascii_model); i++) {
			bt->model[i] = model_data.ascii_model[i];
			if (bt->model[i] == ' ')
				break;
		}
		bt->model[i] = '\0';
	}

	/* SG element limits */
	bt->max_sg = esetup_info.max_sg;

	/* Set feature flags */
	bt->wide_bus = esetup_info.wide_bus;
	bt->diff_bus = esetup_info.diff_bus;
	bt->ultra_scsi = esetup_info.ultra_scsi;

	if ((bt->firmware_ver[0] == '5')
	 || (bt->firmware_ver[0] == '4' && bt->wide_bus))
		bt->extended_lun = TRUE;

	bt->strict_rr = (strcmp(bt->firmware_ver, "3.31") >= 0);

	bt->extended_trans =
	    ((bt_inb(bt, GEOMETRY_REG) & EXTENDED_TRANSLATION) != 0);

	/*
	 * Determine max CCB count and whether tagged queuing is
	 * available based on controller type. Tagged queuing
	 * only works on 'W' series adapters, 'C' series adapters
	 * with firmware of rev 4.42 and higher, and 'S' series
	 * adapters with firmware of rev 3.35 and higher.  The
	 * maximum CCB counts are as follows:
	 *
	 *	192	BT-948/958/958D
	 *	100	BT-946C/956C/956CD/747C/757C/757CD/445C
	 * 	50	BT-545C/540CF
	 * 	30	BT-747S/747D/757S/757D/445S/545S/542D/542B/742A
	 */
	if (bt->firmware_ver[0] == '5') {
		bt->max_ccbs = 192;
		bt->tag_capable = TRUE;
	} else if (bt->firmware_ver[0] == '4') {
		if (bt->model[0] == '5')
			bt->max_ccbs = 50;
		else
			bt->max_ccbs = 100;
		bt->tag_capable = (strcmp(bt->firmware_ver, "4.22") >= 0);
	} else {
		bt->max_ccbs = 30;
		if (bt->firmware_ver[0] == '3'
		 && (strcmp(bt->firmware_ver, "3.35") >= 0))
			bt->tag_capable = TRUE;
		else
			bt->tag_capable = FALSE;
	}

	if (bt->tag_capable != FALSE)
		bt->tags_permitted = ALL_TARGETS;

	/* Determine Sync/Wide/Disc settings */
	if (bt->firmware_ver[0] >= '4') {
		auto_scsi_data_t auto_scsi_data;
		fetch_lram_params_t fetch_lram_params;
		int error;
		
		/*
		 * These settings are stored in the
		 * AutoSCSI data in LRAM of 'W' and 'C'
		 * adapters.
		 */
		fetch_lram_params.offset = AUTO_SCSI_BYTE_OFFSET;
		fetch_lram_params.response_len = sizeof(auto_scsi_data);
		error = bt_cmd(bt, BOP_FETCH_LRAM,
			       (u_int8_t*)&fetch_lram_params,
			       sizeof(fetch_lram_params),
			       (u_int8_t*)&auto_scsi_data,
			       sizeof(auto_scsi_data), DEFAULT_CMD_TIMEOUT);

		if (error != 0) {
			printf("%s: bt_fetch_adapter_info - Failed "
			       "Get Auto SCSI Info\n", bt_name(bt));
			return (error);
		}

		bt->disc_permitted = auto_scsi_data.low_disc_permitted
				   | (auto_scsi_data.high_disc_permitted << 8);
		bt->sync_permitted = auto_scsi_data.low_sync_permitted
				   | (auto_scsi_data.high_sync_permitted << 8);
		bt->fast_permitted = auto_scsi_data.low_fast_permitted
				   | (auto_scsi_data.high_fast_permitted << 8);
		bt->ultra_permitted = auto_scsi_data.low_ultra_permitted
				   | (auto_scsi_data.high_ultra_permitted << 8);
		bt->wide_permitted = auto_scsi_data.low_wide_permitted
				   | (auto_scsi_data.high_wide_permitted << 8);

		if (bt->ultra_scsi == FALSE)
			bt->ultra_permitted = 0;

		if (bt->wide_bus == FALSE)
			bt->wide_permitted = 0;
	} else {
		/*
		 * 'S' and 'A' series have this information in the setup
		 * information structure.
		 */
		setup_data_t	setup_info;

		length_param = sizeof(setup_info);
		error = bt_cmd(bt, BOP_INQUIRE_SETUP_INFO, &length_param,
			       /*paramlen*/1, (u_int8_t*)&setup_info,
			       sizeof(setup_info), DEFAULT_CMD_TIMEOUT);

		if (error != 0) {
			printf("%s: bt_fetch_adapter_info - Failed "
			       "Get Setup Info\n", bt_name(bt));
			return (error);
		}

		if (setup_info.initiate_sync != 0) {
			bt->sync_permitted = ALL_TARGETS;

			if (bt->model[0] == '7') {
				if (esetup_info.sync_neg10MB != 0)
					bt->fast_permitted = ALL_TARGETS;
				if (strcmp(bt->model, "757") == 0)
					bt->wide_permitted = ALL_TARGETS;
			}
		}
		bt->disc_permitted = ALL_TARGETS;
	}

	/* We need as many mailboxes as we can have ccbs */
	bt->num_boxes = bt->max_ccbs;

	/* Determine our SCSI ID */
	
	error = bt_cmd(bt, BOP_INQUIRE_CONFIG, NULL, /*parmlen*/0,
		       (u_int8_t*)&config_data, sizeof(config_data),
		       DEFAULT_CMD_TIMEOUT);
	if (error != 0) {
		printf("%s: bt_fetch_adapter_info - Failed Get Config\n",
		       bt_name(bt));
		return (error);
	}
	bt->scsi_id = config_data.scsi_id;

	return (0);
}

/*
 * Start the board, ready for normal operation
 */
int
bt_init(struct bt_softc* bt)
{
	/* Announce the Adapter */
	printf("%s: BT-%s FW Rev. %s ", bt_name(bt),
	       bt->model, bt->firmware_ver);

	if (bt->ultra_scsi != 0)
		printf("Ultra ");

	if (bt->wide_bus != 0)
		printf("Wide ");
	else
		printf("Narrow ");

	if (bt->diff_bus != 0)
		printf("Diff ");

	printf("SCSI Host Adapter, SCSI ID %d, %d CCBs\n", bt->scsi_id,
	       bt->max_ccbs);

	/*
	 * Create our DMA tags.  These tags define the kinds of device
	 * accessable memory allocations and memory mappings we will 
	 * need to perform during normal operation.
	 *
	 * Unless we need to further restrict the allocation, we rely
	 * on the restrictions of the parent dmat, hence the common
	 * use of MAXADDR and MAXSIZE.
	 */

	/* DMA tag for mapping buffers into device visible space. */
	if (bus_dma_tag_create(bt->parent_dmat, /*alignment*/0, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/MAXBSIZE, /*nsegments*/BT_NSEG,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/BUS_DMA_ALLOCNOW,
			       &bt->buffer_dmat) != 0) {
		goto error_exit;
	}

	bt->init_level++;
	/* DMA tag for our mailboxes */
	if (bus_dma_tag_create(bt->parent_dmat, /*alignment*/0, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       bt->num_boxes * (sizeof(bt_mbox_in_t)
					      + sizeof(bt_mbox_out_t)),
			       /*nsegments*/1,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &bt->mailbox_dmat) != 0) {
		goto error_exit;
        }

	bt->init_level++;

	/* Allocation for our mailboxes */
	if (bus_dmamem_alloc(bt->mailbox_dmat, (void **)&bt->out_boxes,
			     BUS_DMA_NOWAIT, &bt->mailbox_dmamap) != 0) {
		goto error_exit;
	}

	bt->init_level++;

	/* And permanently map them */
	bus_dmamap_load(bt->mailbox_dmat, bt->mailbox_dmamap,
       			bt->out_boxes,
			bt->num_boxes * (sizeof(bt_mbox_in_t)
				       + sizeof(bt_mbox_out_t)),
			btmapmboxes, bt, /*flags*/0);

	bt->init_level++;

	bt->in_boxes = (bt_mbox_in_t *)&bt->out_boxes[bt->num_boxes];

	btinitmboxes(bt);

	/* DMA tag for our ccb structures */
	if (bus_dma_tag_create(bt->parent_dmat, /*alignment*/0, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       bt->max_ccbs * sizeof(struct bt_ccb),
			       /*nsegments*/1,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &bt->ccb_dmat) != 0) {
		goto error_exit;
        }

	bt->init_level++;

	/* Allocation for our ccbs */
	if (bus_dmamem_alloc(bt->ccb_dmat, (void **)&bt->bt_ccb_array,
			     BUS_DMA_NOWAIT, &bt->ccb_dmamap) != 0) {
		goto error_exit;
	}

	bt->init_level++;

	/* And permanently map them */
	bus_dmamap_load(bt->ccb_dmat, bt->ccb_dmamap,
       			bt->bt_ccb_array,
			bt->max_ccbs * sizeof(struct bt_ccb),
			btmapccbs, bt, /*flags*/0);

	bt->init_level++;

	/* DMA tag for our S/G structures.  We allocate in page sized chunks */
	if (bus_dma_tag_create(bt->parent_dmat, /*alignment*/0, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       PAGE_SIZE, /*nsegments*/1,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &bt->sg_dmat) != 0) {
		goto error_exit;
        }

	bt->init_level++;

	/* Perform initial CCB allocation */
	bzero(bt->bt_ccb_array, bt->max_ccbs * sizeof(struct bt_ccb));
	btallocccbs(bt);

	if (bt->num_ccbs == 0) {
		printf("%s: bt_init - Unable to allocate initial ccbs\n");
		goto error_exit;
	}

	/*
	 * Note that we are going and return (to probe)
	 */
	return 0;

error_exit:

	return (ENXIO);
}

int
bt_attach(struct bt_softc *bt)
{
	int tagged_dev_openings;
	struct cam_devq *devq;

	/*
	 * We reserve 1 ccb for error recovery, so don't
	 * tell the XPT about it.
	 */
	if (bt->tag_capable != 0)
		tagged_dev_openings = bt->max_ccbs - 1;
	else
		tagged_dev_openings = 0;

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(bt->max_ccbs - 1);
	if (devq == NULL)
		return (ENOMEM);

	/*
	 * Construct our SIM entry
	 */
	bt->sim = cam_sim_alloc(btaction, btpoll, "bt", bt, bt->unit,
				2, tagged_dev_openings, devq);
	if (bt->sim == NULL) {
		cam_simq_free(devq);
		return (ENOMEM);
	}
	
	if (xpt_bus_register(bt->sim, 0) != CAM_SUCCESS) {
		cam_sim_free(bt->sim, /*free_devq*/TRUE);
		return (ENXIO);
	}
	
	if (xpt_create_path(&bt->path, /*periph*/NULL,
			    cam_sim_path(bt->sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(bt->sim));
		cam_sim_free(bt->sim, /*free_devq*/TRUE);
		return (ENXIO);
	}
		
	return (0);
}

char *
bt_name(struct bt_softc *bt)
{
	static char name[10];

	sprintf(name, "bt%d", bt->unit);
	return (name);
}

int
bt_check_probed_iop(u_int ioport)
{
	u_int i;

	for (i=0; i < BT_NUM_ISAPORTS; i++) {
		if (bt_isa_ports[i].addr == ioport) {
			if (bt_isa_ports[i].probed != 0)
				return (1);
			else {
				return (0);
			}
		}
	}
	return (1);
}

void
bt_mark_probed_bio(isa_compat_io_t port)
{
	if (port < BIO_DISABLED)
		bt_isa_ports[port].probed = 1;
}

void
bt_mark_probed_iop(u_int ioport)
{
	u_int i;

	for (i = 0; i < BT_NUM_ISAPORTS; i++) {
		if (ioport == bt_isa_ports[i].addr) {
			bt_isa_ports[i].probed = 1;
			break;
		}
	}
}

static void
btallocccbs(struct bt_softc *bt)
{
	struct bt_ccb *next_ccb;
	struct sg_map_node *sg_map;
	bus_addr_t physaddr;
	bt_sg_t *segs;
	int newcount;
	int i;

	next_ccb = &bt->bt_ccb_array[bt->num_ccbs];

	sg_map = malloc(sizeof(*sg_map), M_DEVBUF, M_NOWAIT);

	if (sg_map == NULL)
		return;

	/* Allocate S/G space for the next batch of CCBS */
	if (bus_dmamem_alloc(bt->sg_dmat, (void **)&sg_map->sg_vaddr,
			     BUS_DMA_NOWAIT, &sg_map->sg_dmamap) != 0) {
		free(sg_map, M_DEVBUF);
		return;
	}

	SLIST_INSERT_HEAD(&bt->sg_maps, sg_map, links);

	bus_dmamap_load(bt->sg_dmat, sg_map->sg_dmamap, sg_map->sg_vaddr,
			PAGE_SIZE, btmapsgs, bt, /*flags*/0);
	
	segs = sg_map->sg_vaddr;
	physaddr = sg_map->sg_physaddr;

	newcount = (PAGE_SIZE / (BT_NSEG * sizeof(bt_sg_t)));
	for (i = 0; bt->num_ccbs < bt->max_ccbs && i < newcount; i++) {
		int error;

		next_ccb->sg_list = segs;
		next_ccb->sg_list_phys = physaddr;
		next_ccb->flags = BCCB_FREE;
		error = bus_dmamap_create(bt->buffer_dmat, /*flags*/0,
					  &next_ccb->dmamap);
		if (error != 0)
			break;
		SLIST_INSERT_HEAD(&bt->free_bt_ccbs, next_ccb, links);
		segs += BT_NSEG;
		physaddr += (BT_NSEG * sizeof(bt_sg_t));
		next_ccb++;
		bt->num_ccbs++;
	}

	/* Reserve a CCB for error recovery */
	if (bt->recovery_bccb == NULL) {
		bt->recovery_bccb = SLIST_FIRST(&bt->free_bt_ccbs);
		SLIST_REMOVE_HEAD(&bt->free_bt_ccbs, links);
	}
}

static __inline void
btfreeccb(struct bt_softc *bt, struct bt_ccb *bccb)
{
	int s;

	s = splcam();
	if ((bccb->flags & BCCB_ACTIVE) != 0)
		LIST_REMOVE(&bccb->ccb->ccb_h, sim_links.le);
	if (bt->resource_shortage != 0
	 && (bccb->ccb->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
		bccb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		bt->resource_shortage = FALSE;
	}
	bccb->flags = BCCB_FREE;
	SLIST_INSERT_HEAD(&bt->free_bt_ccbs, bccb, links);
	splx(s);
}

static __inline struct bt_ccb*
btgetccb(struct bt_softc *bt)
{
	struct	bt_ccb* bccb;
	int	s;

	s = splcam();
	if ((bccb = SLIST_FIRST(&bt->free_bt_ccbs)) != NULL) {
		SLIST_REMOVE_HEAD(&bt->free_bt_ccbs, links);
	} else if (bt->num_ccbs < bt->max_ccbs) {
		btallocccbs(bt);
		bccb = SLIST_FIRST(&bt->free_bt_ccbs);
		if (bccb == NULL)
			printf("%s: Can't malloc BCCB\n", bt_name(bt));
		else
			SLIST_REMOVE_HEAD(&bt->free_bt_ccbs, links);
	}
	splx(s);

	return (bccb);
}

static void
btaction(struct cam_sim *sim, union ccb *ccb)
{
	struct	bt_softc *bt;
	int	s;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("btaction\n"));
	
	bt = (struct bt_softc *)cam_sim_softc(sim);
	
	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	{
		struct	bt_ccb	*bccb;
		struct	bt_hccb *hccb;
		u_int16_t targ_mask;

		/*
		 * get a bccb to use.
		 */
		if ((bccb = btgetccb(bt)) == NULL) {
			int s;

			s = splcam();
			bt->resource_shortage = TRUE;
			splx(s);
			xpt_freeze_simq(bt->sim, /*count*/1);
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			xpt_done(ccb);
			return;
		}
		
		hccb = &bccb->hccb;

		/*
		 * So we can find the BCCB when an abort is requested
		 */
		bccb->ccb = ccb;
		ccb->ccb_h.ccb_bccb_ptr = bccb;
		ccb->ccb_h.ccb_bt_ptr = bt;

		/*
		 * Put all the arguments for the xfer in the bccb
		 */
		hccb->target_id = ccb->ccb_h.target_id;
		hccb->target_lun = ccb->ccb_h.target_lun;
		hccb->btstat = 0;
		hccb->sdstat = 0;
		targ_mask = (0x01 << hccb->target_id);

		if (ccb->ccb_h.func_code == XPT_SCSI_IO) {
			struct ccb_scsiio *csio;
			struct ccb_hdr *ccbh;

			csio = &ccb->csio;
			ccbh = &csio->ccb_h;
			hccb->opcode = INITIATOR_CCB_WRESID;
			hccb->datain = (ccb->ccb_h.flags & CAM_DIR_IN) != 0;
			hccb->dataout = (ccb->ccb_h.flags & CAM_DIR_OUT) != 0;
			hccb->cmd_len = csio->cdb_len;
			if (hccb->cmd_len > sizeof(hccb->scsi_cdb)) {
				ccb->ccb_h.status = CAM_REQ_INVALID;
				btfreeccb(bt, bccb);
				xpt_done(ccb);
				return;
			}
			hccb->sense_len = csio->sense_len;
			if ((ccbh->flags & CAM_TAG_ACTION_VALID) != 0) {
				hccb->tag_enable = TRUE;
				hccb->tag_type = (ccb->csio.tag_action & 0x3);
			} else {
				hccb->tag_enable = FALSE;
				hccb->tag_type = 0;
			}
			if ((ccbh->flags & CAM_CDB_POINTER) != 0) {
				if ((ccbh->flags & CAM_CDB_PHYS) == 0) {
					bcopy(csio->cdb_io.cdb_ptr,
					      hccb->scsi_cdb, hccb->cmd_len);
				} else {
					/* I guess I could map it in... */
					ccbh->status = CAM_REQ_INVALID;
					btfreeccb(bt, bccb);
					xpt_done(ccb);
					return;
				}
			} else {
				bcopy(csio->cdb_io.cdb_bytes,
				      hccb->scsi_cdb, hccb->cmd_len);
			}
			/* If need be, bounce our sense buffer */
			if (bt->sense_buffers != NULL) {
				hccb->sense_addr = btsensepaddr(bt, bccb);
			} else {
				hccb->sense_addr = vtophys(&csio->sense_data);
			}
			/*
			 * If we have any data to send with this command,
			 * map it into bus space.
			 */
		        /* Only use S/G if there is a transfer */
			if ((ccbh->flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
				if ((ccbh->flags & CAM_SCATTER_VALID) == 0) {
					/*
					 * We've been given a pointer
					 * to a single buffer.
					 */
					if ((ccbh->flags & CAM_DATA_PHYS)==0) {
						int s;
						int error;

						s = splsoftvm();
						error = bus_dmamap_load(
						    bt->buffer_dmat,
						    bccb->dmamap,
						    csio->data_ptr,
						    csio->dxfer_len,
						    btexecuteccb,
						    bccb,
						    /*flags*/0);
						if (error == EINPROGRESS) {
							/*
							 * So as to maintain
							 * ordering, freeze the
							 * controller queue
							 * until our mapping is
							 * returned.
							 */
							xpt_freeze_simq(bt->sim,
									1);
							csio->ccb_h.status |=
							    CAM_RELEASE_SIMQ;
						}
						splx(s);
					} else {
						struct bus_dma_segment seg; 

						/* Pointer to physical buffer */
						seg.ds_addr =
						    (bus_addr_t)csio->data_ptr;
						seg.ds_len = csio->dxfer_len;
						btexecuteccb(bccb, &seg, 1, 0);
					}
				} else {
					struct bus_dma_segment *segs;

					if ((ccbh->flags & CAM_DATA_PHYS) != 0)
						panic("btaction - Physical "
						      "segment pointers "
						      "unsupported");

					if ((ccbh->flags&CAM_SG_LIST_PHYS)==0)
						panic("btaction - Virtual "
						      "segment addresses "
						      "unsupported");

					/* Just use the segments provided */
					segs = (struct bus_dma_segment *)
					    csio->data_ptr;
					btexecuteccb(bccb, segs,
						     csio->sglist_cnt, 0);
				}
			} else {
				btexecuteccb(bccb, NULL, 0, 0);
			}
		} else {
			hccb->opcode = INITIATOR_BUS_DEV_RESET;
			/* No data transfer */
			hccb->datain = TRUE;
			hccb->dataout = TRUE;
			hccb->cmd_len = 0;
			hccb->sense_len = 0;
			hccb->tag_enable = FALSE;
			hccb->tag_type = 0;
			btexecuteccb(bccb, NULL, 0, 0);
		}
		break;
	}
	case XPT_EN_LUN:		/* Enable LUN as a target */
	case XPT_TARGET_IO:		/* Execute target I/O request */
	case XPT_ACCEPT_TARGET_IO:	/* Accept Host Target Mode CDB */
	case XPT_CONT_TARGET_IO:	/* Continue Host Target I/O Connection*/
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
	{
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	/* Get default/user set transfer settings for the target */
	{
		struct	ccb_trans_settings *cts;
		u_int	target_mask;

		cts = &ccb->cts;
		target_mask = 0x01 << ccb->ccb_h.target_id;
		if ((cts->flags & CCB_TRANS_USER_SETTINGS) != 0) {
			cts->flags = 0;
			if ((bt->disc_permitted & target_mask) != 0)
				cts->flags |= CCB_TRANS_DISC_ENB;
			if ((bt->tags_permitted & target_mask) != 0)
				cts->flags |= CCB_TRANS_TAG_ENB;
			if ((bt->wide_permitted & target_mask) != 0)
				cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			else
				cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
			if ((bt->ultra_permitted & target_mask) != 0)
				cts->sync_period = 12;
			else if ((bt->fast_permitted & target_mask) != 0)
				cts->sync_period = 25;
			else if ((bt->sync_permitted & target_mask) != 0)
				cts->sync_period = 50;
			else
				cts->sync_period = 0;

			if (cts->sync_period != 0)
				cts->sync_offset = 15;

			cts->valid = CCB_TRANS_SYNC_RATE_VALID
				   | CCB_TRANS_SYNC_OFFSET_VALID
				   | CCB_TRANS_BUS_WIDTH_VALID
				   | CCB_TRANS_DISC_VALID
				   | CCB_TRANS_TQ_VALID;
		} else {
			btfetchtransinfo(bt, cts);
		}

		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;

		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		
		if (size_mb >= 1024 && (bt->extended_trans != 0)) {
			if (size_mb >= 2048) {
				ccg->heads = 255;
				ccg->secs_per_track = 63;
			} else {
				ccg->heads = 128;
				ccg->secs_per_track = 32;
			}
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_RESET_BUS:		/* Reset the specified SCSI bus */
	{
		btreset(bt, /*hardreset*/TRUE);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	case XPT_TERM_IO:		/* Terminate the I/O process */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;
		
		cpi->version_num = 1; /* XXX??? */
		cpi->hba_inquiry = PI_SDTR_ABLE;
		if (bt->tag_capable != 0)
			cpi->hba_inquiry |= PI_TAG_ABLE;
		if (bt->wide_bus != 0)
			cpi->hba_inquiry |= PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = bt->wide_bus ? 15 : 7;
		cpi->max_lun = 7;
		cpi->initiator_id = bt->scsi_id;
		cpi->bus_id = cam_sim_bus(sim);
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "BusLogic", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

static void
btexecuteccb(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	struct	 bt_ccb *bccb;
	union	 ccb *ccb;
	struct	 bt_softc *bt;
	int	 s, i;

	bccb = (struct bt_ccb *)arg;
	ccb = bccb->ccb;
	bt = (struct bt_softc *)ccb->ccb_h.ccb_bt_ptr;

	if (error != 0) {
		if (error != EFBIG)
			printf("%s: Unexepected error 0x%x returned from "
			       "bus_dmamap_load\n", bt_name(bt));
		if (ccb->ccb_h.status == CAM_REQ_INPROG) {
			xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
			ccb->ccb_h.status = CAM_REQ_TOO_BIG|CAM_DEV_QFRZN;
		}
		btfreeccb(bt, bccb);
		xpt_done(ccb);
		return;
	}
		
	if (nseg != 0) {
		bt_sg_t *sg;
		bus_dma_segment_t *end_seg;
		bus_dmasync_op_t op;

		end_seg = dm_segs + nseg;

		/* Copy the segments into our SG list */
		sg = bccb->sg_list;
		while (dm_segs < end_seg) {
			sg->len = dm_segs->ds_len;
			sg->addr = dm_segs->ds_addr;
			sg++;
			dm_segs++;
		}

		if (nseg > 1) {
			bccb->hccb.opcode = INITIATOR_SG_CCB_WRESID;
			bccb->hccb.data_len = sizeof(bt_sg_t) * nseg;
			bccb->hccb.data_addr = bccb->sg_list_phys;
		} else {
			bccb->hccb.data_len = bccb->sg_list->len;
			bccb->hccb.data_addr = bccb->sg_list->addr;
		}

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(bt->buffer_dmat, bccb->dmamap, op);

	} else {
		bccb->hccb.opcode = INITIATOR_SG_CCB;
		bccb->hccb.data_len = 0;
		bccb->hccb.data_addr = 0;
	}

	s = splcam();

	/*
	 * Last time we need to check if this CCB needs to
	 * be aborted.
	 */
	if (ccb->ccb_h.status != CAM_REQ_INPROG) {
		if (nseg != 0)
			bus_dmamap_unload(bt->buffer_dmat, bccb->dmamap);
		btfreeccb(bt, bccb);
		xpt_done(ccb);
		splx(s);
		return;
	}
		
	bccb->flags = BCCB_ACTIVE;
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	LIST_INSERT_HEAD(&bt->pending_ccbs, &ccb->ccb_h, sim_links.le);

	ccb->ccb_h.timeout_ch =
	    timeout(bttimeout, (caddr_t)bccb,
		    (ccb->ccb_h.timeout * hz) / 1000);

	/* Tell the adapter about this command */
	bt->cur_outbox->ccb_addr = btccbvtop(bt, bccb);
	if (bt->cur_outbox->action_code != BMBO_FREE)
		panic("%s: Too few mailboxes or to many ccbs???", bt_name(bt));
	bt->cur_outbox->action_code = BMBO_START;	
	bt_outb(bt, COMMAND_REG, BOP_START_MBOX);
	btnextoutbox(bt);
	splx(s);
}

void
bt_intr(void *arg)
{
	struct	bt_softc *bt;
	u_int	intstat;

	bt = (struct bt_softc *)arg;
	while (((intstat = bt_inb(bt, INTSTAT_REG)) & INTR_PENDING) != 0) {

		if ((intstat & CMD_COMPLETE) != 0) {
			bt->latched_status = bt_inb(bt, STATUS_REG);
			bt->command_cmp = TRUE;
		}

		bt_outb(bt, CONTROL_REG, RESET_INTR);

		if ((intstat & IMB_LOADED) != 0) {
			while (bt->cur_inbox->comp_code != BMBI_FREE) {
				btdone(bt,
				       btccbptov(bt, bt->cur_inbox->ccb_addr),
				       bt->cur_inbox->comp_code);
				bt->cur_inbox->comp_code = BMBI_FREE;
				btnextinbox(bt);
			}
		}

		if ((intstat & SCSI_BUS_RESET) != 0) {
			btreset(bt, /*hardreset*/FALSE);
		}
	}
}

static void
btdone(struct bt_softc *bt, struct bt_ccb *bccb, bt_mbi_comp_code_t comp_code)
{
	union  ccb	  *ccb;
	struct ccb_scsiio *csio;

	ccb = bccb->ccb;
	csio = &bccb->ccb->csio;

	if ((bccb->flags & BCCB_ACTIVE) == 0) {
		printf("%s: btdone - Attempt to free non-active BCCB 0x%x\n",
		       bt_name(bt), bccb);
		return;
	}

	if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
		bus_dmasync_op_t op;

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_POSTREAD;
		else
			op = BUS_DMASYNC_POSTWRITE;
		bus_dmamap_sync(bt->buffer_dmat, bccb->dmamap, op);
		bus_dmamap_unload(bt->buffer_dmat, bccb->dmamap);
	}

	if (bccb == bt->recovery_bccb) {
		/*
		 * The recovery BCCB does not have a CCB associated
		 * with it, so short circuit the normal error handling.
		 * We now traverse our list of pending CCBs and process
		 * any that were terminated by the recovery CCBs action.
		 * We also reinstate timeouts for all remaining, pending,
		 * CCBs.
		 */
		struct cam_path *path;
		struct ccb_hdr *ccb_h;
		cam_status error;

		/* Notify all clients that a BDR occured */
		error = xpt_create_path(&path, /*periph*/NULL,
					cam_sim_path(bt->sim),
					bccb->hccb.target_id,
					CAM_LUN_WILDCARD);
		
		if (error == CAM_REQ_CMP)
			xpt_async(AC_SENT_BDR, path, NULL);

		ccb_h = LIST_FIRST(&bt->pending_ccbs);
		while (ccb_h != NULL) {
			struct bt_ccb *pending_bccb;

			pending_bccb = (struct bt_ccb *)ccb_h->ccb_bccb_ptr;
			if (pending_bccb->hccb.target_id
			 == bccb->hccb.target_id) {
				pending_bccb->hccb.btstat = BTSTAT_HA_BDR;
				ccb_h = LIST_NEXT(ccb_h, sim_links.le);
				btdone(bt, pending_bccb, BMBI_ERROR);
			} else {
				ccb_h->timeout_ch =
				    timeout(bttimeout, (caddr_t)pending_bccb,
					    (ccb_h->timeout * hz) / 1000);
				ccb_h = LIST_NEXT(ccb_h, sim_links.le);
			}
		}
		printf("%s: No longer in timeout\n", bt_name(bt));
		return;
	}

	untimeout(bttimeout, bccb, ccb->ccb_h.timeout_ch);

	switch (comp_code) {
	case BMBI_FREE:
		printf("%s: btdone - CCB completed with free status!\n",
		       bt_name(bt));
		break;
	case BMBI_NOT_FOUND:
		printf("%s: btdone - CCB Abort failed to find CCB\n",
		       bt_name(bt));
		break;
	case BMBI_ABORT:
	case BMBI_ERROR:
#if 0
		printf("bt: ccb %x - error %x occured.  btstat = %x, sdstat = %x\n",
		       bccb, comp_code, bccb->hccb.btstat, bccb->hccb.sdstat);
#endif
		/* An error occured */
		switch(bccb->hccb.btstat) {
		case BTSTAT_DATARUN_ERROR:
			if (bccb->hccb.data_len <= 0) {
				csio->ccb_h.status = CAM_DATA_RUN_ERR;
				break;
			}
			/* FALLTHROUGH */
		case BTSTAT_NOERROR:
		case BTSTAT_LINKED_CMD_COMPLETE:
		case BTSTAT_LINKED_CMD_FLAG_COMPLETE:
		case BTSTAT_DATAUNDERUN_ERROR:

			csio->scsi_status = bccb->hccb.sdstat;
			csio->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
			switch(csio->scsi_status) {
			case SCSI_STATUS_CHECK_COND:
			case SCSI_STATUS_CMD_TERMINATED:
				csio->ccb_h.status |= CAM_AUTOSNS_VALID;
				/* Bounce sense back if necessary */
				if (bt->sense_buffers != NULL) {
					csio->sense_data =
					    *btsensevaddr(bt, bccb);
				}
				break;
			default:
				break;
			case SCSI_STATUS_OK:
				csio->ccb_h.status = CAM_REQ_CMP;
				break;
			}
			csio->resid = bccb->hccb.data_len;
			break;
		case BTSTAT_SELTIMEOUT:
			csio->ccb_h.status = CAM_SEL_TIMEOUT;
			break;
		case BTSTAT_UNEXPECTED_BUSFREE:
			csio->ccb_h.status = CAM_UNEXP_BUSFREE;
			break;
		case BTSTAT_INVALID_PHASE:
			csio->ccb_h.status = CAM_SEQUENCE_FAIL;
			break;
		case BTSTAT_INVALID_ACTION_CODE:
			panic("%s: Inavlid Action code", bt_name(bt));
			break;
		case BTSTAT_INVALID_OPCODE:
			panic("%s: Inavlid CCB Opcode code", bt_name(bt));
			break;
		case BTSTAT_LINKED_CCB_LUN_MISMATCH:
			/* We don't even support linked commands... */
			panic("%s: Linked CCB Lun Mismatch", bt_name(bt));
			break;
		case BTSTAT_INVALID_CCB_OR_SG_PARAM:
			panic("%s: Invalid CCB or SG list", bt_name(bt));
			break;
		case BTSTAT_AUTOSENSE_FAILED:
			csio->ccb_h.status = CAM_AUTOSENSE_FAIL;
			break;
		case BTSTAT_TAGGED_MSG_REJECTED:
		{
			struct ccb_trans_settings neg; 
 
			xpt_print_path(csio->ccb_h.path);
			printf("refuses tagged commands.  Performing "
			       "non-tagged I/O\n");
			neg.flags = 0;
			neg.valid = CCB_TRANS_TQ_VALID;
			xpt_setup_ccb(&neg.ccb_h, csio->ccb_h.path,
				      /*priority*/1); 
			xpt_async(AC_TRANSFER_NEG, csio->ccb_h.path, &neg);
			bt->tags_permitted &= ~(0x01 << csio->ccb_h.target_id);
			csio->ccb_h.status = CAM_MSG_REJECT_REC;
			break;
		}
		case BTSTAT_UNSUPPORTED_MSG_RECEIVED:
			/*
			 * XXX You would think that this is
			 *     a recoverable error... Hmmm.
			 */
			csio->ccb_h.status = CAM_REQ_CMP_ERR;
			break;
		case BTSTAT_HA_SOFTWARE_ERROR:
		case BTSTAT_HA_WATCHDOG_ERROR:
		case BTSTAT_HARDWARE_FAILURE:
			/* Hardware reset ??? Can we recover ??? */
			csio->ccb_h.status = CAM_NO_HBA;
			break;
		case BTSTAT_TARGET_IGNORED_ATN:
		case BTSTAT_OTHER_SCSI_BUS_RESET:
		case BTSTAT_HA_SCSI_BUS_RESET:
			if ((csio->ccb_h.status & CAM_STATUS_MASK)
			 != CAM_CMD_TIMEOUT)
				csio->ccb_h.status = CAM_SCSI_BUS_RESET;
			break;
		case BTSTAT_HA_BDR:
			if ((bccb->flags & BCCB_DEVICE_RESET) == 0)
				csio->ccb_h.status = CAM_BDR_SENT;
			else
				csio->ccb_h.status = CAM_CMD_TIMEOUT;
			break;
		case BTSTAT_INVALID_RECONNECT:
		case BTSTAT_ABORT_QUEUE_GENERATED:
			csio->ccb_h.status = CAM_REQ_TERMIO;
			break;
		case BTSTAT_SCSI_PERROR_DETECTED:
			csio->ccb_h.status = CAM_UNCOR_PARITY;
			break;
		}
		if (csio->ccb_h.status != CAM_REQ_CMP) {
			xpt_freeze_devq(csio->ccb_h.path, /*count*/1);
			csio->ccb_h.status |= CAM_DEV_QFRZN;
		}
		if ((bccb->flags & BCCB_RELEASE_SIMQ) != 0)
			ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		btfreeccb(bt, bccb);
		xpt_done(ccb);
		break;
	case BMBI_OK:
		/* All completed without incident */
		ccb->ccb_h.status |= CAM_REQ_CMP;
		if ((bccb->flags & BCCB_RELEASE_SIMQ) != 0)
			ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		btfreeccb(bt, bccb);
		xpt_done(ccb);
		break;
	}
}

static int
btreset(struct bt_softc* bt, int hard_reset)
{
	struct	 ccb_hdr *ccb_h;
	u_int	 status;
	u_int	 timeout;
	u_int8_t reset_type;

	if (hard_reset != 0)
		reset_type = HARD_RESET;
	else
		reset_type = SOFT_RESET;
	bt_outb(bt, CONTROL_REG, reset_type);

	/* Wait 5sec. for Diagnostic start */
	timeout = 5 * 10000;
	while (--timeout) {
		status = bt_inb(bt, STATUS_REG);
		if ((status & DIAG_ACTIVE) != 0)
			break;
		DELAY(100);
	}
	if (timeout == 0) {
		if (bootverbose)
			printf("%s: btreset - Diagnostic Active failed to "
				"assert. status = 0x%x\n", bt_name(bt), status);
		return (ETIMEDOUT);
	}

	/* Wait 10sec. for Diagnostic end */
	timeout = 10 * 10000;
	while (--timeout) {
		status = bt_inb(bt, STATUS_REG);
		if ((status & DIAG_ACTIVE) == 0)
			break;
		DELAY(100);
	}
	if (timeout == 0) {
		panic("%s: btreset - Diagnostic Active failed to drop. "
		       "status = 0x%x\n", bt_name(bt), status);
		return (ETIMEDOUT);
	}

	/* Wait for the host adapter to become ready or report a failure */
	timeout = 10000;
	while (--timeout) {
		status = bt_inb(bt, STATUS_REG);
		if ((status & (DIAG_FAIL|HA_READY|DATAIN_REG_READY)) != 0)
			break;
		DELAY(100);
	}
	if (timeout == 0) {
		printf("%s: btreset - Host adapter failed to come ready. "
		       "status = 0x%x\n", bt_name(bt), status);
		return (ETIMEDOUT);
	}

	/* If the diagnostics failed, tell the user */
	if ((status & DIAG_FAIL) != 0
	 || (status & HA_READY) == 0) {
		printf("%s: btreset - Adapter failed diagnostics\n",
		       bt_name(bt));

		if ((status & DATAIN_REG_READY) != 0)
			printf("%s: btreset - Host Adapter Error code = 0x%x\n",
			       bt_inb(bt, DATAIN_REG));
		return (ENXIO);
	}

	/* If we've allocated mailboxes, initialize them */
	if (bt->init_level > 4)
		btinitmboxes(bt);

	/* If we've attached to the XPT, tell it about the event */
	if (bt->path != NULL)
		xpt_async(AC_BUS_RESET, bt->path, NULL);

	/*
	 * Perform completion processing for all outstanding CCBs.
	 */
	while ((ccb_h = LIST_FIRST(&bt->pending_ccbs)) != NULL) {
		struct bt_ccb *pending_bccb;

		pending_bccb = (struct bt_ccb *)ccb_h->ccb_bccb_ptr;
		pending_bccb->hccb.btstat = BTSTAT_HA_SCSI_BUS_RESET;
		btdone(bt, pending_bccb, BMBI_ERROR);
	}

	return (0);
}

/*
 * Send a command to the adapter.
 */
int
bt_cmd(struct bt_softc *bt, bt_op_t opcode, u_int8_t *params, u_int param_len,
      u_int8_t *reply_data, u_int reply_len, u_int cmd_timeout)
{
	u_int	timeout;
	u_int	status;
	u_int	intstat;
	u_int	reply_buf_size;
	int	s;

	/* No data returned to start */
	reply_buf_size = reply_len;
	reply_len = 0;
	intstat = 0;

	bt->command_cmp = 0;
	/*
	 * Wait up to 1 sec. for the adapter to become
	 * ready to accept commands.
	 */
	timeout = 10000;
	while (--timeout) {

		status = bt_inb(bt, STATUS_REG);
		if ((status & HA_READY) != 0
		 && (status & CMD_REG_BUSY) == 0)
			break;
		DELAY(100);
	}
	if (timeout == 0) {
		printf("%s: bt_cmd: Timeout waiting for adapter ready, "
		       "status = 0x%x\n", bt_name(bt), status);
		return (ETIMEDOUT);
	}

	/*
	 * Send the opcode followed by any necessary parameter bytes.
	 */
	bt_outb(bt, COMMAND_REG, opcode);

	/*
	 * Wait for up to 1sec to get the parameter list sent
	 */
	timeout = 10000;
	while (param_len && --timeout) {
		DELAY(100);
		status = bt_inb(bt, STATUS_REG);
		intstat = bt_inb(bt, INTSTAT_REG);
		if ((intstat & (INTR_PENDING|CMD_COMPLETE))
		 == (INTR_PENDING|CMD_COMPLETE))
			break;
		if (bt->command_cmp != 0) {
			status = bt->latched_status;
			break;
		}
		if ((status & DATAIN_REG_READY) != 0)
			break;
		if ((status & CMD_REG_BUSY) == 0) {
			bt_outb(bt, COMMAND_REG, *params++);
			param_len--;
		}
	}
	if (timeout == 0) {
		printf("%s: bt_cmd: Timeout sending parameters, "
		       "status = 0x%x\n", bt_name(bt), status);
		return (ETIMEDOUT);
	}

	/*
	 * The BOP_MODIFY_IO_ADDR does not issue a CMD_COMPLETE, but
	 * it should update the status register.  So, we wait for
	 * the CMD_REG_BUSY status to clear and check for a command
	 * failure.
	 */
	if (opcode == BOP_MODIFY_IO_ADDR) {

		while (--cmd_timeout) {
			status = bt_inb(bt, STATUS_REG);
			if ((status & CMD_REG_BUSY) == 0) {
				if ((status & CMD_INVALID) != 0) {
					printf("%s: bt_cmd - Modify I/O Address"
					       " invalid\n", bt_name(bt));
					return (EINVAL);
				}
				return (0);
			}
			DELAY(100);
		}
		if (timeout == 0) {
			printf("%s: bt_cmd: Timeout on Modify I/O Address CMD, "
			       "status = 0x%x\n", bt_name(bt), status);
			return (ETIMEDOUT);
		}
	}

	/*
	 * For all other commands, we wait for any output data
	 * and the final comand completion interrupt.
	 */
	while (--cmd_timeout) {

		status = bt_inb(bt, STATUS_REG);
		intstat = bt_inb(bt, INTSTAT_REG);
		if ((intstat & (INTR_PENDING|CMD_COMPLETE))
		 == (INTR_PENDING|CMD_COMPLETE))
			break;

		if (bt->command_cmp != 0) {
			status = bt->latched_status;
			break;
		}

		if ((status & DATAIN_REG_READY) != 0) {
			u_int8_t data;

			data = bt_inb(bt, DATAIN_REG);
			if (reply_len < reply_buf_size) {
				*reply_data++ = data;
			} else {
				printf("%s: bt_cmd - Discarded reply data byte "
				       "for opcode 0x%x\n", bt_name(bt),
				       opcode);
			}
			reply_len++;
		}

		if ((opcode == BOP_FETCH_LRAM)
		 && (status & HA_READY) != 0)
			break;
		DELAY(100);
	}
	if (timeout == 0) {
		printf("%s: bt_cmd: Timeout waiting for reply data and "
		       "command complete.\n%s: status = 0x%x, intstat = 0x%x, "
		       "reply_len = %d\n", bt_name(bt), bt_name(bt), status,
		       intstat, reply_len);
		return (ETIMEDOUT);
	}

	/*
	 * Clear any pending interrupts.  Block interrupts so our
	 * interrupt handler is not re-entered.
	 */
	s = splcam();
	bt_intr(bt);
	splx(s);
	
	/*
	 * If the command was rejected by the controller, tell the caller.
	 */
	if ((status & CMD_INVALID) != 0) {
		/*
		 * Some early adapters may not recover properly from
		 * an invalid command.  If it appears that the controller
		 * has wedged (i.e. status was not cleared by our interrupt
		 * reset above), perform a soft reset.
      		 */
		if (bootverbose)
			printf("%s: Invalid Command 0x%x\n", bt_name(bt), 
				opcode);
		DELAY(1000);
		status = bt_inb(bt, STATUS_REG);
		if ((status & (CMD_INVALID|STATUS_REG_RSVD|DATAIN_REG_READY|
			      CMD_REG_BUSY|DIAG_FAIL|DIAG_ACTIVE)) != 0
		 || (status & (HA_READY|INIT_REQUIRED))
		  != (HA_READY|INIT_REQUIRED)) {
			btreset(bt, /*hard_reset*/FALSE);
		}
		return (EINVAL);
	}

	
	if (param_len > 0) {
		/* The controller did not accept the full argument list */
	 	return (E2BIG);
	}

	if (reply_len != reply_buf_size) {
		/* Too much or too little data received */
		return (EMSGSIZE);
	}

	/* We were successful */
	return (0);
}

static int
btinitmboxes(struct bt_softc *bt) {
	init_32b_mbox_params_t init_mbox;
	int error;

	bzero(bt->in_boxes, sizeof(bt_mbox_in_t) * bt->num_boxes);
	bzero(bt->out_boxes, sizeof(bt_mbox_out_t) * bt->num_boxes);
	bt->cur_inbox = bt->in_boxes;
	bt->last_inbox = bt->in_boxes + bt->num_boxes - 1;
	bt->cur_outbox = bt->out_boxes;
	bt->last_outbox = bt->out_boxes + bt->num_boxes - 1;

	/* Tell the adapter about them */
	init_mbox.num_boxes = bt->num_boxes;
	init_mbox.base_addr[0] = bt->mailbox_physbase & 0xFF;
	init_mbox.base_addr[1] = (bt->mailbox_physbase >> 8) & 0xFF;
	init_mbox.base_addr[2] = (bt->mailbox_physbase >> 16) & 0xFF;
	init_mbox.base_addr[3] = (bt->mailbox_physbase >> 24) & 0xFF;
	error = bt_cmd(bt, BOP_INITIALIZE_32BMBOX, (u_int8_t *)&init_mbox,
		       /*parmlen*/sizeof(init_mbox), /*reply_buf*/NULL,
		       /*reply_len*/0, DEFAULT_CMD_TIMEOUT);

	if (error != 0)
		printf("btinitmboxes: Initialization command failed\n");
	else if (bt->strict_rr != 0) {
		/*
		 * If the controller supports
		 * strict round robin mode,
		 * enable it
		 */
		u_int8_t param;

		param = 0;
		error = bt_cmd(bt, BOP_ENABLE_STRICT_RR, &param, 1,
			       /*reply_buf*/NULL, /*reply_len*/0,
			       DEFAULT_CMD_TIMEOUT);

		if (error != 0) {
			printf("btinitmboxes: Unable to enable strict RR\n");
			error = 0;
		} else if (bootverbose) {
			printf("%s: Using Strict Round Robin Mailbox Mode\n",
			       bt_name(bt));
		}
	}
	
	return (error);
}

/*
 * Update the XPT's idea of the negotiated transfer
 * parameters for a particular target.
 */
static void
btfetchtransinfo(struct bt_softc *bt, struct ccb_trans_settings* cts)
{
	setup_data_t	setup_info;
	u_int		target;
	u_int		targ_offset;
	u_int		targ_mask;
	u_int		sync_period;
	int		error;
	u_int8_t	param;
	targ_syncinfo_t	sync_info;

	target = cts->ccb_h.target_id;
	targ_offset = (target & 0x7);
	targ_mask = (0x01 << targ_offset);

	/*
	 * Inquire Setup Information.  This command retreives the
	 * Wide negotiation status for recent adapters as well as
	 * the sync info for older models.
	 */
	param = sizeof(setup_info);
	error = bt_cmd(bt, BOP_INQUIRE_SETUP_INFO, &param, /*paramlen*/1,
		       (u_int8_t*)&setup_info, sizeof(setup_info),
		       DEFAULT_CMD_TIMEOUT);

	if (error != 0) {
		printf("%s: btfetchtransinfo - Inquire Setup Info Failed\n",
		       bt_name(bt));
		return;
	}

	sync_info = (target < 8) ? setup_info.low_syncinfo[targ_offset]
				 : setup_info.high_syncinfo[targ_offset];

	if (sync_info.sync == 0)
		cts->sync_offset = 0;
	else
		cts->sync_offset = sync_info.offset;

	cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
	if (strcmp(bt->firmware_ver, "5.06L") >= 0) {
		u_int wide_active;

		wide_active =
		    (target < 8) ? (setup_info.low_wide_active & targ_mask)
		    		 : (setup_info.high_wide_active & targ_mask);

		if (wide_active)
			cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
	}

	if (bt->firmware_ver[0] >= 3) {
		/*
		 * For adapters that can do fast or ultra speeds,
		 * use the more exact Target Sync Information command.
		 */
		target_sync_info_data_t sync_info;

		param = sizeof(sync_info);
		error = bt_cmd(bt, BOP_TARG_SYNC_INFO, &param, /*paramlen*/1,
			       (u_int8_t*)&sync_info, sizeof(sync_info),
			       DEFAULT_CMD_TIMEOUT);
		
		if (error != 0) {
			printf("%s: btfetchtransinfo - Inquire Sync "
			       "Info Failed 0x%x\n", bt_name(bt), error);
			return;
		}
		sync_period = sync_info.sync_rate[target] * 100;
	} else {
		sync_period = 2000 + (500 * sync_info.period);
	}

	/* Convert ns value to standard SCSI sync rate */
	if (cts->sync_offset != 0)
		cts->sync_period = scsi_calc_syncparam(sync_period);
	else
		cts->sync_period = 0;
	
	cts->valid = CCB_TRANS_SYNC_RATE_VALID
		   | CCB_TRANS_SYNC_OFFSET_VALID
		   | CCB_TRANS_BUS_WIDTH_VALID;
        xpt_async(AC_TRANSFER_NEG, cts->ccb_h.path, cts);
}

static void
btmapmboxes(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bt_softc* bt;

	bt = (struct bt_softc*)arg;
	bt->mailbox_physbase = segs->ds_addr;
}

static void
btmapccbs(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct bt_softc* bt;

	bt = (struct bt_softc*)arg;
	bt->bt_ccb_physbase = segs->ds_addr;
}

static void
btmapsgs(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{

	struct bt_softc* bt;

	bt = (struct bt_softc*)arg;
	SLIST_FIRST(&bt->sg_maps)->sg_physaddr = segs->ds_addr;
}

static void
btpoll(struct cam_sim *sim)
{
}

void
bttimeout(void *arg)
{
	struct bt_ccb	*bccb;
	union  ccb	*ccb;
	struct bt_softc *bt;
	int		 s;

	bccb = (struct bt_ccb *)arg;
	ccb = bccb->ccb;
	bt = (struct bt_softc *)ccb->ccb_h.ccb_bt_ptr;
	xpt_print_path(ccb->ccb_h.path);
	printf("CCB 0x%x - timed out\n", bccb);

	s = splcam();

	if ((bccb->flags & BCCB_ACTIVE) == 0) {
		xpt_print_path(ccb->ccb_h.path);
		printf("CCB 0x%x - timed out CCB already completed\n", bccb);
		splx(s);
		return;
	}

	/*
	 * In order to simplify the recovery process, we ask the XPT
	 * layer to halt the queue of new transactions and we traverse
	 * the list of pending CCBs and remove their timeouts. This
	 * means that the driver attempts to clear only one error
	 * condition at a time.  In general, timeouts that occur
	 * close together are related anyway, so there is no benefit
	 * in attempting to handle errors in parrallel.  Timeouts will
	 * be reinstated when the recovery process ends.
	 */
	if ((bccb->flags & BCCB_DEVICE_RESET) == 0) {
		struct ccb_hdr *ccb_h;

		if ((bccb->flags & BCCB_RELEASE_SIMQ) == 0) {
			xpt_freeze_simq(bt->sim, /*count*/1);
			bccb->flags |= BCCB_RELEASE_SIMQ;
		}

		ccb_h = LIST_FIRST(&bt->pending_ccbs);
		while (ccb_h != NULL) {
			struct bt_ccb *pending_bccb;

			pending_bccb = (struct bt_ccb *)ccb_h->ccb_bccb_ptr;
			untimeout(bttimeout, pending_bccb, ccb_h->timeout_ch);
			ccb_h = LIST_NEXT(ccb_h, sim_links.le);
		}
	}

	if ((bccb->flags & BCCB_DEVICE_RESET) != 0
	 || bt->cur_outbox->action_code != BMBO_FREE
	 || ((ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0
	  && (bt->firmware_ver[0] < '5'))) {
		/*
		 * Try a full host adapter/SCSI bus reset.
		 * We do this only if we have already attempted
		 * to clear the condition with a BDR, or we cannot
		 * attempt a BDR for lack of mailbox resources
		 * or because of faulty firmware.  It turns out
		 * that firmware versions prior to 5.xx treat BDRs
		 * as untagged commands that cannot be sent until
		 * all outstanding tagged commands have been processed.
		 * This makes it somewhat difficult to use a BDR to
		 * clear up a problem with an uncompleted tagged command.
		 */
		ccb->ccb_h.status = CAM_CMD_TIMEOUT;
		btreset(bt, /*hardreset*/TRUE);
		printf("%s: No longer in timeout\n", bt_name(bt));
	} else {
		/*    
		 * Send a Bus Device Reset message:
		 * The target that is holding up the bus may not
		 * be the same as the one that triggered this timeout
		 * (different commands have different timeout lengths),
		 * but we have no way of determining this from our
		 * timeout handler.  Our strategy here is to queue a
		 * BDR message to the target of the timed out command.
		 * If this fails, we'll get another timeout 2 seconds
		 * later which will attempt a bus reset.
		 */
		bccb->flags |= BCCB_DEVICE_RESET;
		ccb->ccb_h.timeout_ch =
		    timeout(bttimeout, (caddr_t)bccb, 2 * hz);

		bt->recovery_bccb->hccb.opcode = INITIATOR_BUS_DEV_RESET;

		/* No Data Transfer */
		bt->recovery_bccb->hccb.datain = TRUE;
		bt->recovery_bccb->hccb.dataout = TRUE;
		bt->recovery_bccb->hccb.btstat = 0;
		bt->recovery_bccb->hccb.sdstat = 0;
		bt->recovery_bccb->hccb.target_id = ccb->ccb_h.target_id;

		/* Tell the adapter about this command */
		bt->cur_outbox->ccb_addr = btccbvtop(bt, bt->recovery_bccb);
		bt->cur_outbox->action_code = BMBO_START;
		bt_outb(bt, COMMAND_REG, BOP_START_MBOX);
		btnextoutbox(bt);
	}

	splx(s);
}

