/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2000 BSDi
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

/********************************************************************************
 ********************************************************************************
                                                     Driver Parameter Definitions
 ********************************************************************************
 ********************************************************************************/

/*
 * The firmware interface allows for a 16-bit command identifier.  A lookup
 * table this size (256k) would be too expensive, so we cap ourselves at a
 * reasonable limit.
 */
#define MLY_MAXCOMMANDS		256	/* max outstanding commands per controller, limit 65535 */

/*
 * The firmware interface allows for a 16-bit s/g list length.  We limit 
 * ourselves to a reasonable maximum and ensure alignment.
 */
#define MLY_MAXSGENTRIES	64	/* max S/G entries, limit 65535 */		

/********************************************************************************
 ********************************************************************************
                                                      Driver Variable Definitions
 ********************************************************************************
 ********************************************************************************/

#if __FreeBSD_version >= 500005
# include <sys/taskqueue.h>
#endif

/*
 * Debugging levels:
 *  0 - quiet, only emit warnings
 *  1 - noisy, emit major function points and things done
 *  2 - extremely noisy, emit trace items in loops, etc.
 */
#ifdef MLY_DEBUG
# define debug(level, fmt, args...)	do { if (level <= MLY_DEBUG) printf("%s: " fmt "\n", __FUNCTION__ , ##args); } while(0)
# define debug_called(level)		do { if (level <= MLY_DEBUG) printf(__FUNCTION__ ": called\n"); } while(0)
# define debug_struct(s)		printf("  SIZE %s: %d\n", #s, sizeof(struct s))
# define debug_union(s)			printf("  SIZE %s: %d\n", #s, sizeof(union s))
# define debug_field(s, f)		printf("  OFFSET %s.%s: %d\n", #s, #f, ((int)&(((struct s *)0)->f)))
extern void		mly_printstate0(void);
extern struct mly_softc	*mly_softc0;
#else
# define debug(level, fmt, args...)
# define debug_called(level)
# define debug_struct(s)
#endif

#define mly_printf(sc, fmt, args...)	device_printf(sc->mly_dev, fmt , ##args)

/*
 * Per-device structure, used to save persistent state on devices.
 *
 * Note that this isn't really Bus/Target/Lun since we don't support
 * lun != 0 at this time.
 */
struct mly_btl {
    int			mb_flags;
#define MLY_BTL_PHYSICAL	(1<<0)		/* physical device */
#define MLY_BTL_LOGICAL		(1<<1)		/* logical device */
#define MLY_BTL_PROTECTED	(1<<2)		/* device is protected - I/O not allowed */
#define MLY_BTL_RESCAN		(1<<3)		/* device needs to be rescanned */
    char		mb_name[16];		/* peripheral attached to this device */
    int			mb_state;		/* see 8.1 */
    int			mb_type;		/* see 8.2 */
};

/*
 * Per-command control structure.
 */
struct mly_command {
    TAILQ_ENTRY(mly_command)	mc_link;	/* list linkage */

    struct mly_softc		*mc_sc;		/* controller that owns us */
    u_int16_t			mc_slot;	/* command slot we occupy */
    int				mc_flags;
#define MLY_CMD_STATEMASK	((1<<8)-1)
#define MLY_CMD_STATE(mc)	((mc)->mc_flags & MLY_CMD_STATEMASK)
#define MLY_CMD_SETSTATE(mc, s)	((mc)->mc_flags = ((mc)->mc_flags &= ~MLY_CMD_STATEMASK) | (s))
#define MLY_CMD_FREE		0		/* command is on the free list */
#define MLY_CMD_SETUP		1		/* command is being built */
#define MLY_CMD_BUSY		2		/* command is being run, or ready to run, or not completed */
#define MLY_CMD_COMPLETE	3		/* command has been completed */
#define MLY_CMD_SLOTTED		(1<<8)		/* command has a slot number */
#define MLY_CMD_MAPPED		(1<<9)		/* command has had its data mapped */
#define MLY_CMD_PRIORITY	(1<<10)		/* allow use of "priority" slots */
#define MLY_CMD_DATAIN		(1<<11)		/* data moves controller->system */
#define MLY_CMD_DATAOUT		(1<<12)		/* data moves system->controller */
    u_int16_t			mc_status;	/* command completion status */
    u_int8_t			mc_sense;	/* sense data length */
    int32_t			mc_resid;	/* I/O residual count */

    union mly_command_packet	*mc_packet;	/* our controller command */
    u_int64_t			mc_packetphys;	/* physical address of the mapped packet */

    void			*mc_data;	/* data buffer */
    size_t			mc_length;	/* data length */
    bus_dmamap_t		mc_datamap;	/* DMA map for data */

    void	(* mc_complete)(struct mly_command *mc);	/* completion handler */
    void	*mc_private;					/* caller-private data */

};

/*
 * Command slot regulation.
 *
 * We can't use slot 0 due to the memory mailbox implementation.
 */
#define MLY_SLOT_START		1
#define MLY_SLOT_MAX		(MLY_SLOT_START + MLY_MAXCOMMANDS)

/*
 * Command/command packet cluster.
 *
 * Due to the difficulty of using the zone allocator to create a new
 * zone from within a module, we use our own clustering to reduce 
 * memory wastage caused by allocating lots of these small structures.
 *
 * Note that it is possible to require more than MLY_MAXCOMMANDS 
 * command structures.
 *
 * Since we may need to allocate extra clusters at any time, and since this
 * process needs to allocate a physically contiguous slab of controller
 * addressible memory in which to place the command packets, do not allow more
 * command packets in a cluster than will fit in a page.
 */
#define MLY_CMD_CLUSTERCOUNT	(PAGE_SIZE / sizeof(union mly_command_packet))

struct mly_command_cluster {
    TAILQ_ENTRY(mly_command_cluster)	mcc_link;
    union mly_command_packet		*mcc_packet;
    bus_dmamap_t			mcc_packetmap;
    u_int64_t				mcc_packetphys;
    struct mly_command			mcc_command[MLY_CMD_CLUSTERCOUNT];
};

/*
 * Per-controller structure.
 */
struct mly_softc {
    /* bus connections */
    device_t		mly_dev;
    struct resource	*mly_regs_resource;	/* register interface window */
    int			mly_regs_rid;		/* resource ID */
    bus_space_handle_t	mly_bhandle;		/* bus space handle */
    bus_space_tag_t	mly_btag;		/* bus space tag */
    bus_dma_tag_t	mly_parent_dmat;	/* parent DMA tag */
    bus_dma_tag_t	mly_buffer_dmat;	/* data buffer/command DMA tag */
    struct resource	*mly_irq;		/* interrupt */
    int			mly_irq_rid;
    void		*mly_intr;		/* interrupt handle */

    /* scatter/gather lists and their controller-visible mappings */
    struct mly_sg_entry	*mly_sg_table;		/* s/g lists */
    u_int32_t		mly_sg_busaddr;		/* s/g table base address in bus space */
    bus_dma_tag_t	mly_sg_dmat;		/* s/g buffer DMA tag */
    bus_dmamap_t	mly_sg_dmamap;		/* map for s/g buffers */

    /* controller hardware interface */
    int			mly_hwif;
#define MLY_HWIF_I960RX		0
#define MLY_HWIF_STRONGARM	1
    u_int8_t		mly_doorbell_true;	/* xor map to make hardware doorbell 'true' bits into 1s */
    u_int8_t		mly_command_mailbox;	/* register offsets */
    u_int8_t		mly_status_mailbox;
    u_int8_t		mly_idbr;
    u_int8_t		mly_odbr;
    u_int8_t		mly_error_status;
    u_int8_t		mly_interrupt_status;
    u_int8_t		mly_interrupt_mask;
    struct mly_mmbox	*mly_mmbox;			/* kernel-space address of memory mailbox */
    u_int64_t		mly_mmbox_busaddr;		/* bus-space address of memory mailbox */
    bus_dma_tag_t	mly_mmbox_dmat;			/* memory mailbox DMA tag */
    bus_dmamap_t	mly_mmbox_dmamap;		/* memory mailbox DMA map */
    u_int32_t		mly_mmbox_command_index;	/* next slot to use */
    u_int32_t		mly_mmbox_status_index;		/* slot we next expect status in */

    /* controller features, limits and status */
    int			mly_state;
#define MLY_STATE_SUSPEND	(1<<0)
#define	MLY_STATE_OPEN		(1<<1)
#define MLY_STATE_INTERRUPTS_ON	(1<<2)
#define MLY_STATE_MMBOX_ACTIVE	(1<<3)
    int			mly_max_commands;	/* max parallel commands we allow */
    struct mly_ioctl_getcontrollerinfo	*mly_controllerinfo;
    struct mly_param_controller		*mly_controllerparam;
    struct mly_btl			mly_btl[MLY_MAX_CHANNELS][MLY_MAX_TARGETS];

    /* command management */
    struct mly_command		*mly_busycmds[MLY_SLOT_MAX];	/* busy commands */
    int				mly_busy_count;
    int				mly_last_slot;
    TAILQ_HEAD(,mly_command)	mly_freecmds;		/* commands available for reuse */
    TAILQ_HEAD(,mly_command)	mly_ready;		/* commands ready to be submitted */
    TAILQ_HEAD(,mly_command)	mly_completed;		/* commands which have been returned by the controller */
    TAILQ_HEAD(,mly_command_cluster)	mly_clusters;	/* command memory blocks */
    bus_dma_tag_t		mly_packet_dmat;	/* command packet DMA tag */

    /* health monitoring */
    u_int32_t			mly_event_change;	/* event status change indicator */
    u_int32_t			mly_event_counter;	/* next event for which we anticpiate status */
    u_int32_t			mly_event_waiting;	/* next event the controller will post status for */
    struct callout_handle	mly_periodic;		/* periodic event handling */

    /* CAM connection */
    TAILQ_HEAD(,ccb_hdr)	mly_cam_ccbq;			/* outstanding I/O from CAM */
    struct cam_sim		*mly_cam_sim[MLY_MAX_CHANNELS];
    int				mly_cam_lowbus;

#if __FreeBSD_version >= 500005
    /* command-completion task */
    struct task		mly_task_complete;	/* deferred-completion task */
#endif
};

/*
 * Register access helpers.
 */
#define MLY_SET_REG(sc, reg, val)	bus_space_write_1(sc->mly_btag, sc->mly_bhandle, reg, val)
#define MLY_GET_REG(sc, reg)		bus_space_read_1 (sc->mly_btag, sc->mly_bhandle, reg)
#define MLY_GET_REG2(sc, reg)		bus_space_read_2 (sc->mly_btag, sc->mly_bhandle, reg)
#define MLY_GET_REG4(sc, reg)		bus_space_read_4 (sc->mly_btag, sc->mly_bhandle, reg)

#define MLY_SET_MBOX(sc, mbox, ptr)									\
	do {												\
	    bus_space_write_4(sc->mly_btag, sc->mly_bhandle, mbox,      *((u_int32_t *)ptr));		\
	    bus_space_write_4(sc->mly_btag, sc->mly_bhandle, mbox +  4, *((u_int32_t *)ptr + 1));	\
	    bus_space_write_4(sc->mly_btag, sc->mly_bhandle, mbox +  8, *((u_int32_t *)ptr + 2));	\
	    bus_space_write_4(sc->mly_btag, sc->mly_bhandle, mbox + 12, *((u_int32_t *)ptr + 3));	\
	} while(0);
#define MLY_GET_MBOX(sc, mbox, ptr)									\
	do {												\
	    *((u_int32_t *)ptr) = bus_space_read_4(sc->mly_btag, sc->mly_bhandle, mbox);		\
	    *((u_int32_t *)ptr + 1) = bus_space_read_4(sc->mly_btag, sc->mly_bhandle, mbox + 4);	\
	    *((u_int32_t *)ptr + 2) = bus_space_read_4(sc->mly_btag, sc->mly_bhandle, mbox + 8);	\
	    *((u_int32_t *)ptr + 3) = bus_space_read_4(sc->mly_btag, sc->mly_bhandle, mbox + 12);	\
	} while(0);

#define MLY_IDBR_TRUE(sc, mask)								\
	((((MLY_GET_REG((sc), (sc)->mly_idbr)) ^ (sc)->mly_doorbell_true) & (mask)) == (mask))
#define MLY_ODBR_TRUE(sc, mask)								\
	((MLY_GET_REG((sc), (sc)->mly_odbr) & (mask)) == (mask))
#define MLY_ERROR_VALID(sc)								\
	((((MLY_GET_REG((sc), (sc)->mly_error_status)) ^ (sc)->mly_doorbell_true) & (MLY_MSG_EMPTY)) == 0)

#define MLY_MASK_INTERRUPTS(sc)								\
	do {										\
	    MLY_SET_REG((sc), (sc)->mly_interrupt_mask, MLY_INTERRUPT_MASK_DISABLE);	\
	    sc->mly_state &= ~MLY_STATE_INTERRUPTS_ON;					\
	} while(0);
#define MLY_UNMASK_INTERRUPTS(sc)							\
	do {										\
	    MLY_SET_REG((sc), (sc)->mly_interrupt_mask, MLY_INTERRUPT_MASK_ENABLE);	\
	    sc->mly_state |= MLY_STATE_INTERRUPTS_ON;					\
	} while(0);

/*
 * Logical device number -> bus/target translation
 */
#define MLY_LOGDEV_BUS(sc, x)	(((x) / MLY_MAX_TARGETS) + (sc)->mly_controllerinfo->physical_channels_present)
#define MLY_LOGDEV_TARGET(x)	((x) % MLY_MAX_TARGETS)

/*
 * Public functions/variables
 */
/* mly.c */
extern int	mly_attach(struct mly_softc *sc);
extern void	mly_detach(struct mly_softc *sc);
extern void	mly_free(struct mly_softc *sc);
extern void	mly_startio(struct mly_softc *sc);
extern void	mly_done(struct mly_softc *sc);
extern int	mly_alloc_command(struct mly_softc *sc, struct mly_command **mcp);
extern void	mly_release_command(struct mly_command *mc);

/* mly_cam.c */
extern int	mly_cam_attach(struct mly_softc *sc);
extern void	mly_cam_detach(struct mly_softc *sc);
extern int	mly_cam_command(struct mly_softc *sc, struct mly_command **mcp);
extern int	mly_name_device(struct mly_softc *sc, int bus, int target);

/********************************************************************************
 * Queue primitives
 *
 * These are broken out individually to make statistics gathering easier.
 */

static __inline void
mly_enqueue_ready(struct mly_command *mc)
{
    int		s;

    s = splcam();
    TAILQ_INSERT_TAIL(&mc->mc_sc->mly_ready, mc, mc_link);
    MLY_CMD_SETSTATE(mc, MLY_CMD_BUSY);
    splx(s);
}

static __inline void
mly_requeue_ready(struct mly_command *mc)
{
    int		s;

    s = splcam();
    TAILQ_INSERT_HEAD(&mc->mc_sc->mly_ready, mc, mc_link);
    splx(s);
}

static __inline struct mly_command *
mly_dequeue_ready(struct mly_softc *sc)
{
    struct mly_command	*mc;
    int			s;

    s = splcam();
    if ((mc = TAILQ_FIRST(&sc->mly_ready)) != NULL)
	TAILQ_REMOVE(&sc->mly_ready, mc, mc_link);
    splx(s);
    return(mc);
}

static __inline void
mly_enqueue_completed(struct mly_command *mc)
{
    int		s;

    s = splcam();
    TAILQ_INSERT_TAIL(&mc->mc_sc->mly_completed, mc, mc_link);
    /* don't set MLY_CMD_COMPLETE here to avoid wakeup race */
    splx(s);
}

static __inline struct mly_command *
mly_dequeue_completed(struct mly_softc *sc)
{
    struct mly_command	*mc;
    int			s;

    s = splcam();
    if ((mc = TAILQ_FIRST(&sc->mly_completed)) != NULL)
	TAILQ_REMOVE(&sc->mly_completed, mc, mc_link);
    splx(s);
    return(mc);
}

static __inline void
mly_enqueue_free(struct mly_command *mc)
{
    int		s;

    s = splcam();
    TAILQ_INSERT_HEAD(&mc->mc_sc->mly_freecmds, mc, mc_link);
    MLY_CMD_SETSTATE(mc, MLY_CMD_FREE);
    splx(s);
}

static __inline struct mly_command *
mly_dequeue_free(struct mly_softc *sc)
{
    struct mly_command	*mc;
    int			s;

    s = splcam();
    if ((mc = TAILQ_FIRST(&sc->mly_freecmds)) != NULL)
	TAILQ_REMOVE(&sc->mly_freecmds, mc, mc_link);
    splx(s);
    return(mc);
}

static __inline void
mly_enqueue_cluster(struct mly_softc *sc, struct mly_command_cluster *mcc)
{
    int		s;

    s = splcam();
    TAILQ_INSERT_HEAD(&sc->mly_clusters, mcc, mcc_link);
    splx(s);
}

static __inline struct mly_command_cluster *
mly_dequeue_cluster(struct mly_softc *sc)
{
    struct mly_command_cluster	*mcc;
    int				s;

    s = splcam();
    if ((mcc = TAILQ_FIRST(&sc->mly_clusters)) != NULL)
	TAILQ_REMOVE(&sc->mly_clusters, mcc, mcc_link);
    splx(s);
    return(mcc);
}


