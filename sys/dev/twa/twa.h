/*-
 * Copyright (c) 2003-04 3ware, Inc.
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

/*
 * 3ware driver for 9000 series storage controllers.
 *
 * Author: Vinod Kashyap
 */


#define TWA_DRIVER_VERSION_STRING		"2.50.02.012"

#define TWA_REQUEST_TIMEOUT_PERIOD		60 /* seconds */
#define TWA_MESSAGE_SOURCE_CONTROLLER_ERROR	3
#define TWA_MESSAGE_SOURCE_CONTROLLER_EVENT	4
#define TWA_MESSAGE_SOURCE_FREEBSD_DRIVER	6
#define TWA_MESSAGE_SOURCE_FREEBSD_OS		9

#define TWA_MALLOC_CLASS			M_TWA

/* Macros for bus-space calls. */
#define TWA_READ_REGISTER(sc, offset)		\
	(u_int32_t)bus_space_read_4(sc->twa_bus_tag, sc->twa_bus_handle, offset)
#define TWA_WRITE_REGISTER(sc, offset, val)	\
	bus_space_write_4(sc->twa_bus_tag, sc->twa_bus_handle, offset, (u_int32_t)val)

/* Possible values of tr->tr_status. */
#define TWA_CMD_SETUP		0x0	/* being assembled */
#define TWA_CMD_BUSY		0x1	/* submitted to controller */
#define TWA_CMD_PENDING		0x2	/* in pending queue */
#define TWA_CMD_COMPLETE	0x3	/* completed by controller (maybe with error) */

/* Possible values of tr->tr_flags. */
#define TWA_CMD_DATA_IN			(1<<0)	/* read request */
#define TWA_CMD_DATA_OUT		(1<<1)	/* write request */
#define TWA_CMD_DATA_COPY_NEEDED	(1<<2)	/* data in ccb is misaligned, have to copy to/from private buffer */
#define TWA_CMD_SLEEP_ON_REQUEST	(1<<3)	/* owner is sleeping on this command */
#define TWA_CMD_MAPPED			(1<<4)	/* request has been mapped */
#define TWA_CMD_IN_PROGRESS		(1<<5)	/* bus_dmamap_load returned EINPROGRESS */
#define TWA_CMD_TIMER_SET		(1<<6)	/* request is being timed */

/* Possible values of tr->tr_cmd_pkt_type. */
#define TWA_CMD_PKT_TYPE_7K		(1<<0)
#define TWA_CMD_PKT_TYPE_9K		(1<<1)
#define TWA_CMD_PKT_TYPE_INTERNAL	(1<<2)
#define TWA_CMD_PKT_TYPE_IOCTL		(1<<3)
#define TWA_CMD_PKT_TYPE_EXTERNAL	(1<<4)

/* Possible values of sc->twa_state. */
#define TWA_STATE_INTR_ENABLED		(1<<0)	/* interrupts have been enabled */
#define TWA_STATE_SHUTDOWN		(1<<1)	/* controller is shut down */
#define TWA_STATE_OPEN			(1<<2)	/* control device is open */
#define TWA_STATE_SUSPEND		(1<<3)	/* controller is suspended */
#define TWA_STATE_SIMQ_FROZEN		(1<<4)	/* simq frozen */

/* Possible values of sc->twa_ioctl_lock.lock. */
#define TWA_LOCK_FREE		0x0	/* lock is free */
#define TWA_LOCK_HELD		0x1	/* lock is held */


/* Error/AEN message structure. */
struct twa_message {
	u_int32_t	code;
	char		*message;
};

#ifdef TWA_DEBUG
struct twa_q_statistics {
	u_int32_t	q_length;
	u_int32_t	q_max;
};

#define TWAQ_FREE	0
#define TWAQ_BUSY	1
#define TWAQ_PENDING	2
#define TWAQ_COMPLETE	3
#define TWAQ_COUNT	4	/* total number of queues */
#endif /* TWA_DEBUG */

/* Driver's request packet. */
struct twa_request {
	struct twa_command_packet *tr_command;	/* ptr to cmd pkt submitted to controller */
	u_int32_t		tr_request_id;	/* request id for tracking with firmware */

	void			*tr_data;	/* ptr to data being passed to firmware */
	u_int32_t		tr_length;	/* length of buffer being passed to firmware */

	void			*tr_real_data;	/* ptr to, and length of data passed */
	u_int32_t		tr_real_length; /* to us from above, in case a buffer copy
							was done due to non-compliance to 
							alignment requirements */

	TAILQ_ENTRY(twa_request) tr_link;	/* to link this request in a list */
	struct twa_softc	*tr_sc;		/* controller that owns us */

	u_int32_t		tr_status;	/* command status */
	u_int32_t		tr_flags;	/* request flags */
	u_int32_t		tr_error;	/* error encountered before request submission */
	u_int32_t		tr_cmd_pkt_type;/* type of request */
	void			*tr_private;	/* request specific data to use during callback */
	void			(*tr_callback)(struct twa_request *tr);/* callback handler */
	bus_addr_t		tr_cmd_phys;	/* physical address of command in controller space */
	bus_dmamap_t		tr_buf_map;	/* DMA map for data */
} __attribute__ ((packed));


/* Per-controller structure. */
struct twa_softc {
	/* Request queues and arrays. */
	TAILQ_HEAD(, twa_request) twa_free;	/* free request packets */
	TAILQ_HEAD(, twa_request) twa_busy;	/* requests busy in the controller */
	TAILQ_HEAD(, twa_request) twa_pending;	/* internal requests pending */
	TAILQ_HEAD(, twa_request) twa_complete;	/* requests completed by firmware (not by us) */

	struct twa_request	*twa_lookup[TWA_Q_LENGTH];/* requests indexed by request_id */

	struct twa_request	*twa_req_buf;
	struct twa_command_packet *twa_cmd_pkt_buf;

	/* AEN handler fields. */
	struct twa_event_packet	*twa_aen_queue[TWA_Q_LENGTH];/* circular queue of AENs from firmware */
	uint16_t		working_srl;	/* driver & firmware negotiated srl */
	uint16_t		working_branch;	/* branch # of the firmware that the driver is compatible with */
	uint16_t		working_build;	/* build # of the firmware that the driver is compatible with */
	u_int32_t		twa_operating_mode; /* base mode/current mode */
	u_int32_t		twa_aen_head;	/* AEN queue head */
	u_int32_t		twa_aen_tail;	/* AEN queue tail */
	u_int32_t		twa_current_sequence_id;/* index of the last event + 1 */
	u_int32_t		twa_aen_queue_overflow;	/* indicates if unretrieved events were overwritten */
	u_int32_t		twa_aen_queue_wrapped;	/* indicates if AEN queue ever wrapped */
	u_int32_t		twa_wait_timeout; /* identifier for calling tsleep */

	/* Controller state. */
	u_int32_t		twa_state;
#ifdef TWA_DEBUG
	struct twa_q_statistics	twa_qstats[TWAQ_COUNT];	/* queue statistics */
#endif /* TWA_DEBUG */
	struct {
		u_int32_t	lock;	/* lock state */
		u_int32_t	timeout;/* time at which the lock will become available,
						even if not released */
	} twa_ioctl_lock;	/* lock for use by user applications, for synchronization
					between ioctl calls */
    
	device_t		twa_bus_dev;	/* bus device */
	struct cdev		*twa_ctrl_dev;	/* control device */
	struct resource		*twa_io_res;	/* register interface window */
	bus_space_handle_t	twa_bus_handle;	/* bus space handle */
	bus_space_tag_t		twa_bus_tag;	/* bus space tag */
	bus_dma_tag_t		twa_parent_tag;	/* parent DMA tag */
	bus_dma_tag_t		twa_cmd_tag;	/* cmd DMA tag */
	bus_dma_tag_t		twa_buf_tag;	/* data buffer DMA tag */
	bus_dmamap_t		twa_cmd_map;	/* DMA map for the array of cmd pkts */
	bus_addr_t		twa_cmd_pkt_phys;/* phys addr of first of array of cmd pkts */
	struct resource		*twa_irq_res;	/* interrupt resource*/
	void			*twa_intr_handle;/* interrupt handle */
	struct intr_config_hook	twa_ich;	/* delayed-startup hook */

	struct sysctl_ctx_list	twa_sysctl_ctx;
	struct sysctl_oid	*twa_sysctl_tree;

	struct cam_sim		*twa_sim;	/* sim for this controller */
	struct cam_path		*twa_path;	/* peripheral, path, tgt, lun
						associated with this controller */
};


/*
 * Queue primitives
 */

#ifdef TWA_DEBUG

#define TWAQ_INIT(sc, qname)				\
	do {						\
		sc->twa_qstats[qname].q_length = 0;	\
		sc->twa_qstats[qname].q_max = 0;	\
	} while(0)

#define TWAQ_ADD(sc, qname)					\
	do {							\
	struct twa_q_statistics *qs = &(sc)->twa_qstats[qname];	\
								\
		qs->q_length++;					\
		if (qs->q_length > qs->q_max)			\
			qs->q_max = qs->q_length;		\
	} while(0)

#define TWAQ_REMOVE(sc, qname)	(sc)->twa_qstats[qname].q_length--

#else /* TWA_DEBUG */

#define TWAQ_INIT(sc, qname)
#define TWAQ_ADD(sc, qname)
#define TWAQ_REMOVE(sc, qname)

#endif /* TWA_DEBUG */

#define TWAQ_REQUEST_QUEUE(name, index)					\
static __inline void twa_initq_ ## name(struct twa_softc *sc)		\
{									\
	TAILQ_INIT(&sc->twa_ ## name);					\
	TWAQ_INIT(sc, index);						\
}									\
static __inline void twa_enqueue_ ## name(struct twa_request *tr)	\
{									\
	int	s;							\
									\
	s = splcam();							\
	TAILQ_INSERT_TAIL(&tr->tr_sc->twa_ ## name, tr, tr_link);	\
	TWAQ_ADD(tr->tr_sc, index);					\
	splx(s);							\
}									\
static __inline void twa_requeue_ ## name(struct twa_request *tr)	\
{									\
	int	s;							\
									\
	s = splcam();							\
	TAILQ_INSERT_HEAD(&tr->tr_sc->twa_ ## name, tr, tr_link);	\
	TWAQ_ADD(tr->tr_sc, index);					\
	splx(s);							\
}									\
static __inline struct twa_request *twa_dequeue_ ## name(struct twa_softc *sc)\
{									\
	struct twa_request	*tr;					\
	int			s;					\
									\
	s = splcam();							\
	if ((tr = TAILQ_FIRST(&sc->twa_ ## name)) != NULL) {		\
		TAILQ_REMOVE(&sc->twa_ ## name, tr, tr_link);		\
		TWAQ_REMOVE(sc, index);					\
	}								\
	splx(s);							\
	return(tr);							\
}									\
static __inline void twa_remove_ ## name(struct twa_request *tr)	\
{									\
	int	s;							\
									\
	s = splcam();							\
	TAILQ_REMOVE(&tr->tr_sc->twa_ ## name, tr, tr_link);		\
	TWAQ_REMOVE(tr->tr_sc, index);					\
	splx(s);							\
}

TWAQ_REQUEST_QUEUE(free, TWAQ_FREE)
TWAQ_REQUEST_QUEUE(busy, TWAQ_BUSY)
TWAQ_REQUEST_QUEUE(pending, TWAQ_PENDING)
TWAQ_REQUEST_QUEUE(complete, TWAQ_COMPLETE)


#ifdef TWA_DEBUG

extern u_int8_t	twa_dbg_level;
extern u_int8_t	twa_call_dbg_level;

/* Printf with the bus device in question. */
#define twa_dbg_dprint(dbg_level, sc, fmt, args...)		\
	do {							\
		if (dbg_level <= twa_dbg_level)			\
			device_printf(sc->twa_bus_dev,		\
				"%s: " fmt "\n", __func__ , ##args);\
	} while(0)

#define twa_dbg_dprint_enter(dbg_level, sc)			\
	do {							\
		if (dbg_level <= twa_call_dbg_level)		\
			device_printf(sc->twa_bus_dev,		\
				"%s: entered.\n", __func__);	\
	} while(0)

#define twa_dbg_dprint_exit(dbg_level, sc)			\
	do {							\
		if (dbg_level <= twa_call_dbg_level)		\
			device_printf(sc->twa_bus_dev,		\
				"%s: exiting.\n", __func__);	\
	} while(0)

#define twa_dbg_print(dbg_level, fmt, args...)			\
	do {							\
		if (dbg_level <= twa_dbg_level)			\
			printf("%s: " fmt "\n", __func__ , ##args);\
	} while(0)

#else
#define twa_dbg_dprint(dbg_level, sc, fmt, args...)
#define twa_dbg_dprint_enter(dbg_level, sc)
#define twa_dbg_dprint_exit(dbg_level, sc)
#define twa_dbg_print(dbg_level, fmt, args...)
#endif

#define twa_printf(sc, fmt, args...)	\
	device_printf(sc->twa_bus_dev, fmt, ##args)
