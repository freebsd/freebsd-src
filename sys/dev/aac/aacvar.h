/*-
 * Copyright (c) 2000 Michael Smith
 * Copyright (c) 2001 Scott Long
 * Copyright (c) 2000 BSDi
 * Copyright (c) 2001 Adaptec, Inc.
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

#include <sys/bio.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/taskqueue.h>
#include <sys/selinfo.h>
#include <geom/geom_disk.h>


/*
 * Driver Parameter Definitions
 */

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
 * FIBs are allocated in page-size chunks and can grow up to the 512
 * limit imposed by the hardware.
 */
#define AAC_FIB_COUNT		(PAGE_SIZE/sizeof(struct aac_fib))
#define AAC_PREALLOCATE_FIBS	128
#define AAC_MAX_FIBS		504

/*
 * The controller reports status events in AIFs.  We hang on to a number of
 * these in order to pass them out to user-space management tools.
 */
#define AAC_AIFQ_LENGTH		64

/*
 * Firmware messages are passed in the printf buffer.
 */
#define AAC_PRINTF_BUFSIZE	256

/*
 * We wait this many seconds for the adapter to come ready if it is still 
 * booting
 */
#define AAC_BOOT_TIMEOUT	(3 * 60)

/*
 * Timeout for immediate commands.
 */
#define AAC_IMMEDIATE_TIMEOUT	30		/* seconds */

/*
 * Timeout for normal commands
 */
#define AAC_CMD_TIMEOUT		30		/* seconds */

/*
 * Rate at which we periodically check for timed out commands and kick the
 * controller.
 */
#define AAC_PERIODIC_INTERVAL	20		/* seconds */

/*
 * Per-container data structure
 */
struct aac_container
{
	struct aac_mntobj		co_mntobj;
	device_t			co_disk;
	int				co_found;
	TAILQ_ENTRY(aac_container)	co_link;
};

/*
 * Per-SIM data structure
 */
struct aac_sim
{
	device_t		sim_dev;
	int			TargetsPerBus;
	int			BusNumber;
	int			InitiatorBusId;
	struct aac_softc	*aac_sc;
	TAILQ_ENTRY(aac_sim)	sim_link;
};

/*
 * Per-disk structure
 */
struct aac_disk 
{
	device_t			ad_dev;
	struct aac_softc		*ad_controller;
	struct aac_container		*ad_container;
	struct disk			*ad_disk;
	int				ad_flags;
#define AAC_DISK_OPEN	(1<<0)
	int				ad_cylinders;
	int				ad_heads;
	int				ad_sectors;
	u_int32_t			ad_size;
	int				unit;
};

/*
 * Per-command control structure.
 */
struct aac_command
{
	TAILQ_ENTRY(aac_command) cm_link;	/* list linkage */

	struct aac_softc	*cm_sc;		/* controller that owns us */

	struct aac_fib		*cm_fib;	/* FIB associated with this
						 * command */
	u_int32_t		cm_fibphys;	/* bus address of the FIB */
	struct bio		*cm_data;	/* pointer to data in kernel
						 * space */
	u_int32_t		cm_datalen;	/* data length */
	bus_dmamap_t		cm_datamap;	/* DMA map for bio data */
	struct aac_sg_table	*cm_sgtable;	/* pointer to s/g table in
						 * command */
	int			cm_flags;
#define AAC_CMD_MAPPED		(1<<0)		/* command has had its data
						 * mapped */
#define AAC_CMD_DATAIN		(1<<1)		/* command involves data moving
						 * from controller to host */
#define AAC_CMD_DATAOUT		(1<<2)		/* command involves data moving
						 * from host to controller */
#define AAC_CMD_COMPLETED	(1<<3)		/* command has been completed */
#define AAC_CMD_TIMEDOUT	(1<<4)		/* command taken too long */
#define AAC_ON_AACQ_FREE	(1<<5)
#define AAC_ON_AACQ_READY	(1<<6)
#define AAC_ON_AACQ_BUSY	(1<<7)
#define AAC_ON_AACQ_MASK	((1<<5)|(1<<6)|(1<<7))
#define AAC_QUEUE_FRZN		(1<<9)		/* Freeze the processing of
						 * commands on the queue. */

	void			(* cm_complete)(struct aac_command *cm);
	void			*cm_private;
	time_t			cm_timestamp;	/* command creation time */
	int			cm_queue;
	int			cm_index;
};

struct aac_fibmap {
	TAILQ_ENTRY(aac_fibmap) fm_link;	/* list linkage */
	struct aac_fib		*aac_fibs;
	bus_dmamap_t		aac_fibmap;
	struct aac_command	*aac_commands;
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
	u_int8_t		ac_qbuf[sizeof(struct aac_queue_table) +
				AAC_QUEUE_ALIGN];

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
	int	(*aif_get_fwstatus)(struct aac_softc *sc);
	void	(*aif_qnotify)(struct aac_softc *sc, int qbit);
	int	(*aif_get_istatus)(struct aac_softc *sc);
	void	(*aif_clr_istatus)(struct aac_softc *sc, int mask);
	void	(*aif_set_mailbox)(struct aac_softc *sc, u_int32_t command,
				   u_int32_t arg0, u_int32_t arg1,
				   u_int32_t arg2, u_int32_t arg3);
	int	(*aif_get_mailbox)(struct aac_softc *sc, int mb);
	void	(*aif_set_interrupts)(struct aac_softc *sc, int enable);
};
extern struct aac_interface	aac_rx_interface;
extern struct aac_interface	aac_sa_interface;
extern struct aac_interface	aac_fa_interface;
extern struct aac_interface	aac_rkt_interface;

#define AAC_GET_FWSTATUS(sc)		((sc)->aac_if.aif_get_fwstatus((sc)))
#define AAC_QNOTIFY(sc, qbit)		((sc)->aac_if.aif_qnotify((sc), (qbit)))
#define AAC_GET_ISTATUS(sc)		((sc)->aac_if.aif_get_istatus((sc)))
#define AAC_CLEAR_ISTATUS(sc, mask)	((sc)->aac_if.aif_clr_istatus((sc), \
					(mask)))
#define AAC_SET_MAILBOX(sc, command, arg0, arg1, arg2, arg3) \
	((sc)->aac_if.aif_set_mailbox((sc), (command), (arg0), (arg1), (arg2), \
	(arg3)))
#define AAC_GET_MAILBOX(sc, mb)		((sc)->aac_if.aif_get_mailbox((sc), \
					(mb)))
#define	AAC_MASK_INTERRUPTS(sc)		((sc)->aac_if.aif_set_interrupts((sc), \
					0))
#define AAC_UNMASK_INTERRUPTS(sc)	((sc)->aac_if.aif_set_interrupts((sc), \
					1))

#define AAC_SETREG4(sc, reg, val)	bus_space_write_4(sc->aac_btag, \
					sc->aac_bhandle, reg, val)
#define AAC_GETREG4(sc, reg)		bus_space_read_4 (sc->aac_btag, \
					sc->aac_bhandle, reg)
#define AAC_SETREG2(sc, reg, val)	bus_space_write_2(sc->aac_btag, \
					sc->aac_bhandle, reg, val)
#define AAC_GETREG2(sc, reg)		bus_space_read_2 (sc->aac_btag, \
					sc->aac_bhandle, reg)
#define AAC_SETREG1(sc, reg, val)	bus_space_write_1(sc->aac_btag, \
					sc->aac_bhandle, reg, val)
#define AAC_GETREG1(sc, reg)		bus_space_read_1 (sc->aac_btag, \
					sc->aac_bhandle, reg)

/*
 * Per-controller structure.
 */
struct aac_softc 
{
	/* bus connections */
	device_t		aac_dev;
	struct resource		*aac_regs_resource;	/* register interface
							 * window */
	int			aac_regs_rid;		/* resource ID */
	bus_space_handle_t	aac_bhandle;		/* bus space handle */
	bus_space_tag_t		aac_btag;		/* bus space tag */
	bus_dma_tag_t		aac_parent_dmat;	/* parent DMA tag */
	bus_dma_tag_t		aac_buffer_dmat;	/* data buffer/command
							 * DMA tag */
	struct resource		*aac_irq;		/* interrupt */
	int			aac_irq_rid;
	void			*aac_intr;		/* interrupt handle */
	eventhandler_tag	eh;

	/* controller features, limits and status */
	int			aac_state;
#define AAC_STATE_SUSPEND	(1<<0)
#define	AAC_STATE_OPEN		(1<<1)
#define AAC_STATE_INTERRUPTS_ON	(1<<2)
#define AAC_STATE_AIF_SLEEPER	(1<<3)
	struct FsaRevision		aac_revision;

	/* controller hardware interface */
	int			aac_hwif;
#define AAC_HWIF_I960RX		0
#define AAC_HWIF_STRONGARM	1
#define AAC_HWIF_FALCON		2
#define AAC_HWIF_RKT		3
#define AAC_HWIF_UNKNOWN	-1
	bus_dma_tag_t		aac_common_dmat;	/* common structure
							 * DMA tag */
	bus_dmamap_t		aac_common_dmamap;	/* common structure
							 * DMA map */
	struct aac_common	*aac_common;
	u_int32_t		aac_common_busaddr;
	struct aac_interface	aac_if;

	/* command/fib resources */
	bus_dma_tag_t		aac_fib_dmat;	/* DMA tag for allocing FIBs */
	TAILQ_HEAD(,aac_fibmap)	aac_fibmap_tqh;
	u_int			total_fibs;
	struct aac_command	*aac_commands;

	/* command management */
	TAILQ_HEAD(,aac_command) aac_free;	/* command structures 
						 * available for reuse */
	TAILQ_HEAD(,aac_command) aac_ready;	/* commands on hold for
						 * controller resources */
	TAILQ_HEAD(,aac_command) aac_busy;
	struct bio_queue_head	aac_bioq;
	struct aac_queue_table	*aac_queues;
	struct aac_queue_entry	*aac_qentries[AAC_QUEUE_COUNT];

	struct aac_qstat	aac_qstat[AACQ_COUNT];	/* queue statistics */

	/* connected containters */
	TAILQ_HEAD(,aac_container)	aac_container_tqh;
	struct mtx		aac_container_lock;

	/*
	 * The general I/O lock.  This protects the sync fib, the lists, the
	 * queues, and the registers.
	 */
	struct mtx		aac_io_lock;

	/* delayed activity infrastructure */
	struct task		aac_task_complete;	/* deferred-completion
							 * task */
	struct intr_config_hook	aac_ich;

	/* management interface */
	struct cdev *aac_dev_t;
	struct mtx		aac_aifq_lock;
	struct aac_aif_command	aac_aifq[AAC_AIFQ_LENGTH];
	int			aac_aifq_head;
	int			aac_aifq_tail;
	struct selinfo		rcv_select;
	struct proc		*aifthread;
	int			aifflags;
#define AAC_AIFFLAGS_RUNNING	(1 << 0)
#define AAC_AIFFLAGS_AIF	(1 << 1)
#define	AAC_AIFFLAGS_EXIT	(1 << 2)
#define AAC_AIFFLAGS_EXITED	(1 << 3)
#define AAC_AIFFLAGS_PRINTF	(1 << 4)
#define	AAC_AIFFLAGS_ALLOCFIBS	(1 << 5)
#define AAC_AIFFLAGS_PENDING	(AAC_AIFFLAGS_AIF | AAC_AIFFLAGS_PRINTF | \
				 AAC_AIFFLAGS_ALLOCFIBS)
	u_int32_t		flags;
#define AAC_FLAGS_PERC2QC	(1 << 0)
#define	AAC_FLAGS_ENABLE_CAM	(1 << 1)	/* No SCSI passthrough */
#define	AAC_FLAGS_CAM_NORESET	(1 << 2)	/* Fake SCSI resets */
#define	AAC_FLAGS_CAM_PASSONLY	(1 << 3)	/* Only create pass devices */
#define	AAC_FLAGS_SG_64BIT	(1 << 4)	/* Use 64-bit S/G addresses */
#define	AAC_FLAGS_4GB_WINDOW	(1 << 5)	/* Device can access host mem
						 * 2GB-4GB range */
#define	AAC_FLAGS_NO4GB		(1 << 6)	/* Can't access host mem >2GB */
#define	AAC_FLAGS_256FIBS	(1 << 7)	/* Can only do 256 commands */
#define	AAC_FLAGS_BROKEN_MEMMAP (1 << 8)	/* Broken HostPhysMemPages */

	u_int32_t		supported_options;
	int			aac_max_fibs;
	u_int32_t		scsi_method_id;
	TAILQ_HEAD(,aac_sim)	aac_sim_tqh;
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
extern void		aac_submit_bio(struct bio *bp);
extern void		aac_biodone(struct bio *bp);
extern void		aac_startio(struct aac_softc *sc);
extern int		aac_alloc_command(struct aac_softc *sc,
					  struct aac_command **cmp);
extern void		aac_release_command(struct aac_command *cm);
extern int		aac_sync_fib(struct aac_softc *sc, u_int32_t command,
				     u_int32_t xferstate, struct aac_fib *fib,
				     u_int16_t datasize);

/*
 * Debugging levels:
 *  0 - quiet, only emit warnings
 *  1 - noisy, emit major function points and things done
 *  2 - extremely noisy, emit trace items in loops, etc.
 */
#ifdef AAC_DEBUG
# define debug(level, fmt, args...)					\
	do {								\
	if (level <=AAC_DEBUG) printf("%s: " fmt "\n", __func__ , ##args); \
	} while (0)
# define debug_called(level)						\
	do {								\
	if (level <= AAC_DEBUG) printf("%s: called\n", __func__);	\
	} while (0)

extern void	aac_print_queues(struct aac_softc *sc);
extern void	aac_panic(struct aac_softc *sc, char *reason);
extern void	aac_print_fib(struct aac_softc *sc, struct aac_fib *fib,
			      const char *caller);
extern void	aac_print_aif(struct aac_softc *sc,
			      struct aac_aif_command *aif);

#define AAC_PRINT_FIB(sc, fib)	aac_print_fib(sc, fib, __func__)

#else
# define debug(level, fmt, args...)
# define debug_called(level)

# define aac_print_queues(sc)
# define aac_panic(sc, reason)

# define AAC_PRINT_FIB(sc, fib)
# define aac_print_aif(sc, aac_aif_command)
#endif

struct aac_code_lookup {
	char	*string;
	u_int32_t	code;
};

/*
 * Queue primitives for driver queues.
 */
#define AACQ_ADD(sc, qname)					\
	do {							\
		struct aac_qstat *qs;				\
								\
		qs = &(sc)->aac_qstat[qname];			\
								\
		qs->q_length++;					\
		if (qs->q_length > qs->q_max)			\
			qs->q_max = qs->q_length;		\
	} while (0)

#define AACQ_REMOVE(sc, qname)    (sc)->aac_qstat[qname].q_length--
#define AACQ_INIT(sc, qname)				\
	do {						\
		sc->aac_qstat[qname].q_length = 0;	\
		sc->aac_qstat[qname].q_max = 0;		\
	} while (0)


#define AACQ_COMMAND_QUEUE(name, index)					\
static __inline void							\
aac_initq_ ## name (struct aac_softc *sc)				\
{									\
	TAILQ_INIT(&sc->aac_ ## name);					\
	AACQ_INIT(sc, index);						\
}									\
static __inline void							\
aac_enqueue_ ## name (struct aac_command *cm)				\
{									\
	if ((cm->cm_flags & AAC_ON_AACQ_MASK) != 0) {			\
		printf("command %p is on another queue, flags = %#x\n",	\
		       cm, cm->cm_flags);				\
		panic("command is on another queue");			\
	}								\
	TAILQ_INSERT_TAIL(&cm->cm_sc->aac_ ## name, cm, cm_link);	\
	cm->cm_flags |= AAC_ON_ ## index;				\
	AACQ_ADD(cm->cm_sc, index);					\
}									\
static __inline void							\
aac_requeue_ ## name (struct aac_command *cm)				\
{									\
	if ((cm->cm_flags & AAC_ON_AACQ_MASK) != 0) {			\
		printf("command %p is on another queue, flags = %#x\n",	\
		       cm, cm->cm_flags);				\
		panic("command is on another queue");			\
	}								\
	TAILQ_INSERT_HEAD(&cm->cm_sc->aac_ ## name, cm, cm_link);	\
	cm->cm_flags |= AAC_ON_ ## index;				\
	AACQ_ADD(cm->cm_sc, index);					\
}									\
static __inline struct aac_command *					\
aac_dequeue_ ## name (struct aac_softc *sc)				\
{									\
	struct aac_command *cm;						\
									\
	if ((cm = TAILQ_FIRST(&sc->aac_ ## name)) != NULL) {		\
		if ((cm->cm_flags & AAC_ON_ ## index) == 0) {		\
			printf("command %p not in queue, flags = %#x, "	\
		       	       "bit = %#x\n", cm, cm->cm_flags,		\
			       AAC_ON_ ## index);			\
			panic("command not in queue");			\
		}							\
		TAILQ_REMOVE(&sc->aac_ ## name, cm, cm_link);		\
		cm->cm_flags &= ~AAC_ON_ ## index;			\
		AACQ_REMOVE(sc, index);					\
	}								\
	return(cm);							\
}									\
static __inline void							\
aac_remove_ ## name (struct aac_command *cm)				\
{									\
	if ((cm->cm_flags & AAC_ON_ ## index) == 0) {			\
		printf("command %p not in queue, flags = %#x, "		\
		       "bit = %#x\n", cm, cm->cm_flags, 		\
		       AAC_ON_ ## index);				\
		panic("command not in queue");				\
	}								\
	TAILQ_REMOVE(&cm->cm_sc->aac_ ## name, cm, cm_link);		\
	cm->cm_flags &= ~AAC_ON_ ## index;				\
	AACQ_REMOVE(cm->cm_sc, index);					\
}									\
struct hack

AACQ_COMMAND_QUEUE(free, AACQ_FREE);
AACQ_COMMAND_QUEUE(ready, AACQ_READY);
AACQ_COMMAND_QUEUE(busy, AACQ_BUSY);

/*
 * outstanding bio queue
 */
static __inline void
aac_initq_bio(struct aac_softc *sc)
{
	bioq_init(&sc->aac_bioq);
	AACQ_INIT(sc, AACQ_BIO);
}

static __inline void
aac_enqueue_bio(struct aac_softc *sc, struct bio *bp)
{
	bioq_insert_tail(&sc->aac_bioq, bp);
	AACQ_ADD(sc, AACQ_BIO);
}

static __inline struct bio *
aac_dequeue_bio(struct aac_softc *sc)
{
	struct bio *bp;

	if ((bp = bioq_first(&sc->aac_bioq)) != NULL) {
		bioq_remove(&sc->aac_bioq, bp);
		AACQ_REMOVE(sc, AACQ_BIO);
	}
	return(bp);
}

static __inline void
aac_print_printf(struct aac_softc *sc)
{
	/*
	 * XXX We have the ability to read the length of the printf string
	 * from out of the mailboxes.
	 */
	device_printf(sc->aac_dev, "**Monitor** %.*s", AAC_PRINTF_BUFSIZE,
		      sc->aac_common->ac_printf);
	sc->aac_common->ac_printf[0] = 0;
	AAC_QNOTIFY(sc, AAC_DB_PRINTF);
}

static __inline int
aac_alloc_sync_fib(struct aac_softc *sc, struct aac_fib **fib)
{

	mtx_lock(&sc->aac_io_lock);
	*fib = &sc->aac_common->ac_sync_fib;
	return (0);
}

static __inline void
aac_release_sync_fib(struct aac_softc *sc)
{

	mtx_unlock(&sc->aac_io_lock);
}

