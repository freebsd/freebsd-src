/*
 *       Copyright (c) 1997 by Simon Shapiro
 *       All Rights Reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
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
 */

/*
 * dpt_scsi.c: SCSI dependant code for the DPT driver
 *
 * credits:	Assisted by Mike Neuffer in the early low level DPT code
 *		Thanx to Mark Salyzyn of DPT for his assistance.
 *		Special thanx to Justin Gibbs for invaluable help in
 *		making this driver look and work like a FreeBSD component.
 *		Last but not least, many thanx to UCB and the FreeBSD
 *		team for creating and maintaining such a wonderful O/S.
 *
 * TODO:     * Add ISA probe code.
 *	     * Add driver-level RAID-0. This will allow interoperability with
 *	       NiceTry, M$-Doze, Win-Dog, Slowlaris, etc., in recognizing RAID
 *	       arrays that span controllers (Wow!).
 */

#ident "$FreeBSD$"

#define _DPT_C_

#include "opt_dpt.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <stddef.h>	/* For offsetof */

#include <sys/bus.h>

#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#include <machine/bus.h>


#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/dpt/dpt.h>

/* dpt_isa.c, dpt_eisa.c, and dpt_pci.c need this in a central place */
int dpt_controllers_present;

u_long	dpt_unit;	/* Next unit number to use */

/* The linked list of softc structures */
struct dpt_softc_list dpt_softcs = TAILQ_HEAD_INITIALIZER(dpt_softcs);

#define microtime_now dpt_time_now()

#define dpt_inl(dpt, port)				\
	bus_space_read_4((dpt)->tag, (dpt)->bsh, port)
#define dpt_inb(dpt, port)				\
	bus_space_read_1((dpt)->tag, (dpt)->bsh, port)
#define dpt_outl(dpt, port, value)			\
	bus_space_write_4((dpt)->tag, (dpt)->bsh, port, value)
#define dpt_outb(dpt, port, value)			\
	bus_space_write_1((dpt)->tag, (dpt)->bsh, port, value)

/*
 * These will have to be setup by parameters passed at boot/load time. For
 * perfromance reasons, we make them constants for the time being.
 */
#define	dpt_min_segs	DPT_MAX_SEGS
#define	dpt_max_segs	DPT_MAX_SEGS

/* Definitions for our use of the SIM private CCB area */
#define ccb_dccb_ptr spriv_ptr0
#define ccb_dpt_ptr spriv_ptr1

/* ================= Private Inline Function declarations ===================*/
static __inline int		dpt_just_reset(dpt_softc_t * dpt);
static __inline int		dpt_raid_busy(dpt_softc_t * dpt);
static __inline int		dpt_pio_wait (u_int32_t, u_int, u_int, u_int);
static __inline int		dpt_wait(dpt_softc_t *dpt, u_int bits,
					 u_int state);
static __inline struct dpt_ccb* dptgetccb(struct dpt_softc *dpt);
static __inline void		dptfreeccb(struct dpt_softc *dpt,
					   struct dpt_ccb *dccb);
static __inline u_int32_t	dptccbvtop(struct dpt_softc *dpt,
					   struct dpt_ccb *dccb);

static __inline int		dpt_send_immediate(dpt_softc_t *dpt,
						   eata_ccb_t *cmd_block,
						   u_int32_t cmd_busaddr,  
						   u_int retries,
						   u_int ifc, u_int code,
						   u_int code2);

/* ==================== Private Function declarations =======================*/
static void		dptmapmem(void *arg, bus_dma_segment_t *segs,
				  int nseg, int error);

static struct sg_map_node*
			dptallocsgmap(struct dpt_softc *dpt);

static int		dptallocccbs(dpt_softc_t *dpt);

static int		dpt_get_conf(dpt_softc_t *dpt, dpt_ccb_t *dccb,
				     u_int32_t dccb_busaddr, u_int size,
				     u_int page, u_int target, int extent);
static void		dpt_detect_cache(dpt_softc_t *dpt, dpt_ccb_t *dccb,
					 u_int32_t dccb_busaddr,
					 u_int8_t *buff);

static void		dpt_poll(struct cam_sim *sim);

static void		dptexecuteccb(void *arg, bus_dma_segment_t *dm_segs,
				      int nseg, int error);

static void		dpt_action(struct cam_sim *sim, union ccb *ccb);

static int		dpt_send_eata_command(dpt_softc_t *dpt, eata_ccb_t *cmd,
					      u_int32_t cmd_busaddr,
					      u_int command, u_int retries,
					      u_int ifc, u_int code,
					      u_int code2);
static void		dptprocesserror(dpt_softc_t *dpt, dpt_ccb_t *dccb,
					union ccb *ccb, u_int hba_stat,
					u_int scsi_stat, u_int32_t resid);

static void		dpttimeout(void *arg);
static void		dptshutdown(void *arg, int howto);

/* ================= Private Inline Function definitions ====================*/
static __inline int
dpt_just_reset(dpt_softc_t * dpt)
{
	if ((dpt_inb(dpt, 2) == 'D')
	 && (dpt_inb(dpt, 3) == 'P')
	 && (dpt_inb(dpt, 4) == 'T')
	 && (dpt_inb(dpt, 5) == 'H'))
		return (1);
	else
		return (0);
}

static __inline int
dpt_raid_busy(dpt_softc_t * dpt)
{
	if ((dpt_inb(dpt, 0) == 'D')
	 && (dpt_inb(dpt, 1) == 'P')
	 && (dpt_inb(dpt, 2) == 'T'))
		return (1);
	else
		return (0);
}

static __inline int
dpt_pio_wait (u_int32_t base, u_int reg, u_int bits, u_int state)
{
	int   i;
	u_int c;

	for (i = 0; i < 20000; i++) {	/* wait 20ms for not busy */
		c = inb(base + reg) & bits;
		if (!(c == state))
			return (0);
		else
			DELAY(50);
	}
	return (-1);
}

static __inline int
dpt_wait(dpt_softc_t *dpt, u_int bits, u_int state)
{
	int   i;
	u_int c;

	for (i = 0; i < 20000; i++) {	/* wait 20ms for not busy */
		c = dpt_inb(dpt, HA_RSTATUS) & bits;
		if (c == state)
			return (0);
		else
			DELAY(50);
	}
	return (-1);
}

static __inline struct dpt_ccb*
dptgetccb(struct dpt_softc *dpt)
{
	struct	dpt_ccb* dccb;
	int	s;

	s = splcam();
	if ((dccb = SLIST_FIRST(&dpt->free_dccb_list)) != NULL) {
		SLIST_REMOVE_HEAD(&dpt->free_dccb_list, links);
		dpt->free_dccbs--;
	} else if (dpt->total_dccbs < dpt->max_dccbs) {
		dptallocccbs(dpt);
		dccb = SLIST_FIRST(&dpt->free_dccb_list);
		if (dccb == NULL)
			printf("dpt%d: Can't malloc DCCB\n", dpt->unit);
		else {
			SLIST_REMOVE_HEAD(&dpt->free_dccb_list, links);
			dpt->free_dccbs--;
		}
	}
	splx(s);

	return (dccb);
}

static __inline void
dptfreeccb(struct dpt_softc *dpt, struct dpt_ccb *dccb)
{
	int s;

	s = splcam();
	if ((dccb->state & DCCB_ACTIVE) != 0)
		LIST_REMOVE(&dccb->ccb->ccb_h, sim_links.le);
	if ((dccb->state & DCCB_RELEASE_SIMQ) != 0)
		dccb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
	else if (dpt->resource_shortage != 0
	 && (dccb->ccb->ccb_h.status & CAM_RELEASE_SIMQ) == 0) {
		dccb->ccb->ccb_h.status |= CAM_RELEASE_SIMQ;
		dpt->resource_shortage = FALSE;
	}
	dccb->state = DCCB_FREE;
	SLIST_INSERT_HEAD(&dpt->free_dccb_list, dccb, links);
	++dpt->free_dccbs;
	splx(s);
}

static __inline u_int32_t
dptccbvtop(struct dpt_softc *dpt, struct dpt_ccb *dccb)
{
	return (dpt->dpt_ccb_busbase
	      + (u_int32_t)((caddr_t)dccb - (caddr_t)dpt->dpt_dccbs));
}

static __inline struct dpt_ccb *
dptccbptov(struct dpt_softc *dpt, u_int32_t busaddr)
{
	return (dpt->dpt_dccbs
	     +  ((struct dpt_ccb *)busaddr
	       - (struct dpt_ccb *)dpt->dpt_ccb_busbase));
}

/*
 * Send a command for immediate execution by the DPT
 * See above function for IMPORTANT notes.
 */
static __inline int
dpt_send_immediate(dpt_softc_t *dpt, eata_ccb_t *cmd_block,
		   u_int32_t cmd_busaddr, u_int retries,
		   u_int ifc, u_int code, u_int code2)
{
	return (dpt_send_eata_command(dpt, cmd_block, cmd_busaddr,
				      EATA_CMD_IMMEDIATE, retries, ifc,
				      code, code2));
}


/* ===================== Private Function definitions =======================*/
static void
dptmapmem(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *busaddrp;

	busaddrp = (bus_addr_t *)arg;
	*busaddrp = segs->ds_addr;
}

static struct sg_map_node *
dptallocsgmap(struct dpt_softc *dpt)
{
	struct sg_map_node *sg_map;

	sg_map = malloc(sizeof(*sg_map), M_DEVBUF, M_NOWAIT);

	if (sg_map == NULL)
		return (NULL);

	/* Allocate S/G space for the next batch of CCBS */
	if (bus_dmamem_alloc(dpt->sg_dmat, (void **)&sg_map->sg_vaddr,
			     BUS_DMA_NOWAIT, &sg_map->sg_dmamap) != 0) {
		free(sg_map, M_DEVBUF);
		return (NULL);
	}

	(void)bus_dmamap_load(dpt->sg_dmat, sg_map->sg_dmamap, sg_map->sg_vaddr,
			      PAGE_SIZE, dptmapmem, &sg_map->sg_physaddr,
			      /*flags*/0);

	SLIST_INSERT_HEAD(&dpt->sg_maps, sg_map, links);

	return (sg_map);
}

/*
 * Allocate another chunk of CCB's. Return count of entries added.
 * Assumed to be called at splcam().
 */
static int
dptallocccbs(dpt_softc_t *dpt)
{
	struct dpt_ccb *next_ccb;
	struct sg_map_node *sg_map;
	bus_addr_t physaddr;
	dpt_sg_t *segs;
	int newcount;
	int i;

	next_ccb = &dpt->dpt_dccbs[dpt->total_dccbs];

	if (next_ccb == dpt->dpt_dccbs) {
		/*
		 * First time through.  Re-use the S/G
		 * space we allocated for initialization
		 * CCBS.
		 */
		sg_map = SLIST_FIRST(&dpt->sg_maps);
	} else {
		sg_map = dptallocsgmap(dpt);
	}

	if (sg_map == NULL)
		return (0);

	segs = sg_map->sg_vaddr;
	physaddr = sg_map->sg_physaddr;

	newcount = (PAGE_SIZE / (dpt->sgsize * sizeof(dpt_sg_t)));
	for (i = 0; dpt->total_dccbs < dpt->max_dccbs && i < newcount; i++) {
		int error;

		error = bus_dmamap_create(dpt->buffer_dmat, /*flags*/0,
					  &next_ccb->dmamap);
		if (error != 0)
			break;
		next_ccb->sg_list = segs;
		next_ccb->sg_busaddr = htonl(physaddr);
		next_ccb->eata_ccb.cp_dataDMA = htonl(physaddr);
		next_ccb->eata_ccb.cp_statDMA = htonl(dpt->sp_physaddr);
		next_ccb->eata_ccb.cp_reqDMA =
		    htonl(dptccbvtop(dpt, next_ccb)
			+ offsetof(struct dpt_ccb, sense_data));
		next_ccb->eata_ccb.cp_busaddr = dpt->dpt_ccb_busend;
		next_ccb->state = DCCB_FREE;
		next_ccb->tag = dpt->total_dccbs;
		SLIST_INSERT_HEAD(&dpt->free_dccb_list, next_ccb, links);
		segs += dpt->sgsize;
		physaddr += (dpt->sgsize * sizeof(dpt_sg_t));
		dpt->dpt_ccb_busend += sizeof(*next_ccb);
		next_ccb++;
		dpt->total_dccbs++;
	}
	return (i);
}

dpt_conf_t *
dpt_pio_get_conf (u_int32_t base)
{
	static dpt_conf_t *	conf;
	u_int16_t *		p;
	int			i;

	/*
	 * Allocate a dpt_conf_t
	 */
	if (!conf) {
		conf = (dpt_conf_t *)malloc(sizeof(dpt_conf_t),
						 M_DEVBUF, M_NOWAIT);
	}
	
	/*
	 * If we didn't get one then we probably won't ever get one.
	 */
	if (!conf) {
		printf("dpt: unable to allocate dpt_conf_t\n");
		return (NULL);
	}

	/*
	 * If we have one, clean it up.
	 */
	bzero(conf, sizeof(dpt_conf_t));

	/*
	 * Reset the controller.
	 */
	outb((base + HA_WCOMMAND), EATA_CMD_RESET);

	/*
	 * Wait for the controller to become ready.
	 * For some reason there can be -no- delays after calling reset
	 * before we wait on ready status.
	 */
	if (dpt_pio_wait(base, HA_RSTATUS, HA_SBUSY, 0)) {
		printf("dpt: timeout waiting for controller to become ready\n");
		return (NULL);
	}

	if (dpt_pio_wait(base, HA_RAUXSTAT, HA_ABUSY, 0)) {
		printf("dpt: timetout waiting for adapter ready.\n");
		return (NULL);
	}

	/*
	 * Send the PIO_READ_CONFIG command.
	 */
	outb((base + HA_WCOMMAND), EATA_CMD_PIO_READ_CONFIG);

	/*
	 * Read the data into the struct.
	 */
	p = (u_int16_t *)conf;
	for (i = 0; i < (sizeof(dpt_conf_t) / 2); i++) {

		if (dpt_pio_wait(base, HA_RSTATUS, HA_SDRQ, 0)) {
			printf("dpt: timeout in data read.\n");
			return (NULL);
		}

		(*p) = inw(base + HA_RDATA);
		p++;
	}

	if (inb(base + HA_RSTATUS) & HA_SERROR) {
		printf("dpt: error reading configuration data.\n");
		return (NULL);
	}

#define BE_EATA_SIGNATURE	0x45415441
#define LE_EATA_SIGNATURE	0x41544145

	/*
	 * Test to see if we have a valid card.
	 */
	if ((conf->signature == BE_EATA_SIGNATURE) ||
	    (conf->signature == LE_EATA_SIGNATURE)) {

		while (inb(base + HA_RSTATUS) & HA_SDRQ) {
 			inw(base + HA_RDATA);
		}

		return (conf);
	}
	return (NULL);
}

/*
 * Read a configuration page into the supplied dpt_cont_t buffer.
 */
static int
dpt_get_conf(dpt_softc_t *dpt, dpt_ccb_t *dccb, u_int32_t dccb_busaddr,
	     u_int size, u_int page, u_int target, int extent)
{
	eata_ccb_t *cp;

	u_int8_t   status;

	int	   ndx;
	int	   ospl;
	int	   result;

	cp = &dccb->eata_ccb;
	bzero((void *)(uintptr_t)(volatile void *)dpt->sp, sizeof(*dpt->sp));

	cp->Interpret = 1;
	cp->DataIn = 1;
	cp->Auto_Req_Sen = 1;
	cp->reqlen = sizeof(struct scsi_sense_data);

	cp->cp_id = target;
	cp->cp_LUN = 0;		/* In the EATA packet */
	cp->cp_lun = 0;		/* In the SCSI command */

	cp->cp_scsi_cmd = INQUIRY;
	cp->cp_len = size;

	cp->cp_extent = extent;

	cp->cp_page = page;
	cp->cp_channel = 0;	/* DNC, Interpret mode is set */
	cp->cp_identify = 1;
	cp->cp_datalen = htonl(size);

	ospl = splcam();

	/*
	 * This could be a simple for loop, but we suspected the compiler To
	 * have optimized it a bit too much. Wait for the controller to
	 * become ready
	 */
	while (((status = dpt_inb(dpt, HA_RSTATUS)) != (HA_SREADY | HA_SSC)
	     && (status != (HA_SREADY | HA_SSC | HA_SERROR))
	     && (status != (HA_SDRDY | HA_SERROR | HA_SDRQ)))
	    || (dpt_wait(dpt, HA_SBUSY, 0))) {

		/*
		 * RAID Drives still Spinning up? (This should only occur if
		 * the DPT controller is in a NON PC (PCI?) platform).
		 */
		if (dpt_raid_busy(dpt)) {
			printf("dpt%d WARNING: Get_conf() RSUS failed.\n",
			       dpt->unit);
			splx(ospl);
			return (0);
		}
	}

	DptStat_Reset_BUSY(dpt->sp);

	/*
	 * XXXX We might want to do something more clever than aborting at
	 * this point, like resetting (rebooting) the controller and trying
	 * again.
	 */
	if ((result = dpt_send_eata_command(dpt, cp, dccb_busaddr,
					    EATA_CMD_DMA_SEND_CP,
					    10000, 0, 0, 0)) != 0) {
		printf("dpt%d WARNING: Get_conf() failed (%d) to send "
		       "EATA_CMD_DMA_READ_CONFIG\n",
		       dpt->unit, result);
		splx(ospl);
		return (0);
	}
	/* Wait for two seconds for a response.  This can be slow  */
	for (ndx = 0;
	     (ndx < 20000)
	     && !((status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ);
	     ndx++) {
		DELAY(50);
	}

	/* Grab the status and clear interrupts */
	status = dpt_inb(dpt, HA_RSTATUS);

	splx(ospl);

	/*
	 * Check the status carefully.  Return only if the
	 * command was successful.
	 */
	if (((status & HA_SERROR) == 0)
	 && (dpt->sp->hba_stat == 0)
	 && (dpt->sp->scsi_stat == 0)
	 && (dpt->sp->residue_len == 0))
		return (0);

	if (dpt->sp->scsi_stat == SCSI_STATUS_CHECK_COND)
		return (0);

	return (1);
}

/* Detect Cache parameters and size */
static void
dpt_detect_cache(dpt_softc_t *dpt, dpt_ccb_t *dccb, u_int32_t dccb_busaddr,
		 u_int8_t *buff)
{
	eata_ccb_t *cp;
	u_int8_t   *param;
	int	    bytes;
	int	    result;
	int	    ospl;
	int	    ndx;
	u_int8_t    status;

	/*
	 * Default setting, for best perfromance..
	 * This is what virtually all cards default to..
	 */
	dpt->cache_type = DPT_CACHE_WRITEBACK;
	dpt->cache_size = 0;

	cp = &dccb->eata_ccb;
	bzero((void *)(uintptr_t)(volatile void *)dpt->sp, sizeof(dpt->sp));
	bzero(buff, 512);

	/* Setup the command structure */
	cp->Interpret = 1;
	cp->DataIn = 1;
	cp->Auto_Req_Sen = 1;
	cp->reqlen = sizeof(struct scsi_sense_data);

	cp->cp_id = 0;		/* who cares?  The HBA will interpret.. */
	cp->cp_LUN = 0;		/* In the EATA packet */
	cp->cp_lun = 0;		/* In the SCSI command */
	cp->cp_channel = 0;

	cp->cp_scsi_cmd = EATA_CMD_DMA_SEND_CP;
	cp->cp_len = 56;

	cp->cp_extent = 0;
	cp->cp_page = 0;
	cp->cp_identify = 1;
	cp->cp_dispri = 1;

	/*
	 * Build the EATA Command Packet structure
	 * for a Log Sense Command.
	 */
	cp->cp_cdb[0] = 0x4d;
	cp->cp_cdb[1] = 0x0;
	cp->cp_cdb[2] = 0x40 | 0x33;
	cp->cp_cdb[7] = 1;

	cp->cp_datalen = htonl(512);

	ospl = splcam();
	result = dpt_send_eata_command(dpt, cp, dccb_busaddr,
				       EATA_CMD_DMA_SEND_CP,
				       10000, 0, 0, 0);
	if (result != 0) {
		printf("dpt%d WARNING: detect_cache() failed (%d) to send "
		       "EATA_CMD_DMA_SEND_CP\n", dpt->unit, result);
		splx(ospl);
		return;
	}
	/* Wait for two seconds for a response.  This can be slow... */
	for (ndx = 0;
	     (ndx < 20000) &&
	     !((status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ);
	     ndx++) {
		DELAY(50);
	}

	/* Grab the status and clear interrupts */
	status = dpt_inb(dpt, HA_RSTATUS);
	splx(ospl);

	/*
	 * Sanity check
	 */
	if (buff[0] != 0x33) {
		return;
	}
	bytes = DPT_HCP_LENGTH(buff);
	param = DPT_HCP_FIRST(buff);

	if (DPT_HCP_CODE(param) != 1) {
		/*
		 * DPT Log Page layout error
		 */
		printf("dpt%d: NOTICE: Log Page (1) layout error\n",
		       dpt->unit);
		return;
	}
	if (!(param[4] & 0x4)) {
		dpt->cache_type = DPT_NO_CACHE;
		return;
	}
	while (DPT_HCP_CODE(param) != 6) {
		param = DPT_HCP_NEXT(param);
		if ((param < buff)
		 || (param >= &buff[bytes])) {
			return;
		}
	}

	if (param[4] & 0x2) {
		/*
		 * Cache disabled
		 */
		dpt->cache_type = DPT_NO_CACHE;
		return;
	}

	if (param[4] & 0x4) {
		dpt->cache_type = DPT_CACHE_WRITETHROUGH;
	}

	/* XXX This isn't correct.  This log parameter only has two bytes.... */
#if 0
	dpt->cache_size = param[5]
			| (param[6] << 8)
			| (param[7] << 16)
			| (param[8] << 24);
#endif
}

static void
dpt_poll(struct cam_sim *sim)
{
	dpt_intr(cam_sim_softc(sim));
}

static void
dptexecuteccb(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	struct	 dpt_ccb *dccb;
	union	 ccb *ccb;
	struct	 dpt_softc *dpt;
	int	 s;

	dccb = (struct dpt_ccb *)arg;
	ccb = dccb->ccb;
	dpt = (struct dpt_softc *)ccb->ccb_h.ccb_dpt_ptr;

	if (error != 0) {
		if (error != EFBIG)
			printf("dpt%d: Unexepected error 0x%x returned from "
			       "bus_dmamap_load\n", dpt->unit, error);
		if (ccb->ccb_h.status == CAM_REQ_INPROG) {
			xpt_freeze_devq(ccb->ccb_h.path, /*count*/1);
			ccb->ccb_h.status = CAM_REQ_TOO_BIG|CAM_DEV_QFRZN;
		}
		dptfreeccb(dpt, dccb);
		xpt_done(ccb);
		return;
	}
		
	if (nseg != 0) {
		dpt_sg_t *sg;
		bus_dma_segment_t *end_seg;
		bus_dmasync_op_t op;

		end_seg = dm_segs + nseg;

		/* Copy the segments into our SG list */
		sg = dccb->sg_list;
		while (dm_segs < end_seg) {
			sg->seg_len = htonl(dm_segs->ds_len);
			sg->seg_addr = htonl(dm_segs->ds_addr);
			sg++;
			dm_segs++;
		}

		if (nseg > 1) {
			dccb->eata_ccb.scatter = 1;
			dccb->eata_ccb.cp_dataDMA = dccb->sg_busaddr;
			dccb->eata_ccb.cp_datalen =
			    htonl(nseg * sizeof(dpt_sg_t));
		} else {
			dccb->eata_ccb.cp_dataDMA = dccb->sg_list[0].seg_addr;
			dccb->eata_ccb.cp_datalen = dccb->sg_list[0].seg_len;
		}

		if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
			op = BUS_DMASYNC_PREREAD;
		else
			op = BUS_DMASYNC_PREWRITE;

		bus_dmamap_sync(dpt->buffer_dmat, dccb->dmamap, op);

	} else {
		dccb->eata_ccb.cp_dataDMA = 0;
		dccb->eata_ccb.cp_datalen = 0;
	}

	s = splcam();

	/*
	 * Last time we need to check if this CCB needs to
	 * be aborted.
	 */
	if (ccb->ccb_h.status != CAM_REQ_INPROG) {
		if (nseg != 0)
			bus_dmamap_unload(dpt->buffer_dmat, dccb->dmamap);
		dptfreeccb(dpt, dccb);
		xpt_done(ccb);
		splx(s);
		return;
	}
		
	dccb->state |= DCCB_ACTIVE;
	ccb->ccb_h.status |= CAM_SIM_QUEUED;
	LIST_INSERT_HEAD(&dpt->pending_ccb_list, &ccb->ccb_h, sim_links.le);
	ccb->ccb_h.timeout_ch =
	    timeout(dpttimeout, (caddr_t)dccb,
		    (ccb->ccb_h.timeout * hz) / 1000);
	if (dpt_send_eata_command(dpt, &dccb->eata_ccb,
				  dccb->eata_ccb.cp_busaddr,
				  EATA_CMD_DMA_SEND_CP, 0, 0, 0, 0) != 0) {
		ccb->ccb_h.status = CAM_NO_HBA; /* HBA dead or just busy?? */
		if (nseg != 0)
			bus_dmamap_unload(dpt->buffer_dmat, dccb->dmamap);
		dptfreeccb(dpt, dccb);
		xpt_done(ccb);
	}

	splx(s);
}

static void
dpt_action(struct cam_sim *sim, union ccb *ccb)
{
	struct	  dpt_softc *dpt;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("dpt_action\n"));
	
	dpt = (struct dpt_softc *)cam_sim_softc(sim);

	if ((dpt->state & DPT_HA_SHUTDOWN_ACTIVE) != 0) {
		xpt_print_path(ccb->ccb_h.path);
		printf("controller is shutdown. Aborting CCB.\n");
		ccb->ccb_h.status = CAM_NO_HBA;
		xpt_done(ccb);
		return;
	}

	switch (ccb->ccb_h.func_code) {
	/* Common cases first */
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
	{
		struct	ccb_scsiio *csio;
		struct	ccb_hdr *ccbh;
		struct	dpt_ccb *dccb;
		struct	eata_ccb *eccb;

		csio = &ccb->csio;
		ccbh = &ccb->ccb_h;
		/* Max CDB length is 12 bytes */
		if (csio->cdb_len > 12) { 
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}
		if ((dccb = dptgetccb(dpt)) == NULL) {
			int s;
	
			s = splcam();
			dpt->resource_shortage = 1;
			splx(s);
			xpt_freeze_simq(sim, /*count*/1);
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			xpt_done(ccb);
			return;
		}
		eccb = &dccb->eata_ccb;

		/* Link dccb and ccb so we can find one from the other */
		dccb->ccb = ccb;
		ccb->ccb_h.ccb_dccb_ptr = dccb;
		ccb->ccb_h.ccb_dpt_ptr = dpt;

		/*
		 * Explicitly set all flags so that the compiler can
		 * be smart about setting them.
		 */
		eccb->SCSI_Reset = 0;
		eccb->HBA_Init = 0;
		eccb->Auto_Req_Sen = (ccb->ccb_h.flags & CAM_DIS_AUTOSENSE)
				   ? 0 : 1;
		eccb->scatter = 0;
		eccb->Quick = 0;
		eccb->Interpret =
		    ccb->ccb_h.target_id == dpt->hostid[cam_sim_bus(sim)]
		    ? 1 : 0;
		eccb->DataOut = (ccb->ccb_h.flags & CAM_DIR_OUT) ? 1 : 0;
		eccb->DataIn = (ccb->ccb_h.flags & CAM_DIR_IN) ? 1 : 0;
		eccb->reqlen = csio->sense_len;
		eccb->cp_id = ccb->ccb_h.target_id;
		eccb->cp_channel = cam_sim_bus(sim);
		eccb->cp_LUN = ccb->ccb_h.target_lun;
		eccb->cp_luntar = 0;
		eccb->cp_dispri = (ccb->ccb_h.flags & CAM_DIS_DISCONNECT)
				? 0 : 1;
		eccb->cp_identify = 1;

		if ((ccb->ccb_h.flags & CAM_TAG_ACTION_VALID) != 0
		 && csio->tag_action != CAM_TAG_ACTION_NONE) {
			eccb->cp_msg[0] = csio->tag_action;
			eccb->cp_msg[1] = dccb->tag;
		} else {
			eccb->cp_msg[0] = 0;
			eccb->cp_msg[1] = 0;
		}
		eccb->cp_msg[2] = 0;

		if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0) {
			if ((ccb->ccb_h.flags & CAM_CDB_PHYS) == 0) {
				bcopy(csio->cdb_io.cdb_ptr,
				      eccb->cp_cdb, csio->cdb_len);
			} else {
				/* I guess I could map it in... */
				ccb->ccb_h.status = CAM_REQ_INVALID;
				dptfreeccb(dpt, dccb);
				xpt_done(ccb);
				return;
			}
		} else {
			bcopy(csio->cdb_io.cdb_bytes,
			      eccb->cp_cdb, csio->cdb_len);
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
				if ((ccbh->flags & CAM_DATA_PHYS) == 0) {
					int s;
					int error;

					s = splsoftvm();
					error =
					    bus_dmamap_load(dpt->buffer_dmat,
							    dccb->dmamap,
							    csio->data_ptr,
							    csio->dxfer_len,
							    dptexecuteccb,
							    dccb, /*flags*/0);
					if (error == EINPROGRESS) {
						/*
						 * So as to maintain ordering,
						 * freeze the controller queue
						 * until our mapping is
						 * returned.
						 */
						xpt_freeze_simq(sim, 1);
						dccb->state |= CAM_RELEASE_SIMQ;
					}
					splx(s);
				} else {
					struct bus_dma_segment seg; 

					/* Pointer to physical buffer */
					seg.ds_addr =
					    (bus_addr_t)csio->data_ptr;
					seg.ds_len = csio->dxfer_len;
					dptexecuteccb(dccb, &seg, 1, 0);
				}
			} else {
				struct bus_dma_segment *segs;

				if ((ccbh->flags & CAM_DATA_PHYS) != 0)
					panic("dpt_action - Physical "
					      "segment pointers "
					      "unsupported");

				if ((ccbh->flags&CAM_SG_LIST_PHYS)==0)
					panic("dpt_action - Virtual "
					      "segment addresses "
					      "unsupported");

				/* Just use the segments provided */
				segs = (struct bus_dma_segment *)csio->data_ptr;
				dptexecuteccb(dccb, segs, csio->sglist_cnt, 0);
			}
		} else {
			/*
			 * XXX JGibbs.
			 * Does it want them both on or both off?
			 * CAM_DIR_NONE is both on, so this code can
			 * be removed if this is also what the DPT
			 * exptects.
			 */
			eccb->DataOut = 0;
			eccb->DataIn = 0;
			dptexecuteccb(dccb, NULL, 0, 0);
		}
		break;
	}
	case XPT_RESET_DEV:	/* Bus Device Reset the specified SCSI device */
	case XPT_ABORT:			/* Abort the specified CCB */
		/* XXX Implement */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	case XPT_SET_TRAN_SETTINGS:
	{
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
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
			cts->flags = CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB;
			cts->bus_width = (dpt->max_id > 7)
				       ? MSG_EXT_WDTR_BUS_8_BIT
				       : MSG_EXT_WDTR_BUS_16_BIT;
			cts->sync_period = 25; /* 10MHz */

			if (cts->sync_period != 0)
				cts->sync_offset = 15;

			cts->valid = CCB_TRANS_SYNC_RATE_VALID
				   | CCB_TRANS_SYNC_OFFSET_VALID
				   | CCB_TRANS_BUS_WIDTH_VALID
				   | CCB_TRANS_DISC_VALID
				   | CCB_TRANS_TQ_VALID;
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else {
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		}
		xpt_done(ccb);
		break;
	}
	case XPT_CALC_GEOMETRY:
	{
		struct	  ccb_calc_geometry *ccg;
		u_int32_t size_mb;
		u_int32_t secs_per_cylinder;
		int	  extended;

		/*
		 * XXX Use Adaptec translation until I find out how to
		 *     get this information from the card.
		 */
		ccg = &ccb->ccg;
		size_mb = ccg->volume_size
			/ ((1024L * 1024L) / ccg->block_size);
		extended = 1;
		
		if (size_mb > 1024 && extended) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
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
		/* XXX Implement */
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
		
		cpi->version_num = 1;
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE;
		if (dpt->max_id > 7)
			cpi->hba_inquiry |= PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_misc = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = dpt->max_id;
		cpi->max_lun = dpt->max_lun;
		cpi->initiator_id = dpt->hostid[cam_sim_bus(sim)];
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 3300;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "DPT", HBA_IDLEN);
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

/*
 * This routine will try to send an EATA command to the DPT HBA.
 * It will, by default, try 20,000 times, waiting 50us between tries.
 * It returns 0 on success and 1 on failure.
 * It is assumed to be called at splcam().
 */
static int
dpt_send_eata_command(dpt_softc_t *dpt, eata_ccb_t *cmd_block,
		      u_int32_t cmd_busaddr, u_int command, u_int retries,
		      u_int ifc, u_int code, u_int code2)
{
	u_int	loop;
	
	if (!retries)
		retries = 20000;

	/*
	 * I hate this polling nonsense. Wish there was a way to tell the DPT
	 * to go get commands at its own pace,  or to interrupt when ready.
	 * In the mean time we will measure how many itterations it really
	 * takes.
	 */
	for (loop = 0; loop < retries; loop++) {
		if ((dpt_inb(dpt, HA_RAUXSTAT) & HA_ABUSY) == 0)
			break;
		else
			DELAY(50);
	}

	if (loop < retries) {
#ifdef DPT_MEASURE_PERFORMANCE
		if (loop > dpt->performance.max_eata_tries)
			dpt->performance.max_eata_tries = loop;

		if (loop < dpt->performance.min_eata_tries)
			dpt->performance.min_eata_tries = loop;
#endif
	} else {
#ifdef DPT_MEASURE_PERFORMANCE
		++dpt->performance.command_too_busy;
#endif
		return (1);
	}

	/* The controller is alive, advance the wedge timer */
#ifdef DPT_RESET_HBA
	dpt->last_contact = microtime_now;
#endif

	if (cmd_block == NULL)
		cmd_busaddr = 0;
#if (BYTE_ORDER == BIG_ENDIAN)
	else {
		cmd_busaddr = ((cmd_busaddr >> 24) & 0xFF)
			    | ((cmd_busaddr >> 16) & 0xFF)
			    | ((cmd_busaddr >> 8) & 0xFF)
			    | (cmd_busaddr & 0xFF);
	}
#endif
	/* And now the address */
	dpt_outl(dpt, HA_WDMAADDR, cmd_busaddr);

	if (command == EATA_CMD_IMMEDIATE) {
		if (cmd_block == NULL) {
			dpt_outb(dpt, HA_WCODE2, code2);
			dpt_outb(dpt, HA_WCODE, code);
		}
		dpt_outb(dpt, HA_WIFC, ifc);
	}
	dpt_outb(dpt, HA_WCOMMAND, command);

	return (0);
}


/* ==================== Exported Function definitions =======================*/
dpt_softc_t *
dpt_alloc(device_t dev, bus_space_tag_t tag, bus_space_handle_t bsh)
{
	dpt_softc_t	*dpt = device_get_softc(dev);
	int    i;

	bzero(dpt, sizeof(dpt_softc_t));
	dpt->tag = tag;
	dpt->bsh = bsh;
	dpt->unit = device_get_unit(dev);
	SLIST_INIT(&dpt->free_dccb_list);
	LIST_INIT(&dpt->pending_ccb_list);
	TAILQ_INSERT_TAIL(&dpt_softcs, dpt, links);
	for (i = 0; i < MAX_CHANNELS; i++)
		dpt->resetlevel[i] = DPT_HA_OK;

#ifdef DPT_MEASURE_PERFORMANCE
	dpt_reset_performance(dpt);
#endif /* DPT_MEASURE_PERFORMANCE */
	return (dpt);
}

void
dpt_free(struct dpt_softc *dpt)
{
	switch (dpt->init_level) {
	default:
	case 5:
		bus_dmamap_unload(dpt->dccb_dmat, dpt->dccb_dmamap);
	case 4:
		bus_dmamem_free(dpt->dccb_dmat, dpt->dpt_dccbs,
				dpt->dccb_dmamap);
		bus_dmamap_destroy(dpt->dccb_dmat, dpt->dccb_dmamap);
	case 3:
		bus_dma_tag_destroy(dpt->dccb_dmat);
	case 2:
		bus_dma_tag_destroy(dpt->buffer_dmat);
	case 1:
	{
		struct sg_map_node *sg_map;

		while ((sg_map = SLIST_FIRST(&dpt->sg_maps)) != NULL) {
			SLIST_REMOVE_HEAD(&dpt->sg_maps, links);
			bus_dmamap_unload(dpt->sg_dmat,
					  sg_map->sg_dmamap);
			bus_dmamem_free(dpt->sg_dmat, sg_map->sg_vaddr,
					sg_map->sg_dmamap);
			free(sg_map, M_DEVBUF);
		}
		bus_dma_tag_destroy(dpt->sg_dmat);
	}
	case 0:
		break;
	}
	TAILQ_REMOVE(&dpt_softcs, dpt, links);
}

static u_int8_t string_sizes[] =
{
	sizeof(((dpt_inq_t*)NULL)->vendor),
	sizeof(((dpt_inq_t*)NULL)->modelNum),
	sizeof(((dpt_inq_t*)NULL)->firmware),
	sizeof(((dpt_inq_t*)NULL)->protocol),
};

int
dpt_init(struct dpt_softc *dpt)
{
	dpt_conf_t  conf;
	struct	    sg_map_node *sg_map;
	dpt_ccb_t  *dccb;
	u_int8_t   *strp;
	int	    index;
	int	    i;
	int	    retval;

	dpt->init_level = 0;
	SLIST_INIT(&dpt->sg_maps);

#ifdef DPT_RESET_BOARD
	printf("dpt%d: resetting HBA\n", dpt->unit);
	dpt_outb(dpt, HA_WCOMMAND, EATA_CMD_RESET);
	DELAY(750000);
	/* XXX Shouldn't we poll a status register or something??? */
#endif
	/* DMA tag for our S/G structures.  We allocate in page sized chunks */
	if (bus_dma_tag_create(dpt->parent_dmat, /*alignment*/1, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       PAGE_SIZE, /*nsegments*/1,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &dpt->sg_dmat) != 0) {
		goto error_exit;
        }

	dpt->init_level++;

	/*
	 * We allocate our DPT ccbs as a contiguous array of bus dma'able
	 * memory.  To get the allocation size, we need to know how many
	 * ccbs the card supports.  This requires a ccb.  We solve this
	 * chicken and egg problem by allocating some re-usable S/G space
	 * up front, and treating it as our status packet, CCB, and target
	 * memory space for these commands.
	 */
	sg_map = dptallocsgmap(dpt);
	if (sg_map == NULL)
		goto error_exit;

	dpt->sp = (volatile dpt_sp_t *)sg_map->sg_vaddr;
	dccb = (struct dpt_ccb *)(uintptr_t)(volatile void *)&dpt->sp[1];
	bzero(dccb, sizeof(*dccb));
	dpt->sp_physaddr = sg_map->sg_physaddr;
	dccb->eata_ccb.cp_dataDMA =
	    htonl(sg_map->sg_physaddr + sizeof(dpt_sp_t) + sizeof(*dccb));
	dccb->eata_ccb.cp_busaddr = ~0;
	dccb->eata_ccb.cp_statDMA = htonl(dpt->sp_physaddr);
	dccb->eata_ccb.cp_reqDMA = htonl(dpt->sp_physaddr + sizeof(*dccb)
				       + offsetof(struct dpt_ccb, sense_data));

	/* Okay.  Fetch our config */
	bzero(&dccb[1], sizeof(conf)); /* data area */
	retval = dpt_get_conf(dpt, dccb, sg_map->sg_physaddr + sizeof(dpt_sp_t),
			      sizeof(conf), 0xc1, 7, 1);

	if (retval != 0) {
		printf("dpt%d: Failed to get board configuration\n", dpt->unit);
		return (retval);
	}
	bcopy(&dccb[1], &conf, sizeof(conf));

	bzero(&dccb[1], sizeof(dpt->board_data));
	retval = dpt_get_conf(dpt, dccb, sg_map->sg_physaddr + sizeof(dpt_sp_t),
			      sizeof(dpt->board_data), 0, conf.scsi_id0, 0);
	if (retval != 0) {
		printf("dpt%d: Failed to get inquiry information\n", dpt->unit);
		return (retval);
	}
	bcopy(&dccb[1], &dpt->board_data, sizeof(dpt->board_data));

	dpt_detect_cache(dpt, dccb, sg_map->sg_physaddr + sizeof(dpt_sp_t),
			 (u_int8_t *)&dccb[1]);

	switch (ntohl(conf.splen)) {
	case DPT_EATA_REVA:
		dpt->EATA_revision = 'a';
		break;
	case DPT_EATA_REVB:
		dpt->EATA_revision = 'b';
		break;
	case DPT_EATA_REVC:
		dpt->EATA_revision = 'c';
		break;
	case DPT_EATA_REVZ:
		dpt->EATA_revision = 'z';
		break;
	default:
		dpt->EATA_revision = '?';
	}

	dpt->max_id	 = conf.MAX_ID;
	dpt->max_lun	 = conf.MAX_LUN;
	dpt->irq	 = conf.IRQ;
	dpt->dma_channel = (8 - conf.DMA_channel) & 7;
	dpt->channels	 = conf.MAX_CHAN + 1;
	dpt->state	|= DPT_HA_OK;
	if (conf.SECOND)
		dpt->primary = FALSE;
	else
		dpt->primary = TRUE;

	dpt->more_support = conf.MORE_support;

	if (strncmp(dpt->board_data.firmware, "07G0", 4) >= 0)
		dpt->immediate_support = 1;
	else
		dpt->immediate_support = 0;

	dpt->broken_INQUIRY = FALSE;

	dpt->cplen = ntohl(conf.cplen);
	dpt->cppadlen = ntohs(conf.cppadlen);
	dpt->max_dccbs = ntohs(conf.queuesiz);

	if (dpt->max_dccbs > 256) {
		printf("dpt%d: Max CCBs reduced from %d to "
		       "256 due to tag algorithm\n", dpt->unit, dpt->max_dccbs);
		dpt->max_dccbs = 256;
	}

	dpt->hostid[0] = conf.scsi_id0;
	dpt->hostid[1] = conf.scsi_id1;
	dpt->hostid[2] = conf.scsi_id2;

	if (conf.SG_64K)
		dpt->sgsize = 8192;
	else
		dpt->sgsize = ntohs(conf.SGsiz);

	/* We can only get 64k buffers, so don't bother to waste space. */
	if (dpt->sgsize < 17 || dpt->sgsize > 32)
		dpt->sgsize = 32; 

	if (dpt->sgsize > dpt_max_segs)
		dpt->sgsize = dpt_max_segs;
	
	/* DMA tag for mapping buffers into device visible space. */
	if (bus_dma_tag_create(dpt->parent_dmat, /*alignment*/1, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       /*maxsize*/MAXBSIZE, /*nsegments*/dpt->sgsize,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/BUS_DMA_ALLOCNOW,
			       &dpt->buffer_dmat) != 0) {
		printf("dpt: bus_dma_tag_create(...,dpt->buffer_dmat) failed\n");
		goto error_exit;
	}

	dpt->init_level++;

	/* DMA tag for our ccb structures and interrupt status packet */
	if (bus_dma_tag_create(dpt->parent_dmat, /*alignment*/1, /*boundary*/0,
			       /*lowaddr*/BUS_SPACE_MAXADDR,
			       /*highaddr*/BUS_SPACE_MAXADDR,
			       /*filter*/NULL, /*filterarg*/NULL,
			       (dpt->max_dccbs * sizeof(struct dpt_ccb))
			       + sizeof(dpt_sp_t),
			       /*nsegments*/1,
			       /*maxsegsz*/BUS_SPACE_MAXSIZE_32BIT,
			       /*flags*/0, &dpt->dccb_dmat) != 0) {
		printf("dpt: bus_dma_tag_create(...,dpt->dccb_dmat) failed\n");
		goto error_exit;
        }

	dpt->init_level++;

	/* Allocation for our ccbs and interrupt status packet */
	if (bus_dmamem_alloc(dpt->dccb_dmat, (void **)&dpt->dpt_dccbs,
			     BUS_DMA_NOWAIT, &dpt->dccb_dmamap) != 0) {
		printf("dpt: bus_dmamem_alloc(dpt->dccb_dmat,...) failed\n");
		goto error_exit;
	}

	dpt->init_level++;

	/* And permanently map them */
	bus_dmamap_load(dpt->dccb_dmat, dpt->dccb_dmamap,
       			dpt->dpt_dccbs,
			(dpt->max_dccbs * sizeof(struct dpt_ccb))
			+ sizeof(dpt_sp_t),
			dptmapmem, &dpt->dpt_ccb_busbase, /*flags*/0);

	/* Clear them out. */
	bzero(dpt->dpt_dccbs,
	      (dpt->max_dccbs * sizeof(struct dpt_ccb)) + sizeof(dpt_sp_t));

	dpt->dpt_ccb_busend = dpt->dpt_ccb_busbase;

	dpt->sp = (dpt_sp_t*)&dpt->dpt_dccbs[dpt->max_dccbs];
	dpt->sp_physaddr = dpt->dpt_ccb_busbase
			 + (dpt->max_dccbs * sizeof(dpt_ccb_t));
	dpt->init_level++;

	/* Allocate our first batch of ccbs */
	if (dptallocccbs(dpt) == 0) {
		printf("dpt: dptallocccbs(dpt) == 0\n");
		return (2);
	}

	/* Prepare for Target Mode */
	dpt->target_mode_enabled = 1;

	/* Nuke excess spaces from inquiry information */
	strp = dpt->board_data.vendor;
	for (i = 0; i < sizeof(string_sizes); i++) {
		index = string_sizes[i] - 1;	
		while (index && (strp[index] == ' '))
			strp[index--] = '\0';
		strp += string_sizes[i];
	}

	printf("dpt%d: %.8s %.16s FW Rev. %.4s, ",
	       dpt->unit, dpt->board_data.vendor,
	       dpt->board_data.modelNum, dpt->board_data.firmware);

	printf("%d channel%s, ", dpt->channels, dpt->channels > 1 ? "s" : "");

	if (dpt->cache_type != DPT_NO_CACHE
	 && dpt->cache_size != 0) {
		printf("%s Cache, ",
		       dpt->cache_type == DPT_CACHE_WRITETHROUGH
		     ? "Write-Through" : "Write-Back");
	}

	printf("%d CCBs\n", dpt->max_dccbs);
	return (0);
		
error_exit:
	return (1);
}

int
dpt_attach(dpt_softc_t *dpt)
{
	struct cam_devq *devq;
	int i;

	/*
	 * Create the device queue for our SIM.
	 */
	devq = cam_simq_alloc(dpt->max_dccbs);
	if (devq == NULL)
		return (0);

	for (i = 0; i < dpt->channels; i++) {
		/*
		 * Construct our SIM entry
		 */
		dpt->sims[i] = cam_sim_alloc(dpt_action, dpt_poll, "dpt",
					     dpt, dpt->unit, /*untagged*/2,
					     /*tagged*/dpt->max_dccbs, devq);
		if (xpt_bus_register(dpt->sims[i], i) != CAM_SUCCESS) {
			cam_sim_free(dpt->sims[i], /*free_devq*/i == 0);
			break;
		}

		if (xpt_create_path(&dpt->paths[i], /*periph*/NULL,
				    cam_sim_path(dpt->sims[i]),
				    CAM_TARGET_WILDCARD,
				    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			xpt_bus_deregister(cam_sim_path(dpt->sims[i]));
			cam_sim_free(dpt->sims[i], /*free_devq*/i == 0);
			break;
		}

	}
	if (i > 0)
		EVENTHANDLER_REGISTER(shutdown_final, dptshutdown,
				      dpt, SHUTDOWN_PRI_DEFAULT);
	return (i);
}


/*
 * This is the interrupt handler for the DPT driver.
 */
void
dpt_intr(void *arg)
{
	dpt_softc_t    *dpt;
	dpt_ccb_t      *dccb;
	union ccb      *ccb;
	u_int		status;
	u_int		aux_status;
	u_int		hba_stat;
	u_int		scsi_stat;
	u_int32_t	residue_len;	/* Number of bytes not transferred */

	dpt = (dpt_softc_t *)arg;

	/* First order of business is to check if this interrupt is for us */
	while (((aux_status = dpt_inb(dpt, HA_RAUXSTAT)) & HA_AIRQ) != 0) {

		/*
		 * What we want to do now, is to capture the status, all of it,
		 * move it where it belongs, wake up whoever sleeps waiting to
		 * process this result, and get out of here.
		 */
		if (dpt->sp->ccb_busaddr < dpt->dpt_ccb_busbase
		 || dpt->sp->ccb_busaddr >= dpt->dpt_ccb_busend) {
			printf("Encountered bogus status packet\n");
			status = dpt_inb(dpt, HA_RSTATUS);
			return;
		}

		dccb = dptccbptov(dpt, dpt->sp->ccb_busaddr);

		dpt->sp->ccb_busaddr = ~0;

		/* Ignore status packets with EOC not set */
		if (dpt->sp->EOC == 0) {
			printf("dpt%d ERROR: Request %d recieved with "
			       "clear EOC.\n     Marking as LOST.\n",
			       dpt->unit, dccb->transaction_id);

#ifdef DPT_HANDLE_TIMEOUTS
			dccb->state |= DPT_CCB_STATE_MARKED_LOST;
#endif
			/* This CLEARS the interrupt! */
			status = dpt_inb(dpt, HA_RSTATUS);
			continue;
		}
		dpt->sp->EOC = 0;

		/*
		 * Double buffer the status information so the hardware can
		 * work on updating the status packet while we decifer the
		 * one we were just interrupted for.
		 * According to Mark Salyzyn, we only need few pieces of it.
		 */
		hba_stat = dpt->sp->hba_stat;
		scsi_stat = dpt->sp->scsi_stat;
		residue_len = dpt->sp->residue_len;

		/* Clear interrupts, check for error */
		if ((status = dpt_inb(dpt, HA_RSTATUS)) & HA_SERROR) {
			/*
			 * Error Condition. Check for magic cookie. Exit
			 * this test on earliest sign of non-reset condition
			 */

			/* Check that this is not a board reset interrupt */
			if (dpt_just_reset(dpt)) {
				printf("dpt%d: HBA rebooted.\n"
				       "      All transactions should be "
				       "resubmitted\n",
				       dpt->unit);

				printf("dpt%d: >>---->>  This is incomplete, "
				       "fix me....  <<----<<", dpt->unit);
				panic("DPT Rebooted");

			}
		}
		/* Process CCB */
		ccb = dccb->ccb;
		untimeout(dpttimeout, dccb, ccb->ccb_h.timeout_ch);
		if ((ccb->ccb_h.flags & CAM_DIR_MASK) != CAM_DIR_NONE) {
			bus_dmasync_op_t op;

			if ((ccb->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN)
				op = BUS_DMASYNC_POSTREAD;
			else
				op = BUS_DMASYNC_POSTWRITE;
			bus_dmamap_sync(dpt->buffer_dmat, dccb->dmamap, op);
			bus_dmamap_unload(dpt->buffer_dmat, dccb->dmamap);
		}

		/* Common Case inline... */
		if (hba_stat == HA_NO_ERROR) {
			ccb->csio.scsi_status = scsi_stat;
			ccb->ccb_h.status = 0;
			switch (scsi_stat) {
			case SCSI_STATUS_OK:
				ccb->ccb_h.status |= CAM_REQ_CMP;
				break;
			case SCSI_STATUS_CHECK_COND:
			case SCSI_STATUS_CMD_TERMINATED:
				bcopy(&dccb->sense_data, &ccb->csio.sense_data,
				      ccb->csio.sense_len);
				ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
				/* FALLTHROUGH */
			default:
				ccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
				/* XXX Freeze DevQ */
				break;
			}
			ccb->csio.resid = residue_len;
			dptfreeccb(dpt, dccb);
			xpt_done(ccb);
		} else {
			dptprocesserror(dpt, dccb, ccb, hba_stat, scsi_stat,
					residue_len);
		}
	}
}

static void
dptprocesserror(dpt_softc_t *dpt, dpt_ccb_t *dccb, union ccb *ccb,
		u_int hba_stat, u_int scsi_stat, u_int32_t resid)
{
	ccb->csio.resid = resid;
	switch (hba_stat) {
	case HA_ERR_SEL_TO:
		ccb->ccb_h.status = CAM_SEL_TIMEOUT;
		break;
	case HA_ERR_CMD_TO:
		ccb->ccb_h.status = CAM_CMD_TIMEOUT;
		break;
	case HA_SCSIBUS_RESET:
	case HA_HBA_POWER_UP:	/* Similar effect to a bus reset??? */
		ccb->ccb_h.status = CAM_SCSI_BUS_RESET;
		break;
	case HA_CP_ABORTED:
	case HA_CP_RESET:	/* XXX ??? */
	case HA_CP_ABORT_NA:	/* XXX ??? */
	case HA_CP_RESET_NA:	/* XXX ??? */
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG)
			ccb->ccb_h.status = CAM_REQ_ABORTED;
		break;
	case HA_PCI_PARITY:
	case HA_PCI_MABORT:
	case HA_PCI_TABORT:
	case HA_PCI_STABORT:
	case HA_BUS_PARITY:
	case HA_PARITY_ERR:
	case HA_ECC_ERR:
		ccb->ccb_h.status = CAM_UNCOR_PARITY;
		break;
	case HA_UNX_MSGRJCT:
		ccb->ccb_h.status = CAM_MSG_REJECT_REC;
		break;
	case HA_UNX_BUSPHASE:
		ccb->ccb_h.status = CAM_SEQUENCE_FAIL;
		break;
	case HA_UNX_BUS_FREE:
		ccb->ccb_h.status = CAM_UNEXP_BUSFREE;
		break;
	case HA_SCSI_HUNG:
	case HA_RESET_STUCK:
		/*
		 * Dead???  Can the controller get unstuck
		 * from these conditions
		 */
		ccb->ccb_h.status = CAM_NO_HBA;
		break;
	case HA_RSENSE_FAIL:
		ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;
		break;
	default:
		printf("dpt%d: Undocumented Error %x\n", dpt->unit, hba_stat);
		printf("Please mail this message to shimon@simon-shapiro.org\n");
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		break;
	}
	dptfreeccb(dpt, dccb);
	xpt_done(ccb);
}

static void
dpttimeout(void *arg)
{
	struct dpt_ccb	 *dccb;
	union  ccb	 *ccb;
	struct dpt_softc *dpt;
	int		  s;

	dccb = (struct dpt_ccb *)arg;
	ccb = dccb->ccb;
	dpt = (struct dpt_softc *)ccb->ccb_h.ccb_dpt_ptr;
	xpt_print_path(ccb->ccb_h.path);
	printf("CCB %p - timed out\n", (void *)dccb);

	s = splcam();

	/*
	 * Try to clear any pending jobs.  FreeBSD will loose interrupts,
	 * leaving the controller suspended, and commands timed-out.
	 * By calling the interrupt handler, any command thus stuck will be
	 * completed.
	 */
	dpt_intr(dpt);
	
	if ((dccb->state & DCCB_ACTIVE) == 0) {
		xpt_print_path(ccb->ccb_h.path);
		printf("CCB %p - timed out CCB already completed\n",
		       (void *)dccb);
		splx(s);
		return;
	}

	/* Abort this particular command.  Leave all others running */
	dpt_send_immediate(dpt, &dccb->eata_ccb, dccb->eata_ccb.cp_busaddr,
			   /*retries*/20000, EATA_SPECIFIC_ABORT, 0, 0);
	ccb->ccb_h.status = CAM_CMD_TIMEOUT;
	splx(s);
}

/*
 * Shutdown the controller and ensure that the cache is completely flushed.
 * Called from the shutdown_final event after all disk access has completed.
 */
static void
dptshutdown(void *arg, int howto)
{
	dpt_softc_t *dpt;

	dpt = (dpt_softc_t *)arg;

	printf("dpt%d: Shutting down (mode %x) HBA.	Please wait...\n",
	       dpt->unit, howto);

	/*
	 * What we do for a shutdown, is give the DPT early power loss warning
	 */
	dpt_send_immediate(dpt, NULL, 0, EATA_POWER_OFF_WARN, 0, 0, 0);
	DELAY(1000 * 1000 * 5);
	printf("dpt%d: Controller was warned of shutdown and is now "
	       "disabled\n", dpt->unit);
}

/*============================================================================*/

#if 0
#ifdef DPT_RESET_HBA

/*
**	Function name : dpt_reset_hba
**
**	Description : Reset the HBA and properly discard all pending work
**	Input :       Softc
**	Output :      Nothing
*/
static void
dpt_reset_hba(dpt_softc_t *dpt)
{
	eata_ccb_t       *ccb;
	int               ospl;
	dpt_ccb_t         dccb, *dccbp;
	int               result;
	struct scsi_xfer *xs;
    
	/* Prepare a control block.  The SCSI command part is immaterial */
	dccb.xs = NULL;
	dccb.flags = 0;
	dccb.state = DPT_CCB_STATE_NEW;
	dccb.std_callback = NULL;
	dccb.wrbuff_callback = NULL;

	ccb = &dccb.eata_ccb;
	ccb->CP_OpCode = EATA_CMD_RESET;
	ccb->SCSI_Reset = 0;
	ccb->HBA_Init = 1;
	ccb->Auto_Req_Sen = 1;
	ccb->cp_id = 0; /* Should be ignored */
	ccb->DataIn = 1;
	ccb->DataOut = 0;
	ccb->Interpret = 1;
	ccb->reqlen = htonl(sizeof(struct scsi_sense_data));
	ccb->cp_statDMA = htonl(vtophys(&ccb->cp_statDMA));
	ccb->cp_reqDMA = htonl(vtophys(&ccb->cp_reqDMA));
	ccb->cp_viraddr = (u_int32_t) & ccb;

	ccb->cp_msg[0] = HA_IDENTIFY_MSG | HA_DISCO_RECO;
	ccb->cp_scsi_cmd = 0;  /* Should be ignored */

	/* Lock up the submitted queue.  We are very persistant here */
	ospl = splcam();
	while (dpt->queue_status & DPT_SUBMITTED_QUEUE_ACTIVE) {
		DELAY(100);
	}
	
	dpt->queue_status |= DPT_SUBMITTED_QUEUE_ACTIVE;
	splx(ospl);

	/* Send the RESET message */
	if ((result = dpt_send_eata_command(dpt, &dccb.eata_ccb,
					    EATA_CMD_RESET, 0, 0, 0, 0)) != 0) {
		printf("dpt%d: Failed to send the RESET message.\n"
		       "      Trying cold boot (ouch!)\n", dpt->unit);
	
	
		if ((result = dpt_send_eata_command(dpt, &dccb.eata_ccb,
						    EATA_COLD_BOOT, 0, 0,
						    0, 0)) != 0) {
			panic("dpt%d:  Faild to cold boot the HBA\n",
			      dpt->unit);
		}
#ifdef DPT_MEASURE_PERFORMANCE
		dpt->performance.cold_boots++;
#endif /* DPT_MEASURE_PERFORMANCE */
	}
	
#ifdef DPT_MEASURE_PERFORMANCE
	dpt->performance.warm_starts++;
#endif /* DPT_MEASURE_PERFORMANCE */
	
	printf("dpt%d:  Aborting pending requests.  O/S should re-submit\n",
	       dpt->unit);

	while ((dccbp = TAILQ_FIRST(&dpt->completed_ccbs)) != NULL) {
		struct scsi_xfer *xs = dccbp->xs;
	    
		/* Not all transactions have xs structs */
		if (xs != NULL) {
			/* Tell the kernel proper this did not complete well */
			xs->error |= XS_SELTIMEOUT;
			xs->flags |= SCSI_ITSDONE;
			scsi_done(xs);
		}
	    
		dpt_Qremove_submitted(dpt, dccbp);
	
		/* Remember, Callbacks are NOT in the standard queue */
		if (dccbp->std_callback != NULL) {
			(dccbp->std_callback)(dpt, dccbp->eata_ccb.cp_channel,
					       dccbp);
		} else {
			ospl = splcam();
			dpt_Qpush_free(dpt, dccbp);
			splx(ospl);
		}
	}

	printf("dpt%d: reset done aborting all pending commands\n", dpt->unit);
	dpt->queue_status &= ~DPT_SUBMITTED_QUEUE_ACTIVE;
}

#endif /* DPT_RESET_HBA */ 

/*
 * Build a Command Block for target mode READ/WRITE BUFFER,
 * with the ``sync'' bit ON.
 *
 * Although the length and offset are 24 bit fields in the command, they cannot
 * exceed 8192 bytes, so we take them as short integers andcheck their range.
 * If they are sensless, we round them to zero offset, maximum length and
 * complain.
 */

static void
dpt_target_ccb(dpt_softc_t * dpt, int bus, u_int8_t target, u_int8_t lun,
	       dpt_ccb_t * ccb, int mode, u_int8_t command,
	       u_int16_t length, u_int16_t offset)
{
	eata_ccb_t     *cp;
	int             ospl;

	if ((length + offset) > DPT_MAX_TARGET_MODE_BUFFER_SIZE) {
		printf("dpt%d:  Length of %d, and offset of %d are wrong\n",
		       dpt->unit, length, offset);
		length = DPT_MAX_TARGET_MODE_BUFFER_SIZE;
		offset = 0;
	}
	ccb->xs = NULL;
	ccb->flags = 0;
	ccb->state = DPT_CCB_STATE_NEW;
	ccb->std_callback = (ccb_callback) dpt_target_done;
	ccb->wrbuff_callback = NULL;

	cp = &ccb->eata_ccb;
	cp->CP_OpCode = EATA_CMD_DMA_SEND_CP;
	cp->SCSI_Reset = 0;
	cp->HBA_Init = 0;
	cp->Auto_Req_Sen = 1;
	cp->cp_id = target;
	cp->DataIn = 1;
	cp->DataOut = 0;
	cp->Interpret = 0;
	cp->reqlen = htonl(sizeof(struct scsi_sense_data));
	cp->cp_statDMA = htonl(vtophys(&cp->cp_statDMA));
	cp->cp_reqDMA = htonl(vtophys(&cp->cp_reqDMA));
	cp->cp_viraddr = (u_int32_t) & ccb;

	cp->cp_msg[0] = HA_IDENTIFY_MSG | HA_DISCO_RECO;

	cp->cp_scsi_cmd = command;
	cp->cp_cdb[1] = (u_int8_t) (mode & SCSI_TM_MODE_MASK);
	cp->cp_lun = lun;	/* Order is important here! */
	cp->cp_cdb[2] = 0x00;	/* Buffer Id, only 1 :-( */
	cp->cp_cdb[3] = (length >> 16) & 0xFF;	/* Buffer offset MSB */
	cp->cp_cdb[4] = (length >> 8) & 0xFF;
	cp->cp_cdb[5] = length & 0xFF;
	cp->cp_cdb[6] = (length >> 16) & 0xFF;	/* Length MSB */
	cp->cp_cdb[7] = (length >> 8) & 0xFF;
	cp->cp_cdb[8] = length & 0xFF;	/* Length LSB */
	cp->cp_cdb[9] = 0;	/* No sync, no match bits */

	/*
	 * This could be optimized to live in dpt_register_buffer.
	 * We keep it here, just in case the kernel decides to reallocate pages
	 */
	if (dpt_scatter_gather(dpt, ccb, DPT_RW_BUFFER_SIZE,
			       dpt->rw_buffer[bus][target][lun])) {
		printf("dpt%d: Failed to setup Scatter/Gather for "
		       "Target-Mode buffer\n", dpt->unit);
	}
}

/* Setup a target mode READ command */

static void
dpt_set_target(int redo, dpt_softc_t * dpt,
	       u_int8_t bus, u_int8_t target, u_int8_t lun, int mode,
	       u_int16_t length, u_int16_t offset, dpt_ccb_t * ccb)
{
	int ospl;

	if (dpt->target_mode_enabled) {
		ospl = splcam();

		if (!redo)
			dpt_target_ccb(dpt, bus, target, lun, ccb, mode,
				       SCSI_TM_READ_BUFFER, length, offset);

		ccb->transaction_id = ++dpt->commands_processed;

#ifdef DPT_MEASURE_PERFORMANCE
		dpt->performance.command_count[ccb->eata_ccb.cp_scsi_cmd]++;
		ccb->command_started = microtime_now;
#endif
		dpt_Qadd_waiting(dpt, ccb);
		dpt_sched_queue(dpt);

		splx(ospl);
	} else {
		printf("dpt%d:  Target Mode Request, but Target Mode is OFF\n",
		       dpt->unit);
	}
}

/*
 * Schedule a buffer to be sent to another target.
 * The work will be scheduled and the callback provided will be called when
 * the work is actually done.
 *
 * Please NOTE:  ``Anyone'' can send a buffer, but only registered clients
 * get notified of receipt of buffers.
 */

int
dpt_send_buffer(int unit, u_int8_t channel, u_int8_t target, u_int8_t lun,
		u_int8_t mode, u_int16_t length, u_int16_t offset, void *data,
		buff_wr_done callback)
{
	dpt_softc_t    *dpt;
	dpt_ccb_t      *ccb = NULL;
	int             ospl;

	/* This is an external call.  Be a bit paranoid */
	for (dpt = TAILQ_FIRST(&dpt_softc_list);
	     dpt != NULL;
	     dpt = TAILQ_NEXT(dpt, links)) {
		if (dpt->unit == unit)
			goto valid_unit;
	}

	return (INVALID_UNIT);

valid_unit:

	if (dpt->target_mode_enabled) {
		if ((channel >= dpt->channels) || (target > dpt->max_id) ||
		    (lun > dpt->max_lun)) {
			return (INVALID_SENDER);
		}
		if ((dpt->rw_buffer[channel][target][lun] == NULL) ||
		    (dpt->buffer_receiver[channel][target][lun] == NULL))
			return (NOT_REGISTERED);

		ospl = splsoftcam();
		/* Process the free list */
		if ((TAILQ_EMPTY(&dpt->free_ccbs)) && dpt_alloc_freelist(dpt)) {
			printf("dpt%d ERROR: Cannot allocate any more free CCB's.\n"
			       "             Please try later\n",
			       dpt->unit);
			splx(ospl);
			return (NO_RESOURCES);
		}
		/* Now grab the newest CCB */
		if ((ccb = dpt_Qpop_free(dpt)) == NULL) {
			splx(ospl);
			panic("dpt%d: Got a NULL CCB from pop_free()\n", dpt->unit);
		}
		splx(ospl);

		bcopy(dpt->rw_buffer[channel][target][lun] + offset, data, length);
		dpt_target_ccb(dpt, channel, target, lun, ccb, mode, 
					   SCSI_TM_WRITE_BUFFER,
					   length, offset);
		ccb->std_callback = (ccb_callback) callback; /* Potential trouble */

		ospl = splcam();
		ccb->transaction_id = ++dpt->commands_processed;

#ifdef DPT_MEASURE_PERFORMANCE
		dpt->performance.command_count[ccb->eata_ccb.cp_scsi_cmd]++;
		ccb->command_started = microtime_now;
#endif
		dpt_Qadd_waiting(dpt, ccb);
		dpt_sched_queue(dpt);

		splx(ospl);
		return (0);
	}
	return (DRIVER_DOWN);
}

static void
dpt_target_done(dpt_softc_t * dpt, int bus, dpt_ccb_t * ccb)
{
	int             ospl;
	eata_ccb_t     *cp;

	cp = &ccb->eata_ccb;

	/*
	 * Remove the CCB from the waiting queue.
	 *  We do NOT put it back on the free, etc., queues as it is a special
	 * ccb, owned by the dpt_softc of this unit.
	 */
	ospl = splsoftcam();
	dpt_Qremove_completed(dpt, ccb);
	splx(ospl);

#define br_channel           (ccb->eata_ccb.cp_channel)
#define br_target            (ccb->eata_ccb.cp_id)
#define br_lun               (ccb->eata_ccb.cp_LUN)
#define br_index	     [br_channel][br_target][br_lun]
#define read_buffer_callback (dpt->buffer_receiver br_index )
#define	read_buffer	     (dpt->rw_buffer[br_channel][br_target][br_lun])
#define cb(offset)           (ccb->eata_ccb.cp_cdb[offset])
#define br_offset            ((cb(3) << 16) | (cb(4) << 8) | cb(5))
#define br_length            ((cb(6) << 16) | (cb(7) << 8) | cb(8))

	/* Different reasons for being here, you know... */
	switch (ccb->eata_ccb.cp_scsi_cmd) {
	case SCSI_TM_READ_BUFFER:
		if (read_buffer_callback != NULL) {
			/* This is a buffer generated by a kernel process */
			read_buffer_callback(dpt->unit, br_channel,
					     br_target, br_lun,
					     read_buffer,
					     br_offset, br_length);
		} else {
			/*
			 * This is a buffer waited for by a user (sleeping)
			 * command
			 */
			wakeup(ccb);
		}

		/* We ALWAYS re-issue the same command; args are don't-care  */
		dpt_set_target(1, 0, 0, 0, 0, 0, 0, 0, 0);
		break;

	case SCSI_TM_WRITE_BUFFER:
		(ccb->wrbuff_callback) (dpt->unit, br_channel, br_target,
					br_offset, br_length,
					br_lun, ccb->status_packet.hba_stat);
		break;
	default:
		printf("dpt%d:  %s is an unsupported command for target mode\n",
		       dpt->unit, scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd));
	}
	ospl = splsoftcam();
	dpt->target_ccb[br_channel][br_target][br_lun] = NULL;
	dpt_Qpush_free(dpt, ccb);
	splx(ospl);
}


/*
 * Use this function to register a client for a buffer read target operation.
 * The function you register will be called every time a buffer is received
 * by the target mode code.
 */
dpt_rb_t
dpt_register_buffer(int unit, u_int8_t channel, u_int8_t target, u_int8_t lun,
		    u_int8_t mode, u_int16_t length, u_int16_t offset,
		    dpt_rec_buff callback, dpt_rb_op_t op)
{
	dpt_softc_t    *dpt;
	dpt_ccb_t      *ccb = NULL;
	int             ospl;

	for (dpt = TAILQ_FIRST(&dpt_softc_list);
	     dpt != NULL;
	     dpt = TAILQ_NEXT(dpt, links)) {
		if (dpt->unit == unit)
			goto valid_unit;
	}

	return (INVALID_UNIT);

valid_unit:

	if (dpt->state & DPT_HA_SHUTDOWN_ACTIVE)
		return (DRIVER_DOWN);

	if ((channel > (dpt->channels - 1)) || (target > (dpt->max_id - 1)) ||
	    (lun > (dpt->max_lun - 1)))
		return (INVALID_SENDER);

	if (dpt->buffer_receiver[channel][target][lun] == NULL) {
		if (op == REGISTER_BUFFER) {
			/* Assign the requested callback */
			dpt->buffer_receiver[channel][target][lun] = callback;
			/* Get a CCB */
			ospl = splsoftcam();

			/* Process the free list */
			if ((TAILQ_EMPTY(&dpt->free_ccbs)) && dpt_alloc_freelist(dpt)) {
				printf("dpt%d ERROR: Cannot allocate any more free CCB's.\n"
				       "             Please try later\n",
				       dpt->unit);
				splx(ospl);
				return (NO_RESOURCES);
			}
			/* Now grab the newest CCB */
			if ((ccb = dpt_Qpop_free(dpt)) == NULL) {
				splx(ospl);
				panic("dpt%d: Got a NULL CCB from pop_free()\n",
				      dpt->unit);
			}
			splx(ospl);

			/* Clean up the leftover of the previous tenant */
			ccb->status = DPT_CCB_STATE_NEW;
			dpt->target_ccb[channel][target][lun] = ccb;

			dpt->rw_buffer[channel][target][lun] =
				malloc(DPT_RW_BUFFER_SIZE, M_DEVBUF, M_NOWAIT);
			if (dpt->rw_buffer[channel][target][lun] == NULL) {
				printf("dpt%d: Failed to allocate "
				       "Target-Mode buffer\n", dpt->unit);
				ospl = splsoftcam();
				dpt_Qpush_free(dpt, ccb);
				splx(ospl);
				return (NO_RESOURCES);
			}
			dpt_set_target(0, dpt, channel, target, lun, mode,
				       length, offset, ccb);
			return (SUCCESSFULLY_REGISTERED);
		} else
			return (NOT_REGISTERED);
	} else {
		if (op == REGISTER_BUFFER) {
			if (dpt->buffer_receiver[channel][target][lun] == callback)
				return (ALREADY_REGISTERED);
			else
				return (REGISTERED_TO_ANOTHER);
		} else {
			if (dpt->buffer_receiver[channel][target][lun] == callback) {
				dpt->buffer_receiver[channel][target][lun] = NULL;
				ospl = splsoftcam();
				dpt_Qpush_free(dpt, ccb);
				splx(ospl);
				free(dpt->rw_buffer[channel][target][lun], M_DEVBUF);
				return (SUCCESSFULLY_REGISTERED);
			} else
				return (INVALID_CALLBACK);
		}

	}
}

/* Return the state of the blinking DPT LED's */
u_int8_t
dpt_blinking_led(dpt_softc_t * dpt)
{
	int             ndx;
	int             ospl;
	u_int32_t       state;
	u_int32_t       previous;
	u_int8_t        result;

	ospl = splcam();

	result = 0;

	for (ndx = 0, state = 0, previous = 0;
	     (ndx < 10) && (state != previous);
	     ndx++) {
		previous = state;
		state = dpt_inl(dpt, 1);
	}

	if ((state == previous) && (state == DPT_BLINK_INDICATOR))
		result = dpt_inb(dpt, 5);

	splx(ospl);
	return (result);
}

/*
 * Execute a command which did not come from the kernel's SCSI layer.
 * The only way to map user commands to bus and target is to comply with the
 * standard DPT wire-down scheme:
 */
int
dpt_user_cmd(dpt_softc_t * dpt, eata_pt_t * user_cmd,
	     caddr_t cmdarg, int minor_no)
{
	dpt_ccb_t *ccb;
	void	  *data;
	int	   channel, target, lun;
	int	   huh;
	int	   result;
	int	   ospl;
	int	   submitted;

	data = NULL;
	channel = minor2hba(minor_no);
	target = minor2target(minor_no);
	lun = minor2lun(minor_no);

	if ((channel > (dpt->channels - 1))
	 || (target > dpt->max_id)
	 || (lun > dpt->max_lun))
		return (ENXIO);

	if (target == dpt->sc_scsi_link[channel].adapter_targ) {
		/* This one is for the controller itself */
		if ((user_cmd->eataID[0] != 'E')
		 || (user_cmd->eataID[1] != 'A')
		 || (user_cmd->eataID[2] != 'T')
		 || (user_cmd->eataID[3] != 'A')) {
			return (ENXIO);
		}
	}
	/* Get a DPT CCB, so we can prepare a command */
	ospl = splsoftcam();

	/* Process the free list */
	if ((TAILQ_EMPTY(&dpt->free_ccbs)) && dpt_alloc_freelist(dpt)) {
		printf("dpt%d ERROR: Cannot allocate any more free CCB's.\n"
		       "             Please try later\n",
		       dpt->unit);
		splx(ospl);
		return (EFAULT);
	}
	/* Now grab the newest CCB */
	if ((ccb = dpt_Qpop_free(dpt)) == NULL) {
		splx(ospl);
		panic("dpt%d: Got a NULL CCB from pop_free()\n", dpt->unit);
	} else {
		splx(ospl);
		/* Clean up the leftover of the previous tenant */
		ccb->status = DPT_CCB_STATE_NEW;
	}

	bcopy((caddr_t) & user_cmd->command_packet, (caddr_t) & ccb->eata_ccb,
	      sizeof(eata_ccb_t));

	/* We do not want to do user specified scatter/gather.  Why?? */
	if (ccb->eata_ccb.scatter == 1)
		return (EINVAL);

	ccb->eata_ccb.Auto_Req_Sen = 1;
	ccb->eata_ccb.reqlen = htonl(sizeof(struct scsi_sense_data));
	ccb->eata_ccb.cp_datalen = htonl(sizeof(ccb->eata_ccb.cp_datalen));
	ccb->eata_ccb.cp_dataDMA = htonl(vtophys(ccb->eata_ccb.cp_dataDMA));
	ccb->eata_ccb.cp_statDMA = htonl(vtophys(&ccb->eata_ccb.cp_statDMA));
	ccb->eata_ccb.cp_reqDMA = htonl(vtophys(&ccb->eata_ccb.cp_reqDMA));
	ccb->eata_ccb.cp_viraddr = (u_int32_t) & ccb;

	if (ccb->eata_ccb.DataIn || ccb->eata_ccb.DataOut) {
		/* Data I/O is involved in this command.  Alocate buffer */
		if (ccb->eata_ccb.cp_datalen > PAGE_SIZE) {
			data = contigmalloc(ccb->eata_ccb.cp_datalen,
					    M_TEMP, M_WAITOK, 0, ~0,
					    ccb->eata_ccb.cp_datalen,
					    0x10000);
		} else {
			data = malloc(ccb->eata_ccb.cp_datalen, M_TEMP,
				      M_WAITOK);
		}

		if (data == NULL) {
			printf("dpt%d: Cannot allocate %d bytes "
			       "for EATA command\n", dpt->unit,
			       ccb->eata_ccb.cp_datalen);
			return (EFAULT);
		}
#define usr_cmd_DMA (caddr_t)user_cmd->command_packet.cp_dataDMA
		if (ccb->eata_ccb.DataIn == 1) {
			if (copyin(usr_cmd_DMA,
				   data, ccb->eata_ccb.cp_datalen) == -1)
				return (EFAULT);
		}
	} else {
		/* No data I/O involved here.  Make sure the DPT knows that */
		ccb->eata_ccb.cp_datalen = 0;
		data = NULL;
	}

	if (ccb->eata_ccb.FWNEST == 1)
		ccb->eata_ccb.FWNEST = 0;

	if (ccb->eata_ccb.cp_datalen != 0) {
		if (dpt_scatter_gather(dpt, ccb, ccb->eata_ccb.cp_datalen,
				       data) != 0) {
			if (data != NULL)
				free(data, M_TEMP);
			return (EFAULT);
		}
	}
	/**
	 * We are required to quiet a SCSI bus.
	 * since we do not queue comands on a bus basis,
	 * we wait for ALL commands on a controller to complete.
	 * In the mean time, sched_queue() will not schedule new commands.
	 */
	if ((ccb->eata_ccb.cp_cdb[0] == MULTIFUNCTION_CMD)
	    && (ccb->eata_ccb.cp_cdb[2] == BUS_QUIET)) {
		/* We wait for ALL traffic for this HBa to subside */
		ospl = splsoftcam();
		dpt->state |= DPT_HA_QUIET;
		splx(ospl);

		while ((submitted = dpt->submitted_ccbs_count) != 0) {
			huh = tsleep((void *) dpt, PCATCH | PRIBIO, "dptqt",
				     100 * hz);
			switch (huh) {
			case 0:
				/* Wakeup call received */
				break;
			case EWOULDBLOCK:
				/* Timer Expired */
				break;
			default:
				/* anything else */
				break;
			}
		}
	}
	/* Resume normal operation */
	if ((ccb->eata_ccb.cp_cdb[0] == MULTIFUNCTION_CMD)
	    && (ccb->eata_ccb.cp_cdb[2] == BUS_UNQUIET)) {
		ospl = splsoftcam();
		dpt->state &= ~DPT_HA_QUIET;
		splx(ospl);
	}
	/**
	 * Schedule the command and submit it.
	 * We bypass dpt_sched_queue, as it will block on DPT_HA_QUIET
	 */
	ccb->xs = NULL;
	ccb->flags = 0;
	ccb->eata_ccb.Auto_Req_Sen = 1;	/* We always want this feature */

	ccb->transaction_id = ++dpt->commands_processed;
	ccb->std_callback = (ccb_callback) dpt_user_cmd_done;
	ccb->result = (u_int32_t) & cmdarg;
	ccb->data = data;

#ifdef DPT_MEASURE_PERFORMANCE
	++dpt->performance.command_count[ccb->eata_ccb.cp_scsi_cmd];
	ccb->command_started = microtime_now;
#endif
	ospl = splcam();
	dpt_Qadd_waiting(dpt, ccb);
	splx(ospl);

	dpt_sched_queue(dpt);

	/* Wait for the command to complete */
	(void) tsleep((void *) ccb, PCATCH | PRIBIO, "dptucw", 100 * hz);

	/* Free allocated memory */
	if (data != NULL)
		free(data, M_TEMP);

	return (0);
}

static void
dpt_user_cmd_done(dpt_softc_t * dpt, int bus, dpt_ccb_t * ccb)
{
	int             ospl = splsoftcam();
	u_int32_t       result;
	caddr_t         cmd_arg;

	/**
	 * If Auto Request Sense is on, copyout the sense struct
	 */
#define usr_pckt_DMA 	(caddr_t)(intptr_t)ntohl(ccb->eata_ccb.cp_reqDMA)
#define usr_pckt_len	ntohl(ccb->eata_ccb.cp_datalen)
	if (ccb->eata_ccb.Auto_Req_Sen == 1) {
		if (copyout((caddr_t) & ccb->sense_data, usr_pckt_DMA,
			    sizeof(struct scsi_sense_data))) {
			ccb->result = EFAULT;
			dpt_Qpush_free(dpt, ccb);
			splx(ospl);
			wakeup(ccb);
			return;
		}
	}
	/* If DataIn is on, copyout the data */
	if ((ccb->eata_ccb.DataIn == 1)
	    && (ccb->status_packet.hba_stat == HA_NO_ERROR)) {
		if (copyout(ccb->data, usr_pckt_DMA, usr_pckt_len)) {
			dpt_Qpush_free(dpt, ccb);
			ccb->result = EFAULT;

			splx(ospl);
			wakeup(ccb);
			return;
		}
	}
	/* Copyout the status */
	result = ccb->status_packet.hba_stat;
	cmd_arg = (caddr_t) ccb->result;

	if (copyout((caddr_t) & result, cmd_arg, sizeof(result))) {
		dpt_Qpush_free(dpt, ccb);
		ccb->result = EFAULT;
		splx(ospl);
		wakeup(ccb);
		return;
	}
	/* Put the CCB back in the freelist */
	ccb->state |= DPT_CCB_STATE_COMPLETED;
	dpt_Qpush_free(dpt, ccb);

	/* Free allocated memory */
	splx(ospl);
	return;
}

#ifdef DPT_HANDLE_TIMEOUTS
/**
 * This function walks down the SUBMITTED queue.
 * Every request that is too old gets aborted and marked.
 * Since the DPT will complete (interrupt) immediately (what does that mean?),
 * We just walk the list, aborting old commands and marking them as such.
 * The dpt_complete function will get rid of the that were interrupted in the
 * normal manner.
 *
 * This function needs to run at splcam(), as it interacts with the submitted
 * queue, as well as the completed and free queues.  Just like dpt_intr() does.
 * To run it at any ISPL other than that of dpt_intr(), will mean that dpt_intr
 * willbe able to pre-empt it, grab a transaction in progress (towards
 * destruction) and operate on it.  The state of this transaction will be not
 * very clear.
 * The only other option, is to lock it only as long as necessary but have
 * dpt_intr() spin-wait on it. In a UP environment this makes no sense and in
 * a SMP environment, the advantage is dubvious for a function that runs once
 * every ten seconds for few microseconds and, on systems with healthy
 * hardware, does not do anything anyway.
 */

static void
dpt_handle_timeouts(dpt_softc_t * dpt)
{
	dpt_ccb_t      *ccb;
	int             ospl;

	ospl = splcam();

	if (dpt->state & DPT_HA_TIMEOUTS_ACTIVE) {
		printf("dpt%d WARNING: Timeout Handling Collision\n",
		       dpt->unit);
		splx(ospl);
		return;
	}
	dpt->state |= DPT_HA_TIMEOUTS_ACTIVE;

	/* Loop through the entire submitted queue, looking for lost souls */
	for (ccb = TAILQ_FIRST(&dpt->submitted_ccbs);
	     ccb != NULL;
	     ccb = TAILQ_NEXT(ccb, links)) {
		struct scsi_xfer *xs;
		u_int32_t       age, max_age;

		xs = ccb->xs;
		age = dpt_time_delta(ccb->command_started, microtime_now);

#define TenSec	10000000

		if (xs == NULL) {	/* Local, non-kernel call */
			max_age = TenSec;
		} else {
			max_age = (((xs->timeout * (dpt->submitted_ccbs_count
						    + DPT_TIMEOUT_FACTOR))
				    > TenSec)
				 ? (xs->timeout * (dpt->submitted_ccbs_count
						   + DPT_TIMEOUT_FACTOR))
				   : TenSec);
		}

		/*
		 * If a transaction is marked lost and is TWICE as old as we
		 * care, then, and only then do we destroy it!
		 */
		if (ccb->state & DPT_CCB_STATE_MARKED_LOST) {
			/* Remember who is next */
			if (age > (max_age * 2)) {
				dpt_Qremove_submitted(dpt, ccb);
				ccb->state &= ~DPT_CCB_STATE_MARKED_LOST;
				ccb->state |= DPT_CCB_STATE_ABORTED;
#define cmd_name scsi_cmd_name(ccb->eata_ccb.cp_scsi_cmd)
				if (ccb->retries++ > DPT_RETRIES) {
					printf("dpt%d ERROR: Destroying stale "
					       "%d (%s)\n"
					       "		on "
					       "c%db%dt%du%d (%d/%d)\n",
					     dpt->unit, ccb->transaction_id,
					       cmd_name,
					       dpt->unit,
					       ccb->eata_ccb.cp_channel,
					       ccb->eata_ccb.cp_id,
					       ccb->eata_ccb.cp_LUN, age,
					       ccb->retries);
#define send_ccb &ccb->eata_ccb
#define ESA	 EATA_SPECIFIC_ABORT
					(void) dpt_send_immediate(dpt,
								  send_ccb,
								  ESA,
								  0, 0);
					dpt_Qpush_free(dpt, ccb);

					/* The SCSI layer should re-try */
					xs->error |= XS_TIMEOUT;
					xs->flags |= SCSI_ITSDONE;
					scsi_done(xs);
				} else {
					printf("dpt%d ERROR: Stale %d (%s) on "
					       "c%db%dt%du%d (%d)\n"
					     "		gets another "
					       "chance(%d/%d)\n",
					     dpt->unit, ccb->transaction_id,
					       cmd_name,
					       dpt->unit,
					       ccb->eata_ccb.cp_channel,
					       ccb->eata_ccb.cp_id,
					       ccb->eata_ccb.cp_LUN,
					    age, ccb->retries, DPT_RETRIES);

					dpt_Qpush_waiting(dpt, ccb);
					dpt_sched_queue(dpt);
				}
			}
		} else {
			/*
			 * This is a transaction that is not to be destroyed
			 * (yet) But it is too old for our liking. We wait as
			 * long as the upper layer thinks. Not really, we
			 * multiply that by the number of commands in the
			 * submitted queue + 1.
			 */
			if (!(ccb->state & DPT_CCB_STATE_MARKED_LOST) &&
			    (age != ~0) && (age > max_age)) {
				printf("dpt%d ERROR: Marking %d (%s) on "
				       "c%db%dt%du%d \n"
				       "            as late after %dusec\n",
				       dpt->unit, ccb->transaction_id,
				       cmd_name,
				       dpt->unit, ccb->eata_ccb.cp_channel,
				       ccb->eata_ccb.cp_id,
				       ccb->eata_ccb.cp_LUN, age);
				ccb->state |= DPT_CCB_STATE_MARKED_LOST;
			}
		}
	}

	dpt->state &= ~DPT_HA_TIMEOUTS_ACTIVE;
	splx(ospl);
}

static void
dpt_timeout(void *arg)
{
	dpt_softc_t    *dpt = (dpt_softc_t *) arg;

	if (!(dpt->state & DPT_HA_TIMEOUTS_ACTIVE))
		dpt_handle_timeouts(dpt);

	timeout(dpt_timeout, (caddr_t) dpt, hz * 10);
}

#endif				/* DPT_HANDLE_TIMEOUTS */

#endif
