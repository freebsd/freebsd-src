/*-
 * Copyright (C) 1997,1998 Julian Elischer.  All rights reserved.
 * julian@freebsd.org
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 * 
 *	$Id: slice.h,v 1.2 1998/05/06 22:14:33 julian Exp $
 */

typedef struct slice_handler *sh_p;
typedef struct slice *sl_p;

struct slicelimits {
	u_int32_t       blksize;	/* IN BYTES */
	u_int64_t       slicesize;	/* IN BYTES */
};
typedef struct slicelimits *slmt_p;

/*
 * This struct is only used by the IDE geometry guessing hack in 
 * the MBR and disklabel code, when talked to by the IDE driver with a VERY
 * OLD DISK
 */
struct ide_geom {
	u_int32_t       secpertrack;	/* set to 0 if geom not known */
	u_int16_t       trackpercyl;
	u_int32_t       cyls;
};

/*
 * The probehints are set by the lower handler, to give direction as to 
 * what handler is probably required above. If a slice is repartitioned,
 * these may change e.g. mbr may set 165 meaning "FreeBSD slice" or 4 "DOS".
 * -type: a string for the type that should be used.
 *        if it's a null string ("") then don't even try find a sub handler.
 * 		defined as NO_SUBPART
 * 	  if it's a NULL pointer (NULL) then probe all known types.
 * -typespecific: A pointer to SOMETHING that teh lower handler thinks
 *    may be of interest to the higher hamdlers. The "something" is dependent
 *    on the type of the lower handler so the upper handler must know of
 *    this in advance. The type of this should be specified in an
 *    include file associated with the lower type. This is probably rarely
 *    needed.
 */
struct probehints {
	char	*type;		/* don't probe, just use this type */
	void	*typespecific;	/* the lower layer specifies this */
};
#define NO_SUBPART ""
/*
 * The common slice structure with data, methods and linkages.
 */
struct slice {
	/* Per slice data */
	char		*name;		/* e.g. sd0 wd0s1, wd0s1a ?? */
	struct probehints probeinfo;	/* how we should probe this */
	u_int32_t        flags;		/* this device open, etc. */
	u_int16_t	 refs;		/* active references, free if 1->0 */
	u_int16_t	 opencount;	/* actual count of opens if allowed */
	struct slicelimits limits;	/* limits on this slice */
	sh_p             handler_up;	/* type methods etc. */
	void		*private_up;	/* data for the slice type */
	sh_p             handler_down;	/* type methods etc. */
	void		*private_down;	/* data for the slice type */
	/*------- fields for the slice device driver -------*/
	LIST_ENTRY(slice) hash_list;	/* next slice in this bucket */
	u_int32_t	 minor;		/* the key for finding us */
	void		*devfs_btoken;
	void		*devfs_ctoken;
};

/* bit definitions for the slice flags */
#define		SLF_CLOSED	0x00000000	/* slice not open */
#define		SLF_OPEN_BLK_RD	0x00000001	/* blk slice readable */
#define		SLF_OPEN_BLK_WR	0x00000002	/* blk slice writeable */
#define		SLF_OPEN_BLK	(SLF_OPEN_BLK_RD|SLF_OPEN_BLK_WR)
#define		SLF_OPEN_CHR_RD	0x00000004	/* raw slice readable */
#define		SLF_OPEN_CHR_WR	0x00000008	/* raw slice writeable */
#define		SLF_OPEN_CHR	(SLF_OPEN_CHR_RD|SLF_OPEN_CHR_WR)
#define		SLF_OPEN_DEV_RD	(SLF_OPEN_CHR_RD|SLF_OPEN_BLK_RD)
#define		SLF_OPEN_DEV_WR	(SLF_OPEN_CHR_WR|SLF_OPEN_BLK_WR)
#define		SLF_OPEN_DEV	(SLF_OPEN_DEV_RD|SLF_OPEN_DEV_WR)
#define		SLF_OPEN_UP_RD	0x00000010	/* upper layer is readable */
#define		SLF_OPEN_UP_WR	0x00000020	/* upper layer is writable */
#define		SLF_OPEN_UP	0x00000030	/* upper layer is open */
#define		SLF_OPEN_WR (SLF_OPEN_UP_WR|SLF_OPEN_DEV_WR)
#define		SLF_OPEN_RD (SLF_OPEN_UP_RD|SLF_OPEN_DEV_RD)
#define		SLF_OPEN_STATE	(SLF_OPEN_WR|SLF_OPEN_RD) /* Mask open state */

#define		SLF_INVALID	0x00000100	/* Everything aborts */
#define		SLF_LOCKED	0x00000200	/* Hold off, It's busy */
#define		SLF_WANTED	0x00000400	/* I held off, wake me up */

/*
 * prototypes for slice methods
 */
typedef void	sl_h_IO_req_t(void *private, struct buf * buf);
typedef int	sl_h_ioctl_t(void *private, u_long cmd, caddr_t data,
	     			int fflag, struct proc * p);
typedef int	sl_h_constructor_t(sl_p slice);
typedef int	sl_h_open_t(void *private, int flags, int mode, struct proc * p);
typedef void	sl_h_close_t(void *private, int flags, int mode, struct proc * p);
typedef int	sl_h_revoke_t(void *private);
typedef int	sl_h_claim_t(struct slice * slice, struct slice * lower,
	     		void *ID);		/* eg ID=165 for BSD */
typedef int	sl_h_verify_t(struct slice *slice);
typedef int	sl_h_upconfig_t(struct slice *slice, int cmd, caddr_t data,
	     			int fflag, struct proc *p);
typedef int	sl_h_dump_t(void *private, int32_t blkoff, int32_t blkcnt);

struct slice_handler {
	char           *name;
	int		version;/* the version of this handler */
	struct slice_handler *next;	/* next registered type */
	int             refs;	/* references to this type */
	sl_h_constructor_t *constructor;	/* make new instantiation */
	sl_h_IO_req_t  *IOreq;	/* IO req downward (to device) */
	sl_h_ioctl_t   *ioctl;	/* ioctl downward (to device) */
	sl_h_open_t    *open;	/* downwards travelling open */
	sl_h_close_t   *close;	/* downwards travelling close */
	sl_h_revoke_t  *revoke;	/* revoke upwards (towards user ) */
	sl_h_claim_t   *claim;	/* claim a new slice */
	sl_h_verify_t  *verify;	/* verify that a slice as it was before */
	sl_h_upconfig_t *upconf; /* config requests from slice below */
	sl_h_dump_t    *dump;	/* dump the core */
};

/*
 * general routines that handlers need.
 */
int	sl_make_slice(sh_p handler_down, void *private_down,
	      		struct slicelimits *limits,
	      		sl_p *slicepp, char *type, char *name);
void	sl_rmslice(sl_p slice);
int	sl_newtype(sh_p tp);
sh_p	sl_findtype(char *type);
sh_p	slice_probeall(sl_p slice);
int	lockslice(sl_p slice);
int	unlockslice(sl_p slice);
int	slice_readblock(struct slice *slice, int blkno, struct buf **bpp);
int	slice_writeblock(struct slice *slice, int blkno, struct buf *bp);

/*
 * Definitions for "SLICE" utilities. (handler or device acting on a slice).
 */
enum slc_who { SLW_ABOVE, SLW_DEVICE }; /* helps to know who's calling */

void	sliceio(sl_p slice, struct buf * bp, enum slc_who who);
int	sliceopen(sl_p slice, int flags, int mode,
		struct proc * p, enum slc_who who);
void	sliceclose(sl_p slice, int flags, int mode,
		struct proc * p, enum slc_who who);

void	sl_unref(sl_p slice);
void	slice_add_device(sl_p slice);
void	slice_remove_device(sl_p slice);

/*
 * The geometry guessing HACK functions
 */
int mbr_geom_hack(struct slice * slice, struct ide_geom *geom);
int dkl_geom_hack(struct slice * slice, struct ide_geom *geom);
/*
 * The routine to produce a dummy disklabel from a slice.
 * Lives in disklabel.c because that's where everyhting is in scope,
 * but is used in slice_device.c.  XXX hack.
 */
int dkl_dummy_ioctl(struct slice *slice, u_long cmd, caddr_t addr,
					int flag, struct proc * p);

/*
 * debugging 
 */
#if 0
#define RR printf(__FUNCTION__ " called\n")
#else
#define RR /* nothing */
#endif
