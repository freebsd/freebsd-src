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
 *	$FreeBSD$
 */

/*
 * We could actually use all 33 segments, but using only 32 means that
 * each scatter/gather map is 256 bytes in size, and thus we don't have to worry about
 * maps crossing page boundaries.
 */
#define	MLX_NSEG	32		/* max scatter/gather segments we use */
#define MLX_NSLOTS	256		/* max number of command slots */

#define MLX_CFG_BASE0   0x10		/* first region */
#define MLX_CFG_BASE1   0x14		/* second region (type 3 only) */

#define MLX_MAXDRIVES	32

#define MLX_BLKSIZE	512		/* fixed feature */

/*
 * Structure describing a System Drive as attached to the controller.
 */
struct mlx_sysdrive 
{
    /* from MLX_CMD_ENQSYSDRIVE */
    u_int32_t		ms_size;
    int			ms_state;
    int			ms_raidlevel;

    /* synthetic geometry */
    int			ms_cylinders;
    int			ms_heads;
    int			ms_sectors;

    /* handle for attached driver */
    device_t		ms_disk;
};

/*
 * Per-command control structure.
 */
struct mlx_command 
{
    TAILQ_ENTRY(mlx_command)	mc_link;	/* list linkage */

    struct mlx_softc		*mc_sc;		/* controller that owns us */
    u_int8_t			mc_slot;	/* command slot we occupy */
    u_int16_t			mc_status;	/* command completion status */
    u_int8_t			mc_mailbox[16];	/* command mailbox */
    u_int32_t			mc_sgphys;	/* physical address of s/g array in controller space */
    int				mc_nsgent;	/* number of entries in s/g map */
    int				mc_flags;
#define MLX_CMD_DATAIN		(1<<0)
#define MLX_CMD_DATAOUT		(1<<1)
#define MLX_CMD_PRIORITY	(1<<2)		/* high-priority command */

    void			*mc_data;	/* data buffer */
    size_t			mc_length;
    bus_dmamap_t		mc_dmamap;	/* DMA map for data */
    u_int32_t			mc_dataphys;	/* data buffer base address controller space */

    void			(* mc_complete)(struct mlx_command *mc);	/* completion handler */
    void			*mc_private;	/* submitter-private data or wait channel */
};

/*
 * Per-controller structure.
 */
struct mlx_softc 
{
    /* bus connections */
    device_t		mlx_dev;
    struct resource	*mlx_mem;	/* mailbox interface window */
    bus_space_handle_t	mlx_bhandle;	/* bus space handle */
    bus_space_tag_t	mlx_btag;	/* bus space tag */
    bus_dma_tag_t	mlx_parent_dmat;/* parent DMA tag */
    bus_dma_tag_t	mlx_buffer_dmat;/* data buffer DMA tag */
    struct resource	*mlx_irq;	/* interrupt */
    void		*mlx_intr;	/* interrupt handle */

    /* scatter/gather lists and their controller-visible mappings */
    struct mlx_sgentry	*mlx_sgtable;	/* s/g lists */
    u_int32_t		mlx_sgbusaddr;	/* s/g table base address in bus space */
    bus_dma_tag_t	mlx_sg_dmat;	/* s/g buffer DMA tag */
    bus_dmamap_t	mlx_sg_dmamap;	/* map for s/g buffers */
    
    /* controller limits and features */
    int			mlx_hwid;	/* hardware identifier */
    int			mlx_maxiop;	/* maximum number of I/O operations */
    int			mlx_nchan;	/* number of active channels */
    int			mlx_maxiosize;	/* largest I/O for this controller */
    int			mlx_maxtarg;	/* maximum number of targets per channel */
    int			mlx_maxtags;	/* maximum number of tags per device */
    int			mlx_scsicap;	/* SCSI capabilities */
    int			mlx_feature;	/* controller features/quirks */
#define MLX_FEAT_PAUSEWORKS	(1<<0)	/* channel pause works as expected */

    /* controller queues and arrays */
    TAILQ_HEAD(, mlx_command)	mlx_freecmds;		/* command structures available for reuse */
    TAILQ_HEAD(, mlx_command)	mlx_donecmd;		/* commands waiting for completion processing */
    struct mlx_command	*mlx_busycmd[MLX_NSLOTS];	/* busy commands */
    int			mlx_busycmds;			/* count of busy commands */
    struct mlx_sysdrive	mlx_sysdrive[MLX_MAXDRIVES];	/* system drives */
    struct buf_queue_head mlx_bufq;			/* outstanding I/O operations */
    int			mlx_waitbufs;			/* number of bufs awaiting commands */

    /* controller status */
    u_int8_t		mlx_fwminor;	/* firmware revision */
    u_int8_t		mlx_fwmajor;
    int			mlx_geom;
#define MLX_GEOM_128_32		0	/* geoemetry translation modes */
#define MLX_GEOM_256_63		1
    int			mlx_state;
#define MLX_STATE_INTEN		(1<<0)	/* interrupts have been enabled */
#define MLX_STATE_SHUTDOWN	(1<<1)	/* controller is shut down */
#define MLX_STATE_OPEN		(1<<2)	/* control device is open */
#define MLX_STATE_SUSPEND	(1<<3)	/* controller is suspended */
    struct callout_handle mlx_timeout;	/* periodic status monitor */
    time_t		mlx_lastpoll;	/* last time_second we polled for status */
    u_int16_t		mlx_lastevent;	/* sequence number of the last event we recorded */
    u_int16_t		mlx_currevent;	/* sequence number last time we looked */
    int			mlx_polling;	/* if > 0, polling operations still running */
    int			mlx_rebuild;	/* if >= 0, drive is being rebuilt */
    u_int32_t		mlx_rebuildstat;/* blocks left to rebuild if active */
    int			mlx_check;	/* if >= 0, drive is being checked */
    struct mlx_pause	mlx_pause;	/* pending pause operation details */

    /* interface-specific accessor functions */
    int			mlx_iftype;	/* interface protocol */
#define MLX_IFTYPE_3	3
#define MLX_IFTYPE_4	4
#define MLX_IFTYPE_5	5
    int			(* mlx_tryqueue)(struct mlx_softc *sc, struct mlx_command *mc);
    int			(* mlx_findcomplete)(struct mlx_softc *sc, u_int8_t *slot, u_int16_t *status);
    void		(* mlx_intaction)(struct mlx_softc *sc, int action);
#define MLX_INTACTION_DISABLE		0
#define MLX_INTACTION_ENABLE		1
#define MLX_INTACTION_ACKNOWLEDGE	2

};

/*
 * Interface between bus connections and driver core.
 */
extern void		mlx_free(struct mlx_softc *sc);
extern int		mlx_attach(struct mlx_softc *sc);
extern void		mlx_startup(struct mlx_softc *sc);
extern void		mlx_intr(void *data);
extern int		mlx_detach(device_t dev);
extern int		mlx_shutdown(device_t dev);
extern int		mlx_suspend(device_t dev); 
extern int		mlx_resume(device_t dev);
extern d_open_t		mlx_open;
extern d_close_t	mlx_close;
extern d_ioctl_t	mlx_ioctl;

extern devclass_t	mlx_devclass;

/*
 * Mylex System Disk driver
 */
struct mlxd_softc 
{
    device_t		mlxd_dev;
    struct mlx_softc	*mlxd_controller;
    struct mlx_sysdrive	*mlxd_drive;
    struct disk		mlxd_disk;
    struct devstat	mlxd_stats;
    struct disklabel	mlxd_label;
    int			mlxd_unit;
    int			mlxd_flags;
#define MLXD_OPEN	(1<<0)		/* drive is open (can't shut down) */
};

/*
 * Interface between driver core and disk driver (should be using a bus?)
 */
extern int	mlx_submit_buf(struct mlx_softc *sc, struct buf *bp);
extern int	mlx_submit_ioctl(struct mlx_softc *sc, struct mlx_sysdrive *drive, u_long cmd, 
				 caddr_t addr, int32_t flag, struct proc *p);
extern void	mlxd_intr(void *data);

/*
 * Accessor defines for the V3 interface.
 */
#define MLX_V3_MAILBOX		0x00
#define	MLX_V3_STATUS_IDENT	0x0d
#define MLX_V3_STATUS		0x0e
#define MLX_V3_IDBR		0x40
#define MLX_V3_ODBR		0x41
#define MLX_V3_IER		0x43

#define MLX_V3_PUT_MAILBOX(sc, idx, val) bus_space_write_1(sc->mlx_btag, sc->mlx_bhandle, MLX_V3_MAILBOX + idx, val)
#define MLX_V3_GET_STATUS_IDENT(sc)	 bus_space_read_1 (sc->mlx_btag, sc->mlx_bhandle, MLX_V3_STATUS_IDENT)
#define MLX_V3_GET_STATUS(sc)		 bus_space_read_2 (sc->mlx_btag, sc->mlx_bhandle, MLX_V3_STATUS)
#define MLX_V3_GET_IDBR(sc)		 bus_space_read_1 (sc->mlx_btag, sc->mlx_bhandle, MLX_V3_IDBR)
#define MLX_V3_PUT_IDBR(sc, val)	 bus_space_write_1(sc->mlx_btag, sc->mlx_bhandle, MLX_V3_IDBR, val)
#define MLX_V3_GET_ODBR(sc)		 bus_space_read_1 (sc->mlx_btag, sc->mlx_bhandle, MLX_V3_ODBR)
#define MLX_V3_PUT_ODBR(sc, val)	 bus_space_write_1(sc->mlx_btag, sc->mlx_bhandle, MLX_V3_ODBR, val)
#define MLX_V3_PUT_IER(sc, val)		 bus_space_write_1(sc->mlx_btag, sc->mlx_bhandle, MLX_V3_IER, val)

#define MLX_V3_IDB_FULL		(1<<0)		/* mailbox is full */
#define MLX_V3_IDB_SACK		(1<<1)		/* acknowledge status read */
#define MLX_V3_IDB_RESET	(1<<3)		/* request soft reset */

#define MLX_V3_ODB_SAVAIL	(1<<0)		/* status is available */

/*
 * Inlines to build various command structures
 */
static __inline void
mlx_make_type1(struct mlx_command *mc,
	       u_int8_t code, 
	       u_int16_t f1,
	       u_int32_t f2,
	       u_int8_t f3,
	       u_int32_t f4,
	       u_int8_t f5) 
{
    mc->mc_mailbox[0x0] = code;
    mc->mc_mailbox[0x2] = f1 & 0xff;
    mc->mc_mailbox[0x3] = (((f2 >> 24) & 0x3) << 6) | ((f1 >> 8) & 0x3f);
    mc->mc_mailbox[0x4] = f2 & 0xff;
    mc->mc_mailbox[0x5] = (f2 >> 8) & 0xff;
    mc->mc_mailbox[0x6] = (f2 >> 16) & 0xff;
    mc->mc_mailbox[0x7] = f3;
    mc->mc_mailbox[0x8] = f4 & 0xff;
    mc->mc_mailbox[0x9] = (f4 >> 8) & 0xff;
    mc->mc_mailbox[0xa] = (f4 >> 16) & 0xff;
    mc->mc_mailbox[0xb] = (f4 >> 24) & 0xff;
    mc->mc_mailbox[0xc] = f5;
}

static __inline void
mlx_make_type2(struct mlx_command *mc,
	       u_int8_t code, 
	       u_int8_t f1,
	       u_int8_t f2,
	       u_int8_t f3,
	       u_int8_t f4,
	       u_int8_t f5,
	       u_int8_t f6,
	       u_int32_t f7,
	       u_int8_t f8)
{
    mc->mc_mailbox[0x0] = code;
    mc->mc_mailbox[0x2] = f1;
    mc->mc_mailbox[0x3] = f2;
    mc->mc_mailbox[0x4] = f3;
    mc->mc_mailbox[0x5] = f4;
    mc->mc_mailbox[0x6] = f5;
    mc->mc_mailbox[0x7] = f6;
    mc->mc_mailbox[0x8] = f7 & 0xff;
    mc->mc_mailbox[0x9] = (f7 >> 8) & 0xff;
    mc->mc_mailbox[0xa] = (f7 >> 16) & 0xff;
    mc->mc_mailbox[0xb] = (f7 >> 24) & 0xff;
    mc->mc_mailbox[0xc] = f8;
}

static __inline void
mlx_make_type3(struct mlx_command *mc,
	       u_int8_t code, 
	       u_int8_t f1,
	       u_int8_t f2,
	       u_int16_t f3,
	       u_int8_t f4,
	       u_int8_t f5,
	       u_int32_t f6,
	       u_int8_t f7)
{
    mc->mc_mailbox[0x0] = code;
    mc->mc_mailbox[0x2] = f1;
    mc->mc_mailbox[0x3] = f2;
    mc->mc_mailbox[0x4] = f3 & 0xff;
    mc->mc_mailbox[0x5] = (f3 >> 8) & 0xff;
    mc->mc_mailbox[0x6] = f4;
    mc->mc_mailbox[0x7] = f5;
    mc->mc_mailbox[0x8] = f6 & 0xff;
    mc->mc_mailbox[0x9] = (f6 >> 8) & 0xff;
    mc->mc_mailbox[0xa] = (f6 >> 16) & 0xff;
    mc->mc_mailbox[0xb] = (f6 >> 24) & 0xff;
    mc->mc_mailbox[0xc] = f7;
}

static __inline void
mlx_make_type4(struct mlx_command *mc,
	       u_int8_t code, 
	       u_int16_t f1,
	       u_int32_t f2,
	       u_int32_t f3,
	       u_int8_t f4)
{
    mc->mc_mailbox[0x0] = code;
    mc->mc_mailbox[0x2] = f1 & 0xff;
    mc->mc_mailbox[0x3] = (f1 >> 8) & 0xff;
    mc->mc_mailbox[0x4] = f2 & 0xff;
    mc->mc_mailbox[0x5] = (f2 >> 8) & 0xff;
    mc->mc_mailbox[0x6] = (f2 >> 16) & 0xff;
    mc->mc_mailbox[0x7] = (f2 >> 24) & 0xff;
    mc->mc_mailbox[0x8] = f3 & 0xff;
    mc->mc_mailbox[0x9] = (f3 >> 8) & 0xff;
    mc->mc_mailbox[0xa] = (f3 >> 16) & 0xff;
    mc->mc_mailbox[0xb] = (f3 >> 24) & 0xff;
    mc->mc_mailbox[0xc] = f4;
}

static __inline void
mlx_make_type5(struct mlx_command *mc,
	       u_int8_t code, 
	       u_int8_t f1,
	       u_int8_t f2,
	       u_int32_t f3,
	       u_int32_t f4,
	       u_int8_t f5)
{
    mc->mc_mailbox[0x0] = code;
    mc->mc_mailbox[0x2] = f1;
    mc->mc_mailbox[0x3] = f2;
    mc->mc_mailbox[0x4] = f3 & 0xff;
    mc->mc_mailbox[0x5] = (f3 >> 8) & 0xff;
    mc->mc_mailbox[0x6] = (f3 >> 16) & 0xff;
    mc->mc_mailbox[0x7] = (f3 >> 24) & 0xff;
    mc->mc_mailbox[0x8] = f4 & 0xff;
    mc->mc_mailbox[0x9] = (f4 >> 8) & 0xff;
    mc->mc_mailbox[0xa] = (f4 >> 16) & 0xff;
    mc->mc_mailbox[0xb] = (f4 >> 24) & 0xff;
    mc->mc_mailbox[0xc] = f5;
}

