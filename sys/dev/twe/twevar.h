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
 *	$FreeBSD: src/sys/dev/twe/twevar.h,v 1.1.2.1 2000/05/25 01:50:48 msmith Exp $
 */

#ifdef TWE_DEBUG
#define debug(level, fmt, args...)	do { if (level <= TWE_DEBUG) printf("%s: " fmt "\n", __FUNCTION__ , ##args); } while(0)
#define debug_called(level)		do { if (level <= TWE_DEBUG) printf(__FUNCTION__ ": called\n"); } while(0)
#else
#define debug(level, fmt, args...)
#define debug_called(level)
#endif

/*
 * Structure describing a logical unit as attached to the controller
 */
struct twe_drive
{
    /* unit properties */
    u_int32_t		td_size;
    int			td_cylinders;
    int			td_heads;
    int			td_sectors;
    int			td_unit;

    /* unit state */
    int			td_state;
#define TWE_DRIVE_READY		0
#define TWE_DRIVE_DEGRADED	1
#define TWE_DRIVE_OFFLINE	2
    int			td_raidlevel;
#define TWE_DRIVE_RAID0		0
#define TWE_DRIVE_RAID1		1

#define TWE_DRIVE_UNKNOWN	0xff

    /* handle for attached driver */
    device_t		td_disk;
};

/*
 * Per-command control structure.
 *
 * Note that due to alignment constraints on the tc_command field, this *must* be 64-byte aligned.
 * We achieve this by placing the command at the beginning of the structure, and using malloc()
 * to allocate each structure.
 */
struct twe_request
{
    /* controller command */
    TWE_Command			tr_command;	/* command as submitted to controller */
    bus_dmamap_t		tr_cmdmap;	/* DMA map for command */
    u_int32_t			tr_cmdphys;	/* address of command in controller space */

    /* command payload */
    void			*tr_data;	/* data buffer */
    void			*tr_realdata;	/* copy of real data buffer pointer for alignment fixup */
    size_t			tr_length;
    bus_dmamap_t		tr_dmamap;	/* DMA map for data */
    u_int32_t			tr_dataphys;	/* data buffer base address in controller space */

    TAILQ_ENTRY(twe_request)	tr_link;	/* list linkage */
    struct twe_softc		*tr_sc;		/* controller that owns us */
    int				tr_status;	/* command status */
#define TWE_CMD_SETUP		0	/* being assembled */
#define TWE_CMD_BUSY		1	/* submitted to controller */
#define TWE_CMD_COMPLETE	2	/* completed by controller (maybe with error) */
#define TWE_CMD_FAILED		3	/* failed submission to controller */
    int				tr_flags;
#define TWE_CMD_DATAIN		(1<<0)
#define TWE_CMD_DATAOUT		(1<<1)
#define TWE_CMD_ALIGNBUF	(1<<2)	/* data in bio is misaligned, have to copy to/from private buffer */
    void			(* tr_complete)(struct twe_request *tr);	/* completion handler */
    void			*tr_private;	/* submitter-private data or wait channel */
};

/*
 * Per-controller state.
 */
struct twe_softc 
{
    /* bus connections */
    device_t		twe_dev;
    dev_t		twe_dev_t;
    struct resource	*twe_io;		/* register interface window */
    bus_space_handle_t	twe_bhandle;		/* bus space handle */
    bus_space_tag_t	twe_btag;		/* bus space tag */
    bus_dma_tag_t	twe_parent_dmat;	/* parent DMA tag */
    bus_dma_tag_t	twe_buffer_dmat;	/* data buffer DMA tag */
    struct resource	*twe_irq;		/* interrupt */
    void		*twe_intr;		/* interrupt handle */

    /* controller queues and arrays */
    TAILQ_HEAD(, twe_request)	twe_freecmds;		/* command structures available for reuse */
    TAILQ_HEAD(, twe_request)	twe_work;		/* active commands (busy or waiting for completion) */
    struct twe_request	*twe_cmdlookup[TWE_Q_LENGTH];	/* busy commands indexed by request ID */
    int			twe_busycmds;			/* count of busy commands */
    struct twe_drive	twe_drive[TWE_MAX_UNITS];		/* attached drives */
    struct bio_queue_head twe_bioq;			/* outstanding I/O operations */
    struct twe_request	*twe_deferred;			/* request that the controller wouldn't take */

    u_int16_t		twe_aen_queue[TWE_Q_LENGTH];	/* AENs queued for userland tool(s) */
    int			twe_aen_head, twe_aen_tail;	/* ringbuffer pointers for AEN queue */

    /* controller status */
    int			twe_state;
#define TWE_STATE_INTEN		(1<<0)	/* interrupts have been enabled */
#define TWE_STATE_SHUTDOWN	(1<<1)	/* controller is shut down */
#define TWE_STATE_OPEN		(1<<2)	/* control device is open */
#define TWE_STATE_SUSPEND	(1<<3)	/* controller is suspended */

    /* delayed configuration hook */
    struct intr_config_hook	twe_ich;

    /* wait-for-aen notification */
    int			twe_wait_aen;
};

/*
 * Virtual disk driver.
 */
struct twed_softc 
{
    device_t		twed_dev;
    dev_t		twed_dev_t;
    struct twe_softc	*twed_controller;	/* parent device softc */
    struct twe_drive	*twed_drive;		/* drive data in parent softc */
    struct disk		twed_disk;		/* generic disk handle */
    struct devstat	twed_stats;		/* accounting */
    struct disklabel	twed_label;		/* synthetic label */
    int			twed_flags;
#define TWED_OPEN	(1<<0)			/* drive is open (can't shut down) */
};

/*
 * Interface betwen driver core and disk driver (should be using a bus?)
 */
extern int	twe_submit_buf(struct twe_softc *sc, struct bio *bp);
extern void	twed_intr(void *data);
