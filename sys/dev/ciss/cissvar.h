/*-
 * Copyright (c) 2001 Michael Smith
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
 *	$FreeBSD: src/sys/dev/ciss/cissvar.h,v 1.11 2007/05/01 05:13:15 scottl Exp $
 */

/*
 * CISS adapter driver datastructures
 */

/************************************************************************
 * Tunable parameters
 */

/*
 * There is no guaranteed upper bound on the number of concurrent
 * commands an adapter may claim to support.  Cap it at a reasonable
 * value.
 */
#define CISS_MAX_REQUESTS	256

/*
 * Maximum number of logical drives we support.
 */
#define CISS_MAX_LOGICAL	15

/*
 * Maximum number of physical devices we support.
 */
#define CISS_MAX_PHYSICAL	1024

/*
 * Interrupt reduction can be controlled by tuning the interrupt
 * coalesce delay and count paramters.  The delay (in microseconds)
 * defers delivery of interrupts to increase the chance of there being
 * more than one completed command ready when the interrupt is
 * delivered.  The count expedites the delivery of the interrupt when
 * the given number of commands are ready.
 *
 * If the delay is set to 0, interrupts are delivered immediately.
 */
#define CISS_INTERRUPT_COALESCE_DELAY	1000
#define CISS_INTERRUPT_COALESCE_COUNT	16

/*
 * Heartbeat routine timeout in seconds.  Note that since event
 * handling is performed on a callback basis, we don't need this to
 * run very often.
 */
#define CISS_HEARTBEAT_RATE		10

/************************************************************************
 * Compatibility with older versions of FreeBSD
 */
#if __FreeBSD_version < 440001
#warning testing old-FreeBSD compat
typedef struct proc	d_thread_t;
#endif

/************************************************************************
 * Command queue statistics
 */

#define CISSQ_FREE	0
#define CISSQ_BUSY	1
#define CISSQ_COMPLETE	2
#define CISSQ_NOTIFY	3
#define CISSQ_COUNT	4

struct ciss_qstat
{
    u_int32_t	q_length;
    u_int32_t	q_max;
};

/************************************************************************
 * Driver version.  Only really significant to the ACU interface.
 */
#define CISS_DRIVER_VERSION	20011201

/************************************************************************
 * Driver data structures
 */

/*
 * Each command issued to the adapter is managed by a request
 * structure.
 */
struct ciss_request
{
    TAILQ_ENTRY(ciss_request)	cr_link;
    int				cr_onq;		/* which queue we are on */

    struct ciss_softc		*cr_sc;		/* controller softc */
    void			*cr_data;	/* data buffer */
    u_int32_t			cr_length;	/* data length */
    bus_dmamap_t		cr_datamap;	/* DMA map for data */
    int				cr_tag;
    int				cr_flags;
#define CISS_REQ_MAPPED		(1<<0)		/* data mapped */
#define CISS_REQ_SLEEP		(1<<1)		/* submitter sleeping */
#define CISS_REQ_POLL		(1<<2)		/* submitter polling */
#define CISS_REQ_DATAOUT	(1<<3)		/* data host->adapter */
#define CISS_REQ_DATAIN		(1<<4)		/* data adapter->host */

    void			(* cr_complete)(struct ciss_request *);
    void			*cr_private;
};

/*
 * The adapter command structure is defined with a zero-length
 * scatter/gather list size.  In practise, we want space for a
 * scatter-gather list, and we also want to avoid having commands
 * cross page boundaries.
 *
 * Note that 512 bytes yields 28 scatter/gather entries, or the
 * ability to map (26 * PAGE_SIZE) + 2 bytes of data.  On x86, this is
 * 104kB.  256 bytes would only yield 12 entries, giving a mere 40kB,
 * too small.
 */

#define CISS_COMMAND_ALLOC_SIZE		512	/* XXX tune to get sensible s/g list length */
#define CISS_COMMAND_SG_LENGTH	((CISS_COMMAND_ALLOC_SIZE - sizeof(struct ciss_command)) \
				 / sizeof(struct ciss_sg_entry))

/*
 * Per-logical-drive data.
 */
struct ciss_ldrive
{
    union ciss_device_address	cl_address;
    union ciss_device_address	*cl_controller;
    int				cl_status;
#define CISS_LD_NONEXISTENT	0
#define CISS_LD_ONLINE		1
#define CISS_LD_OFFLINE		2

    int				cl_update;

    struct ciss_bmic_id_ldrive	*cl_ldrive;
    struct ciss_bmic_id_lstatus	*cl_lstatus;
    struct ciss_ldrive_geometry	cl_geometry;

    char			cl_name[16];		/* device name */
};

/*
 * Per-physical-drive data
 */
struct ciss_pdrive
{
    union ciss_device_address	cp_address;
    int				cp_online;
};

#define CISS_PHYSICAL_SHIFT	5
#define CISS_PHYSICAL_BASE	(1 << CISS_PHYSICAL_SHIFT)
#define CISS_MAX_PHYSTGT	15

#define CISS_IS_PHYSICAL(bus)	(bus >= CISS_PHYSICAL_BASE)
#define CISS_CAM_TO_PBUS(bus)	(bus - CISS_PHYSICAL_BASE)

/*
 * Per-adapter data
 */
struct ciss_softc
{
    /* bus connections */
    device_t			ciss_dev;		/* bus attachment */
    struct cdev *ciss_dev_t;		/* control device */

    struct resource		*ciss_regs_resource;	/* register interface window */
    int				ciss_regs_rid;		/* resource ID */
    bus_space_handle_t		ciss_regs_bhandle;	/* bus space handle */
    bus_space_tag_t		ciss_regs_btag;		/* bus space tag */

    struct resource		*ciss_cfg_resource;	/* config struct interface window */
    int				ciss_cfg_rid;		/* resource ID */
    struct ciss_config_table	*ciss_cfg;		/* config table in adapter memory */
    struct ciss_bmic_id_table	*ciss_id;		/* ID table in host memory */
    u_int32_t			ciss_heartbeat;		/* last heartbeat value */
    int				ciss_heart_attack;	/* number of times we have seen this value */

    struct resource		*ciss_irq_resource;	/* interrupt */
    int				ciss_irq_rid;		/* resource ID */
    void			*ciss_intr;		/* interrupt handle */

    bus_dma_tag_t		ciss_parent_dmat;	/* parent DMA tag */
    bus_dma_tag_t		ciss_buffer_dmat;	/* data buffer/command DMA tag */

    u_int32_t			ciss_interrupt_mask;	/* controller interrupt mask bits */

    int				ciss_max_requests;
    struct ciss_request		ciss_request[CISS_MAX_REQUESTS];	/* requests */
    void			*ciss_command;		/* command structures */
    bus_dma_tag_t		ciss_command_dmat;	/* command DMA tag */
    bus_dmamap_t		ciss_command_map;	/* command DMA map */
    u_int32_t			ciss_command_phys;	/* command array base address */
    TAILQ_HEAD(,ciss_request)	ciss_free;		/* requests available for reuse */
    TAILQ_HEAD(,ciss_request)	ciss_busy;		/* requests in the adapter */
    TAILQ_HEAD(,ciss_request)	ciss_complete;		/* requests which have been returned by the adapter */
    TAILQ_HEAD(,ciss_request)	ciss_notify;		/* requests which are defered for processing */
    struct proc			*ciss_notify_thread;

    struct callout		ciss_periodic;		/* periodic event handling */
    struct ciss_request		*ciss_periodic_notify;	/* notify callback request */

    struct mtx			ciss_mtx;
    struct ciss_ldrive		**ciss_logical;
    struct ciss_pdrive		**ciss_physical;
    union ciss_device_address	*ciss_controllers;	/* controller address */
    int				ciss_max_bus_number;	/* maximum bus number */
    int				ciss_max_logical_bus;
    int				ciss_max_physical_bus;

    struct cam_devq		*ciss_cam_devq;
    struct cam_sim		**ciss_cam_sim;

    int				ciss_soft_reset;

    int				ciss_flags;
#define CISS_FLAG_NOTIFY_OK	(1<<0)		/* notify command running OK */
#define CISS_FLAG_CONTROL_OPEN	(1<<1)		/* control device is open */
#define CISS_FLAG_ABORTING	(1<<2)		/* driver is going away */
#define CISS_FLAG_RUNNING	(1<<3)		/* driver is running (interrupts usable) */

#define CISS_FLAG_FAKE_SYNCH	(1<<16)		/* needs SYNCHRONISE_CACHE faked */
#define CISS_FLAG_BMIC_ABORT	(1<<17)		/* use BMIC command to abort Notify on Event */
#define CISS_FLAG_THREAD_SHUT	(1<<20)		/* shutdown the kthread */

    struct ciss_qstat		ciss_qstat[CISSQ_COUNT];	/* queue statistics */
};

/*
 * Given a request tag, find the corresponding command in virtual or
 * physical space.
 *
 * The arithmetic here is due to the allocation of ciss_command structures
 * inside CISS_COMMAND_ALLOC_SIZE blocks.  See the comment at the definition
 * of CISS_COMMAND_ALLOC_SIZE above.
 */
#define CISS_FIND_COMMAND(cr)							\
	(struct ciss_command *)((u_int8_t *)(cr)->cr_sc->ciss_command +		\
				((cr)->cr_tag * CISS_COMMAND_ALLOC_SIZE))
#define CISS_FIND_COMMANDPHYS(cr)	((cr)->cr_sc->ciss_command_phys + \
					 ((cr)->cr_tag * CISS_COMMAND_ALLOC_SIZE))

/************************************************************************
 * Debugging/diagnostic output.
 */

/*
 * Debugging levels:
 *  0 - quiet, only emit warnings
 *  1 - talkative, log major events, but nothing on the I/O path
 *  2 - noisy, log events on the I/O path
 *  3 - extremely noisy, log items in loops
 */
#ifdef CISS_DEBUG
# define debug(level, fmt, args...)							\
	do {										\
	    if (level <= CISS_DEBUG) printf("%s: " fmt "\n", __func__ , ##args);	\
	} while(0)
# define debug_called(level)						\
	do {								\
	    if (level <= CISS_DEBUG) printf("%s: called\n", __func__);	\
	} while(0)
# define debug_struct(s)		printf("  SIZE %s: %zu\n", #s, sizeof(struct s))
# define debug_union(s)			printf("  SIZE %s: %zu\n", #s, sizeof(union s))
# define debug_type(s)			printf("  SIZE %s: %zu\n", #s, sizeof(s))
# define debug_field(s, f)		printf("  OFFSET %s.%s: %d\n", #s, #f, ((int)&(((struct s *)0)->f)))
# define debug_const(c)			printf("  CONST %s %jd/0x%jx\n", #c, (intmax_t)c, (intmax_t)c);
#else
# define debug(level, fmt, args...)
# define debug_called(level)
# define debug_struct(s)
# define debug_union(s)
# define debug_type(s)
# define debug_field(s, f)
# define debug_const(c)
#endif

#define ciss_printf(sc, fmt, args...)	device_printf(sc->ciss_dev, fmt , ##args)

/************************************************************************
 * Queue primitives
 */

#define CISSQ_ADD(sc, qname)					\
	do {							\
	    struct ciss_qstat *qs = &(sc)->ciss_qstat[qname];	\
								\
	    qs->q_length++;					\
	    if (qs->q_length > qs->q_max)			\
		qs->q_max = qs->q_length;			\
	} while(0)

#define CISSQ_REMOVE(sc, qname)    (sc)->ciss_qstat[qname].q_length--
#define CISSQ_INIT(sc, qname)			\
	do {					\
	    sc->ciss_qstat[qname].q_length = 0;	\
	    sc->ciss_qstat[qname].q_max = 0;	\
	} while(0)


#define CISSQ_REQUEST_QUEUE(name, index)				\
static __inline void							\
ciss_initq_ ## name (struct ciss_softc *sc)				\
{									\
    TAILQ_INIT(&sc->ciss_ ## name);					\
    CISSQ_INIT(sc, index);						\
}									\
static __inline void							\
ciss_enqueue_ ## name (struct ciss_request *cr)				\
{									\
    int		s;							\
									\
    s = splcam();							\
    TAILQ_INSERT_TAIL(&cr->cr_sc->ciss_ ## name, cr, cr_link);		\
    CISSQ_ADD(cr->cr_sc, index);					\
    cr->cr_onq = index;							\
    splx(s);								\
}									\
static __inline void							\
ciss_requeue_ ## name (struct ciss_request *cr)				\
{									\
    int		s;							\
									\
    s = splcam();							\
    TAILQ_INSERT_HEAD(&cr->cr_sc->ciss_ ## name, cr, cr_link);		\
    CISSQ_ADD(cr->cr_sc, index);					\
    cr->cr_onq = index;							\
    splx(s);								\
}									\
static __inline struct ciss_request *					\
ciss_dequeue_ ## name (struct ciss_softc *sc)				\
{									\
    struct ciss_request	*cr;						\
    int			s;						\
									\
    s = splcam();							\
    if ((cr = TAILQ_FIRST(&sc->ciss_ ## name)) != NULL) {		\
	TAILQ_REMOVE(&sc->ciss_ ## name, cr, cr_link);			\
	CISSQ_REMOVE(sc, index);					\
	cr->cr_onq = -1;						\
    }									\
    splx(s);								\
    return(cr);								\
}									\
static __inline int							\
ciss_remove_ ## name (struct ciss_request *cr)				\
{									\
    int			s, error;					\
									\
    s = splcam();							\
    if (cr->cr_onq != index) {						\
	printf("request on queue %d (expected %d)\n", cr->cr_onq, index);\
	error = 1;							\
    } else {								\
	TAILQ_REMOVE(&cr->cr_sc->ciss_ ## name, cr, cr_link);		\
	CISSQ_REMOVE(cr->cr_sc, index);					\
	cr->cr_onq = -1;						\
	error = 0;							\
    }									\
    splx(s);								\
    return(error);							\
}									\
struct hack

CISSQ_REQUEST_QUEUE(free, CISSQ_FREE);
CISSQ_REQUEST_QUEUE(busy, CISSQ_BUSY);
CISSQ_REQUEST_QUEUE(complete, CISSQ_COMPLETE);
CISSQ_REQUEST_QUEUE(notify, CISSQ_NOTIFY);

/********************************************************************************
 * space-fill a character string
 */
static __inline void
padstr(char *targ, const char *src, int len)
{
    while (len-- > 0) {
	if (*src != 0) {
	    *targ++ = *src++;
	} else {
	    *targ++ = ' ';
	}
    }
}
