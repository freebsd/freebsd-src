/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)buf.h	7.11 (Berkeley) 5/9/90
 *	$Id: buf.h,v 1.7 1993/12/22 12:51:48 davidg Exp $
 */

#ifndef _SYS_BUF_H_
#define _SYS_BUF_H_ 1

/*
 * The header for buffers in the buffer pool and otherwise used
 * to describe a block i/o request is given here.
 *
 * Each buffer in the pool is usually doubly linked into 2 lists:
 * hashed into a chain by <dev,blkno> so it can be located in the cache,
 * and (usually) on (one of several) queues.  These lists are circular and
 * doubly linked for easy removal.
 *
 * There are currently three queues for buffers:
 *	one for buffers which must be kept permanently (super blocks)
 * 	one for buffers containing ``useful'' information (the cache)
 *	one for buffers containing ``non-useful'' information
 *		(and empty buffers, pushed onto the front)
 * The latter two queues contain the buffers which are available for
 * reallocation, are kept in lru order.  When not on one of these queues,
 * the buffers are ``checked out'' to drivers which use the available list
 * pointers to keep track of them in their i/o active queues.
 */

/*
 * Bufhd structures used at the head of the hashed buffer queues.
 * We only need three words for these, so this abbreviated
 * definition saves some space.
 */
struct bufhd
{
	long	b_flags;		/* see defines below */
	struct	buf *b_forw, *b_back;	/* fwd/bkwd pointer in chain */
};

#ifdef __STDC__
struct buf;
#endif
typedef void b_iodone_t __P((struct buf *));

struct buf
{
	long	b_flags;		/* too much goes here to describe */
	struct	buf *b_forw, *b_back;	/* hash chain (2 way street) */
	struct	buf *av_forw, *av_back;	/* position on free list if not BUSY */
	struct	buf *b_blockf, **b_blockb;/* associated vnode */
#define	b_actf	av_forw			/* alternate names for driver queue */
#define	b_actl	av_back			/*    head - isn't history wonderful */
	long	b_bcount;		/* transfer count */
	long	b_bufsize;		/* size of allocated buffer */
#define	b_active b_bcount		/* driver queue head: drive active */
	short	b_error;		/* returned after I/O */
	dev_t	b_dev;			/* major+minor device name */
	union {
	    caddr_t b_addr;		/* low order core address */
	    int	*b_words;		/* words for clearing */
	    struct fs *b_fs;		/* superblocks */
	    struct csum *b_cs;		/* superblock summary information */
	    struct cg *b_cg;		/* cylinder group block */
	    struct dinode *b_dino;	/* ilist */
	    daddr_t *b_daddr;		/* indirect block */
	} b_un;
	daddr_t	b_lblkno;		/* logical block number */
	daddr_t	b_blkno;		/* block # on device */
	long	b_resid;		/* words not transferred after error */
#define	b_errcnt b_resid		/* while i/o in progress: # retries */
	struct  proc *b_proc;		/* proc doing physical or swap I/O */
	b_iodone_t *b_iodone;		/* function called by iodone */
	struct	vnode *b_vp;		/* vnode for dev */
	int	b_pfcent;		/* center page when swapping cluster */
	struct	ucred *b_rcred;		/* ref to read credentials */
	struct	ucred *b_wcred;		/* ref to write credendtials */
	int	b_dirtyoff;		/* offset in buffer of dirty region */
	int	b_dirtyend;		/* offset of end of dirty region */
	caddr_t	b_saveaddr;		/* original b_addr for PHYSIO */
	void *	b_driver1;		/* for private use by the driver */
	void *	b_driver2;		/* for private use by the driver */
	void *	b_spc;			/* swap pager info */
};

#define	BQUEUES		4		/* number of free buffer queues */

#define	BQ_LOCKED	0		/* super-blocks &c */
#define	BQ_LRU		1		/* lru, useful buffers */
#define	BQ_AGE		2		/* rubbish */
#define	BQ_EMPTY	3		/* buffer headers with no memory */

#ifdef	KERNEL
#define	BUFHSZ	512
#define RND	(MAXBSIZE/DEV_BSIZE)
/* 20 Aug 92	BUFHASH*/
#if	((BUFHSZ&(BUFHSZ-1)) == 0)
#define	BUFHASH(dvp, dblkno)	\
	((struct buf *)&bufhash[((int)(dvp)/sizeof(struct vnode)+(int)(dblkno))&(BUFHSZ-1)])
#else
#define	BUFHASH(dvp, dblkno)	\
	((struct buf *)&bufhash[((int)(dvp)/sizeof(struct vnode)+(int)(dblkno)) % BUFHSZ])
#endif

extern struct	buf *buf;		/* the buffer pool itself */
extern char	*buffers;
extern int	nbuf;			/* number of buffer headers */
extern int	bufpages;	/* number of memory pages in the buffer pool */
extern struct	buf *swbuf;		/* swap I/O headers */
extern int	nswbuf;
extern struct	bufhd bufhash[BUFHSZ];	/* heads of hash lists */
extern struct	buf bfreelist[BQUEUES];	/* heads of available lists */
extern struct	buf bswlist;		/* head of free swap header list */
extern struct	buf *bclnlist;		/* head of cleaned page list */

void bufinit(void);
int bread(struct vnode *, daddr_t, int, struct ucred *, struct buf **);
int breada(struct vnode *, daddr_t, int, daddr_t, int, struct ucred *,
	struct buf **);
int bwrite(struct buf *);
void bawrite(struct buf *);
void bdwrite(struct buf *);
void brelse(struct buf *);
struct buf *incore(struct vnode *, daddr_t);
struct buf *getblk(struct vnode *, daddr_t, int);
struct buf *geteblk(int);
int biowait(struct buf *);
void biodone(struct buf *);
void allocbuf(struct buf *, int);

extern void vwakeup(struct buf *);
extern void bgetvp(struct vnode *, struct buf *);
extern void brelvp(struct buf *);
extern void reassignbuf(struct buf *, struct vnode *);
extern void bufstats(void);
extern int physio(void (*)(struct buf *), int, struct buf *, int, int,
		  caddr_t, int *, struct proc *);

#endif /* KERNEL */

/*
 * These flags are kept in b_flags.
 */
#define	B_WRITE		0x000000	/* non-read pseudo-flag */
#define	B_READ		0x000001	/* read when I/O occurs */
#define	B_DONE		0x000002	/* transaction finished */
#define	B_ERROR		0x000004	/* transaction aborted */
#define	B_BUSY		0x000008	/* not on av_forw/back list */
#define	B_PHYS		0x000010	/* physical IO */
#define	B_XXX		0x000020	/* was B_MAP, alloc UNIBUS on pdp-11 */
#define	B_WANTED	0x000040	/* issue wakeup when BUSY goes off */
#define	B_AGE		0x000080	/* delayed write for correct aging */
#define	B_ASYNC		0x000100	/* don't wait for I/O completion */
#define	B_DELWRI	0x000200	/* write at exit of avail list */
#define	B_TAPE		0x000400	/* this is a magtape (no bdwrite) */
#define	B_VMPAGE	0x000800	/* buffer from virtual memory */
#define	B_MALLOC	0x001000	/* buffer from malloc space */
#define	B_DIRTY		0x002000	/* dirty page to be pushed out async */
#define	B_PGIN		0x004000	/* pagein op, so swap() can count it */
#define	B_CACHE		0x008000	/* did bread find us in the cache ? */
#define	B_INVAL		0x010000	/* does not contain valid info  */
#define	B_LOCKED	0x020000	/* locked in core (not reusable) */
#define	B_HEAD		0x040000	/* a buffer header, not a buffer */
#define	B_BAD		0x100000	/* bad block revectoring in progress */
#define	B_CALL		0x200000	/* call b_iodone from iodone */
#define	B_RAW		0x400000	/* set by physio for raw transfers */
#define	B_NOCACHE	0x800000	/* do not cache block after use */
#define	B_DRIVER       0xF000000	/* Four bits for the driver to use */
#define	B_DRIVER1      0x1000000	/* bits for the driver to use */
#define	B_DRIVER2      0x2000000	/* bits for the driver to use */
#define	B_DRIVER4      0x3000000	/* bits for the driver to use */
#define	B_DRIVER8      0x4000000	/* bits for the driver to use */

/*
 * Insq/Remq for the buffer hash lists.
 */
#define	bremhash(bp) { \
	(bp)->b_back->b_forw = (bp)->b_forw; \
	(bp)->b_forw->b_back = (bp)->b_back; \
}
#define	binshash(bp, dp) { \
	(bp)->b_forw = (dp)->b_forw; \
	(bp)->b_back = (dp); \
	(dp)->b_forw->b_back = (bp); \
	(dp)->b_forw = (bp); \
}

/*
 * Insq/Remq for the buffer free lists.
 */
#define	bremfree(bp) { \
	(bp)->av_back->av_forw = (bp)->av_forw; \
	(bp)->av_forw->av_back = (bp)->av_back; \
}
#define	binsheadfree(bp, dp) { \
	(dp)->av_forw->av_back = (bp); \
	(bp)->av_forw = (dp)->av_forw; \
	(dp)->av_forw = (bp); \
	(bp)->av_back = (dp); \
}
#define	binstailfree(bp, dp) { \
	(dp)->av_back->av_forw = (bp); \
	(bp)->av_back = (dp)->av_back; \
	(dp)->av_back = (bp); \
	(bp)->av_forw = (dp); \
}

#define	iodone	biodone
#define	iowait	biowait

/*
 * Zero out a buffer's data portion.
 */
#define	clrbuf(bp) { \
	bzero((bp)->b_un.b_addr, (unsigned)(bp)->b_bcount); \
	(bp)->b_resid = 0; \
}
#define B_CLRBUF	0x1	/* request allocated buffer be cleared */
#define B_SYNC		0x2	/* do all allocations synchronously */

#endif /* _SYS_BUF_H_ */
