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
 * The firmware interface allows for a 16-bit s/g list length.  We limit 
 * ourselves to a reasonable maximum and ensure alignment.
 */
#define AAC_MAXSGENTRIES	64	/* max S/G entries, limit 65535 */		

/*
 * We allocate a small set of FIBs for the adapter to use to send us messages.
 */
#define AAC_ADAPTER_FIBS	8

/*
 * FIBs are allocated in clusters as we need them; each cluster must be physically
 * contiguous.  Set the number of FIBs to try to allocate in a cluster.
 * Setting this value too high may result in FIBs not being available in conditions
 * of high load with fragmented physical memory.  The value must be a multiple of
 * (PAGE_SIZE / 512).
 */
#define AAC_CLUSTER_COUNT	64

/*
 * The controller reports status events in AIFs.  We hang on to a number of these
 * in order to pass them out to user-space management tools.
 */
#define AAC_AIFQ_LENGTH		64

/*
 * Firmware messages are passed in the printf buffer.
 */
#define AAC_PRINTF_BUFSIZE	256

/*
 * We wait this many seconds for the adapter to come ready if it is still booting
 */
#define AAC_BOOT_TIMEOUT	(3 * 60)

/*
 * Timeout for immediate commands.
 */
#define AAC_IMMEDIATE_TIMEOUT	30

/*
 * Character device major numbers.
 */
#define AAC_DISK_MAJOR	200

/********************************************************************************
 ********************************************************************************
                                                      Driver Variable Definitions
 ********************************************************************************
 ********************************************************************************/

#if __FreeBSD_version >= 500005
# include <sys/taskqueue.h>
#endif

/*
 * Per-container data structure
 */
struct aac_container
{
    struct aac_mntobj	co_mntobj;
    device_t		co_disk;
};

/*
 * Per-disk structure
 */
struct aac_disk 
{
    device_t			ad_dev;
    dev_t			ad_dev_t;
    struct aac_softc		*ad_controller;
    struct aac_container	*ad_container;
    struct disk			ad_disk;
    struct devstat		ad_stats;
    struct disklabel		ad_label;
    int				ad_flags;
#define AAC_DISK_OPEN	(1<<0)
    int				ad_cylinders;
    int				ad_heads;
    int				ad_sectors;
    u_int32_t			ad_size;
};

/*
 * Per-command control structure.
 */
struct aac_command
{
    TAILQ_ENTRY(aac_command)	cm_link;	/* list linkage */

    struct aac_softc		*cm_sc;		/* controller that owns us */

    struct aac_fib		*cm_fib;	/* FIB associated with this command */
    u_int32_t			cm_fibphys;	/* bus address of the FIB */
    struct bio			*cm_data;	/* pointer to data in kernel space */
    u_int32_t			cm_datalen;	/* data length */
    bus_dmamap_t		cm_datamap;	/* DMA map for bio data */
    struct aac_sg_table		*cm_sgtable;	/* pointer to s/g table in command */

    int				cm_flags;
#define AAC_CMD_MAPPED		(1<<0)		/* command has had its data mapped */
#define AAC_CMD_DATAIN		(1<<1)		/* command involves data moving from controller to host */
#define AAC_CMD_DATAOUT		(1<<2)		/* command involves data moving from host to controller */
#define AAC_CMD_COMPLETED	(1<<3)		/* command has been completed */

    void			(* cm_complete)(struct aac_command *cm);
    void			*cm_private;
};

/*
 * Command/command packet cluster.
 *
 * Due to the difficulty of using the zone allocator to create a new
 * zone from within a module, we use our own clustering to reduce 
 * memory wastage due to allocating lots of these small structures.
 */
struct aac_command_cluster
{
    TAILQ_ENTRY(aac_command_cluster)	cmc_link;
    struct aac_fib			*cmc_fibs;
    bus_dmamap_t			cmc_fibmap;
    u_int32_t				cmc_fibphys;
    struct aac_command			cmc_command[AAC_CLUSTER_COUNT];
};

/*
 * We gather a number of adapter-visible items into a single structure.
 *
 * The ordering of this strucure may be important; we copy the Linux driver:
 *
 * Adapter FIBs
 * Init struct
 * Queue headers (Comm Area)
 * Printf buffer
 *
 * In addition, we add:
 * Sync Fib
 */
struct aac_common {
    /* fibs for the controller to send us messages */
    struct aac_fib		ac_fibs[AAC_ADAPTER_FIBS];

    /* the init structure */
    struct aac_adapter_init	ac_init;

    /* arena within which the queue structures are kept */
    u_int8_t			ac_qbuf[sizeof(struct aac_queue_table) + AAC_QUEUE_ALIGN];

    /* buffer for text messages from the controller */
    char		       	ac_printf[AAC_PRINTF_BUFSIZE];
    
    /* fib for synchronous commands */
    struct aac_fib		ac_sync_fib;
};

/*
 * Interface operations
 */
struct aac_interface 
{
    int		(* aif_get_fwstatus)(struct aac_softc *sc);
    void	(* aif_qnotify)(struct aac_softc *sc, int qbit);
    int		(* aif_get_istatus)(struct aac_softc *sc);
    void	(* aif_set_istatus)(struct aac_softc *sc, int mask);
    void	(* aif_set_mailbox)(struct aac_softc *sc, u_int32_t command,
				    u_int32_t arg0, u_int32_t arg1, u_int32_t arg2, u_int32_t arg3);
    int		(* aif_get_mailboxstatus)(struct aac_softc *sc);
    void	(* aif_set_interrupts)(struct aac_softc *sc, int enable);
};
extern struct aac_interface	aac_rx_interface;
extern struct aac_interface	aac_sa_interface;

#define AAC_GET_FWSTATUS(sc)		((sc)->aac_if.aif_get_fwstatus((sc)))
#define AAC_QNOTIFY(sc, qbit)		((sc)->aac_if.aif_qnotify((sc), (qbit)))
#define AAC_GET_ISTATUS(sc)		((sc)->aac_if.aif_get_istatus((sc)))
#define AAC_CLEAR_ISTATUS(sc, mask)	((sc)->aac_if.aif_set_istatus((sc), (mask)))
#define AAC_SET_MAILBOX(sc, command, arg0, arg1, arg2, arg3) \
	((sc)->aac_if.aif_set_mailbox((sc), (command), (arg0), (arg1), (arg2), (arg3)))
#define AAC_GET_MAILBOXSTATUS(sc)	((sc)->aac_if.aif_get_mailboxstatus((sc)))
#define	AAC_MASK_INTERRUPTS(sc)		((sc)->aac_if.aif_set_interrupts((sc), 0))
#define AAC_UNMASK_INTERRUPTS(sc)	((sc)->aac_if.aif_set_interrupts((sc), 1))

#define AAC_SETREG4(sc, reg, val)	bus_space_write_4(sc->aac_btag, sc->aac_bhandle, reg, val)
#define AAC_GETREG4(sc, reg)		bus_space_read_4 (sc->aac_btag, sc->aac_bhandle, reg)
#define AAC_SETREG2(sc, reg, val)	bus_space_write_2(sc->aac_btag, sc->aac_bhandle, reg, val)
#define AAC_GETREG2(sc, reg)		bus_space_read_2 (sc->aac_btag, sc->aac_bhandle, reg)
#define AAC_SETREG1(sc, reg, val)	bus_space_write_1(sc->aac_btag, sc->aac_bhandle, reg, val)
#define AAC_GETREG1(sc, reg)		bus_space_read_1 (sc->aac_btag, sc->aac_bhandle, reg)

/*
 * Per-controller structure.
 */
struct aac_softc 
{
    /* bus connections */
    device_t		aac_dev;
    struct resource	*aac_regs_resource;	/* register interface window */
    int			aac_regs_rid;		/* resource ID */
    bus_space_handle_t	aac_bhandle;		/* bus space handle */
    bus_space_tag_t	aac_btag;		/* bus space tag */
    bus_dma_tag_t	aac_parent_dmat;	/* parent DMA tag */
    bus_dma_tag_t	aac_buffer_dmat;	/* data buffer/command DMA tag */
    struct resource	*aac_irq;		/* interrupt */
    int			aac_irq_rid;
    void		*aac_intr;		/* interrupt handle */

    /* controller features, limits and status */
    int			aac_state;
#define AAC_STATE_SUSPEND	(1<<0)
#define	AAC_STATE_OPEN		(1<<1)
#define AAC_STATE_INTERRUPTS_ON	(1<<2)
#define AAC_STATE_AIF_SLEEPER	(1<<3)
    struct FsaRevision	aac_revision;

    /* controller hardware interface */
    int				aac_hwif;
#define AAC_HWIF_I960RX		0
#define AAC_HWIF_STRONGARM	1
    bus_dma_tag_t		aac_common_dmat;	/* common structure DMA tag */
    bus_dmamap_t		aac_common_dmamap;	/* common structure DMA map */
    struct aac_common		*aac_common;
    u_int32_t			aac_common_busaddr;
    struct aac_interface	aac_if;

    /* command/fib resources */
    TAILQ_HEAD(,aac_command_cluster)	aac_clusters;	/* command memory blocks */
    bus_dma_tag_t			aac_fib_dmat;	/* DMA tag for allocating FIBs */

    /* command management */
    TAILQ_HEAD(,aac_command)	aac_freecmds;	/* command structures available for reuse */
    TAILQ_HEAD(,aac_command)	aac_ready;	/* commands on hold for controller resources */
    TAILQ_HEAD(,aac_command)	aac_completed;	/* commands which have been returned by the controller */
    struct bio_queue_head	aac_bioq;
    struct aac_queue_table	*aac_queues;
    struct aac_queue_entry	*aac_qentries[AAC_QUEUE_COUNT];

    /* connected containters */
    struct aac_container	aac_container[AAC_MAX_CONTAINERS];

    /* delayed activity infrastructure */
#if __FreeBSD_version >= 500005
    struct task			aac_task_complete;	/* deferred-completion task */
#endif
    struct intr_config_hook	aac_ich;

    /* management interface */
    dev_t			aac_dev_t;
    struct aac_aif_command	aac_aifq[AAC_AIFQ_LENGTH];
    int				aac_aifq_head;
    int				aac_aifq_tail;
};


/*
 * Public functions
 */
extern void		aac_free(struct aac_softc *sc);
extern int		aac_attach(struct aac_softc *sc);
extern int		aac_detach(device_t dev);
extern int		aac_shutdown(device_t dev);
extern int		aac_suspend(device_t dev); 
extern int		aac_resume(device_t dev);
extern void		aac_intr(void *arg);
extern devclass_t	aac_devclass;
extern void		aac_submit_bio(struct bio *bp);
extern void		aac_complete_bio(struct bio *bp);

/*
 * Debugging levels:
 *  0 - quiet, only emit warnings
 *  1 - noisy, emit major function points and things done
 *  2 - extremely noisy, emit trace items in loops, etc.
 */
#ifdef AAC_DEBUG
#define debug(level, fmt, args...)	do { if (level <= AAC_DEBUG) printf("%s: " fmt "\n", __FUNCTION__ , ##args); } while(0)
#define debug_called(level)		do { if (level <= AAC_DEBUG) printf(__FUNCTION__ ": called\n"); } while(0)

extern void	aac_print_queues(struct aac_softc *sc);
extern void	aac_panic(struct aac_softc *sc, char *reason);
extern void	aac_print_fib(struct aac_softc *sc, struct aac_fib *fib, char *caller);
extern void	aac_print_aif(struct aac_softc *sc, struct aac_aif_command *aif);

#define AAC_PRINT_FIB(sc, fib)	aac_print_fib(sc, fib, __FUNCTION__)

#else
#define debug(level, fmt, args...)
#define debug_called(level)

#define aac_print_queues(sc)
#define aac_panic(sc, reason)
#define aac_print_aif(sc, aif)

#define AAC_PRINT_FIB(sc, fib)
#endif

struct aac_code_lookup {
    char	*string;
    u_int32_t	code;
};

/*
 * Borrowed from <struct.h>
 */
/* Offset of the field in the structure. */
#define fldoff(name, field) \
        ((int)&(((struct name *)0)->field))

/********************************************************************************
 * Queue primitives
 *
 * These are broken out individually to make statistics gathering easier.
 */

static __inline void
aac_enqueue_ready(struct aac_command *cm)
{
    int		s;

    s = splbio();
    TAILQ_INSERT_TAIL(&cm->cm_sc->aac_ready, cm, cm_link);
    splx(s);
}

static __inline void
aac_requeue_ready(struct aac_command *cm)
{
    int		s;

    s = splbio();
    TAILQ_INSERT_HEAD(&cm->cm_sc->aac_ready, cm, cm_link);
    splx(s);
}

static __inline struct aac_command *
aac_dequeue_ready(struct aac_softc *sc)
{
    struct aac_command	*cm;
    int			s;

    s = splbio();
    if ((cm = TAILQ_FIRST(&sc->aac_ready)) != NULL)
	TAILQ_REMOVE(&sc->aac_ready, cm, cm_link);
    splx(s);
    return(cm);
}

static __inline void
aac_enqueue_completed(struct aac_command *cm)
{
    int		s;

    s = splbio();
    TAILQ_INSERT_TAIL(&cm->cm_sc->aac_completed, cm, cm_link);
    splx(s);
}

static __inline struct aac_command *
aac_dequeue_completed(struct aac_softc *sc)
{
    struct aac_command	*cm;
    int			s;

    s = splbio();
    if ((cm = TAILQ_FIRST(&sc->aac_completed)) != NULL)
	TAILQ_REMOVE(&sc->aac_completed, cm, cm_link);
    splx(s);
    return(cm);
}

static __inline void
aac_enqueue_free(struct aac_command *cm)
{
    int		s;

    s = splbio();
    TAILQ_INSERT_HEAD(&cm->cm_sc->aac_freecmds, cm, cm_link);
    splx(s);
}

static __inline struct aac_command *
aac_dequeue_free(struct aac_softc *sc)
{
    struct aac_command	*cm;
    int			s;

    s = splbio();
    if ((cm = TAILQ_FIRST(&sc->aac_freecmds)) != NULL)
	TAILQ_REMOVE(&sc->aac_freecmds, cm, cm_link);
    splx(s);
    return(cm);
}

static __inline void
aac_enqueue_cluster(struct aac_softc *sc, struct aac_command_cluster *cmc)
{
    int		s;

    s = splbio();
    TAILQ_INSERT_HEAD(&sc->aac_clusters, cmc, cmc_link);
    splx(s);
}

static __inline struct aac_command_cluster *
aac_dequeue_cluster(struct aac_softc *sc)
{
    struct aac_command_cluster	*cmc;
    int				s;

    s = splbio();
    if ((cmc = TAILQ_FIRST(&sc->aac_clusters)) != NULL)
	TAILQ_REMOVE(&sc->aac_clusters, cmc, cmc_link);
    splx(s);
    return(cmc);
}
