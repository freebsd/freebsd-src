/*-
 * Copyright (c) 1999 Michael Smith
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
 *      $FreeBSD: src/sys/dev/amr/amrvar.h,v 1.2.2.1 2000/04/11 00:12:46 msmith Exp $
 */

/*
 * We could actually use all 17 segments, but using only 16 means that
 * each scatter/gather map is 128 bytes in size, and thus we don't have to worry about
 * maps crossing page boundaries.
 */
#define AMR_NSEG	16

#define AMR_CFG_BASE	0x10
#define AMR_CFG_SIG	0xa0
#define AMR_SIGNATURE	0x3344

#define AMR_MAXCMD	255		/* ident = 0 not allowed */
#define AMR_LIMITCMD	120		/* maximum count of outstanding commands */
#define AMR_MAXLD      	40

#define AMR_BLKSIZE	512

struct amr_softc;

/*
 * Per-logical-drive datastructure
 */
struct amr_logdrive
{
    u_int32_t	al_size;
    int		al_state;
    int		al_properties;
    
    /* synthetic geometry */
    int		al_cylinders;
    int		al_heads;
    int		al_sectors;
    
    /* driver */
    device_t	al_disk;
};


/*
 * Per-command control structure.
 */
struct amr_command
{
    TAILQ_ENTRY(amr_command)	ac_link;

    struct amr_softc		*ac_sc;
    u_int8_t			ac_slot;
    int				ac_status;
#define AMR_STATUS_BUSY		0xffff
#define AMR_STATUS_WEDGED	0xdead
#define AMR_STATUS_LATE		0xdeed
    struct amr_mailbox		ac_mailbox;
    u_int32_t			ac_sgphys;
    int				ac_nsgent;
    int				ac_flags;
#define AMR_CMD_DATAIN		(1<<0)
#define AMR_CMD_DATAOUT		(1<<1)
#define AMR_CMD_PRIORITY	(1<<2)
    time_t			ac_stamp;

    void			*ac_data;
    size_t			ac_length;
    bus_dmamap_t		ac_dmamap;
    u_int32_t			ac_dataphys;

    void			(* ac_complete)(struct amr_command *ac);
    void			*ac_private;
};

struct amr_softc 
{
    /* bus attachments */
    device_t			amr_dev;
    struct resource		*amr_reg;		/* control registers */
    bus_space_handle_t		amr_bhandle;
    bus_space_tag_t		amr_btag;
    bus_dma_tag_t		amr_parent_dmat;	/* parent DMA tag */
    bus_dma_tag_t		amr_buffer_dmat;	/* data buffer DMA tag */
    struct resource		*amr_irq;		/* interrupt */
    void			*amr_intr;

    /* mailbox */
    volatile struct amr_mailbox		*amr_mailbox;
    volatile struct amr_mailbox64	*amr_mailbox64;
    u_int32_t			amr_mailboxphys;
    bus_dma_tag_t		amr_mailbox_dmat;
    bus_dmamap_t		amr_mailbox_dmamap;

    /* scatter/gather lists and their controller-visible mappings */
    struct amr_sgentry		*amr_sgtable;		/* s/g lists */
    u_int32_t			amr_sgbusaddr;		/* s/g table base address in bus space */
    bus_dma_tag_t		amr_sg_dmat;		/* s/g buffer DMA tag */
    bus_dmamap_t		amr_sg_dmamap;		/* map for s/g buffers */

    /* controller limits and features */
    int				amr_maxio;		/* maximum number of I/O transactions */
    int				amr_maxdrives;		/* max number of logical drives */
    
    /* connected logical drives */
    struct amr_logdrive		amr_drive[AMR_MAXLD];

    /* controller status */
    int				amr_state;
#define AMR_STATE_OPEN		(1<<0)
#define AMR_STATE_SUSPEND	(1<<1)
#define AMR_STATE_INTEN		(1<<2)
#define AMR_STATE_SHUTDOWN	(1<<3)
    struct callout_handle	amr_timeout;		/* periodic status check */

    /* per-controller queues */
    struct buf_queue_head 	amr_bufq;		/* pending I/O */
    int				amr_waitbufs;
    struct amr_command		*amr_busycmd[AMR_MAXCMD];
    int				amr_busycmdcount;
    TAILQ_HEAD(,amr_command)	amr_work;
    int				amr_workcount;
    TAILQ_HEAD(,amr_command)	amr_freecmds;

    int				amr_locks;		/* reentrancy avoidance */

    /* controller type-specific support */
    int				amr_type;
#define AMR_TYPE_STD		0
#define AMR_TYPE_QUARTZ		1
    void			(* amr_submit_command)(struct amr_softc *sc);
    int				(* amr_get_work)(struct amr_softc *sc, struct amr_mailbox *mbsave);
    void			(* amr_attach_mailbox)(struct amr_softc *sc);
};

/*
 * Simple (stupid) locks.
 *
 * Note that these are designed to avoid reentrancy, not concurrency, and will
 * need to be replaced with something better.
 */
#define AMR_LOCK_COMPLETING     (1<<0)
#define AMR_LOCK_STARTING       (1<<1)

static __inline int
amr_lock_tas(struct amr_softc *sc, int lock)
{
    if ((sc)->amr_locks & (lock))
        return(1);
    atomic_set_int(&sc->amr_locks, lock);
    return(0);
}

static __inline void
amr_lock_clr(struct amr_softc *sc, int lock)
{
    atomic_clear_int(&sc->amr_locks, lock);
}

/*
 * I/O primitives
 */
/* Quartz */
#define AMR_QPUT_IDB(sc, val)	bus_space_write_4(sc->amr_btag, sc->amr_bhandle, AMR_QIDB, val)
#define AMR_QGET_IDB(sc)	bus_space_read_4 (sc->amr_btag, sc->amr_bhandle, AMR_QIDB)
#define AMR_QPUT_ODB(sc, val)	bus_space_write_4(sc->amr_btag, sc->amr_bhandle, AMR_QODB, val)
#define AMR_QGET_ODB(sc)	bus_space_read_4 (sc->amr_btag, sc->amr_bhandle, AMR_QODB)

/* Standard */
#define AMR_SPUT_ISTAT(sc, val)	bus_space_write_1(sc->amr_btag, sc->amr_bhandle, AMR_SINTR, val)
#define AMR_SGET_ISTAT(sc)	bus_space_read_1 (sc->amr_btag, sc->amr_bhandle, AMR_SINTR)
#define AMR_SACK_INTERRUPT(sc)	bus_space_write_1(sc->amr_btag, sc->amr_bhandle, AMR_SCMD, AMR_SCMD_ACKINTR)
#define AMR_SPOST_COMMAND(sc)	bus_space_write_1(sc->amr_btag, sc->amr_bhandle, AMR_SCMD, AMR_SCMD_POST)
#define AMR_SGET_MBSTAT(sc)	bus_space_read_1 (sc->amr_btag, sc->amr_bhandle, AMR_SMBOX_BUSY)
#define AMR_SENABLE_INTR(sc)											\
	bus_space_write_1(sc->amr_btag, sc->amr_bhandle, AMR_STOGGLE, 						\
			  bus_space_read_1(sc->amr_btag, sc->amr_bhandle, AMR_STOGGLE) | AMR_STOGL_IENABLE)
#define AMR_SDISABLE_INTR(sc)											\
	bus_space_write_1(sc->amr_btag, sc->amr_bhandle, AMR_STOGGLE, 						\
			  bus_space_read_1(sc->amr_btag, sc->amr_bhandle, AMR_STOGGLE) & ~AMR_STOGL_IENABLE)
#define AMR_SBYTE_SET(sc, reg, val)	bus_space_write_1(sc->amr_btag, sc->amr_bhandle, reg, val)

/*
 * Interface between bus connections and driver core.
 */
extern void             amr_free(struct amr_softc *sc);
extern int              amr_attach(struct amr_softc *sc);
extern void             amr_startup(struct amr_softc *sc);
extern void             amr_intr(void *data);
extern int              amr_detach(device_t dev);
extern int              amr_shutdown(device_t dev);
extern int              amr_suspend(device_t dev);
extern int              amr_resume(device_t dev);
extern d_open_t         amr_open;
extern d_close_t        amr_close;
extern d_ioctl_t        amr_ioctl;

extern devclass_t       amr_devclass;

/*
 * MegaRAID logical disk driver
 */
struct amrd_softc 
{
    device_t		amrd_dev;
    dev_t		amrd_dev_t;
    struct amr_softc	*amrd_controller;
    struct amr_logdrive	*amrd_drive;
    struct disk		amrd_disk;
    struct devstat	amrd_stats;
    struct disklabel	amrd_label;
    int			amrd_unit;
    int			amrd_flags;
#define AMRD_OPEN	(1<<0)		/* drive is open (can't shut down) */
};

/*
 * Interface between driver core and disk driver (should be using a bus?)
 */
extern int	amr_submit_buf(struct amr_softc *sc, struct buf *bp);
extern int	amr_submit_ioctl(struct amr_softc *sc, struct amr_logdrive *drive, u_long cmd, 
				 caddr_t addr, int32_t flag, struct proc *p);
extern void	amrd_intr(void *data);

extern void	amr_report(void);
