/*-
 * Copyright 1998, 2000 Marshall Kirk McKusick.
 * Copyright 2009, 2010 Jeffrey W. Roberson <jeff@FreeBSD.org>
 * All rights reserved.
 *
 * The soft updates code is derived from the appendix of a University
 * of Michigan technical report (Gregory R. Ganger and Yale N. Patt,
 * "Soft Updates: A Solution to the Metadata Update Problem in File
 * Systems", CSE-TR-254-95, August 1995).
 *
 * Further information about soft updates can be obtained from:
 *
 *	Marshall Kirk McKusick		http://www.mckusick.com/softdep/
 *	1614 Oxford Street		mckusick@mckusick.com
 *	Berkeley, CA 94709-1608		+1-510-843-9542
 *	USA
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: @(#)ffs_softdep.c	9.59 (McKusick) 6/21/00
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ffs.h"
#include "opt_ddb.h"

/*
 * For now we want the safety net that the DEBUG flag provides.
 */
#ifndef DEBUG
#define DEBUG
#endif

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/bio.h>
#include <sys/buf.h>
#include <sys/kdb.h>
#include <sys/kthread.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mount.h>
#include <sys/mutex.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/stat.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/vnode.h>
#include <sys/conf.h>
#include <ufs/ufs/dir.h>
#include <ufs/ufs/extattr.h>
#include <ufs/ufs/quota.h>
#include <ufs/ufs/inode.h>
#include <ufs/ufs/ufsmount.h>
#include <ufs/ffs/fs.h>
#include <ufs/ffs/softdep.h>
#include <ufs/ffs/ffs_extern.h>
#include <ufs/ufs/ufs_extern.h>

#include <vm/vm.h>

#include <ddb/ddb.h>

#ifndef SOFTUPDATES

int
softdep_flushfiles(oldmnt, flags, td)
	struct mount *oldmnt;
	int flags;
	struct thread *td;
{

	panic("softdep_flushfiles called");
}

int
softdep_mount(devvp, mp, fs, cred)
	struct vnode *devvp;
	struct mount *mp;
	struct fs *fs;
	struct ucred *cred;
{

	return (0);
}

void 
softdep_initialize()
{

	return;
}

void
softdep_uninitialize()
{

	return;
}

void
softdep_unmount(mp)
	struct mount *mp;
{

}

void
softdep_setup_sbupdate(ump, fs, bp)
	struct ufsmount *ump;
	struct fs *fs;
	struct buf *bp;
{
}

void
softdep_setup_inomapdep(bp, ip, newinum)
	struct buf *bp;
	struct inode *ip;
	ino_t newinum;
{

	panic("softdep_setup_inomapdep called");
}

void
softdep_setup_blkmapdep(bp, mp, newblkno, frags, oldfrags)
	struct buf *bp;
	struct mount *mp;
	ufs2_daddr_t newblkno;
	int frags;
	int oldfrags;
{

	panic("softdep_setup_blkmapdep called");
}

void 
softdep_setup_allocdirect(ip, lbn, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;
	ufs_lbn_t lbn;
	ufs2_daddr_t newblkno;
	ufs2_daddr_t oldblkno;
	long newsize;
	long oldsize;
	struct buf *bp;
{
	
	panic("softdep_setup_allocdirect called");
}

void 
softdep_setup_allocext(ip, lbn, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;
	ufs_lbn_t lbn;
	ufs2_daddr_t newblkno;
	ufs2_daddr_t oldblkno;
	long newsize;
	long oldsize;
	struct buf *bp;
{
	
	panic("softdep_setup_allocext called");
}

void
softdep_setup_allocindir_page(ip, lbn, bp, ptrno, newblkno, oldblkno, nbp)
	struct inode *ip;
	ufs_lbn_t lbn;
	struct buf *bp;
	int ptrno;
	ufs2_daddr_t newblkno;
	ufs2_daddr_t oldblkno;
	struct buf *nbp;
{

	panic("softdep_setup_allocindir_page called");
}

void
softdep_setup_allocindir_meta(nbp, ip, bp, ptrno, newblkno)
	struct buf *nbp;
	struct inode *ip;
	struct buf *bp;
	int ptrno;
	ufs2_daddr_t newblkno;
{

	panic("softdep_setup_allocindir_meta called");
}

void
softdep_setup_freeblocks(ip, length, flags)
	struct inode *ip;
	off_t length;
	int flags;
{
	
	panic("softdep_setup_freeblocks called");
}

void
softdep_freefile(pvp, ino, mode)
		struct vnode *pvp;
		ino_t ino;
		int mode;
{

	panic("softdep_freefile called");
}

int 
softdep_setup_directory_add(bp, dp, diroffset, newinum, newdirbp, isnewblk)
	struct buf *bp;
	struct inode *dp;
	off_t diroffset;
	ino_t newinum;
	struct buf *newdirbp;
	int isnewblk;
{

	panic("softdep_setup_directory_add called");
}

void 
softdep_change_directoryentry_offset(bp, dp, base, oldloc, newloc, entrysize)
	struct buf *bp;
	struct inode *dp;
	caddr_t base;
	caddr_t oldloc;
	caddr_t newloc;
	int entrysize;
{

	panic("softdep_change_directoryentry_offset called");
}

void 
softdep_setup_remove(bp, dp, ip, isrmdir)
	struct buf *bp;
	struct inode *dp;
	struct inode *ip;
	int isrmdir;
{
	
	panic("softdep_setup_remove called");
}

void 
softdep_setup_directory_change(bp, dp, ip, newinum, isrmdir)
	struct buf *bp;
	struct inode *dp;
	struct inode *ip;
	ino_t newinum;
	int isrmdir;
{

	panic("softdep_setup_directory_change called");
}

void *
softdep_setup_trunc(vp, length, flags)
	struct vnode *vp;
	off_t length;
	int flags;
{

	panic("%s called", __FUNCTION__);

	return (NULL);
}

int
softdep_complete_trunc(vp, cookie)
	struct vnode *vp;
	void *cookie;
{

	panic("%s called", __FUNCTION__);

	return (0);
}

void
softdep_setup_blkfree(mp, bp, blkno, frags, wkhd)
	struct mount *mp;
	struct buf *bp;
	ufs2_daddr_t blkno;
	int frags;
	struct workhead *wkhd;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_setup_inofree(mp, bp, ino, wkhd)
	struct mount *mp;
	struct buf *bp;
	ino_t ino;
	struct workhead *wkhd;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_setup_unlink(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_setup_link(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_revert_link(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_setup_rmdir(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_revert_rmdir(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_setup_create(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_revert_create(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_setup_mkdir(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_revert_mkdir(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

void
softdep_setup_dotdot_link(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	panic("%s called", __FUNCTION__);
}

int
softdep_prealloc(vp, waitok)
	struct vnode *vp;
	int waitok;
{

	panic("%s called", __FUNCTION__);

	return (0);
}

int
softdep_journal_lookup(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{

	return (ENOENT);
}

void
softdep_change_linkcnt(ip)
	struct inode *ip;
{

	panic("softdep_change_linkcnt called");
}

void 
softdep_load_inodeblock(ip)
	struct inode *ip;
{

	panic("softdep_load_inodeblock called");
}

void 
softdep_update_inodeblock(ip, bp, waitfor)
	struct inode *ip;
	struct buf *bp;
	int waitfor;
{

	panic("softdep_update_inodeblock called");
}

int
softdep_fsync(vp)
	struct vnode *vp;	/* the "in_core" copy of the inode */
{

	return (0);
}

void
softdep_fsync_mountdev(vp)
	struct vnode *vp;
{

	return;
}

int
softdep_flushworklist(oldmnt, countp, td)
	struct mount *oldmnt;
	int *countp;
	struct thread *td;
{

	*countp = 0;
	return (0);
}

int
softdep_sync_metadata(struct vnode *vp)
{

	return (0);
}

int
softdep_slowdown(vp)
	struct vnode *vp;
{

	panic("softdep_slowdown called");
}

void
softdep_releasefile(ip)
	struct inode *ip;	/* inode with the zero effective link count */
{

	panic("softdep_releasefile called");
}

int
softdep_request_cleanup(fs, vp)
	struct fs *fs;
	struct vnode *vp;
{

	return (0);
}

int
softdep_check_suspend(struct mount *mp,
		      struct vnode *devvp,
		      int softdep_deps,
		      int softdep_accdeps,
		      int secondary_writes,
		      int secondary_accwrites)
{
	struct bufobj *bo;
	int error;
	
	(void) softdep_deps,
	(void) softdep_accdeps;

	bo = &devvp->v_bufobj;
	ASSERT_BO_LOCKED(bo);

	MNT_ILOCK(mp);
	while (mp->mnt_secondary_writes != 0) {
		BO_UNLOCK(bo);
		msleep(&mp->mnt_secondary_writes, MNT_MTX(mp),
		    (PUSER - 1) | PDROP, "secwr", 0);
		BO_LOCK(bo);
		MNT_ILOCK(mp);
	}

	/*
	 * Reasons for needing more work before suspend:
	 * - Dirty buffers on devvp.
	 * - Secondary writes occurred after start of vnode sync loop
	 */
	error = 0;
	if (bo->bo_numoutput > 0 ||
	    bo->bo_dirty.bv_cnt > 0 ||
	    secondary_writes != 0 ||
	    mp->mnt_secondary_writes != 0 ||
	    secondary_accwrites != mp->mnt_secondary_accwrites)
		error = EAGAIN;
	BO_UNLOCK(bo);
	return (error);
}

void
softdep_get_depcounts(struct mount *mp,
		      int *softdepactivep,
		      int *softdepactiveaccp)
{
	(void) mp;
	*softdepactivep = 0;
	*softdepactiveaccp = 0;
}

#else
/*
 * These definitions need to be adapted to the system to which
 * this file is being ported.
 */

#define M_SOFTDEP_FLAGS	(M_WAITOK)

#define	D_PAGEDEP	0
#define	D_INODEDEP	1
#define	D_BMSAFEMAP	2
#define	D_NEWBLK	3
#define	D_ALLOCDIRECT	4
#define	D_INDIRDEP	5
#define	D_ALLOCINDIR	6
#define	D_FREEFRAG	7
#define	D_FREEBLKS	8
#define	D_FREEFILE	9
#define	D_DIRADD	10
#define	D_MKDIR		11
#define	D_DIRREM	12
#define	D_NEWDIRBLK	13
#define	D_FREEWORK	14
#define	D_FREEDEP	15
#define	D_JADDREF	16
#define	D_JREMREF	17
#define	D_JMVREF	18
#define	D_JNEWBLK	19
#define	D_JFREEBLK	20
#define	D_JFREEFRAG	21
#define	D_JSEG		22
#define	D_JSEGDEP	23
#define	D_SBDEP		24
#define	D_JTRUNC	25
#define	D_LAST		D_JTRUNC

unsigned long dep_current[D_LAST + 1];
unsigned long dep_total[D_LAST + 1];


SYSCTL_NODE(_debug, OID_AUTO, softdep, CTLFLAG_RW, 0, "soft updates stats");
SYSCTL_NODE(_debug_softdep, OID_AUTO, total, CTLFLAG_RW, 0,
    "total dependencies allocated");
SYSCTL_NODE(_debug_softdep, OID_AUTO, current, CTLFLAG_RW, 0,
    "current dependencies allocated");

#define	SOFTDEP_TYPE(type, str, long)					\
    static MALLOC_DEFINE(M_ ## type, #str, long);			\
    SYSCTL_ULONG(_debug_softdep_total, OID_AUTO, str, CTLFLAG_RD,	\
	&dep_total[D_ ## type], 0, "");					\
    SYSCTL_ULONG(_debug_softdep_current, OID_AUTO, str, CTLFLAG_RD, 	\
	&dep_current[D_ ## type], 0, "");

SOFTDEP_TYPE(PAGEDEP, pagedep, "File page dependencies"); 
SOFTDEP_TYPE(INODEDEP, inodedep, "Inode dependencies");
SOFTDEP_TYPE(BMSAFEMAP, bmsafemap,
    "Block or frag allocated from cyl group map");
SOFTDEP_TYPE(NEWBLK, newblk, "New block or frag allocation dependency");
SOFTDEP_TYPE(ALLOCDIRECT, allocdirect, "Block or frag dependency for an inode");
SOFTDEP_TYPE(INDIRDEP, indirdep, "Indirect block dependencies");
SOFTDEP_TYPE(ALLOCINDIR, allocindir, "Block dependency for an indirect block");
SOFTDEP_TYPE(FREEFRAG, freefrag, "Previously used frag for an inode");
SOFTDEP_TYPE(FREEBLKS, freeblks, "Blocks freed from an inode");
SOFTDEP_TYPE(FREEFILE, freefile, "Inode deallocated");
SOFTDEP_TYPE(DIRADD, diradd, "New directory entry");
SOFTDEP_TYPE(MKDIR, mkdir, "New directory");
SOFTDEP_TYPE(DIRREM, dirrem, "Directory entry deleted");
SOFTDEP_TYPE(NEWDIRBLK, newdirblk, "Unclaimed new directory block");
SOFTDEP_TYPE(FREEWORK, freework, "free an inode block");
SOFTDEP_TYPE(FREEDEP, freedep, "track a block free");
SOFTDEP_TYPE(JADDREF, jaddref, "Journal inode ref add");
SOFTDEP_TYPE(JREMREF, jremref, "Journal inode ref remove");
SOFTDEP_TYPE(JMVREF, jmvref, "Journal inode ref move");
SOFTDEP_TYPE(JNEWBLK, jnewblk, "Journal new block");
SOFTDEP_TYPE(JFREEBLK, jfreeblk, "Journal free block");
SOFTDEP_TYPE(JFREEFRAG, jfreefrag, "Journal free frag");
SOFTDEP_TYPE(JSEG, jseg, "Journal segment");
SOFTDEP_TYPE(JSEGDEP, jsegdep, "Journal segment complete");
SOFTDEP_TYPE(SBDEP, sbdep, "Superblock write dependency");
SOFTDEP_TYPE(JTRUNC, jtrunc, "Journal inode truncation");

static MALLOC_DEFINE(M_SAVEDINO, "savedino", "Saved inodes");
static MALLOC_DEFINE(M_JBLOCKS, "jblocks", "Journal block locations");

/* 
 * translate from workitem type to memory type
 * MUST match the defines above, such that memtype[D_XXX] == M_XXX
 */
static struct malloc_type *memtype[] = {
	M_PAGEDEP,
	M_INODEDEP,
	M_BMSAFEMAP,
	M_NEWBLK,
	M_ALLOCDIRECT,
	M_INDIRDEP,
	M_ALLOCINDIR,
	M_FREEFRAG,
	M_FREEBLKS,
	M_FREEFILE,
	M_DIRADD,
	M_MKDIR,
	M_DIRREM,
	M_NEWDIRBLK,
	M_FREEWORK,
	M_FREEDEP,
	M_JADDREF,
	M_JREMREF,
	M_JMVREF,
	M_JNEWBLK,
	M_JFREEBLK,
	M_JFREEFRAG,
	M_JSEG,
	M_JSEGDEP,
	M_SBDEP,
	M_JTRUNC
};

static LIST_HEAD(mkdirlist, mkdir) mkdirlisthd;

#define DtoM(type) (memtype[type])

/*
 * Names of malloc types.
 */
#define TYPENAME(type)  \
	((unsigned)(type) <= D_LAST ? memtype[type]->ks_shortdesc : "???")
/*
 * End system adaptation definitions.
 */

#define	DOTDOT_OFFSET	offsetof(struct dirtemplate, dotdot_ino)
#define	DOT_OFFSET	offsetof(struct dirtemplate, dot_ino)

/*
 * Forward declarations.
 */
struct inodedep_hashhead;
struct newblk_hashhead;
struct pagedep_hashhead;
struct bmsafemap_hashhead;

/*
 * Internal function prototypes.
 */
static	void softdep_error(char *, int);
static	void drain_output(struct vnode *);
static	struct buf *getdirtybuf(struct buf *, struct mtx *, int);
static	void clear_remove(struct thread *);
static	void clear_inodedeps(struct thread *);
static	void unlinked_inodedep(struct mount *, struct inodedep *);
static	void clear_unlinked_inodedep(struct inodedep *);
static	struct inodedep *first_unlinked_inodedep(struct ufsmount *);
static	int flush_pagedep_deps(struct vnode *, struct mount *,
	    struct diraddhd *);
static	void free_pagedep(struct pagedep *);
static	int flush_newblk_dep(struct vnode *, struct mount *, ufs_lbn_t);
static	int flush_inodedep_deps(struct mount *, ino_t);
static	int flush_deplist(struct allocdirectlst *, int, int *);
static	int handle_written_filepage(struct pagedep *, struct buf *);
static	int handle_written_sbdep(struct sbdep *, struct buf *);
static	void initiate_write_sbdep(struct sbdep *);
static  void diradd_inode_written(struct diradd *, struct inodedep *);
static	int handle_written_indirdep(struct indirdep *, struct buf *,
	    struct buf**);
static	int handle_written_inodeblock(struct inodedep *, struct buf *);
static	int handle_written_bmsafemap(struct bmsafemap *, struct buf *);
static	void handle_written_jaddref(struct jaddref *);
static	void handle_written_jremref(struct jremref *);
static	void handle_written_jseg(struct jseg *, struct buf *);
static	void handle_written_jnewblk(struct jnewblk *);
static	void handle_written_jfreeblk(struct jfreeblk *);
static	void handle_written_jfreefrag(struct jfreefrag *);
static	void complete_jseg(struct jseg *);
static	void jseg_write(struct fs *, struct jblocks *, struct jseg *,
	    uint8_t *);
static	void jaddref_write(struct jaddref *, struct jseg *, uint8_t *);
static	void jremref_write(struct jremref *, struct jseg *, uint8_t *);
static	void jmvref_write(struct jmvref *, struct jseg *, uint8_t *);
static	void jtrunc_write(struct jtrunc *, struct jseg *, uint8_t *);
static	void jnewblk_write(struct jnewblk *, struct jseg *, uint8_t *);
static	void jfreeblk_write(struct jfreeblk *, struct jseg *, uint8_t *);
static	void jfreefrag_write(struct jfreefrag *, struct jseg *, uint8_t *);
static	inline void inoref_write(struct inoref *, struct jseg *,
	    struct jrefrec *);
static	void handle_allocdirect_partdone(struct allocdirect *,
	    struct workhead *);
static	void cancel_newblk(struct newblk *, struct workhead *);
static	void indirdep_complete(struct indirdep *);
static	void handle_allocindir_partdone(struct allocindir *);
static	void initiate_write_filepage(struct pagedep *, struct buf *);
static	void initiate_write_indirdep(struct indirdep*, struct buf *);
static	void handle_written_mkdir(struct mkdir *, int);
static	void initiate_write_bmsafemap(struct bmsafemap *, struct buf *);
static	void initiate_write_inodeblock_ufs1(struct inodedep *, struct buf *);
static	void initiate_write_inodeblock_ufs2(struct inodedep *, struct buf *);
static	void handle_workitem_freefile(struct freefile *);
static	void handle_workitem_remove(struct dirrem *, struct vnode *);
static	struct dirrem *newdirrem(struct buf *, struct inode *,
	    struct inode *, int, struct dirrem **);
static	void cancel_indirdep(struct indirdep *, struct buf *, struct inodedep *,
	    struct freeblks *);
static	void free_indirdep(struct indirdep *);
static	void free_diradd(struct diradd *, struct workhead *);
static	void merge_diradd(struct inodedep *, struct diradd *);
static	void complete_diradd(struct diradd *);
static	struct diradd *diradd_lookup(struct pagedep *, int);
static	struct jremref *cancel_diradd_dotdot(struct inode *, struct dirrem *,
	    struct jremref *);
static	struct jremref *cancel_mkdir_dotdot(struct inode *, struct dirrem *,
	    struct jremref *);
static	void cancel_diradd(struct diradd *, struct dirrem *, struct jremref *,
	    struct jremref *, struct jremref *);
static	void dirrem_journal(struct dirrem *, struct jremref *, struct jremref *,
	    struct jremref *);
static	void cancel_allocindir(struct allocindir *, struct inodedep *,
	    struct freeblks *);
static	void complete_mkdir(struct mkdir *);
static	void free_newdirblk(struct newdirblk *);
static	void free_jremref(struct jremref *);
static	void free_jaddref(struct jaddref *);
static	void free_jsegdep(struct jsegdep *);
static	void free_jseg(struct jseg *);
static	void free_jnewblk(struct jnewblk *);
static	void free_jfreeblk(struct jfreeblk *);
static	void free_jfreefrag(struct jfreefrag *);
static	void free_freedep(struct freedep *);
static	void journal_jremref(struct dirrem *, struct jremref *,
	    struct inodedep *);
static	void cancel_jnewblk(struct jnewblk *, struct workhead *);
static	int cancel_jaddref(struct jaddref *, struct inodedep *,
	    struct workhead *);
static	void cancel_jfreefrag(struct jfreefrag *);
static	void indir_trunc(struct freework *, ufs2_daddr_t, ufs_lbn_t);
static	int deallocate_dependencies(struct buf *, struct inodedep *,
	    struct freeblks *);
static	void free_newblk(struct newblk *);
static	void cancel_allocdirect(struct allocdirectlst *,
	    struct allocdirect *, struct freeblks *, int);
static	int check_inode_unwritten(struct inodedep *);
static	int free_inodedep(struct inodedep *);
static	void freework_freeblock(struct freework *);
static	void handle_workitem_freeblocks(struct freeblks *, int);
static	void handle_complete_freeblocks(struct freeblks *);
static	void handle_workitem_indirblk(struct freework *);
static	void handle_written_freework(struct freework *);
static	void merge_inode_lists(struct allocdirectlst *,struct allocdirectlst *);
static	void setup_allocindir_phase2(struct buf *, struct inode *,
	    struct inodedep *, struct allocindir *, ufs_lbn_t);
static	struct allocindir *newallocindir(struct inode *, int, ufs2_daddr_t,
	    ufs2_daddr_t, ufs_lbn_t);
static	void handle_workitem_freefrag(struct freefrag *);
static	struct freefrag *newfreefrag(struct inode *, ufs2_daddr_t, long,
	    ufs_lbn_t);
static	void allocdirect_merge(struct allocdirectlst *,
	    struct allocdirect *, struct allocdirect *);
static	struct freefrag *allocindir_merge(struct allocindir *,
	    struct allocindir *);
static	int bmsafemap_find(struct bmsafemap_hashhead *, struct mount *, int,
	    struct bmsafemap **);
static	struct bmsafemap *bmsafemap_lookup(struct mount *, struct buf *,
	    int cg);
static	int newblk_find(struct newblk_hashhead *, struct mount *, ufs2_daddr_t,
	    int, struct newblk **);
static	int newblk_lookup(struct mount *, ufs2_daddr_t, int, struct newblk **);
static	int inodedep_find(struct inodedep_hashhead *, struct fs *, ino_t,
	    struct inodedep **);
static	int inodedep_lookup(struct mount *, ino_t, int, struct inodedep **);
static	int pagedep_lookup(struct mount *, ino_t, ufs_lbn_t, int,
	    struct pagedep **);
static	int pagedep_find(struct pagedep_hashhead *, ino_t, ufs_lbn_t,
	    struct mount *mp, int, struct pagedep **);
static	void pause_timer(void *);
static	int request_cleanup(struct mount *, int);
static	int process_worklist_item(struct mount *, int);
static	void process_removes(struct vnode *);
static	void jwork_move(struct workhead *, struct workhead *);
static	void add_to_worklist(struct worklist *, int);
static	void remove_from_worklist(struct worklist *);
static	void softdep_flush(void);
static	int softdep_speedup(void);
static	void worklist_speedup(void);
static	int journal_mount(struct mount *, struct fs *, struct ucred *);
static	void journal_unmount(struct mount *);
static	int journal_space(struct ufsmount *, int);
static	void journal_suspend(struct ufsmount *);
static	int journal_unsuspend(struct ufsmount *ump);
static	void softdep_prelink(struct vnode *, struct vnode *);
static	void add_to_journal(struct worklist *);
static	void remove_from_journal(struct worklist *);
static	void softdep_process_journal(struct mount *, int);
static	struct jremref *newjremref(struct dirrem *, struct inode *,
	    struct inode *ip, off_t, nlink_t);
static	struct jaddref *newjaddref(struct inode *, ino_t, off_t, int16_t,
	    uint16_t);
static inline void newinoref(struct inoref *, ino_t, ino_t, off_t, nlink_t,
	    uint16_t);
static inline struct jsegdep *inoref_jseg(struct inoref *);
static	struct jmvref *newjmvref(struct inode *, ino_t, off_t, off_t);
static	struct jfreeblk *newjfreeblk(struct freeblks *, ufs_lbn_t,
	    ufs2_daddr_t, int);
static	struct jfreefrag *newjfreefrag(struct freefrag *, struct inode *,
	    ufs2_daddr_t, long, ufs_lbn_t);
static	struct freework *newfreework(struct ufsmount *, struct freeblks *,
	    struct freework *, ufs_lbn_t, ufs2_daddr_t, int, int);
static	void jwait(struct worklist *wk);
static	struct inodedep *inodedep_lookup_ip(struct inode *);
static	int bmsafemap_rollbacks(struct bmsafemap *);
static	struct freefile *handle_bufwait(struct inodedep *, struct workhead *);
static	void handle_jwork(struct workhead *);
static	struct mkdir *setup_newdir(struct diradd *, ino_t, ino_t, struct buf *,
	    struct mkdir **);
static	struct jblocks *jblocks_create(void);
static	ufs2_daddr_t jblocks_alloc(struct jblocks *, int, int *);
static	void jblocks_free(struct jblocks *, struct mount *, int);
static	void jblocks_destroy(struct jblocks *);
static	void jblocks_add(struct jblocks *, ufs2_daddr_t, int);

/*
 * Exported softdep operations.
 */
static	void softdep_disk_io_initiation(struct buf *);
static	void softdep_disk_write_complete(struct buf *);
static	void softdep_deallocate_dependencies(struct buf *);
static	int softdep_count_dependencies(struct buf *bp, int);

static struct mtx lk;
MTX_SYSINIT(softdep_lock, &lk, "Softdep Lock", MTX_DEF);

#define TRY_ACQUIRE_LOCK(lk)		mtx_trylock(lk)
#define ACQUIRE_LOCK(lk)		mtx_lock(lk)
#define FREE_LOCK(lk)			mtx_unlock(lk)

#define	BUF_AREC(bp)			lockallowrecurse(&(bp)->b_lock)
#define	BUF_NOREC(bp)			lockdisablerecurse(&(bp)->b_lock)

/*
 * Worklist queue management.
 * These routines require that the lock be held.
 */
#ifndef /* NOT */ DEBUG
#define WORKLIST_INSERT(head, item) do {	\
	(item)->wk_state |= ONWORKLIST;		\
	LIST_INSERT_HEAD(head, item, wk_list);	\
} while (0)
#define WORKLIST_REMOVE(item) do {		\
	(item)->wk_state &= ~ONWORKLIST;	\
	LIST_REMOVE(item, wk_list);		\
} while (0)
#define WORKLIST_INSERT_UNLOCKED	WORKLIST_INSERT
#define WORKLIST_REMOVE_UNLOCKED	WORKLIST_REMOVE

#else /* DEBUG */
static	void worklist_insert(struct workhead *, struct worklist *, int);
static	void worklist_remove(struct worklist *, int);

#define WORKLIST_INSERT(head, item) worklist_insert(head, item, 1)
#define WORKLIST_INSERT_UNLOCKED(head, item) worklist_insert(head, item, 0)
#define WORKLIST_REMOVE(item) worklist_remove(item, 1)
#define WORKLIST_REMOVE_UNLOCKED(item) worklist_remove(item, 0)

static void
worklist_insert(head, item, locked)
	struct workhead *head;
	struct worklist *item;
	int locked;
{

	if (locked)
		mtx_assert(&lk, MA_OWNED);
	if (item->wk_state & ONWORKLIST)
		panic("worklist_insert: %p %s(0x%X) already on list",
		    item, TYPENAME(item->wk_type), item->wk_state);
	item->wk_state |= ONWORKLIST;
	LIST_INSERT_HEAD(head, item, wk_list);
}

static void
worklist_remove(item, locked)
	struct worklist *item;
	int locked;
{

	if (locked)
		mtx_assert(&lk, MA_OWNED);
	if ((item->wk_state & ONWORKLIST) == 0)
		panic("worklist_remove: %p %s(0x%X) not on list",
		    item, TYPENAME(item->wk_type), item->wk_state);
	item->wk_state &= ~ONWORKLIST;
	LIST_REMOVE(item, wk_list);
}
#endif /* DEBUG */

/*
 * Merge two jsegdeps keeping only the oldest one as newer references
 * can't be discarded until after older references.
 */
static inline struct jsegdep *
jsegdep_merge(struct jsegdep *one, struct jsegdep *two)
{
	struct jsegdep *swp;

	if (two == NULL)
		return (one);

	if (one->jd_seg->js_seq > two->jd_seg->js_seq) {
		swp = one;
		one = two;
		two = swp;
	}
	WORKLIST_REMOVE(&two->jd_list);
	free_jsegdep(two);

	return (one);
}

/*
 * If two freedeps are compatible free one to reduce list size.
 */
static inline struct freedep *
freedep_merge(struct freedep *one, struct freedep *two)
{
	if (two == NULL)
		return (one);

	if (one->fd_freework == two->fd_freework) {
		WORKLIST_REMOVE(&two->fd_list);
		free_freedep(two);
	}
	return (one);
}

/*
 * Move journal work from one list to another.  Duplicate freedeps and
 * jsegdeps are coalesced to keep the lists as small as possible.
 */
static void
jwork_move(dst, src)
	struct workhead *dst;
	struct workhead *src;
{
	struct freedep *freedep;
	struct jsegdep *jsegdep;
	struct worklist *wkn;
	struct worklist *wk;

	KASSERT(dst != src,
	    ("jwork_move: dst == src"));
	freedep = NULL;
	jsegdep = NULL;
	LIST_FOREACH_SAFE(wk, dst, wk_list, wkn) {
		if (wk->wk_type == D_JSEGDEP)
			jsegdep = jsegdep_merge(WK_JSEGDEP(wk), jsegdep);
		if (wk->wk_type == D_FREEDEP)
			freedep = freedep_merge(WK_FREEDEP(wk), freedep);
	}

	mtx_assert(&lk, MA_OWNED);
	while ((wk = LIST_FIRST(src)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(dst, wk);
		if (wk->wk_type == D_JSEGDEP) {
			jsegdep = jsegdep_merge(WK_JSEGDEP(wk), jsegdep);
			continue;
		}
		if (wk->wk_type == D_FREEDEP)
			freedep = freedep_merge(WK_FREEDEP(wk), freedep);
	}
}

/*
 * Routines for tracking and managing workitems.
 */
static	void workitem_free(struct worklist *, int);
static	void workitem_alloc(struct worklist *, int, struct mount *);

#define	WORKITEM_FREE(item, type) workitem_free((struct worklist *)(item), (type))

static void
workitem_free(item, type)
	struct worklist *item;
	int type;
{
	struct ufsmount *ump;
	mtx_assert(&lk, MA_OWNED);

#ifdef DEBUG
	if (item->wk_state & ONWORKLIST)
		panic("workitem_free: %s(0x%X) still on list",
		    TYPENAME(item->wk_type), item->wk_state);
	if (item->wk_type != type)
		panic("workitem_free: type mismatch %s != %s",
		    TYPENAME(item->wk_type), TYPENAME(type));
#endif
	ump = VFSTOUFS(item->wk_mp);
	if (--ump->softdep_deps == 0 && ump->softdep_req)
		wakeup(&ump->softdep_deps);
	dep_current[type]--;
	free(item, DtoM(type));
}

static void
workitem_alloc(item, type, mp)
	struct worklist *item;
	int type;
	struct mount *mp;
{
	item->wk_type = type;
	item->wk_mp = mp;
	item->wk_state = 0;
	ACQUIRE_LOCK(&lk);
	dep_current[type]++;
	dep_total[type]++;
	VFSTOUFS(mp)->softdep_deps++;
	VFSTOUFS(mp)->softdep_accdeps++;
	FREE_LOCK(&lk);
}

/*
 * Workitem queue management
 */
static int max_softdeps;	/* maximum number of structs before slowdown */
static int maxindirdeps = 50;	/* max number of indirdeps before slowdown */
static int tickdelay = 2;	/* number of ticks to pause during slowdown */
static int proc_waiting;	/* tracks whether we have a timeout posted */
static int *stat_countp;	/* statistic to count in proc_waiting timeout */
static struct callout softdep_callout;
static int req_pending;
static int req_clear_inodedeps;	/* syncer process flush some inodedeps */
#define FLUSH_INODES		1
static int req_clear_remove;	/* syncer process flush some freeblks */
#define FLUSH_REMOVE		2
#define FLUSH_REMOVE_WAIT	3
static long num_freeblkdep;	/* number of freeblks workitems allocated */

/*
 * runtime statistics
 */
static int stat_worklist_push;	/* number of worklist cleanups */
static int stat_blk_limit_push;	/* number of times block limit neared */
static int stat_ino_limit_push;	/* number of times inode limit neared */
static int stat_blk_limit_hit;	/* number of times block slowdown imposed */
static int stat_ino_limit_hit;	/* number of times inode slowdown imposed */
static int stat_sync_limit_hit;	/* number of synchronous slowdowns imposed */
static int stat_indir_blk_ptrs;	/* bufs redirtied as indir ptrs not written */
static int stat_inode_bitmap;	/* bufs redirtied as inode bitmap not written */
static int stat_direct_blk_ptrs;/* bufs redirtied as direct ptrs not written */
static int stat_dir_entry;	/* bufs redirtied as dir entry cannot write */
static int stat_jaddref;	/* bufs redirtied as ino bitmap can not write */
static int stat_jnewblk;	/* bufs redirtied as blk bitmap can not write */
static int stat_journal_min;	/* Times hit journal min threshold */
static int stat_journal_low;	/* Times hit journal low threshold */
static int stat_journal_wait;	/* Times blocked in jwait(). */
static int stat_jwait_filepage;	/* Times blocked in jwait() for filepage. */
static int stat_jwait_freeblks;	/* Times blocked in jwait() for freeblks. */
static int stat_jwait_inode;	/* Times blocked in jwait() for inodes. */
static int stat_jwait_newblk;	/* Times blocked in jwait() for newblks. */

SYSCTL_INT(_debug_softdep, OID_AUTO, max_softdeps, CTLFLAG_RW,
    &max_softdeps, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, tickdelay, CTLFLAG_RW,
    &tickdelay, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, maxindirdeps, CTLFLAG_RW,
    &maxindirdeps, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, worklist_push, CTLFLAG_RW,
    &stat_worklist_push, 0,"");
SYSCTL_INT(_debug_softdep, OID_AUTO, blk_limit_push, CTLFLAG_RW,
    &stat_blk_limit_push, 0,"");
SYSCTL_INT(_debug_softdep, OID_AUTO, ino_limit_push, CTLFLAG_RW,
    &stat_ino_limit_push, 0,"");
SYSCTL_INT(_debug_softdep, OID_AUTO, blk_limit_hit, CTLFLAG_RW,
    &stat_blk_limit_hit, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, ino_limit_hit, CTLFLAG_RW,
    &stat_ino_limit_hit, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, sync_limit_hit, CTLFLAG_RW,
    &stat_sync_limit_hit, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, indir_blk_ptrs, CTLFLAG_RW,
    &stat_indir_blk_ptrs, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, inode_bitmap, CTLFLAG_RW,
    &stat_inode_bitmap, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, direct_blk_ptrs, CTLFLAG_RW,
    &stat_direct_blk_ptrs, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, dir_entry, CTLFLAG_RW,
    &stat_dir_entry, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, jaddref_rollback, CTLFLAG_RW,
    &stat_jaddref, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, jnewblk_rollback, CTLFLAG_RW,
    &stat_jnewblk, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, journal_low, CTLFLAG_RW,
    &stat_journal_low, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, journal_min, CTLFLAG_RW,
    &stat_journal_min, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, journal_wait, CTLFLAG_RW,
    &stat_journal_wait, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, jwait_filepage, CTLFLAG_RW,
    &stat_jwait_filepage, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, jwait_freeblks, CTLFLAG_RW,
    &stat_jwait_freeblks, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, jwait_inode, CTLFLAG_RW,
    &stat_jwait_inode, 0, "");
SYSCTL_INT(_debug_softdep, OID_AUTO, jwait_newblk, CTLFLAG_RW,
    &stat_jwait_newblk, 0, "");

SYSCTL_DECL(_vfs_ffs);

LIST_HEAD(bmsafemap_hashhead, bmsafemap) *bmsafemap_hashtbl;
static u_long	bmsafemap_hash;	/* size of hash table - 1 */

static int compute_summary_at_mount = 0;	/* Whether to recompute the summary at mount time */
SYSCTL_INT(_vfs_ffs, OID_AUTO, compute_summary_at_mount, CTLFLAG_RW,
	   &compute_summary_at_mount, 0, "Recompute summary at mount");

static struct proc *softdepproc;
static struct kproc_desc softdep_kp = {
	"softdepflush",
	softdep_flush,
	&softdepproc
};
SYSINIT(sdproc, SI_SUB_KTHREAD_UPDATE, SI_ORDER_ANY, kproc_start,
    &softdep_kp);

static void
softdep_flush(void)
{
	struct mount *nmp;
	struct mount *mp;
	struct ufsmount *ump;
	struct thread *td;
	int remaining;
	int progress;
	int vfslocked;

	td = curthread;
	td->td_pflags |= TDP_NORUNNINGBUF;

	for (;;) {	
		kproc_suspend_check(softdepproc);
		vfslocked = VFS_LOCK_GIANT((struct mount *)NULL);
		ACQUIRE_LOCK(&lk);
		/*
		 * If requested, try removing inode or removal dependencies.
		 */
		if (req_clear_inodedeps) {
			clear_inodedeps(td);
			req_clear_inodedeps -= 1;
			wakeup_one(&proc_waiting);
		}
		if (req_clear_remove) {
			clear_remove(td);
			req_clear_remove -= 1;
			wakeup_one(&proc_waiting);
		}
		FREE_LOCK(&lk);
		VFS_UNLOCK_GIANT(vfslocked);
		remaining = progress = 0;
		mtx_lock(&mountlist_mtx);
		for (mp = TAILQ_FIRST(&mountlist); mp != NULL; mp = nmp)  {
			nmp = TAILQ_NEXT(mp, mnt_list);
			if ((mp->mnt_flag & MNT_SOFTDEP) == 0)
				continue;
			if (vfs_busy(mp, MBF_NOWAIT | MBF_MNTLSTLOCK))
				continue;
			vfslocked = VFS_LOCK_GIANT(mp);
			progress += softdep_process_worklist(mp, 0);
			ump = VFSTOUFS(mp);
			remaining += ump->softdep_on_worklist -
				ump->softdep_on_worklist_inprogress;
			VFS_UNLOCK_GIANT(vfslocked);
			mtx_lock(&mountlist_mtx);
			nmp = TAILQ_NEXT(mp, mnt_list);
			vfs_unbusy(mp);
		}
		mtx_unlock(&mountlist_mtx);
		if (remaining && progress)
			continue;
		ACQUIRE_LOCK(&lk);
		if (!req_pending)
			msleep(&req_pending, &lk, PVM, "sdflush", hz);
		req_pending = 0;
		FREE_LOCK(&lk);
	}
}

static void
worklist_speedup(void)
{
	mtx_assert(&lk, MA_OWNED);
	if (req_pending == 0) {
		req_pending = 1;
		wakeup(&req_pending);
	}
}

static int
softdep_speedup(void)
{

	worklist_speedup();
	bd_speedup();
	return speedup_syncer();
}

/*
 * Add an item to the end of the work queue.
 * This routine requires that the lock be held.
 * This is the only routine that adds items to the list.
 * The following routine is the only one that removes items
 * and does so in order from first to last.
 */
static void
add_to_worklist(wk, nodelay)
	struct worklist *wk;
	int nodelay;
{
	struct ufsmount *ump;

	mtx_assert(&lk, MA_OWNED);
	ump = VFSTOUFS(wk->wk_mp);
	if (wk->wk_state & ONWORKLIST)
		panic("add_to_worklist: %s(0x%X) already on list",
		    TYPENAME(wk->wk_type), wk->wk_state);
	wk->wk_state |= ONWORKLIST;
	if (LIST_EMPTY(&ump->softdep_workitem_pending))
		LIST_INSERT_HEAD(&ump->softdep_workitem_pending, wk, wk_list);
	else
		LIST_INSERT_AFTER(ump->softdep_worklist_tail, wk, wk_list);
	ump->softdep_worklist_tail = wk;
	ump->softdep_on_worklist += 1;
	if (nodelay)
		worklist_speedup();
}

/*
 * Remove the item to be processed. If we are removing the last
 * item on the list, we need to recalculate the tail pointer.
 */
static void
remove_from_worklist(wk)
	struct worklist *wk;
{
	struct ufsmount *ump;
	struct worklist *wkend;

	ump = VFSTOUFS(wk->wk_mp);
	WORKLIST_REMOVE(wk);
	if (wk == ump->softdep_worklist_tail) {
		LIST_FOREACH(wkend, &ump->softdep_workitem_pending, wk_list)
			if (LIST_NEXT(wkend, wk_list) == NULL)
				break;
		ump->softdep_worklist_tail = wkend;
	}
	ump->softdep_on_worklist -= 1;
}

/*
 * Process that runs once per second to handle items in the background queue.
 *
 * Note that we ensure that everything is done in the order in which they
 * appear in the queue. The code below depends on this property to ensure
 * that blocks of a file are freed before the inode itself is freed. This
 * ordering ensures that no new <vfsid, inum, lbn> triples will be generated
 * until all the old ones have been purged from the dependency lists.
 */
int 
softdep_process_worklist(mp, full)
	struct mount *mp;
	int full;
{
	struct thread *td = curthread;
	int cnt, matchcnt;
	struct ufsmount *ump;
	long starttime;

	KASSERT(mp != NULL, ("softdep_process_worklist: NULL mp"));
	/*
	 * Record the process identifier of our caller so that we can give
	 * this process preferential treatment in request_cleanup below.
	 */
	matchcnt = 0;
	ump = VFSTOUFS(mp);
	ACQUIRE_LOCK(&lk);
	starttime = time_second;
	softdep_process_journal(mp, full?MNT_WAIT:0);
	while (ump->softdep_on_worklist > 0) {
		if ((cnt = process_worklist_item(mp, LK_NOWAIT)) == -1)
			break;
		else
			matchcnt += cnt;
		/*
		 * If requested, try removing inode or removal dependencies.
		 */
		if (req_clear_inodedeps) {
			clear_inodedeps(td);
			req_clear_inodedeps -= 1;
			wakeup_one(&proc_waiting);
		}
		if (req_clear_remove) {
			clear_remove(td);
			req_clear_remove -= 1;
			wakeup_one(&proc_waiting);
		}
		/*
		 * We do not generally want to stop for buffer space, but if
		 * we are really being a buffer hog, we will stop and wait.
		 */
		if (should_yield()) {
			FREE_LOCK(&lk);
			uio_yield();
			bwillwrite();
			ACQUIRE_LOCK(&lk);
		}
		/*
		 * Never allow processing to run for more than one
		 * second. Otherwise the other mountpoints may get
		 * excessively backlogged.
		 */
		if (!full && starttime != time_second)
			break;
	}
	if (full == 0)
		journal_unsuspend(ump);
	FREE_LOCK(&lk);
	return (matchcnt);
}

/*
 * Process all removes associated with a vnode if we are running out of
 * journal space.  Any other process which attempts to flush these will
 * be unable as we have the vnodes locked.
 */
static void
process_removes(vp)
	struct vnode *vp;
{
	struct inodedep *inodedep;
	struct dirrem *dirrem;
	struct mount *mp;
	ino_t inum;

	mtx_assert(&lk, MA_OWNED);

	mp = vp->v_mount;
	inum = VTOI(vp)->i_number;
	for (;;) {
		if (inodedep_lookup(mp, inum, 0, &inodedep) == 0)
			return;
		LIST_FOREACH(dirrem, &inodedep->id_dirremhd, dm_inonext)
			if ((dirrem->dm_state & (COMPLETE | ONWORKLIST)) ==
			    (COMPLETE | ONWORKLIST))
				break;
		if (dirrem == NULL)
			return;
		/*
		 * If another thread is trying to lock this vnode it will
		 * fail but we must wait for it to do so before we can
		 * proceed.
		 */
		if (dirrem->dm_state & INPROGRESS) {
			dirrem->dm_state |= IOWAITING;
			msleep(&dirrem->dm_list, &lk, PVM, "pwrwait", 0);
			continue;
		}
		remove_from_worklist(&dirrem->dm_list);
		FREE_LOCK(&lk);
		if (vn_start_secondary_write(NULL, &mp, V_NOWAIT))
			panic("process_removes: suspended filesystem");
		handle_workitem_remove(dirrem, vp);
		vn_finished_secondary_write(mp);
		ACQUIRE_LOCK(&lk);
	}
}

/*
 * Process one item on the worklist.
 */
static int
process_worklist_item(mp, flags)
	struct mount *mp;
	int flags;
{
	struct worklist *wk;
	struct ufsmount *ump;
	struct vnode *vp;
	int matchcnt = 0;

	mtx_assert(&lk, MA_OWNED);
	KASSERT(mp != NULL, ("process_worklist_item: NULL mp"));
	/*
	 * If we are being called because of a process doing a
	 * copy-on-write, then it is not safe to write as we may
	 * recurse into the copy-on-write routine.
	 */
	if (curthread->td_pflags & TDP_COWINPROGRESS)
		return (-1);
	/*
	 * Normally we just process each item on the worklist in order.
	 * However, if we are in a situation where we cannot lock any
	 * inodes, we have to skip over any dirrem requests whose
	 * vnodes are resident and locked.
	 */
	vp = NULL;
	ump = VFSTOUFS(mp);
	LIST_FOREACH(wk, &ump->softdep_workitem_pending, wk_list) {
		if (wk->wk_state & INPROGRESS)
			continue;
		if ((flags & LK_NOWAIT) == 0 || wk->wk_type != D_DIRREM)
			break;
		wk->wk_state |= INPROGRESS;
		ump->softdep_on_worklist_inprogress++;
		FREE_LOCK(&lk);
		ffs_vgetf(mp, WK_DIRREM(wk)->dm_oldinum,
		    LK_NOWAIT | LK_EXCLUSIVE, &vp, FFSV_FORCEINSMQ);
		ACQUIRE_LOCK(&lk);
		if (wk->wk_state & IOWAITING) {
			wk->wk_state &= ~IOWAITING;
			wakeup(wk);
		}
		wk->wk_state &= ~INPROGRESS;
		ump->softdep_on_worklist_inprogress--;
		if (vp != NULL)
			break;
	}
	if (wk == 0)
		return (-1);
	remove_from_worklist(wk);
	FREE_LOCK(&lk);
	if (vn_start_secondary_write(NULL, &mp, V_NOWAIT))
		panic("process_worklist_item: suspended filesystem");
	matchcnt++;
	switch (wk->wk_type) {

	case D_DIRREM:
		/* removal of a directory entry */
		handle_workitem_remove(WK_DIRREM(wk), vp);
		if (vp)
			vput(vp);
		break;

	case D_FREEBLKS:
		/* releasing blocks and/or fragments from a file */
		handle_workitem_freeblocks(WK_FREEBLKS(wk), flags & LK_NOWAIT);
		break;

	case D_FREEFRAG:
		/* releasing a fragment when replaced as a file grows */
		handle_workitem_freefrag(WK_FREEFRAG(wk));
		break;

	case D_FREEFILE:
		/* releasing an inode when its link count drops to 0 */
		handle_workitem_freefile(WK_FREEFILE(wk));
		break;

	case D_FREEWORK:
		/* Final block in an indirect was freed. */
		handle_workitem_indirblk(WK_FREEWORK(wk));
		break;

	default:
		panic("%s_process_worklist: Unknown type %s",
		    "softdep", TYPENAME(wk->wk_type));
		/* NOTREACHED */
	}
	vn_finished_secondary_write(mp);
	ACQUIRE_LOCK(&lk);
	return (matchcnt);
}

/*
 * Move dependencies from one buffer to another.
 */
int
softdep_move_dependencies(oldbp, newbp)
	struct buf *oldbp;
	struct buf *newbp;
{
	struct worklist *wk, *wktail;
	int dirty;

	dirty = 0;
	wktail = NULL;
	ACQUIRE_LOCK(&lk);
	while ((wk = LIST_FIRST(&oldbp->b_dep)) != NULL) {
		LIST_REMOVE(wk, wk_list);
		if (wk->wk_type == D_BMSAFEMAP &&
		    bmsafemap_rollbacks(WK_BMSAFEMAP(wk)))
			dirty = 1;
		if (wktail == 0)
			LIST_INSERT_HEAD(&newbp->b_dep, wk, wk_list);
		else
			LIST_INSERT_AFTER(wktail, wk, wk_list);
		wktail = wk;
	}
	FREE_LOCK(&lk);

	return (dirty);
}

/*
 * Purge the work list of all items associated with a particular mount point.
 */
int
softdep_flushworklist(oldmnt, countp, td)
	struct mount *oldmnt;
	int *countp;
	struct thread *td;
{
	struct vnode *devvp;
	int count, error = 0;
	struct ufsmount *ump;

	/*
	 * Alternately flush the block device associated with the mount
	 * point and process any dependencies that the flushing
	 * creates. We continue until no more worklist dependencies
	 * are found.
	 */
	*countp = 0;
	ump = VFSTOUFS(oldmnt);
	devvp = ump->um_devvp;
	while ((count = softdep_process_worklist(oldmnt, 1)) > 0) {
		*countp += count;
		vn_lock(devvp, LK_EXCLUSIVE | LK_RETRY);
		error = VOP_FSYNC(devvp, MNT_WAIT, td);
		VOP_UNLOCK(devvp, 0);
		if (error)
			break;
	}
	return (error);
}

int
softdep_waitidle(struct mount *mp)
{
	struct ufsmount *ump;
	int error;
	int i;

	ump = VFSTOUFS(mp);
	ACQUIRE_LOCK(&lk);
	for (i = 0; i < 10 && ump->softdep_deps; i++) {
		ump->softdep_req = 1;
		if (ump->softdep_on_worklist)
			panic("softdep_waitidle: work added after flush.");
		msleep(&ump->softdep_deps, &lk, PVM, "softdeps", 1);
	}
	ump->softdep_req = 0;
	FREE_LOCK(&lk);
	error = 0;
	if (i == 10) {
		error = EBUSY;
		printf("softdep_waitidle: Failed to flush worklist for %p\n",
		    mp);
	}

	return (error);
}

/*
 * Flush all vnodes and worklist items associated with a specified mount point.
 */
int
softdep_flushfiles(oldmnt, flags, td)
	struct mount *oldmnt;
	int flags;
	struct thread *td;
{
	int error, depcount, loopcnt, retry_flush_count, retry;

	loopcnt = 10;
	retry_flush_count = 3;
retry_flush:
	error = 0;

	/*
	 * Alternately flush the vnodes associated with the mount
	 * point and process any dependencies that the flushing
	 * creates. In theory, this loop can happen at most twice,
	 * but we give it a few extra just to be sure.
	 */
	for (; loopcnt > 0; loopcnt--) {
		/*
		 * Do another flush in case any vnodes were brought in
		 * as part of the cleanup operations.
		 */
		if ((error = ffs_flushfiles(oldmnt, flags, td)) != 0)
			break;
		if ((error = softdep_flushworklist(oldmnt, &depcount, td)) != 0 ||
		    depcount == 0)
			break;
	}
	/*
	 * If we are unmounting then it is an error to fail. If we
	 * are simply trying to downgrade to read-only, then filesystem
	 * activity can keep us busy forever, so we just fail with EBUSY.
	 */
	if (loopcnt == 0) {
		if (oldmnt->mnt_kern_flag & MNTK_UNMOUNT)
			panic("softdep_flushfiles: looping");
		error = EBUSY;
	}
	if (!error)
		error = softdep_waitidle(oldmnt);
	if (!error) {
		if (oldmnt->mnt_kern_flag & MNTK_UNMOUNT) {
			retry = 0;
			MNT_ILOCK(oldmnt);
			KASSERT((oldmnt->mnt_kern_flag & MNTK_NOINSMNTQ) != 0,
			    ("softdep_flushfiles: !MNTK_NOINSMNTQ"));
			if (oldmnt->mnt_nvnodelistsize > 0) {
				if (--retry_flush_count > 0) {
					retry = 1;
					loopcnt = 3;
				} else
					error = EBUSY;
			}
			MNT_IUNLOCK(oldmnt);
			if (retry)
				goto retry_flush;
		}
	}
	return (error);
}

/*
 * Structure hashing.
 * 
 * There are three types of structures that can be looked up:
 *	1) pagedep structures identified by mount point, inode number,
 *	   and logical block.
 *	2) inodedep structures identified by mount point and inode number.
 *	3) newblk structures identified by mount point and
 *	   physical block number.
 *
 * The "pagedep" and "inodedep" dependency structures are hashed
 * separately from the file blocks and inodes to which they correspond.
 * This separation helps when the in-memory copy of an inode or
 * file block must be replaced. It also obviates the need to access
 * an inode or file page when simply updating (or de-allocating)
 * dependency structures. Lookup of newblk structures is needed to
 * find newly allocated blocks when trying to associate them with
 * their allocdirect or allocindir structure.
 *
 * The lookup routines optionally create and hash a new instance when
 * an existing entry is not found.
 */
#define DEPALLOC	0x0001	/* allocate structure if lookup fails */
#define NODELAY		0x0002	/* cannot do background work */

/*
 * Structures and routines associated with pagedep caching.
 */
LIST_HEAD(pagedep_hashhead, pagedep) *pagedep_hashtbl;
u_long	pagedep_hash;		/* size of hash table - 1 */
#define	PAGEDEP_HASH(mp, inum, lbn) \
	(&pagedep_hashtbl[((((register_t)(mp)) >> 13) + (inum) + (lbn)) & \
	    pagedep_hash])

static int
pagedep_find(pagedephd, ino, lbn, mp, flags, pagedeppp)
	struct pagedep_hashhead *pagedephd;
	ino_t ino;
	ufs_lbn_t lbn;
	struct mount *mp;
	int flags;
	struct pagedep **pagedeppp;
{
	struct pagedep *pagedep;

	LIST_FOREACH(pagedep, pagedephd, pd_hash)
		if (ino == pagedep->pd_ino &&
		    lbn == pagedep->pd_lbn &&
		    mp == pagedep->pd_list.wk_mp)
			break;
	if (pagedep) {
		*pagedeppp = pagedep;
		if ((flags & DEPALLOC) != 0 &&
		    (pagedep->pd_state & ONWORKLIST) == 0)
			return (0);
		return (1);
	}
	*pagedeppp = NULL;
	return (0);
}
/*
 * Look up a pagedep. Return 1 if found, 0 if not found or found
 * when asked to allocate but not associated with any buffer.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in pagedeppp.
 * This routine must be called with splbio interrupts blocked.
 */
static int
pagedep_lookup(mp, ino, lbn, flags, pagedeppp)
	struct mount *mp;
	ino_t ino;
	ufs_lbn_t lbn;
	int flags;
	struct pagedep **pagedeppp;
{
	struct pagedep *pagedep;
	struct pagedep_hashhead *pagedephd;
	int ret;
	int i;

	mtx_assert(&lk, MA_OWNED);
	pagedephd = PAGEDEP_HASH(mp, ino, lbn);

	ret = pagedep_find(pagedephd, ino, lbn, mp, flags, pagedeppp);
	if (*pagedeppp || (flags & DEPALLOC) == 0)
		return (ret);
	FREE_LOCK(&lk);
	pagedep = malloc(sizeof(struct pagedep),
	    M_PAGEDEP, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&pagedep->pd_list, D_PAGEDEP, mp);
	ACQUIRE_LOCK(&lk);
	ret = pagedep_find(pagedephd, ino, lbn, mp, flags, pagedeppp);
	if (*pagedeppp) {
		WORKITEM_FREE(pagedep, D_PAGEDEP);
		return (ret);
	}
	pagedep->pd_ino = ino;
	pagedep->pd_lbn = lbn;
	LIST_INIT(&pagedep->pd_dirremhd);
	LIST_INIT(&pagedep->pd_pendinghd);
	for (i = 0; i < DAHASHSZ; i++)
		LIST_INIT(&pagedep->pd_diraddhd[i]);
	LIST_INSERT_HEAD(pagedephd, pagedep, pd_hash);
	*pagedeppp = pagedep;
	return (0);
}

/*
 * Structures and routines associated with inodedep caching.
 */
LIST_HEAD(inodedep_hashhead, inodedep) *inodedep_hashtbl;
static u_long	inodedep_hash;	/* size of hash table - 1 */
static long	num_inodedep;	/* number of inodedep allocated */
#define	INODEDEP_HASH(fs, inum) \
      (&inodedep_hashtbl[((((register_t)(fs)) >> 13) + (inum)) & inodedep_hash])

static int
inodedep_find(inodedephd, fs, inum, inodedeppp)
	struct inodedep_hashhead *inodedephd;
	struct fs *fs;
	ino_t inum;
	struct inodedep **inodedeppp;
{
	struct inodedep *inodedep;

	LIST_FOREACH(inodedep, inodedephd, id_hash)
		if (inum == inodedep->id_ino && fs == inodedep->id_fs)
			break;
	if (inodedep) {
		*inodedeppp = inodedep;
		return (1);
	}
	*inodedeppp = NULL;

	return (0);
}
/*
 * Look up an inodedep. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in inodedeppp.
 * This routine must be called with splbio interrupts blocked.
 */
static int
inodedep_lookup(mp, inum, flags, inodedeppp)
	struct mount *mp;
	ino_t inum;
	int flags;
	struct inodedep **inodedeppp;
{
	struct inodedep *inodedep;
	struct inodedep_hashhead *inodedephd;
	struct fs *fs;

	mtx_assert(&lk, MA_OWNED);
	fs = VFSTOUFS(mp)->um_fs;
	inodedephd = INODEDEP_HASH(fs, inum);

	if (inodedep_find(inodedephd, fs, inum, inodedeppp))
		return (1);
	if ((flags & DEPALLOC) == 0)
		return (0);
	/*
	 * If we are over our limit, try to improve the situation.
	 */
	if (num_inodedep > max_softdeps && (flags & NODELAY) == 0)
		request_cleanup(mp, FLUSH_INODES);
	FREE_LOCK(&lk);
	inodedep = malloc(sizeof(struct inodedep),
		M_INODEDEP, M_SOFTDEP_FLAGS);
	workitem_alloc(&inodedep->id_list, D_INODEDEP, mp);
	ACQUIRE_LOCK(&lk);
	if (inodedep_find(inodedephd, fs, inum, inodedeppp)) {
		WORKITEM_FREE(inodedep, D_INODEDEP);
		return (1);
	}
	num_inodedep += 1;
	inodedep->id_fs = fs;
	inodedep->id_ino = inum;
	inodedep->id_state = ALLCOMPLETE;
	inodedep->id_nlinkdelta = 0;
	inodedep->id_savedino1 = NULL;
	inodedep->id_savedsize = -1;
	inodedep->id_savedextsize = -1;
	inodedep->id_savednlink = -1;
	inodedep->id_bmsafemap = NULL;
	inodedep->id_mkdiradd = NULL;
	LIST_INIT(&inodedep->id_dirremhd);
	LIST_INIT(&inodedep->id_pendinghd);
	LIST_INIT(&inodedep->id_inowait);
	LIST_INIT(&inodedep->id_bufwait);
	TAILQ_INIT(&inodedep->id_inoreflst);
	TAILQ_INIT(&inodedep->id_inoupdt);
	TAILQ_INIT(&inodedep->id_newinoupdt);
	TAILQ_INIT(&inodedep->id_extupdt);
	TAILQ_INIT(&inodedep->id_newextupdt);
	LIST_INSERT_HEAD(inodedephd, inodedep, id_hash);
	*inodedeppp = inodedep;
	return (0);
}

/*
 * Structures and routines associated with newblk caching.
 */
LIST_HEAD(newblk_hashhead, newblk) *newblk_hashtbl;
u_long	newblk_hash;		/* size of hash table - 1 */
#define	NEWBLK_HASH(fs, inum) \
	(&newblk_hashtbl[((((register_t)(fs)) >> 13) + (inum)) & newblk_hash])

static int
newblk_find(newblkhd, mp, newblkno, flags, newblkpp)
	struct newblk_hashhead *newblkhd;
	struct mount *mp;
	ufs2_daddr_t newblkno;
	int flags;
	struct newblk **newblkpp;
{
	struct newblk *newblk;

	LIST_FOREACH(newblk, newblkhd, nb_hash) {
		if (newblkno != newblk->nb_newblkno)
			continue;
		if (mp != newblk->nb_list.wk_mp)
			continue;
		/*
		 * If we're creating a new dependency don't match those that
		 * have already been converted to allocdirects.  This is for
		 * a frag extend.
		 */
		if ((flags & DEPALLOC) && newblk->nb_list.wk_type != D_NEWBLK)
			continue;
		break;
	}
	if (newblk) {
		*newblkpp = newblk;
		return (1);
	}
	*newblkpp = NULL;
	return (0);
}

/*
 * Look up a newblk. Return 1 if found, 0 if not found.
 * If not found, allocate if DEPALLOC flag is passed.
 * Found or allocated entry is returned in newblkpp.
 */
static int
newblk_lookup(mp, newblkno, flags, newblkpp)
	struct mount *mp;
	ufs2_daddr_t newblkno;
	int flags;
	struct newblk **newblkpp;
{
	struct newblk *newblk;
	struct newblk_hashhead *newblkhd;

	newblkhd = NEWBLK_HASH(VFSTOUFS(mp)->um_fs, newblkno);
	if (newblk_find(newblkhd, mp, newblkno, flags, newblkpp))
		return (1);
	if ((flags & DEPALLOC) == 0)
		return (0);
	FREE_LOCK(&lk);
	newblk = malloc(sizeof(union allblk), M_NEWBLK,
	    M_SOFTDEP_FLAGS | M_ZERO);
	workitem_alloc(&newblk->nb_list, D_NEWBLK, mp);
	ACQUIRE_LOCK(&lk);
	if (newblk_find(newblkhd, mp, newblkno, flags, newblkpp)) {
		WORKITEM_FREE(newblk, D_NEWBLK);
		return (1);
	}
	newblk->nb_freefrag = NULL;
	LIST_INIT(&newblk->nb_indirdeps);
	LIST_INIT(&newblk->nb_newdirblk);
	LIST_INIT(&newblk->nb_jwork);
	newblk->nb_state = ATTACHED;
	newblk->nb_newblkno = newblkno;
	LIST_INSERT_HEAD(newblkhd, newblk, nb_hash);
	*newblkpp = newblk;
	return (0);
}

/*
 * Executed during filesystem system initialization before
 * mounting any filesystems.
 */
void 
softdep_initialize()
{

	LIST_INIT(&mkdirlisthd);
	max_softdeps = desiredvnodes * 4;
	pagedep_hashtbl = hashinit(desiredvnodes / 5, M_PAGEDEP, &pagedep_hash);
	inodedep_hashtbl = hashinit(desiredvnodes, M_INODEDEP, &inodedep_hash);
	newblk_hashtbl = hashinit(desiredvnodes / 5,  M_NEWBLK, &newblk_hash);
	bmsafemap_hashtbl = hashinit(1024, M_BMSAFEMAP, &bmsafemap_hash);

	/* initialise bioops hack */
	bioops.io_start = softdep_disk_io_initiation;
	bioops.io_complete = softdep_disk_write_complete;
	bioops.io_deallocate = softdep_deallocate_dependencies;
	bioops.io_countdeps = softdep_count_dependencies;

	/* Initialize the callout with an mtx. */
	callout_init_mtx(&softdep_callout, &lk, 0);
}

/*
 * Executed after all filesystems have been unmounted during
 * filesystem module unload.
 */
void
softdep_uninitialize()
{

	callout_drain(&softdep_callout);
	hashdestroy(pagedep_hashtbl, M_PAGEDEP, pagedep_hash);
	hashdestroy(inodedep_hashtbl, M_INODEDEP, inodedep_hash);
	hashdestroy(newblk_hashtbl, M_NEWBLK, newblk_hash);
	hashdestroy(bmsafemap_hashtbl, M_BMSAFEMAP, bmsafemap_hash);
}

/*
 * Called at mount time to notify the dependency code that a
 * filesystem wishes to use it.
 */
int
softdep_mount(devvp, mp, fs, cred)
	struct vnode *devvp;
	struct mount *mp;
	struct fs *fs;
	struct ucred *cred;
{
	struct csum_total cstotal;
	struct ufsmount *ump;
	struct cg *cgp;
	struct buf *bp;
	int error, cyl;

	MNT_ILOCK(mp);
	mp->mnt_flag = (mp->mnt_flag & ~MNT_ASYNC) | MNT_SOFTDEP;
	if ((mp->mnt_kern_flag & MNTK_SOFTDEP) == 0) {
		mp->mnt_kern_flag = (mp->mnt_kern_flag & ~MNTK_ASYNC) | 
			MNTK_SOFTDEP;
		mp->mnt_noasync++;
	}
	MNT_IUNLOCK(mp);
	ump = VFSTOUFS(mp);
	LIST_INIT(&ump->softdep_workitem_pending);
	LIST_INIT(&ump->softdep_journal_pending);
	TAILQ_INIT(&ump->softdep_unlinked);
	ump->softdep_worklist_tail = NULL;
	ump->softdep_on_worklist = 0;
	ump->softdep_deps = 0;
	if ((fs->fs_flags & FS_SUJ) &&
	    (error = journal_mount(mp, fs, cred)) != 0) {
		printf("Failed to start journal: %d\n", error);
		return (error);
	}
	/*
	 * When doing soft updates, the counters in the
	 * superblock may have gotten out of sync. Recomputation
	 * can take a long time and can be deferred for background
	 * fsck.  However, the old behavior of scanning the cylinder
	 * groups and recalculating them at mount time is available
	 * by setting vfs.ffs.compute_summary_at_mount to one.
	 */
	if (compute_summary_at_mount == 0 || fs->fs_clean != 0)
		return (0);
	bzero(&cstotal, sizeof cstotal);
	for (cyl = 0; cyl < fs->fs_ncg; cyl++) {
		if ((error = bread(devvp, fsbtodb(fs, cgtod(fs, cyl)),
		    fs->fs_cgsize, cred, &bp)) != 0) {
			brelse(bp);
			return (error);
		}
		cgp = (struct cg *)bp->b_data;
		cstotal.cs_nffree += cgp->cg_cs.cs_nffree;
		cstotal.cs_nbfree += cgp->cg_cs.cs_nbfree;
		cstotal.cs_nifree += cgp->cg_cs.cs_nifree;
		cstotal.cs_ndir += cgp->cg_cs.cs_ndir;
		fs->fs_cs(fs, cyl) = cgp->cg_cs;
		brelse(bp);
	}
#ifdef DEBUG
	if (bcmp(&cstotal, &fs->fs_cstotal, sizeof cstotal))
		printf("%s: superblock summary recomputed\n", fs->fs_fsmnt);
#endif
	bcopy(&cstotal, &fs->fs_cstotal, sizeof cstotal);
	return (0);
}

void
softdep_unmount(mp)
	struct mount *mp;
{

	if (mp->mnt_kern_flag & MNTK_SUJ)
		journal_unmount(mp);
}

struct jblocks {
	struct jseglst	jb_segs;	/* TAILQ of current segments. */
	struct jseg	*jb_writeseg;	/* Next write to complete. */
	struct jextent	*jb_extent;	/* Extent array. */
	uint64_t	jb_nextseq;	/* Next sequence number. */
	uint64_t	jb_oldestseq;	/* Oldest active sequence number. */
	int		jb_avail;	/* Available extents. */
	int		jb_used;	/* Last used extent. */
	int		jb_head;	/* Allocator head. */
	int		jb_off;		/* Allocator extent offset. */
	int		jb_blocks;	/* Total disk blocks covered. */
	int		jb_free;	/* Total disk blocks free. */
	int		jb_min;		/* Minimum free space. */
	int		jb_low;		/* Low on space. */
	int		jb_age;		/* Insertion time of oldest rec. */
	int		jb_suspended;	/* Did journal suspend writes? */
};

struct jextent {
	ufs2_daddr_t	je_daddr;	/* Disk block address. */
	int		je_blocks;	/* Disk block count. */
};

static struct jblocks *
jblocks_create(void)
{
	struct jblocks *jblocks;

	jblocks = malloc(sizeof(*jblocks), M_JBLOCKS, M_WAITOK | M_ZERO);
	TAILQ_INIT(&jblocks->jb_segs);
	jblocks->jb_avail = 10;
	jblocks->jb_extent = malloc(sizeof(struct jextent) * jblocks->jb_avail,
	    M_JBLOCKS, M_WAITOK | M_ZERO);

	return (jblocks);
}

static ufs2_daddr_t
jblocks_alloc(jblocks, bytes, actual)
	struct jblocks *jblocks;
	int bytes;
	int *actual;
{
	ufs2_daddr_t daddr;
	struct jextent *jext;
	int freecnt;
	int blocks;

	blocks = bytes / DEV_BSIZE;
	jext = &jblocks->jb_extent[jblocks->jb_head];
	freecnt = jext->je_blocks - jblocks->jb_off;
	if (freecnt == 0) {
		jblocks->jb_off = 0;
		if (++jblocks->jb_head > jblocks->jb_used)
			jblocks->jb_head = 0;
		jext = &jblocks->jb_extent[jblocks->jb_head];
		freecnt = jext->je_blocks;
	}
	if (freecnt > blocks)
		freecnt = blocks;
	*actual = freecnt * DEV_BSIZE;
	daddr = jext->je_daddr + jblocks->jb_off;
	jblocks->jb_off += freecnt;
	jblocks->jb_free -= freecnt;

	return (daddr);
}

static void
jblocks_free(jblocks, mp, bytes)
	struct jblocks *jblocks;
	struct mount *mp;
	int bytes;
{

	jblocks->jb_free += bytes / DEV_BSIZE;
	if (jblocks->jb_suspended)
		worklist_speedup();
	wakeup(jblocks);
}

static void
jblocks_destroy(jblocks)
	struct jblocks *jblocks;
{

	if (jblocks->jb_extent)
		free(jblocks->jb_extent, M_JBLOCKS);
	free(jblocks, M_JBLOCKS);
}

static void
jblocks_add(jblocks, daddr, blocks)
	struct jblocks *jblocks;
	ufs2_daddr_t daddr;
	int blocks;
{
	struct jextent *jext;

	jblocks->jb_blocks += blocks;
	jblocks->jb_free += blocks;
	jext = &jblocks->jb_extent[jblocks->jb_used];
	/* Adding the first block. */
	if (jext->je_daddr == 0) {
		jext->je_daddr = daddr;
		jext->je_blocks = blocks;
		return;
	}
	/* Extending the last extent. */
	if (jext->je_daddr + jext->je_blocks == daddr) {
		jext->je_blocks += blocks;
		return;
	}
	/* Adding a new extent. */
	if (++jblocks->jb_used == jblocks->jb_avail) {
		jblocks->jb_avail *= 2;
		jext = malloc(sizeof(struct jextent) * jblocks->jb_avail,
		    M_JBLOCKS, M_WAITOK | M_ZERO);
		memcpy(jext, jblocks->jb_extent,
		    sizeof(struct jextent) * jblocks->jb_used);
		free(jblocks->jb_extent, M_JBLOCKS);
		jblocks->jb_extent = jext;
	}
	jext = &jblocks->jb_extent[jblocks->jb_used];
	jext->je_daddr = daddr;
	jext->je_blocks = blocks;
	return;
}

int
softdep_journal_lookup(mp, vpp)
	struct mount *mp;
	struct vnode **vpp;
{
	struct componentname cnp;
	struct vnode *dvp;
	ino_t sujournal;
	int error;

	error = VFS_VGET(mp, ROOTINO, LK_EXCLUSIVE, &dvp);
	if (error)
		return (error);
	bzero(&cnp, sizeof(cnp));
	cnp.cn_nameiop = LOOKUP;
	cnp.cn_flags = ISLASTCN;
	cnp.cn_thread = curthread;
	cnp.cn_cred = curthread->td_ucred;
	cnp.cn_pnbuf = SUJ_FILE;
	cnp.cn_nameptr = SUJ_FILE;
	cnp.cn_namelen = strlen(SUJ_FILE);
	error = ufs_lookup_ino(dvp, NULL, &cnp, &sujournal);
	vput(dvp);
	if (error != 0)
		return (error);
	error = VFS_VGET(mp, sujournal, LK_EXCLUSIVE, vpp);
	return (error);
}

/*
 * Open and verify the journal file.
 */
static int
journal_mount(mp, fs, cred)
	struct mount *mp;
	struct fs *fs;
	struct ucred *cred;
{
	struct jblocks *jblocks;
	struct vnode *vp;
	struct inode *ip;
	ufs2_daddr_t blkno;
	int bcount;
	int error;
	int i;

	error = softdep_journal_lookup(mp, &vp);
	if (error != 0) {
		printf("Failed to find journal.  Use tunefs to create one\n");
		return (error);
	}
	ip = VTOI(vp);
	if (ip->i_size < SUJ_MIN) {
		error = ENOSPC;
		goto out;
	}
	bcount = lblkno(fs, ip->i_size);	/* Only use whole blocks. */
	jblocks = jblocks_create();
	for (i = 0; i < bcount; i++) {
		error = ufs_bmaparray(vp, i, &blkno, NULL, NULL, NULL);
		if (error)
			break;
		jblocks_add(jblocks, blkno, fsbtodb(fs, fs->fs_frag));
	}
	if (error) {
		jblocks_destroy(jblocks);
		goto out;
	}
	jblocks->jb_low = jblocks->jb_free / 3;	/* Reserve 33%. */
	jblocks->jb_min = jblocks->jb_free / 10; /* Suspend at 10%. */
	VFSTOUFS(mp)->softdep_jblocks = jblocks;
out:
	if (error == 0) {
		MNT_ILOCK(mp);
		mp->mnt_kern_flag |= MNTK_SUJ;
		MNT_IUNLOCK(mp);
		/*
		 * Only validate the journal contents if the
		 * filesystem is clean, otherwise we write the logs
		 * but they'll never be used.  If the filesystem was
		 * still dirty when we mounted it the journal is
		 * invalid and a new journal can only be valid if it
		 * starts from a clean mount.
		 */
		if (fs->fs_clean) {
			DIP_SET(ip, i_modrev, fs->fs_mtime);
			ip->i_flags |= IN_MODIFIED;
			ffs_update(vp, 1);
		}
	}
	vput(vp);
	return (error);
}

static void
journal_unmount(mp)
	struct mount *mp;
{
	struct ufsmount *ump;

	ump = VFSTOUFS(mp);
	if (ump->softdep_jblocks)
		jblocks_destroy(ump->softdep_jblocks);
	ump->softdep_jblocks = NULL;
}

/*
 * Called when a journal record is ready to be written.  Space is allocated
 * and the journal entry is created when the journal is flushed to stable
 * store.
 */
static void
add_to_journal(wk)
	struct worklist *wk;
{
	struct ufsmount *ump;

	mtx_assert(&lk, MA_OWNED);
	ump = VFSTOUFS(wk->wk_mp);
	if (wk->wk_state & ONWORKLIST)
		panic("add_to_journal: %s(0x%X) already on list",
		    TYPENAME(wk->wk_type), wk->wk_state);
	wk->wk_state |= ONWORKLIST | DEPCOMPLETE;
	if (LIST_EMPTY(&ump->softdep_journal_pending)) {
		ump->softdep_jblocks->jb_age = ticks;
		LIST_INSERT_HEAD(&ump->softdep_journal_pending, wk, wk_list);
	} else
		LIST_INSERT_AFTER(ump->softdep_journal_tail, wk, wk_list);
	ump->softdep_journal_tail = wk;
	ump->softdep_on_journal += 1;
}

/*
 * Remove an arbitrary item for the journal worklist maintain the tail
 * pointer.  This happens when a new operation obviates the need to
 * journal an old operation.
 */
static void
remove_from_journal(wk)
	struct worklist *wk;
{
	struct ufsmount *ump;

	mtx_assert(&lk, MA_OWNED);
	ump = VFSTOUFS(wk->wk_mp);
#ifdef SUJ_DEBUG
	{
		struct worklist *wkn;

		LIST_FOREACH(wkn, &ump->softdep_journal_pending, wk_list)
			if (wkn == wk)
				break;
		if (wkn == NULL)
			panic("remove_from_journal: %p is not in journal", wk);
	}
#endif
	/*
	 * We emulate a TAILQ to save space in most structures which do not
	 * require TAILQ semantics.  Here we must update the tail position
	 * when removing the tail which is not the final entry. This works
	 * only if the worklist linkage are at the beginning of the structure.
	 */
	if (ump->softdep_journal_tail == wk)
		ump->softdep_journal_tail =
		    (struct worklist *)wk->wk_list.le_prev;

	WORKLIST_REMOVE(wk);
	ump->softdep_on_journal -= 1;
}

/*
 * Check for journal space as well as dependency limits so the prelink
 * code can throttle both journaled and non-journaled filesystems.
 * Threshold is 0 for low and 1 for min.
 */
static int
journal_space(ump, thresh)
	struct ufsmount *ump;
	int thresh;
{
	struct jblocks *jblocks;
	int avail;

	jblocks = ump->softdep_jblocks;
	if (jblocks == NULL)
		return (1);
	/*
	 * We use a tighter restriction here to prevent request_cleanup()
	 * running in threads from running into locks we currently hold.
	 */
	if (num_inodedep > (max_softdeps / 10) * 9)
		return (0);
	if (thresh)
		thresh = jblocks->jb_min;
	else
		thresh = jblocks->jb_low;
	avail = (ump->softdep_on_journal * JREC_SIZE) / DEV_BSIZE;
	avail = jblocks->jb_free - avail;

	return (avail > thresh);
}

static void
journal_suspend(ump)
	struct ufsmount *ump;
{
	struct jblocks *jblocks;
	struct mount *mp;

	mp = UFSTOVFS(ump);
	jblocks = ump->softdep_jblocks;
	MNT_ILOCK(mp);
	if ((mp->mnt_kern_flag & MNTK_SUSPEND) == 0) {
		stat_journal_min++;
		mp->mnt_kern_flag |= MNTK_SUSPEND;
		mp->mnt_susp_owner = FIRST_THREAD_IN_PROC(softdepproc);
	}
	jblocks->jb_suspended = 1;
	MNT_IUNLOCK(mp);
}

static int
journal_unsuspend(struct ufsmount *ump)
{
	struct jblocks *jblocks;
	struct mount *mp;

	mp = UFSTOVFS(ump);
	jblocks = ump->softdep_jblocks;

	if (jblocks != NULL && jblocks->jb_suspended &&
	    journal_space(ump, jblocks->jb_min)) {
		jblocks->jb_suspended = 0;
		FREE_LOCK(&lk);
		mp->mnt_susp_owner = curthread;
		vfs_write_resume(mp);
		ACQUIRE_LOCK(&lk);
		return (1);
	}
	return (0);
}

/*
 * Called before any allocation function to be certain that there is
 * sufficient space in the journal prior to creating any new records.
 * Since in the case of block allocation we may have multiple locked
 * buffers at the time of the actual allocation we can not block
 * when the journal records are created.  Doing so would create a deadlock
 * if any of these buffers needed to be flushed to reclaim space.  Instead
 * we require a sufficiently large amount of available space such that
 * each thread in the system could have passed this allocation check and
 * still have sufficient free space.  With 20% of a minimum journal size
 * of 1MB we have 6553 records available.
 */
int
softdep_prealloc(vp, waitok)
	struct vnode *vp;
	int waitok;
{
	struct ufsmount *ump;

	if (DOINGSUJ(vp) == 0)
		return (0);
	ump = VFSTOUFS(vp->v_mount);
	ACQUIRE_LOCK(&lk);
	if (journal_space(ump, 0)) {
		FREE_LOCK(&lk);
		return (0);
	}
	stat_journal_low++;
	FREE_LOCK(&lk);
	if (waitok == MNT_NOWAIT)
		return (ENOSPC);
	/*
	 * Attempt to sync this vnode once to flush any journal
	 * work attached to it.
	 */
	if ((curthread->td_pflags & TDP_COWINPROGRESS) == 0)
		ffs_syncvnode(vp, waitok);
	ACQUIRE_LOCK(&lk);
	process_removes(vp);
	if (journal_space(ump, 0) == 0) {
		softdep_speedup();
		if (journal_space(ump, 1) == 0)
			journal_suspend(ump);
	}
	FREE_LOCK(&lk);

	return (0);
}

/*
 * Before adjusting a link count on a vnode verify that we have sufficient
 * journal space.  If not, process operations that depend on the currently
 * locked pair of vnodes to try to flush space as the syncer, buf daemon,
 * and softdep flush threads can not acquire these locks to reclaim space.
 */
static void
softdep_prelink(dvp, vp)
	struct vnode *dvp;
	struct vnode *vp;
{
	struct ufsmount *ump;

	ump = VFSTOUFS(dvp->v_mount);
	mtx_assert(&lk, MA_OWNED);
	if (journal_space(ump, 0))
		return;
	stat_journal_low++;
	FREE_LOCK(&lk);
	if (vp)
		ffs_syncvnode(vp, MNT_NOWAIT);
	ffs_syncvnode(dvp, MNT_WAIT);
	ACQUIRE_LOCK(&lk);
	/* Process vp before dvp as it may create .. removes. */
	if (vp)
		process_removes(vp);
	process_removes(dvp);
	softdep_speedup();
	process_worklist_item(UFSTOVFS(ump), LK_NOWAIT);
	process_worklist_item(UFSTOVFS(ump), LK_NOWAIT);
	if (journal_space(ump, 0) == 0) {
		softdep_speedup();
		if (journal_space(ump, 1) == 0)
			journal_suspend(ump);
	}
}

static void
jseg_write(fs, jblocks, jseg, data)
	struct fs *fs;
	struct jblocks *jblocks;
	struct jseg *jseg;
	uint8_t *data;
{
	struct jsegrec *rec;

	rec = (struct jsegrec *)data;
	rec->jsr_seq = jseg->js_seq;
	rec->jsr_oldest = jblocks->jb_oldestseq;
	rec->jsr_cnt = jseg->js_cnt;
	rec->jsr_blocks = jseg->js_size / DEV_BSIZE;
	rec->jsr_crc = 0;
	rec->jsr_time = fs->fs_mtime;
}

static inline void
inoref_write(inoref, jseg, rec)
	struct inoref *inoref;
	struct jseg *jseg;
	struct jrefrec *rec;
{

	inoref->if_jsegdep->jd_seg = jseg;
	rec->jr_ino = inoref->if_ino;
	rec->jr_parent = inoref->if_parent;
	rec->jr_nlink = inoref->if_nlink;
	rec->jr_mode = inoref->if_mode;
	rec->jr_diroff = inoref->if_diroff;
}

static void
jaddref_write(jaddref, jseg, data)
	struct jaddref *jaddref;
	struct jseg *jseg;
	uint8_t *data;
{
	struct jrefrec *rec;

	rec = (struct jrefrec *)data;
	rec->jr_op = JOP_ADDREF;
	inoref_write(&jaddref->ja_ref, jseg, rec);
}

static void
jremref_write(jremref, jseg, data)
	struct jremref *jremref;
	struct jseg *jseg;
	uint8_t *data;
{
	struct jrefrec *rec;

	rec = (struct jrefrec *)data;
	rec->jr_op = JOP_REMREF;
	inoref_write(&jremref->jr_ref, jseg, rec);
}

static void
jmvref_write(jmvref, jseg, data)
	struct jmvref *jmvref;
	struct jseg *jseg;
	uint8_t *data;
{
	struct jmvrec *rec;

	rec = (struct jmvrec *)data;
	rec->jm_op = JOP_MVREF;
	rec->jm_ino = jmvref->jm_ino;
	rec->jm_parent = jmvref->jm_parent;
	rec->jm_oldoff = jmvref->jm_oldoff;
	rec->jm_newoff = jmvref->jm_newoff;
}

static void
jnewblk_write(jnewblk, jseg, data)
	struct jnewblk *jnewblk;
	struct jseg *jseg;
	uint8_t *data;
{
	struct jblkrec *rec;

	jnewblk->jn_jsegdep->jd_seg = jseg;
	rec = (struct jblkrec *)data;
	rec->jb_op = JOP_NEWBLK;
	rec->jb_ino = jnewblk->jn_ino;
	rec->jb_blkno = jnewblk->jn_blkno;
	rec->jb_lbn = jnewblk->jn_lbn;
	rec->jb_frags = jnewblk->jn_frags;
	rec->jb_oldfrags = jnewblk->jn_oldfrags;
}

static void
jfreeblk_write(jfreeblk, jseg, data)
	struct jfreeblk *jfreeblk;
	struct jseg *jseg;
	uint8_t *data;
{
	struct jblkrec *rec;

	jfreeblk->jf_jsegdep->jd_seg = jseg;
	rec = (struct jblkrec *)data;
	rec->jb_op = JOP_FREEBLK;
	rec->jb_ino = jfreeblk->jf_ino;
	rec->jb_blkno = jfreeblk->jf_blkno;
	rec->jb_lbn = jfreeblk->jf_lbn;
	rec->jb_frags = jfreeblk->jf_frags;
	rec->jb_oldfrags = 0;
}

static void
jfreefrag_write(jfreefrag, jseg, data)
	struct jfreefrag *jfreefrag;
	struct jseg *jseg;
	uint8_t *data;
{
	struct jblkrec *rec;

	jfreefrag->fr_jsegdep->jd_seg = jseg;
	rec = (struct jblkrec *)data;
	rec->jb_op = JOP_FREEBLK;
	rec->jb_ino = jfreefrag->fr_ino;
	rec->jb_blkno = jfreefrag->fr_blkno;
	rec->jb_lbn = jfreefrag->fr_lbn;
	rec->jb_frags = jfreefrag->fr_frags;
	rec->jb_oldfrags = 0;
}

static void
jtrunc_write(jtrunc, jseg, data)
	struct jtrunc *jtrunc;
	struct jseg *jseg;
	uint8_t *data;
{
	struct jtrncrec *rec;

	rec = (struct jtrncrec *)data;
	rec->jt_op = JOP_TRUNC;
	rec->jt_ino = jtrunc->jt_ino;
	rec->jt_size = jtrunc->jt_size;
	rec->jt_extsize = jtrunc->jt_extsize;
}

/*
 * Flush some journal records to disk.
 */
static void
softdep_process_journal(mp, flags)
	struct mount *mp;
	int flags;
{
	struct jblocks *jblocks;
	struct ufsmount *ump;
	struct worklist *wk;
	struct jseg *jseg;
	struct buf *bp;
	uint8_t *data;
	struct fs *fs;
	int segwritten;
	int jrecmin;	/* Minimum records per block. */
	int jrecmax;	/* Maximum records per block. */
	int size;
	int cnt;
	int off;

	if ((mp->mnt_kern_flag & MNTK_SUJ) == 0)
		return;
	ump = VFSTOUFS(mp);
	fs = ump->um_fs;
	jblocks = ump->softdep_jblocks;
	/*
	 * We write anywhere between a disk block and fs block.  The upper
	 * bound is picked to prevent buffer cache fragmentation and limit
	 * processing time per I/O.
	 */
	jrecmin = (DEV_BSIZE / JREC_SIZE) - 1; /* -1 for seg header */
	jrecmax = (fs->fs_bsize / DEV_BSIZE) * jrecmin;
	segwritten = 0;
	while ((cnt = ump->softdep_on_journal) != 0) {
		/*
		 * Create a new segment to hold as many as 'cnt' journal
		 * entries and add them to the segment.  Notice cnt is
		 * off by one to account for the space required by the
		 * jsegrec.  If we don't have a full block to log skip it
		 * unless we haven't written anything.
		 */
		cnt++;
		if (cnt < jrecmax && segwritten)
			break;
		/*
		 * Verify some free journal space.  softdep_prealloc() should
	 	 * guarantee that we don't run out so this is indicative of
		 * a problem with the flow control.  Try to recover
		 * gracefully in any event.
		 */
		while (jblocks->jb_free == 0) {
			if (flags != MNT_WAIT)
				break;
			printf("softdep: Out of journal space!\n");
			softdep_speedup();
			msleep(jblocks, &lk, PRIBIO, "jblocks", hz);
		}
		FREE_LOCK(&lk);
		jseg = malloc(sizeof(*jseg), M_JSEG, M_SOFTDEP_FLAGS);
		workitem_alloc(&jseg->js_list, D_JSEG, mp);
		LIST_INIT(&jseg->js_entries);
		jseg->js_state = ATTACHED;
		jseg->js_jblocks = jblocks;
		bp = geteblk(fs->fs_bsize, 0);
		ACQUIRE_LOCK(&lk);
		/*
		 * If there was a race while we were allocating the block
		 * and jseg the entry we care about was likely written.
		 * We bail out in both the WAIT and NOWAIT case and assume
		 * the caller will loop if the entry it cares about is
		 * not written.
		 */
		if (ump->softdep_on_journal == 0 || jblocks->jb_free == 0) {
			bp->b_flags |= B_INVAL | B_NOCACHE;
			WORKITEM_FREE(jseg, D_JSEG);
			FREE_LOCK(&lk);
			brelse(bp);
			ACQUIRE_LOCK(&lk);
			break;
		}
		/*
		 * Calculate the disk block size required for the available
		 * records rounded to the min size.
		 */
		cnt = ump->softdep_on_journal;
		if (cnt < jrecmax)
			size = howmany(cnt, jrecmin) * DEV_BSIZE;
		else
			size = fs->fs_bsize;
		/*
		 * Allocate a disk block for this journal data and account
		 * for truncation of the requested size if enough contiguous
		 * space was not available.
		 */
		bp->b_blkno = jblocks_alloc(jblocks, size, &size);
		bp->b_lblkno = bp->b_blkno;
		bp->b_offset = bp->b_blkno * DEV_BSIZE;
		bp->b_bcount = size;
		bp->b_bufobj = &ump->um_devvp->v_bufobj;
		bp->b_flags &= ~B_INVAL;
		bp->b_flags |= B_VALIDSUSPWRT | B_NOCOPY;
		/*
		 * Initialize our jseg with cnt records.  Assign the next
		 * sequence number to it and link it in-order.
		 */
		cnt = MIN(ump->softdep_on_journal,
		    (size / DEV_BSIZE) * jrecmin);
		jseg->js_buf = bp;
		jseg->js_cnt = cnt;
		jseg->js_refs = cnt + 1;	/* Self ref. */
		jseg->js_size = size;
		jseg->js_seq = jblocks->jb_nextseq++;
		if (TAILQ_EMPTY(&jblocks->jb_segs))
			jblocks->jb_oldestseq = jseg->js_seq;
		TAILQ_INSERT_TAIL(&jblocks->jb_segs, jseg, js_next);
		if (jblocks->jb_writeseg == NULL)
			jblocks->jb_writeseg = jseg;
		/*
		 * Start filling in records from the pending list.
		 */
		data = bp->b_data;
		off = 0;
		while ((wk = LIST_FIRST(&ump->softdep_journal_pending))
		    != NULL) {
			/* Place a segment header on every device block. */
			if ((off % DEV_BSIZE) == 0) {
				jseg_write(fs, jblocks, jseg, data);
				off += JREC_SIZE;
				data = bp->b_data + off;
			}
			remove_from_journal(wk);
			wk->wk_state |= IOSTARTED;
			WORKLIST_INSERT(&jseg->js_entries, wk);
			switch (wk->wk_type) {
			case D_JADDREF:
				jaddref_write(WK_JADDREF(wk), jseg, data);
				break;
			case D_JREMREF:
				jremref_write(WK_JREMREF(wk), jseg, data);
				break;
			case D_JMVREF:
				jmvref_write(WK_JMVREF(wk), jseg, data);
				break;
			case D_JNEWBLK:
				jnewblk_write(WK_JNEWBLK(wk), jseg, data);
				break;
			case D_JFREEBLK:
				jfreeblk_write(WK_JFREEBLK(wk), jseg, data);
				break;
			case D_JFREEFRAG:
				jfreefrag_write(WK_JFREEFRAG(wk), jseg, data);
				break;
			case D_JTRUNC:
				jtrunc_write(WK_JTRUNC(wk), jseg, data);
				break;
			default:
				panic("process_journal: Unknown type %s",
				    TYPENAME(wk->wk_type));
				/* NOTREACHED */
			}
			if (--cnt == 0)
				break;
			off += JREC_SIZE;
			data = bp->b_data + off;
		}
		/*
		 * Write this one buffer and continue.
		 */
		WORKLIST_INSERT(&bp->b_dep, &jseg->js_list);
		FREE_LOCK(&lk);
		BO_LOCK(bp->b_bufobj);
		bgetvp(ump->um_devvp, bp);
		BO_UNLOCK(bp->b_bufobj);
		if (flags == MNT_NOWAIT)
			bawrite(bp);
		else
			bwrite(bp);
		ACQUIRE_LOCK(&lk);
	}
	/*
	 * If we've suspended the filesystem because we ran out of journal
	 * space either try to sync it here to make some progress or
	 * unsuspend it if we already have.
	 */
	if (flags == 0 && jblocks->jb_suspended) {
		if (journal_unsuspend(ump))
			return;
		FREE_LOCK(&lk);
		VFS_SYNC(mp, MNT_NOWAIT);
		ffs_sbupdate(ump, MNT_WAIT, 0);
		ACQUIRE_LOCK(&lk);
	}
}

/*
 * Complete a jseg, allowing all dependencies awaiting journal writes
 * to proceed.  Each journal dependency also attaches a jsegdep to dependent
 * structures so that the journal segment can be freed to reclaim space.
 */
static void
complete_jseg(jseg)
	struct jseg *jseg;
{
	struct worklist *wk;
	struct jmvref *jmvref;
	int waiting;
#ifdef INVARIANTS
	int i = 0;
#endif

	while ((wk = LIST_FIRST(&jseg->js_entries)) != NULL) {
		WORKLIST_REMOVE(wk);
		waiting = wk->wk_state & IOWAITING;
		wk->wk_state &= ~(IOSTARTED | IOWAITING);
		wk->wk_state |= COMPLETE;
		KASSERT(i++ < jseg->js_cnt,
		    ("handle_written_jseg: overflow %d >= %d",
		    i - 1, jseg->js_cnt));
		switch (wk->wk_type) {
		case D_JADDREF:
			handle_written_jaddref(WK_JADDREF(wk));
			break;
		case D_JREMREF:
			handle_written_jremref(WK_JREMREF(wk));
			break;
		case D_JMVREF:
			/* No jsegdep here. */
			free_jseg(jseg);
			jmvref = WK_JMVREF(wk);
			LIST_REMOVE(jmvref, jm_deps);
			free_pagedep(jmvref->jm_pagedep);
			WORKITEM_FREE(jmvref, D_JMVREF);
			break;
		case D_JNEWBLK:
			handle_written_jnewblk(WK_JNEWBLK(wk));
			break;
		case D_JFREEBLK:
			handle_written_jfreeblk(WK_JFREEBLK(wk));
			break;
		case D_JFREEFRAG:
			handle_written_jfreefrag(WK_JFREEFRAG(wk));
			break;
		case D_JTRUNC:
			WK_JTRUNC(wk)->jt_jsegdep->jd_seg = jseg;
			WORKITEM_FREE(wk, D_JTRUNC);
			break;
		default:
			panic("handle_written_jseg: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
		if (waiting)
			wakeup(wk);
	}
	/* Release the self reference so the structure may be freed. */
	free_jseg(jseg);
}

/*
 * Mark a jseg as DEPCOMPLETE and throw away the buffer.  Handle jseg
 * completions in order only.
 */
static void
handle_written_jseg(jseg, bp)
	struct jseg *jseg;
	struct buf *bp;
{
	struct jblocks *jblocks;
	struct jseg *jsegn;

	if (jseg->js_refs == 0)
		panic("handle_written_jseg: No self-reference on %p", jseg);
	jseg->js_state |= DEPCOMPLETE;
	/*
	 * We'll never need this buffer again, set flags so it will be
	 * discarded.
	 */
	bp->b_flags |= B_INVAL | B_NOCACHE;
	jblocks = jseg->js_jblocks;
	/*
	 * Don't allow out of order completions.  If this isn't the first
	 * block wait for it to write before we're done.
	 */
	if (jseg != jblocks->jb_writeseg)
		return;
	/* Iterate through available jsegs processing their entries. */
	do {
		jsegn = TAILQ_NEXT(jseg, js_next);
		complete_jseg(jseg);
		jseg = jsegn;
	} while (jseg && jseg->js_state & DEPCOMPLETE);
	jblocks->jb_writeseg = jseg;
}

static inline struct jsegdep *
inoref_jseg(inoref)
	struct inoref *inoref;
{
	struct jsegdep *jsegdep;

	jsegdep = inoref->if_jsegdep;
	inoref->if_jsegdep = NULL;

	return (jsegdep);
}

/*
 * Called once a jremref has made it to stable store.  The jremref is marked
 * complete and we attempt to free it.  Any pagedeps writes sleeping waiting
 * for the jremref to complete will be awoken by free_jremref.
 */
static void
handle_written_jremref(jremref)
	struct jremref *jremref;
{
	struct inodedep *inodedep;
	struct jsegdep *jsegdep;
	struct dirrem *dirrem;

	/* Grab the jsegdep. */
	jsegdep = inoref_jseg(&jremref->jr_ref);
	/*
	 * Remove us from the inoref list.
	 */
	if (inodedep_lookup(jremref->jr_list.wk_mp, jremref->jr_ref.if_ino,
	    0, &inodedep) == 0)
		panic("handle_written_jremref: Lost inodedep");
	TAILQ_REMOVE(&inodedep->id_inoreflst, &jremref->jr_ref, if_deps);
	/*
	 * Complete the dirrem.
	 */
	dirrem = jremref->jr_dirrem;
	jremref->jr_dirrem = NULL;
	LIST_REMOVE(jremref, jr_deps);
	jsegdep->jd_state |= jremref->jr_state & MKDIR_PARENT;
	WORKLIST_INSERT(&dirrem->dm_jwork, &jsegdep->jd_list);
	if (LIST_EMPTY(&dirrem->dm_jremrefhd) &&
	    (dirrem->dm_state & COMPLETE) != 0)
		add_to_worklist(&dirrem->dm_list, 0);
	free_jremref(jremref);
}

/*
 * Called once a jaddref has made it to stable store.  The dependency is
 * marked complete and any dependent structures are added to the inode
 * bufwait list to be completed as soon as it is written.  If a bitmap write
 * depends on this entry we move the inode into the inodedephd of the
 * bmsafemap dependency and attempt to remove the jaddref from the bmsafemap.
 */
static void
handle_written_jaddref(jaddref)
	struct jaddref *jaddref;
{
	struct jsegdep *jsegdep;
	struct inodedep *inodedep;
	struct diradd *diradd;
	struct mkdir *mkdir;

	/* Grab the jsegdep. */
	jsegdep = inoref_jseg(&jaddref->ja_ref);
	mkdir = NULL;
	diradd = NULL;
	if (inodedep_lookup(jaddref->ja_list.wk_mp, jaddref->ja_ino,
	    0, &inodedep) == 0)
		panic("handle_written_jaddref: Lost inodedep.");
	if (jaddref->ja_diradd == NULL)
		panic("handle_written_jaddref: No dependency");
	if (jaddref->ja_diradd->da_list.wk_type == D_DIRADD) {
		diradd = jaddref->ja_diradd;
		WORKLIST_INSERT(&inodedep->id_bufwait, &diradd->da_list);
	} else if (jaddref->ja_state & MKDIR_PARENT) {
		mkdir = jaddref->ja_mkdir;
		WORKLIST_INSERT(&inodedep->id_bufwait, &mkdir->md_list);
	} else if (jaddref->ja_state & MKDIR_BODY)
		mkdir = jaddref->ja_mkdir;
	else
		panic("handle_written_jaddref: Unknown dependency %p",
		    jaddref->ja_diradd);
	jaddref->ja_diradd = NULL;	/* also clears ja_mkdir */
	/*
	 * Remove us from the inode list.
	 */
	TAILQ_REMOVE(&inodedep->id_inoreflst, &jaddref->ja_ref, if_deps);
	/*
	 * The mkdir may be waiting on the jaddref to clear before freeing.
	 */
	if (mkdir) {
		KASSERT(mkdir->md_list.wk_type == D_MKDIR,
		    ("handle_written_jaddref: Incorrect type for mkdir %s",
		    TYPENAME(mkdir->md_list.wk_type)));
		mkdir->md_jaddref = NULL;
		diradd = mkdir->md_diradd;
		mkdir->md_state |= DEPCOMPLETE;
		complete_mkdir(mkdir);
	}
	WORKLIST_INSERT(&diradd->da_jwork, &jsegdep->jd_list);
	if (jaddref->ja_state & NEWBLOCK) {
		inodedep->id_state |= ONDEPLIST;
		LIST_INSERT_HEAD(&inodedep->id_bmsafemap->sm_inodedephd,
		    inodedep, id_deps);
	}
	free_jaddref(jaddref);
}

/*
 * Called once a jnewblk journal is written.  The allocdirect or allocindir
 * is placed in the bmsafemap to await notification of a written bitmap.
 */
static void
handle_written_jnewblk(jnewblk)
	struct jnewblk *jnewblk;
{
	struct bmsafemap *bmsafemap;
	struct jsegdep *jsegdep;
	struct newblk *newblk;

	/* Grab the jsegdep. */
	jsegdep = jnewblk->jn_jsegdep;
	jnewblk->jn_jsegdep = NULL;
	/*
	 * Add the written block to the bmsafemap so it can be notified when
	 * the bitmap is on disk.
	 */
	newblk = jnewblk->jn_newblk;
	jnewblk->jn_newblk = NULL;
	if (newblk == NULL) 
		panic("handle_written_jnewblk: No dependency for the segdep.");

	newblk->nb_jnewblk = NULL;
	bmsafemap = newblk->nb_bmsafemap;
	WORKLIST_INSERT(&newblk->nb_jwork, &jsegdep->jd_list);
	newblk->nb_state |= ONDEPLIST;
	LIST_INSERT_HEAD(&bmsafemap->sm_newblkhd, newblk, nb_deps);
	free_jnewblk(jnewblk);
}

/*
 * Cancel a jfreefrag that won't be needed, probably due to colliding with
 * an in-flight allocation that has not yet been committed.  Divorce us
 * from the freefrag and mark it DEPCOMPLETE so that it may be added
 * to the worklist.
 */
static void
cancel_jfreefrag(jfreefrag)
	struct jfreefrag *jfreefrag;
{
	struct freefrag *freefrag;

	if (jfreefrag->fr_jsegdep) {
		free_jsegdep(jfreefrag->fr_jsegdep);
		jfreefrag->fr_jsegdep = NULL;
	}
	freefrag = jfreefrag->fr_freefrag;
	jfreefrag->fr_freefrag = NULL;
	freefrag->ff_jfreefrag = NULL;
	free_jfreefrag(jfreefrag);
	freefrag->ff_state |= DEPCOMPLETE;
}

/*
 * Free a jfreefrag when the parent freefrag is rendered obsolete.
 */
static void
free_jfreefrag(jfreefrag)
	struct jfreefrag *jfreefrag;
{

	if (jfreefrag->fr_state & IOSTARTED)
		WORKLIST_REMOVE(&jfreefrag->fr_list);
	else if (jfreefrag->fr_state & ONWORKLIST)
		remove_from_journal(&jfreefrag->fr_list);
	if (jfreefrag->fr_freefrag != NULL)
		panic("free_jfreefrag:  Still attached to a freefrag.");
	WORKITEM_FREE(jfreefrag, D_JFREEFRAG);
}

/*
 * Called when the journal write for a jfreefrag completes.  The parent
 * freefrag is added to the worklist if this completes its dependencies.
 */
static void
handle_written_jfreefrag(jfreefrag)
	struct jfreefrag *jfreefrag;
{
	struct jsegdep *jsegdep;
	struct freefrag *freefrag;

	/* Grab the jsegdep. */
	jsegdep = jfreefrag->fr_jsegdep;
	jfreefrag->fr_jsegdep = NULL;
	freefrag = jfreefrag->fr_freefrag;
	if (freefrag == NULL)
		panic("handle_written_jfreefrag: No freefrag.");
	freefrag->ff_state |= DEPCOMPLETE;
	freefrag->ff_jfreefrag = NULL;
	WORKLIST_INSERT(&freefrag->ff_jwork, &jsegdep->jd_list);
	if ((freefrag->ff_state & ALLCOMPLETE) == ALLCOMPLETE)
		add_to_worklist(&freefrag->ff_list, 0);
	jfreefrag->fr_freefrag = NULL;
	free_jfreefrag(jfreefrag);
}

/*
 * Called when the journal write for a jfreeblk completes.  The jfreeblk
 * is removed from the freeblks list of pending journal writes and the
 * jsegdep is moved to the freeblks jwork to be completed when all blocks
 * have been reclaimed.
 */
static void
handle_written_jfreeblk(jfreeblk)
	struct jfreeblk *jfreeblk;
{
	struct freeblks *freeblks;
	struct jsegdep *jsegdep;

	/* Grab the jsegdep. */
	jsegdep = jfreeblk->jf_jsegdep;
	jfreeblk->jf_jsegdep = NULL;
	freeblks = jfreeblk->jf_freeblks;
	LIST_REMOVE(jfreeblk, jf_deps);
	WORKLIST_INSERT(&freeblks->fb_jwork, &jsegdep->jd_list);
	/*
	 * If the freeblks is all journaled, we can add it to the worklist.
	 */
	if (LIST_EMPTY(&freeblks->fb_jfreeblkhd) &&
	    (freeblks->fb_state & ALLCOMPLETE) == ALLCOMPLETE) {
		/* Remove from the b_dep that is waiting on this write. */
		if (freeblks->fb_state & ONWORKLIST)
			WORKLIST_REMOVE(&freeblks->fb_list);
		add_to_worklist(&freeblks->fb_list, 1);
	}

	free_jfreeblk(jfreeblk);
}

static struct jsegdep *
newjsegdep(struct worklist *wk)
{
	struct jsegdep *jsegdep;

	jsegdep = malloc(sizeof(*jsegdep), M_JSEGDEP, M_SOFTDEP_FLAGS);
	workitem_alloc(&jsegdep->jd_list, D_JSEGDEP, wk->wk_mp);
	jsegdep->jd_seg = NULL;

	return (jsegdep);
}

static struct jmvref *
newjmvref(dp, ino, oldoff, newoff)
	struct inode *dp;
	ino_t ino;
	off_t oldoff;
	off_t newoff;
{
	struct jmvref *jmvref;

	jmvref = malloc(sizeof(*jmvref), M_JMVREF, M_SOFTDEP_FLAGS);
	workitem_alloc(&jmvref->jm_list, D_JMVREF, UFSTOVFS(dp->i_ump));
	jmvref->jm_list.wk_state = ATTACHED | DEPCOMPLETE;
	jmvref->jm_parent = dp->i_number;
	jmvref->jm_ino = ino;
	jmvref->jm_oldoff = oldoff;
	jmvref->jm_newoff = newoff;

	return (jmvref);
}

/*
 * Allocate a new jremref that tracks the removal of ip from dp with the
 * directory entry offset of diroff.  Mark the entry as ATTACHED and
 * DEPCOMPLETE as we have all the information required for the journal write
 * and the directory has already been removed from the buffer.  The caller
 * is responsible for linking the jremref into the pagedep and adding it
 * to the journal to write.  The MKDIR_PARENT flag is set if we're doing
 * a DOTDOT addition so handle_workitem_remove() can properly assign
 * the jsegdep when we're done.
 */
static struct jremref *
newjremref(struct dirrem *dirrem, struct inode *dp, struct inode *ip,
    off_t diroff, nlink_t nlink)
{
	struct jremref *jremref;

	jremref = malloc(sizeof(*jremref), M_JREMREF, M_SOFTDEP_FLAGS);
	workitem_alloc(&jremref->jr_list, D_JREMREF, UFSTOVFS(dp->i_ump));
	jremref->jr_state = ATTACHED;
	newinoref(&jremref->jr_ref, ip->i_number, dp->i_number, diroff,
	   nlink, ip->i_mode);
	jremref->jr_dirrem = dirrem;

	return (jremref);
}

static inline void
newinoref(struct inoref *inoref, ino_t ino, ino_t parent, off_t diroff,
    nlink_t nlink, uint16_t mode)
{

	inoref->if_jsegdep = newjsegdep(&inoref->if_list);
	inoref->if_diroff = diroff;
	inoref->if_ino = ino;
	inoref->if_parent = parent;
	inoref->if_nlink = nlink;
	inoref->if_mode = mode;
}

/*
 * Allocate a new jaddref to track the addition of ino to dp at diroff.  The
 * directory offset may not be known until later.  The caller is responsible
 * adding the entry to the journal when this information is available.  nlink
 * should be the link count prior to the addition and mode is only required
 * to have the correct FMT.
 */
static struct jaddref *
newjaddref(struct inode *dp, ino_t ino, off_t diroff, int16_t nlink,
    uint16_t mode)
{
	struct jaddref *jaddref;

	jaddref = malloc(sizeof(*jaddref), M_JADDREF, M_SOFTDEP_FLAGS);
	workitem_alloc(&jaddref->ja_list, D_JADDREF, UFSTOVFS(dp->i_ump));
	jaddref->ja_state = ATTACHED;
	jaddref->ja_mkdir = NULL;
	newinoref(&jaddref->ja_ref, ino, dp->i_number, diroff, nlink, mode);

	return (jaddref);
}

/*
 * Create a new free dependency for a freework.  The caller is responsible
 * for adjusting the reference count when it has the lock held.  The freedep
 * will track an outstanding bitmap write that will ultimately clear the
 * freework to continue.
 */
static struct freedep *
newfreedep(struct freework *freework)
{
	struct freedep *freedep;

	freedep = malloc(sizeof(*freedep), M_FREEDEP, M_SOFTDEP_FLAGS);
	workitem_alloc(&freedep->fd_list, D_FREEDEP, freework->fw_list.wk_mp);
	freedep->fd_freework = freework;

	return (freedep);
}

/*
 * Free a freedep structure once the buffer it is linked to is written.  If
 * this is the last reference to the freework schedule it for completion.
 */
static void
free_freedep(freedep)
	struct freedep *freedep;
{

	if (--freedep->fd_freework->fw_ref == 0)
		add_to_worklist(&freedep->fd_freework->fw_list, 1);
	WORKITEM_FREE(freedep, D_FREEDEP);
}

/*
 * Allocate a new freework structure that may be a level in an indirect
 * when parent is not NULL or a top level block when it is.  The top level
 * freework structures are allocated without lk held and before the freeblks
 * is visible outside of softdep_setup_freeblocks().
 */
static struct freework *
newfreework(ump, freeblks, parent, lbn, nb, frags, journal)
	struct ufsmount *ump;
	struct freeblks *freeblks;
	struct freework *parent;
	ufs_lbn_t lbn;
	ufs2_daddr_t nb;
	int frags;
	int journal;
{
	struct freework *freework;

	freework = malloc(sizeof(*freework), M_FREEWORK, M_SOFTDEP_FLAGS);
	workitem_alloc(&freework->fw_list, D_FREEWORK, freeblks->fb_list.wk_mp);
	freework->fw_freeblks = freeblks;
	freework->fw_parent = parent;
	freework->fw_lbn = lbn;
	freework->fw_blkno = nb;
	freework->fw_frags = frags;
	freework->fw_ref = ((UFSTOVFS(ump)->mnt_kern_flag & MNTK_SUJ) == 0 ||
	    lbn >= -NXADDR) ? 0 : NINDIR(ump->um_fs) + 1;
	freework->fw_off = 0;
	LIST_INIT(&freework->fw_jwork);

	if (parent == NULL) {
		WORKLIST_INSERT_UNLOCKED(&freeblks->fb_freeworkhd,
		    &freework->fw_list);
		freeblks->fb_ref++;
	}
	if (journal)
		newjfreeblk(freeblks, lbn, nb, frags);

	return (freework);
}

/*
 * Allocate a new jfreeblk to journal top level block pointer when truncating
 * a file.  The caller must add this to the worklist when lk is held.
 */
static struct jfreeblk *
newjfreeblk(freeblks, lbn, blkno, frags)
	struct freeblks *freeblks;
	ufs_lbn_t lbn;
	ufs2_daddr_t blkno;
	int frags;
{
	struct jfreeblk *jfreeblk;

	jfreeblk = malloc(sizeof(*jfreeblk), M_JFREEBLK, M_SOFTDEP_FLAGS);
	workitem_alloc(&jfreeblk->jf_list, D_JFREEBLK, freeblks->fb_list.wk_mp);
	jfreeblk->jf_jsegdep = newjsegdep(&jfreeblk->jf_list);
	jfreeblk->jf_state = ATTACHED | DEPCOMPLETE;
	jfreeblk->jf_ino = freeblks->fb_previousinum;
	jfreeblk->jf_lbn = lbn;
	jfreeblk->jf_blkno = blkno;
	jfreeblk->jf_frags = frags;
	jfreeblk->jf_freeblks = freeblks;
	LIST_INSERT_HEAD(&freeblks->fb_jfreeblkhd, jfreeblk, jf_deps);

	return (jfreeblk);
}

static void move_newblock_dep(struct jaddref *, struct inodedep *);
/*
 * If we're canceling a new bitmap we have to search for another ref
 * to move into the bmsafemap dep.  This might be better expressed
 * with another structure.
 */
static void
move_newblock_dep(jaddref, inodedep)
	struct jaddref *jaddref;
	struct inodedep *inodedep;
{
	struct inoref *inoref;
	struct jaddref *jaddrefn;

	jaddrefn = NULL;
	for (inoref = TAILQ_NEXT(&jaddref->ja_ref, if_deps); inoref;
	    inoref = TAILQ_NEXT(inoref, if_deps)) {
		if ((jaddref->ja_state & NEWBLOCK) &&
		    inoref->if_list.wk_type == D_JADDREF) {
			jaddrefn = (struct jaddref *)inoref;
			break;
		}
	}
	if (jaddrefn == NULL)
		return;
	jaddrefn->ja_state &= ~(ATTACHED | UNDONE);
	jaddrefn->ja_state |= jaddref->ja_state &
	    (ATTACHED | UNDONE | NEWBLOCK);
	jaddref->ja_state &= ~(ATTACHED | UNDONE | NEWBLOCK);
	jaddref->ja_state |= ATTACHED;
	LIST_REMOVE(jaddref, ja_bmdeps);
	LIST_INSERT_HEAD(&inodedep->id_bmsafemap->sm_jaddrefhd, jaddrefn,
	    ja_bmdeps);
}

/*
 * Cancel a jaddref either before it has been written or while it is being
 * written.  This happens when a link is removed before the add reaches
 * the disk.  The jaddref dependency is kept linked into the bmsafemap
 * and inode to prevent the link count or bitmap from reaching the disk
 * until handle_workitem_remove() re-adjusts the counts and bitmaps as
 * required.
 *
 * Returns 1 if the canceled addref requires journaling of the remove and
 * 0 otherwise.
 */
static int
cancel_jaddref(jaddref, inodedep, wkhd)
	struct jaddref *jaddref;
	struct inodedep *inodedep;
	struct workhead *wkhd;
{
	struct inoref *inoref;
	struct jsegdep *jsegdep;
	int needsj;

	KASSERT((jaddref->ja_state & COMPLETE) == 0,
	    ("cancel_jaddref: Canceling complete jaddref"));
	if (jaddref->ja_state & (IOSTARTED | COMPLETE))
		needsj = 1;
	else
		needsj = 0;
	if (inodedep == NULL)
		if (inodedep_lookup(jaddref->ja_list.wk_mp, jaddref->ja_ino,
		    0, &inodedep) == 0)
			panic("cancel_jaddref: Lost inodedep");
	/*
	 * We must adjust the nlink of any reference operation that follows
	 * us so that it is consistent with the in-memory reference.  This
	 * ensures that inode nlink rollbacks always have the correct link.
	 */
	if (needsj == 0)
		for (inoref = TAILQ_NEXT(&jaddref->ja_ref, if_deps); inoref;
		    inoref = TAILQ_NEXT(inoref, if_deps))
			inoref->if_nlink--;
	jsegdep = inoref_jseg(&jaddref->ja_ref);
	if (jaddref->ja_state & NEWBLOCK)
		move_newblock_dep(jaddref, inodedep);
	if (jaddref->ja_state & IOWAITING) {
		jaddref->ja_state &= ~IOWAITING;
		wakeup(&jaddref->ja_list);
	}
	jaddref->ja_mkdir = NULL;
	if (jaddref->ja_state & IOSTARTED) {
		jaddref->ja_state &= ~IOSTARTED;
		WORKLIST_REMOVE(&jaddref->ja_list);
		WORKLIST_INSERT(wkhd, &jsegdep->jd_list);
	} else {
		free_jsegdep(jsegdep);
		if (jaddref->ja_state & DEPCOMPLETE)
			remove_from_journal(&jaddref->ja_list);
	}
	/*
	 * Leave NEWBLOCK jaddrefs on the inodedep so handle_workitem_remove
	 * can arrange for them to be freed with the bitmap.  Otherwise we
	 * no longer need this addref attached to the inoreflst and it
	 * will incorrectly adjust nlink if we leave it.
	 */
	if ((jaddref->ja_state & NEWBLOCK) == 0) {
		TAILQ_REMOVE(&inodedep->id_inoreflst, &jaddref->ja_ref,
		    if_deps);
		jaddref->ja_state |= COMPLETE;
		free_jaddref(jaddref);
		return (needsj);
	}
	jaddref->ja_state |= GOINGAWAY;
	/*
	 * Leave the head of the list for jsegdeps for fast merging.
	 */
	if (LIST_FIRST(wkhd) != NULL) {
		jaddref->ja_state |= ONWORKLIST;
		LIST_INSERT_AFTER(LIST_FIRST(wkhd), &jaddref->ja_list, wk_list);
	} else
		WORKLIST_INSERT(wkhd, &jaddref->ja_list);

	return (needsj);
}

/* 
 * Attempt to free a jaddref structure when some work completes.  This
 * should only succeed once the entry is written and all dependencies have
 * been notified.
 */
static void
free_jaddref(jaddref)
	struct jaddref *jaddref;
{

	if ((jaddref->ja_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	if (jaddref->ja_ref.if_jsegdep)
		panic("free_jaddref: segdep attached to jaddref %p(0x%X)\n",
		    jaddref, jaddref->ja_state);
	if (jaddref->ja_state & NEWBLOCK)
		LIST_REMOVE(jaddref, ja_bmdeps);
	if (jaddref->ja_state & (IOSTARTED | ONWORKLIST))
		panic("free_jaddref: Bad state %p(0x%X)",
		    jaddref, jaddref->ja_state);
	if (jaddref->ja_mkdir != NULL)
		panic("free_jaddref: Work pending, 0x%X\n", jaddref->ja_state);
	WORKITEM_FREE(jaddref, D_JADDREF);
}

/*
 * Free a jremref structure once it has been written or discarded.
 */
static void
free_jremref(jremref)
	struct jremref *jremref;
{

	if (jremref->jr_ref.if_jsegdep)
		free_jsegdep(jremref->jr_ref.if_jsegdep);
	if (jremref->jr_state & IOSTARTED)
		panic("free_jremref: IO still pending");
	WORKITEM_FREE(jremref, D_JREMREF);
}

/*
 * Free a jnewblk structure.
 */
static void
free_jnewblk(jnewblk)
	struct jnewblk *jnewblk;
{

	if ((jnewblk->jn_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	LIST_REMOVE(jnewblk, jn_deps);
	if (jnewblk->jn_newblk != NULL)
		panic("free_jnewblk: Dependency still attached.");
	WORKITEM_FREE(jnewblk, D_JNEWBLK);
}

/*
 * Cancel a jnewblk which has been superseded by a freeblk.  The jnewblk
 * is kept linked into the bmsafemap until the free completes, thus
 * preventing the modified state from ever reaching disk.  The free
 * routine must pass this structure via ffs_blkfree() to
 * softdep_setup_freeblks() so there is no race in releasing the space.
 */
static void
cancel_jnewblk(jnewblk, wkhd)
	struct jnewblk *jnewblk;
	struct workhead *wkhd;
{
	struct jsegdep *jsegdep;

	jsegdep = jnewblk->jn_jsegdep;
	jnewblk->jn_jsegdep  = NULL;
	free_jsegdep(jsegdep);
	jnewblk->jn_newblk = NULL;
	jnewblk->jn_state |= GOINGAWAY;
	if (jnewblk->jn_state & IOSTARTED) {
		jnewblk->jn_state &= ~IOSTARTED;
		WORKLIST_REMOVE(&jnewblk->jn_list);
	} else
		remove_from_journal(&jnewblk->jn_list);
	/*
	 * Leave the head of the list for jsegdeps for fast merging.
	 */
	if (LIST_FIRST(wkhd) != NULL) {
		jnewblk->jn_state |= ONWORKLIST;
		LIST_INSERT_AFTER(LIST_FIRST(wkhd), &jnewblk->jn_list, wk_list);
	} else
		WORKLIST_INSERT(wkhd, &jnewblk->jn_list);
	if (jnewblk->jn_state & IOWAITING) {
		jnewblk->jn_state &= ~IOWAITING;
		wakeup(&jnewblk->jn_list);
	}
}

static void
free_jfreeblk(jfreeblk)
	struct jfreeblk *jfreeblk;
{

	WORKITEM_FREE(jfreeblk, D_JFREEBLK);
}

/*
 * Release one reference to a jseg and free it if the count reaches 0.  This
 * should eventually reclaim journal space as well.
 */
static void
free_jseg(jseg)
	struct jseg *jseg;
{
	struct jblocks *jblocks;

	KASSERT(jseg->js_refs > 0,
	    ("free_jseg: Invalid refcnt %d", jseg->js_refs));
	if (--jseg->js_refs != 0)
		return;
	/*
	 * Free only those jsegs which have none allocated before them to
	 * preserve the journal space ordering.
	 */
	jblocks = jseg->js_jblocks;
	while ((jseg = TAILQ_FIRST(&jblocks->jb_segs)) != NULL) {
		jblocks->jb_oldestseq = jseg->js_seq;
		if (jseg->js_refs != 0)
			break;
		TAILQ_REMOVE(&jblocks->jb_segs, jseg, js_next);
		jblocks_free(jblocks, jseg->js_list.wk_mp, jseg->js_size);
		KASSERT(LIST_EMPTY(&jseg->js_entries),
		    ("free_jseg: Freed jseg has valid entries."));
		WORKITEM_FREE(jseg, D_JSEG);
	}
}

/*
 * Release a jsegdep and decrement the jseg count.
 */
static void
free_jsegdep(jsegdep)
	struct jsegdep *jsegdep;
{

	if (jsegdep->jd_seg)
		free_jseg(jsegdep->jd_seg);
	WORKITEM_FREE(jsegdep, D_JSEGDEP);
}

/*
 * Wait for a journal item to make it to disk.  Initiate journal processing
 * if required.
 */
static void
jwait(wk)
	struct worklist *wk;
{

	stat_journal_wait++;
	/*
	 * If IO has not started we process the journal.  We can't mark the
	 * worklist item as IOWAITING because we drop the lock while
	 * processing the journal and the worklist entry may be freed after
	 * this point.  The caller may call back in and re-issue the request.
	 */
	if ((wk->wk_state & IOSTARTED) == 0) {
		softdep_process_journal(wk->wk_mp, MNT_WAIT);
		return;
	}
	wk->wk_state |= IOWAITING;
	msleep(wk, &lk, PRIBIO, "jwait", 0);
}

/*
 * Lookup an inodedep based on an inode pointer and set the nlinkdelta as
 * appropriate.  This is a convenience function to reduce duplicate code
 * for the setup and revert functions below.
 */
static struct inodedep *
inodedep_lookup_ip(ip)
	struct inode *ip;
{
	struct inodedep *inodedep;

	KASSERT(ip->i_nlink >= ip->i_effnlink,
	    ("inodedep_lookup_ip: bad delta"));
	(void) inodedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number,
	    DEPALLOC, &inodedep);
	inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;

	return (inodedep);
}

/*
 * Create a journal entry that describes a truncate that we're about to
 * perform.  The inode allocations and frees between here and the completion
 * of the operation are done asynchronously and without journaling.  At
 * the end of the operation the vnode is sync'd and the journal space
 * is released.  Recovery will discover the partially completed truncate
 * and complete it.
 */
void *
softdep_setup_trunc(vp, length, flags)
	struct vnode *vp;
	off_t length;
	int flags;
{
	struct jsegdep *jsegdep;
	struct jtrunc *jtrunc;
	struct ufsmount *ump;
	struct inode *ip;

	softdep_prealloc(vp, MNT_WAIT);
	ip = VTOI(vp);
	ump = VFSTOUFS(vp->v_mount);
	jtrunc = malloc(sizeof(*jtrunc), M_JTRUNC, M_SOFTDEP_FLAGS);
	workitem_alloc(&jtrunc->jt_list, D_JTRUNC, vp->v_mount);
	jsegdep = jtrunc->jt_jsegdep = newjsegdep(&jtrunc->jt_list);
	jtrunc->jt_ino = ip->i_number;
	jtrunc->jt_extsize = 0;
	jtrunc->jt_size = length;
	if ((flags & IO_EXT) == 0 && ump->um_fstype == UFS2)
		jtrunc->jt_extsize = ip->i_din2->di_extsize;
	if ((flags & IO_NORMAL) == 0)
		jtrunc->jt_size = DIP(ip, i_size);
	ACQUIRE_LOCK(&lk);
	add_to_journal(&jtrunc->jt_list);
	while (jsegdep->jd_seg == NULL) {
		stat_jwait_freeblks++;
		jwait(&jtrunc->jt_list);
	}
	FREE_LOCK(&lk);

	return (jsegdep);
}

/*
 * After synchronous truncation is complete we free sync the vnode and
 * release the jsegdep so the journal space can be freed.
 */
int
softdep_complete_trunc(vp, cookie)
	struct vnode *vp;
	void *cookie;
{
	int error;
	
	error = ffs_syncvnode(vp, MNT_WAIT);
	ACQUIRE_LOCK(&lk);
	free_jsegdep((struct jsegdep *)cookie);
	FREE_LOCK(&lk);

	return (error);
}

/*
 * Called prior to creating a new inode and linking it to a directory.  The
 * jaddref structure must already be allocated by softdep_setup_inomapdep
 * and it is discovered here so we can initialize the mode and update
 * nlinkdelta.
 */
void
softdep_setup_create(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct vnode *dvp;

	KASSERT(ip->i_nlink == 1,
	    ("softdep_setup_create: Invalid link count."));
	dvp = ITOV(dp);
	ACQUIRE_LOCK(&lk);
	inodedep = inodedep_lookup_ip(ip);
	if (DOINGSUJ(dvp)) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref != NULL && jaddref->ja_parent == dp->i_number,
		    ("softdep_setup_create: No addref structure present."));
		jaddref->ja_mode = ip->i_mode;
	}
	softdep_prelink(dvp, NULL);
	FREE_LOCK(&lk);
}

/*
 * Create a jaddref structure to track the addition of a DOTDOT link when
 * we are reparenting an inode as part of a rename.  This jaddref will be
 * found by softdep_setup_directory_change.  Adjusts nlinkdelta for
 * non-journaling softdep.
 */
void
softdep_setup_dotdot_link(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct vnode *dvp;
	struct vnode *vp;

	dvp = ITOV(dp);
	vp = ITOV(ip);
	jaddref = NULL;
	/*
	 * We don't set MKDIR_PARENT as this is not tied to a mkdir and
	 * is used as a normal link would be.
	 */
	if (DOINGSUJ(dvp))
		jaddref = newjaddref(ip, dp->i_number, DOTDOT_OFFSET,
		    dp->i_effnlink - 1, dp->i_mode);
	ACQUIRE_LOCK(&lk);
	inodedep = inodedep_lookup_ip(dp);
	if (jaddref)
		TAILQ_INSERT_TAIL(&inodedep->id_inoreflst, &jaddref->ja_ref,
		    if_deps);
	softdep_prelink(dvp, ITOV(ip));
	FREE_LOCK(&lk);
}

/*
 * Create a jaddref structure to track a new link to an inode.  The directory
 * offset is not known until softdep_setup_directory_add or
 * softdep_setup_directory_change.  Adjusts nlinkdelta for non-journaling
 * softdep.
 */
void
softdep_setup_link(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct vnode *dvp;

	dvp = ITOV(dp);
	jaddref = NULL;
	if (DOINGSUJ(dvp))
		jaddref = newjaddref(dp, ip->i_number, 0, ip->i_effnlink - 1,
		    ip->i_mode);
	ACQUIRE_LOCK(&lk);
	inodedep = inodedep_lookup_ip(ip);
	if (jaddref)
		TAILQ_INSERT_TAIL(&inodedep->id_inoreflst, &jaddref->ja_ref,
		    if_deps);
	softdep_prelink(dvp, ITOV(ip));
	FREE_LOCK(&lk);
}

/*
 * Called to create the jaddref structures to track . and .. references as
 * well as lookup and further initialize the incomplete jaddref created
 * by softdep_setup_inomapdep when the inode was allocated.  Adjusts
 * nlinkdelta for non-journaling softdep.
 */
void
softdep_setup_mkdir(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct inodedep *inodedep;
	struct jaddref *dotdotaddref;
	struct jaddref *dotaddref;
	struct jaddref *jaddref;
	struct vnode *dvp;

	dvp = ITOV(dp);
	dotaddref = dotdotaddref = NULL;
	if (DOINGSUJ(dvp)) {
		dotaddref = newjaddref(ip, ip->i_number, DOT_OFFSET, 1,
		    ip->i_mode);
		dotaddref->ja_state |= MKDIR_BODY;
		dotdotaddref = newjaddref(ip, dp->i_number, DOTDOT_OFFSET,
		    dp->i_effnlink - 1, dp->i_mode);
		dotdotaddref->ja_state |= MKDIR_PARENT;
	}
	ACQUIRE_LOCK(&lk);
	inodedep = inodedep_lookup_ip(ip);
	if (DOINGSUJ(dvp)) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref != NULL,
		    ("softdep_setup_mkdir: No addref structure present."));
		KASSERT(jaddref->ja_parent == dp->i_number, 
		    ("softdep_setup_mkdir: bad parent %d",
		    jaddref->ja_parent));
		jaddref->ja_mode = ip->i_mode;
		TAILQ_INSERT_BEFORE(&jaddref->ja_ref, &dotaddref->ja_ref,
		    if_deps);
	}
	inodedep = inodedep_lookup_ip(dp);
	if (DOINGSUJ(dvp))
		TAILQ_INSERT_TAIL(&inodedep->id_inoreflst,
		    &dotdotaddref->ja_ref, if_deps);
	softdep_prelink(ITOV(dp), NULL);
	FREE_LOCK(&lk);
}

/*
 * Called to track nlinkdelta of the inode and parent directories prior to
 * unlinking a directory.
 */
void
softdep_setup_rmdir(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct vnode *dvp;

	dvp = ITOV(dp);
	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup_ip(ip);
	(void) inodedep_lookup_ip(dp);
	softdep_prelink(dvp, ITOV(ip));
	FREE_LOCK(&lk);
}

/*
 * Called to track nlinkdelta of the inode and parent directories prior to
 * unlink.
 */
void
softdep_setup_unlink(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct vnode *dvp;

	dvp = ITOV(dp);
	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup_ip(ip);
	(void) inodedep_lookup_ip(dp);
	softdep_prelink(dvp, ITOV(ip));
	FREE_LOCK(&lk);
}

/*
 * Called to release the journal structures created by a failed non-directory
 * creation.  Adjusts nlinkdelta for non-journaling softdep.
 */
void
softdep_revert_create(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct vnode *dvp;

	dvp = ITOV(dp);
	ACQUIRE_LOCK(&lk);
	inodedep = inodedep_lookup_ip(ip);
	if (DOINGSUJ(dvp)) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref->ja_parent == dp->i_number,
		    ("softdep_revert_create: addref parent mismatch"));
		cancel_jaddref(jaddref, inodedep, &inodedep->id_inowait);
	}
	FREE_LOCK(&lk);
}

/*
 * Called to release the journal structures created by a failed dotdot link
 * creation.  Adjusts nlinkdelta for non-journaling softdep.
 */
void
softdep_revert_dotdot_link(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct vnode *dvp;

	dvp = ITOV(dp);
	ACQUIRE_LOCK(&lk);
	inodedep = inodedep_lookup_ip(dp);
	if (DOINGSUJ(dvp)) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref->ja_parent == ip->i_number,
		    ("softdep_revert_dotdot_link: addref parent mismatch"));
		cancel_jaddref(jaddref, inodedep, &inodedep->id_inowait);
	}
	FREE_LOCK(&lk);
}

/*
 * Called to release the journal structures created by a failed link
 * addition.  Adjusts nlinkdelta for non-journaling softdep.
 */
void
softdep_revert_link(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct vnode *dvp;

	dvp = ITOV(dp);
	ACQUIRE_LOCK(&lk);
	inodedep = inodedep_lookup_ip(ip);
	if (DOINGSUJ(dvp)) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref->ja_parent == dp->i_number,
		    ("softdep_revert_link: addref parent mismatch"));
		cancel_jaddref(jaddref, inodedep, &inodedep->id_inowait);
	}
	FREE_LOCK(&lk);
}

/*
 * Called to release the journal structures created by a failed mkdir
 * attempt.  Adjusts nlinkdelta for non-journaling softdep.
 */
void
softdep_revert_mkdir(dp, ip)
	struct inode *dp;
	struct inode *ip;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct vnode *dvp;

	dvp = ITOV(dp);

	ACQUIRE_LOCK(&lk);
	inodedep = inodedep_lookup_ip(dp);
	if (DOINGSUJ(dvp)) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref->ja_parent == ip->i_number,
		    ("softdep_revert_mkdir: dotdot addref parent mismatch"));
		cancel_jaddref(jaddref, inodedep, &inodedep->id_inowait);
	}
	inodedep = inodedep_lookup_ip(ip);
	if (DOINGSUJ(dvp)) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref->ja_parent == dp->i_number,
		    ("softdep_revert_mkdir: addref parent mismatch"));
		cancel_jaddref(jaddref, inodedep, &inodedep->id_inowait);
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref->ja_parent == ip->i_number,
		    ("softdep_revert_mkdir: dot addref parent mismatch"));
		cancel_jaddref(jaddref, inodedep, &inodedep->id_inowait);
	}
	FREE_LOCK(&lk);
}

/* 
 * Called to correct nlinkdelta after a failed rmdir.
 */
void
softdep_revert_rmdir(dp, ip)
	struct inode *dp;
	struct inode *ip;
{

	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup_ip(ip);
	(void) inodedep_lookup_ip(dp);
	FREE_LOCK(&lk);
}

/*
 * Protecting the freemaps (or bitmaps).
 * 
 * To eliminate the need to execute fsck before mounting a filesystem
 * after a power failure, one must (conservatively) guarantee that the
 * on-disk copy of the bitmaps never indicate that a live inode or block is
 * free.  So, when a block or inode is allocated, the bitmap should be
 * updated (on disk) before any new pointers.  When a block or inode is
 * freed, the bitmap should not be updated until all pointers have been
 * reset.  The latter dependency is handled by the delayed de-allocation
 * approach described below for block and inode de-allocation.  The former
 * dependency is handled by calling the following procedure when a block or
 * inode is allocated. When an inode is allocated an "inodedep" is created
 * with its DEPCOMPLETE flag cleared until its bitmap is written to disk.
 * Each "inodedep" is also inserted into the hash indexing structure so
 * that any additional link additions can be made dependent on the inode
 * allocation.
 * 
 * The ufs filesystem maintains a number of free block counts (e.g., per
 * cylinder group, per cylinder and per <cylinder, rotational position> pair)
 * in addition to the bitmaps.  These counts are used to improve efficiency
 * during allocation and therefore must be consistent with the bitmaps.
 * There is no convenient way to guarantee post-crash consistency of these
 * counts with simple update ordering, for two main reasons: (1) The counts
 * and bitmaps for a single cylinder group block are not in the same disk
 * sector.  If a disk write is interrupted (e.g., by power failure), one may
 * be written and the other not.  (2) Some of the counts are located in the
 * superblock rather than the cylinder group block. So, we focus our soft
 * updates implementation on protecting the bitmaps. When mounting a
 * filesystem, we recompute the auxiliary counts from the bitmaps.
 */

/*
 * Called just after updating the cylinder group block to allocate an inode.
 */
void
softdep_setup_inomapdep(bp, ip, newinum)
	struct buf *bp;		/* buffer for cylgroup block with inode map */
	struct inode *ip;	/* inode related to allocation */
	ino_t newinum;		/* new inode number being allocated */
{
	struct inodedep *inodedep;
	struct bmsafemap *bmsafemap;
	struct jaddref *jaddref;
	struct mount *mp;
	struct fs *fs;

	mp = UFSTOVFS(ip->i_ump);
	fs = ip->i_ump->um_fs;
	jaddref = NULL;

	/*
	 * Allocate the journal reference add structure so that the bitmap
	 * can be dependent on it.
	 */
	if (mp->mnt_kern_flag & MNTK_SUJ) {
		jaddref = newjaddref(ip, newinum, 0, 0, 0);
		jaddref->ja_state |= NEWBLOCK;
	}

	/*
	 * Create a dependency for the newly allocated inode.
	 * Panic if it already exists as something is seriously wrong.
	 * Otherwise add it to the dependency list for the buffer holding
	 * the cylinder group map from which it was allocated.
	 */
	ACQUIRE_LOCK(&lk);
	if ((inodedep_lookup(mp, newinum, DEPALLOC|NODELAY, &inodedep)))
		panic("softdep_setup_inomapdep: dependency %p for new"
		    "inode already exists", inodedep);
	bmsafemap = bmsafemap_lookup(mp, bp, ino_to_cg(fs, newinum));
	if (jaddref) {
		LIST_INSERT_HEAD(&bmsafemap->sm_jaddrefhd, jaddref, ja_bmdeps);
		TAILQ_INSERT_TAIL(&inodedep->id_inoreflst, &jaddref->ja_ref,
		    if_deps);
	} else {
		inodedep->id_state |= ONDEPLIST;
		LIST_INSERT_HEAD(&bmsafemap->sm_inodedephd, inodedep, id_deps);
	}
	inodedep->id_bmsafemap = bmsafemap;
	inodedep->id_state &= ~DEPCOMPLETE;
	FREE_LOCK(&lk);
}

/*
 * Called just after updating the cylinder group block to
 * allocate block or fragment.
 */
void
softdep_setup_blkmapdep(bp, mp, newblkno, frags, oldfrags)
	struct buf *bp;		/* buffer for cylgroup block with block map */
	struct mount *mp;	/* filesystem doing allocation */
	ufs2_daddr_t newblkno;	/* number of newly allocated block */
	int frags;		/* Number of fragments. */
	int oldfrags;		/* Previous number of fragments for extend. */
{
	struct newblk *newblk;
	struct bmsafemap *bmsafemap;
	struct jnewblk *jnewblk;
	struct fs *fs;

	fs = VFSTOUFS(mp)->um_fs;
	jnewblk = NULL;
	/*
	 * Create a dependency for the newly allocated block.
	 * Add it to the dependency list for the buffer holding
	 * the cylinder group map from which it was allocated.
	 */
	if (mp->mnt_kern_flag & MNTK_SUJ) {
		jnewblk = malloc(sizeof(*jnewblk), M_JNEWBLK, M_SOFTDEP_FLAGS);
		workitem_alloc(&jnewblk->jn_list, D_JNEWBLK, mp);
		jnewblk->jn_jsegdep = newjsegdep(&jnewblk->jn_list);
		jnewblk->jn_state = ATTACHED;
		jnewblk->jn_blkno = newblkno;
		jnewblk->jn_frags = frags;
		jnewblk->jn_oldfrags = oldfrags;
#ifdef SUJ_DEBUG
		{
			struct cg *cgp;
			uint8_t *blksfree;
			long bno;
			int i;
	
			cgp = (struct cg *)bp->b_data;
			blksfree = cg_blksfree(cgp);
			bno = dtogd(fs, jnewblk->jn_blkno);
			for (i = jnewblk->jn_oldfrags; i < jnewblk->jn_frags;
			    i++) {
				if (isset(blksfree, bno + i))
					panic("softdep_setup_blkmapdep: "
					    "free fragment %d from %d-%d "
					    "state 0x%X dep %p", i,
					    jnewblk->jn_oldfrags,
					    jnewblk->jn_frags,
					    jnewblk->jn_state,
					    jnewblk->jn_newblk);
			}
		}
#endif
	}
	ACQUIRE_LOCK(&lk);
	if (newblk_lookup(mp, newblkno, DEPALLOC, &newblk) != 0)
		panic("softdep_setup_blkmapdep: found block");
	newblk->nb_bmsafemap = bmsafemap = bmsafemap_lookup(mp, bp,
	    dtog(fs, newblkno));
	if (jnewblk) {
		jnewblk->jn_newblk = newblk;
		LIST_INSERT_HEAD(&bmsafemap->sm_jnewblkhd, jnewblk, jn_deps);
	} else {
		newblk->nb_state |= ONDEPLIST;
		LIST_INSERT_HEAD(&bmsafemap->sm_newblkhd, newblk, nb_deps);
	}
	newblk->nb_bmsafemap = bmsafemap;
	newblk->nb_jnewblk = jnewblk;
	FREE_LOCK(&lk);
}

#define	BMSAFEMAP_HASH(fs, cg) \
      (&bmsafemap_hashtbl[((((register_t)(fs)) >> 13) + (cg)) & bmsafemap_hash])

static int
bmsafemap_find(bmsafemaphd, mp, cg, bmsafemapp)
	struct bmsafemap_hashhead *bmsafemaphd;
	struct mount *mp;
	int cg;
	struct bmsafemap **bmsafemapp;
{
	struct bmsafemap *bmsafemap;

	LIST_FOREACH(bmsafemap, bmsafemaphd, sm_hash)
		if (bmsafemap->sm_list.wk_mp == mp && bmsafemap->sm_cg == cg)
			break;
	if (bmsafemap) {
		*bmsafemapp = bmsafemap;
		return (1);
	}
	*bmsafemapp = NULL;

	return (0);
}

/*
 * Find the bmsafemap associated with a cylinder group buffer.
 * If none exists, create one. The buffer must be locked when
 * this routine is called and this routine must be called with
 * splbio interrupts blocked.
 */
static struct bmsafemap *
bmsafemap_lookup(mp, bp, cg)
	struct mount *mp;
	struct buf *bp;
	int cg;
{
	struct bmsafemap_hashhead *bmsafemaphd;
	struct bmsafemap *bmsafemap, *collision;
	struct worklist *wk;
	struct fs *fs;

	mtx_assert(&lk, MA_OWNED);
	if (bp)
		LIST_FOREACH(wk, &bp->b_dep, wk_list)
			if (wk->wk_type == D_BMSAFEMAP)
				return (WK_BMSAFEMAP(wk));
	fs = VFSTOUFS(mp)->um_fs;
	bmsafemaphd = BMSAFEMAP_HASH(fs, cg);
	if (bmsafemap_find(bmsafemaphd, mp, cg, &bmsafemap) == 1)
		return (bmsafemap);
	FREE_LOCK(&lk);
	bmsafemap = malloc(sizeof(struct bmsafemap),
		M_BMSAFEMAP, M_SOFTDEP_FLAGS);
	workitem_alloc(&bmsafemap->sm_list, D_BMSAFEMAP, mp);
	bmsafemap->sm_buf = bp;
	LIST_INIT(&bmsafemap->sm_inodedephd);
	LIST_INIT(&bmsafemap->sm_inodedepwr);
	LIST_INIT(&bmsafemap->sm_newblkhd);
	LIST_INIT(&bmsafemap->sm_newblkwr);
	LIST_INIT(&bmsafemap->sm_jaddrefhd);
	LIST_INIT(&bmsafemap->sm_jnewblkhd);
	ACQUIRE_LOCK(&lk);
	if (bmsafemap_find(bmsafemaphd, mp, cg, &collision) == 1) {
		WORKITEM_FREE(bmsafemap, D_BMSAFEMAP);
		return (collision);
	}
	bmsafemap->sm_cg = cg;
	LIST_INSERT_HEAD(bmsafemaphd, bmsafemap, sm_hash);
	WORKLIST_INSERT(&bp->b_dep, &bmsafemap->sm_list);
	return (bmsafemap);
}

/*
 * Direct block allocation dependencies.
 * 
 * When a new block is allocated, the corresponding disk locations must be
 * initialized (with zeros or new data) before the on-disk inode points to
 * them.  Also, the freemap from which the block was allocated must be
 * updated (on disk) before the inode's pointer. These two dependencies are
 * independent of each other and are needed for all file blocks and indirect
 * blocks that are pointed to directly by the inode.  Just before the
 * "in-core" version of the inode is updated with a newly allocated block
 * number, a procedure (below) is called to setup allocation dependency
 * structures.  These structures are removed when the corresponding
 * dependencies are satisfied or when the block allocation becomes obsolete
 * (i.e., the file is deleted, the block is de-allocated, or the block is a
 * fragment that gets upgraded).  All of these cases are handled in
 * procedures described later.
 * 
 * When a file extension causes a fragment to be upgraded, either to a larger
 * fragment or to a full block, the on-disk location may change (if the
 * previous fragment could not simply be extended). In this case, the old
 * fragment must be de-allocated, but not until after the inode's pointer has
 * been updated. In most cases, this is handled by later procedures, which
 * will construct a "freefrag" structure to be added to the workitem queue
 * when the inode update is complete (or obsolete).  The main exception to
 * this is when an allocation occurs while a pending allocation dependency
 * (for the same block pointer) remains.  This case is handled in the main
 * allocation dependency setup procedure by immediately freeing the
 * unreferenced fragments.
 */ 
void 
softdep_setup_allocdirect(ip, off, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;	/* inode to which block is being added */
	ufs_lbn_t off;		/* block pointer within inode */
	ufs2_daddr_t newblkno;	/* disk block number being added */
	ufs2_daddr_t oldblkno;	/* previous block number, 0 unless frag */
	long newsize;		/* size of new block */
	long oldsize;		/* size of new block */
	struct buf *bp;		/* bp for allocated block */
{
	struct allocdirect *adp, *oldadp;
	struct allocdirectlst *adphead;
	struct freefrag *freefrag;
	struct inodedep *inodedep;
	struct pagedep *pagedep;
	struct jnewblk *jnewblk;
	struct newblk *newblk;
	struct mount *mp;
	ufs_lbn_t lbn;

	lbn = bp->b_lblkno;
	mp = UFSTOVFS(ip->i_ump);
	if (oldblkno && oldblkno != newblkno)
		freefrag = newfreefrag(ip, oldblkno, oldsize, lbn);
	else
		freefrag = NULL;

	ACQUIRE_LOCK(&lk);
	if (off >= NDADDR) {
		if (lbn > 0)
			panic("softdep_setup_allocdirect: bad lbn %jd, off %jd",
			    lbn, off);
		/* allocating an indirect block */
		if (oldblkno != 0)
			panic("softdep_setup_allocdirect: non-zero indir");
	} else {
		if (off != lbn)
			panic("softdep_setup_allocdirect: lbn %jd != off %jd",
			    lbn, off);
		/*
		 * Allocating a direct block.
		 *
		 * If we are allocating a directory block, then we must
		 * allocate an associated pagedep to track additions and
		 * deletions.
		 */
		if ((ip->i_mode & IFMT) == IFDIR &&
		    pagedep_lookup(mp, ip->i_number, off, DEPALLOC,
		    &pagedep) == 0)
			WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	}
	if (newblk_lookup(mp, newblkno, 0, &newblk) == 0)
		panic("softdep_setup_allocdirect: lost block");
	KASSERT(newblk->nb_list.wk_type == D_NEWBLK,
	    ("softdep_setup_allocdirect: newblk already initialized"));
	/*
	 * Convert the newblk to an allocdirect.
	 */
	newblk->nb_list.wk_type = D_ALLOCDIRECT;
	adp = (struct allocdirect *)newblk;
	newblk->nb_freefrag = freefrag;
	adp->ad_offset = off;
	adp->ad_oldblkno = oldblkno;
	adp->ad_newsize = newsize;
	adp->ad_oldsize = oldsize;

	/*
	 * Finish initializing the journal.
	 */
	if ((jnewblk = newblk->nb_jnewblk) != NULL) {
		jnewblk->jn_ino = ip->i_number;
		jnewblk->jn_lbn = lbn;
		add_to_journal(&jnewblk->jn_list);
	}
	if (freefrag && freefrag->ff_jfreefrag != NULL)
		add_to_journal(&freefrag->ff_jfreefrag->fr_list);
	inodedep_lookup(mp, ip->i_number, DEPALLOC | NODELAY, &inodedep);
	adp->ad_inodedep = inodedep;

	WORKLIST_INSERT(&bp->b_dep, &newblk->nb_list);
	/*
	 * The list of allocdirects must be kept in sorted and ascending
	 * order so that the rollback routines can quickly determine the
	 * first uncommitted block (the size of the file stored on disk
	 * ends at the end of the lowest committed fragment, or if there
	 * are no fragments, at the end of the highest committed block).
	 * Since files generally grow, the typical case is that the new
	 * block is to be added at the end of the list. We speed this
	 * special case by checking against the last allocdirect in the
	 * list before laboriously traversing the list looking for the
	 * insertion point.
	 */
	adphead = &inodedep->id_newinoupdt;
	oldadp = TAILQ_LAST(adphead, allocdirectlst);
	if (oldadp == NULL || oldadp->ad_offset <= off) {
		/* insert at end of list */
		TAILQ_INSERT_TAIL(adphead, adp, ad_next);
		if (oldadp != NULL && oldadp->ad_offset == off)
			allocdirect_merge(adphead, adp, oldadp);
		FREE_LOCK(&lk);
		return;
	}
	TAILQ_FOREACH(oldadp, adphead, ad_next) {
		if (oldadp->ad_offset >= off)
			break;
	}
	if (oldadp == NULL)
		panic("softdep_setup_allocdirect: lost entry");
	/* insert in middle of list */
	TAILQ_INSERT_BEFORE(oldadp, adp, ad_next);
	if (oldadp->ad_offset == off)
		allocdirect_merge(adphead, adp, oldadp);

	FREE_LOCK(&lk);
}

/*
 * Replace an old allocdirect dependency with a newer one.
 * This routine must be called with splbio interrupts blocked.
 */
static void
allocdirect_merge(adphead, newadp, oldadp)
	struct allocdirectlst *adphead;	/* head of list holding allocdirects */
	struct allocdirect *newadp;	/* allocdirect being added */
	struct allocdirect *oldadp;	/* existing allocdirect being checked */
{
	struct worklist *wk;
	struct freefrag *freefrag;
	struct newdirblk *newdirblk;

	freefrag = NULL;
	mtx_assert(&lk, MA_OWNED);
	if (newadp->ad_oldblkno != oldadp->ad_newblkno ||
	    newadp->ad_oldsize != oldadp->ad_newsize ||
	    newadp->ad_offset >= NDADDR)
		panic("%s %jd != new %jd || old size %ld != new %ld",
		    "allocdirect_merge: old blkno",
		    (intmax_t)newadp->ad_oldblkno,
		    (intmax_t)oldadp->ad_newblkno,
		    newadp->ad_oldsize, oldadp->ad_newsize);
	newadp->ad_oldblkno = oldadp->ad_oldblkno;
	newadp->ad_oldsize = oldadp->ad_oldsize;
	/*
	 * If the old dependency had a fragment to free or had never
	 * previously had a block allocated, then the new dependency
	 * can immediately post its freefrag and adopt the old freefrag.
	 * This action is done by swapping the freefrag dependencies.
	 * The new dependency gains the old one's freefrag, and the
	 * old one gets the new one and then immediately puts it on
	 * the worklist when it is freed by free_newblk. It is
	 * not possible to do this swap when the old dependency had a
	 * non-zero size but no previous fragment to free. This condition
	 * arises when the new block is an extension of the old block.
	 * Here, the first part of the fragment allocated to the new
	 * dependency is part of the block currently claimed on disk by
	 * the old dependency, so cannot legitimately be freed until the
	 * conditions for the new dependency are fulfilled.
	 */
	freefrag = newadp->ad_freefrag;
	if (oldadp->ad_freefrag != NULL || oldadp->ad_oldblkno == 0) {
		newadp->ad_freefrag = oldadp->ad_freefrag;
		oldadp->ad_freefrag = freefrag;
	}
	/*
	 * If we are tracking a new directory-block allocation,
	 * move it from the old allocdirect to the new allocdirect.
	 */
	if ((wk = LIST_FIRST(&oldadp->ad_newdirblk)) != NULL) {
		newdirblk = WK_NEWDIRBLK(wk);
		WORKLIST_REMOVE(&newdirblk->db_list);
		if (!LIST_EMPTY(&oldadp->ad_newdirblk))
			panic("allocdirect_merge: extra newdirblk");
		WORKLIST_INSERT(&newadp->ad_newdirblk, &newdirblk->db_list);
	}
	TAILQ_REMOVE(adphead, oldadp, ad_next);
	/*
	 * We need to move any journal dependencies over to the freefrag
	 * that releases this block if it exists.  Otherwise we are
	 * extending an existing block and we'll wait until that is
	 * complete to release the journal space and extend the
	 * new journal to cover this old space as well.
	 */
	if (freefrag == NULL) {
		struct jnewblk *jnewblk;
		struct jnewblk *njnewblk;

		if (oldadp->ad_newblkno != newadp->ad_newblkno)
			panic("allocdirect_merge: %jd != %jd",
			    oldadp->ad_newblkno, newadp->ad_newblkno);
		jnewblk = oldadp->ad_block.nb_jnewblk;
		cancel_newblk(&oldadp->ad_block, &newadp->ad_block.nb_jwork);
		/*
		 * We have an unwritten jnewblk, we need to merge the
		 * frag bits with our own.  The newer adp's journal can not
		 * be written prior to the old one so no need to check for
		 * it here.
		 */
		if (jnewblk) {
			njnewblk = newadp->ad_block.nb_jnewblk;
			if (njnewblk == NULL)
				panic("allocdirect_merge: No jnewblk");
			if (jnewblk->jn_state & UNDONE) {
				njnewblk->jn_state |= UNDONE | NEWBLOCK;
				njnewblk->jn_state &= ~ATTACHED;
				jnewblk->jn_state &= ~UNDONE;
			}
			njnewblk->jn_oldfrags = jnewblk->jn_oldfrags;
			WORKLIST_REMOVE(&jnewblk->jn_list);
			jnewblk->jn_state |= ATTACHED | COMPLETE;
			free_jnewblk(jnewblk);
		}
	} else {
		/*
		 * We can skip journaling for this freefrag and just complete
		 * any pending journal work for the allocdirect that is being
		 * removed after the freefrag completes.
		 */
		if (freefrag->ff_jfreefrag)
			cancel_jfreefrag(freefrag->ff_jfreefrag);
		cancel_newblk(&oldadp->ad_block, &freefrag->ff_jwork);
	}
	free_newblk(&oldadp->ad_block);
}

/*
 * Allocate a jfreefrag structure to journal a single block free.
 */
static struct jfreefrag *
newjfreefrag(freefrag, ip, blkno, size, lbn)
	struct freefrag *freefrag;
	struct inode *ip;
	ufs2_daddr_t blkno;
	long size;
	ufs_lbn_t lbn;
{
	struct jfreefrag *jfreefrag;
	struct fs *fs;

	fs = ip->i_fs;
	jfreefrag = malloc(sizeof(struct jfreefrag), M_JFREEFRAG,
	    M_SOFTDEP_FLAGS);
	workitem_alloc(&jfreefrag->fr_list, D_JFREEFRAG, UFSTOVFS(ip->i_ump));
	jfreefrag->fr_jsegdep = newjsegdep(&jfreefrag->fr_list);
	jfreefrag->fr_state = ATTACHED | DEPCOMPLETE;
	jfreefrag->fr_ino = ip->i_number;
	jfreefrag->fr_lbn = lbn;
	jfreefrag->fr_blkno = blkno;
	jfreefrag->fr_frags = numfrags(fs, size);
	jfreefrag->fr_freefrag = freefrag;

	return (jfreefrag);
}

/*
 * Allocate a new freefrag structure.
 */
static struct freefrag *
newfreefrag(ip, blkno, size, lbn)
	struct inode *ip;
	ufs2_daddr_t blkno;
	long size;
	ufs_lbn_t lbn;
{
	struct freefrag *freefrag;
	struct fs *fs;

	fs = ip->i_fs;
	if (fragnum(fs, blkno) + numfrags(fs, size) > fs->fs_frag)
		panic("newfreefrag: frag size");
	freefrag = malloc(sizeof(struct freefrag),
	    M_FREEFRAG, M_SOFTDEP_FLAGS);
	workitem_alloc(&freefrag->ff_list, D_FREEFRAG, UFSTOVFS(ip->i_ump));
	freefrag->ff_state = ATTACHED;
	LIST_INIT(&freefrag->ff_jwork);
	freefrag->ff_inum = ip->i_number;
	freefrag->ff_blkno = blkno;
	freefrag->ff_fragsize = size;

	if (fs->fs_flags & FS_SUJ) {
		freefrag->ff_jfreefrag =
		    newjfreefrag(freefrag, ip, blkno, size, lbn);
	} else {
		freefrag->ff_state |= DEPCOMPLETE;
		freefrag->ff_jfreefrag = NULL;
	}

	return (freefrag);
}

/*
 * This workitem de-allocates fragments that were replaced during
 * file block allocation.
 */
static void 
handle_workitem_freefrag(freefrag)
	struct freefrag *freefrag;
{
	struct ufsmount *ump = VFSTOUFS(freefrag->ff_list.wk_mp);
	struct workhead wkhd;

	/*
	 * It would be illegal to add new completion items to the
	 * freefrag after it was schedule to be done so it must be
	 * safe to modify the list head here.
	 */
	LIST_INIT(&wkhd);
	LIST_SWAP(&freefrag->ff_jwork, &wkhd, worklist, wk_list);
	ffs_blkfree(ump, ump->um_fs, ump->um_devvp, freefrag->ff_blkno,
	    freefrag->ff_fragsize, freefrag->ff_inum, &wkhd);
	ACQUIRE_LOCK(&lk);
	WORKITEM_FREE(freefrag, D_FREEFRAG);
	FREE_LOCK(&lk);
}

/*
 * Set up a dependency structure for an external attributes data block.
 * This routine follows much of the structure of softdep_setup_allocdirect.
 * See the description of softdep_setup_allocdirect above for details.
 */
void 
softdep_setup_allocext(ip, off, newblkno, oldblkno, newsize, oldsize, bp)
	struct inode *ip;
	ufs_lbn_t off;
	ufs2_daddr_t newblkno;
	ufs2_daddr_t oldblkno;
	long newsize;
	long oldsize;
	struct buf *bp;
{
	struct allocdirect *adp, *oldadp;
	struct allocdirectlst *adphead;
	struct freefrag *freefrag;
	struct inodedep *inodedep;
	struct jnewblk *jnewblk;
	struct newblk *newblk;
	struct mount *mp;
	ufs_lbn_t lbn;

	if (off >= NXADDR)
		panic("softdep_setup_allocext: lbn %lld > NXADDR",
		    (long long)off);

	lbn = bp->b_lblkno;
	mp = UFSTOVFS(ip->i_ump);
	if (oldblkno && oldblkno != newblkno)
		freefrag = newfreefrag(ip, oldblkno, oldsize, lbn);
	else
		freefrag = NULL;

	ACQUIRE_LOCK(&lk);
	if (newblk_lookup(mp, newblkno, 0, &newblk) == 0)
		panic("softdep_setup_allocext: lost block");
	KASSERT(newblk->nb_list.wk_type == D_NEWBLK,
	    ("softdep_setup_allocext: newblk already initialized"));
	/*
	 * Convert the newblk to an allocdirect.
	 */
	newblk->nb_list.wk_type = D_ALLOCDIRECT;
	adp = (struct allocdirect *)newblk;
	newblk->nb_freefrag = freefrag;
	adp->ad_offset = off;
	adp->ad_oldblkno = oldblkno;
	adp->ad_newsize = newsize;
	adp->ad_oldsize = oldsize;
	adp->ad_state |=  EXTDATA;

	/*
	 * Finish initializing the journal.
	 */
	if ((jnewblk = newblk->nb_jnewblk) != NULL) {
		jnewblk->jn_ino = ip->i_number;
		jnewblk->jn_lbn = lbn;
		add_to_journal(&jnewblk->jn_list);
	}
	if (freefrag && freefrag->ff_jfreefrag != NULL)
		add_to_journal(&freefrag->ff_jfreefrag->fr_list);
	inodedep_lookup(mp, ip->i_number, DEPALLOC | NODELAY, &inodedep);
	adp->ad_inodedep = inodedep;

	WORKLIST_INSERT(&bp->b_dep, &newblk->nb_list);
	/*
	 * The list of allocdirects must be kept in sorted and ascending
	 * order so that the rollback routines can quickly determine the
	 * first uncommitted block (the size of the file stored on disk
	 * ends at the end of the lowest committed fragment, or if there
	 * are no fragments, at the end of the highest committed block).
	 * Since files generally grow, the typical case is that the new
	 * block is to be added at the end of the list. We speed this
	 * special case by checking against the last allocdirect in the
	 * list before laboriously traversing the list looking for the
	 * insertion point.
	 */
	adphead = &inodedep->id_newextupdt;
	oldadp = TAILQ_LAST(adphead, allocdirectlst);
	if (oldadp == NULL || oldadp->ad_offset <= off) {
		/* insert at end of list */
		TAILQ_INSERT_TAIL(adphead, adp, ad_next);
		if (oldadp != NULL && oldadp->ad_offset == off)
			allocdirect_merge(adphead, adp, oldadp);
		FREE_LOCK(&lk);
		return;
	}
	TAILQ_FOREACH(oldadp, adphead, ad_next) {
		if (oldadp->ad_offset >= off)
			break;
	}
	if (oldadp == NULL)
		panic("softdep_setup_allocext: lost entry");
	/* insert in middle of list */
	TAILQ_INSERT_BEFORE(oldadp, adp, ad_next);
	if (oldadp->ad_offset == off)
		allocdirect_merge(adphead, adp, oldadp);
	FREE_LOCK(&lk);
}

/*
 * Indirect block allocation dependencies.
 * 
 * The same dependencies that exist for a direct block also exist when
 * a new block is allocated and pointed to by an entry in a block of
 * indirect pointers. The undo/redo states described above are also
 * used here. Because an indirect block contains many pointers that
 * may have dependencies, a second copy of the entire in-memory indirect
 * block is kept. The buffer cache copy is always completely up-to-date.
 * The second copy, which is used only as a source for disk writes,
 * contains only the safe pointers (i.e., those that have no remaining
 * update dependencies). The second copy is freed when all pointers
 * are safe. The cache is not allowed to replace indirect blocks with
 * pending update dependencies. If a buffer containing an indirect
 * block with dependencies is written, these routines will mark it
 * dirty again. It can only be successfully written once all the
 * dependencies are removed. The ffs_fsync routine in conjunction with
 * softdep_sync_metadata work together to get all the dependencies
 * removed so that a file can be successfully written to disk. Three
 * procedures are used when setting up indirect block pointer
 * dependencies. The division is necessary because of the organization
 * of the "balloc" routine and because of the distinction between file
 * pages and file metadata blocks.
 */

/*
 * Allocate a new allocindir structure.
 */
static struct allocindir *
newallocindir(ip, ptrno, newblkno, oldblkno, lbn)
	struct inode *ip;	/* inode for file being extended */
	int ptrno;		/* offset of pointer in indirect block */
	ufs2_daddr_t newblkno;	/* disk block number being added */
	ufs2_daddr_t oldblkno;	/* previous block number, 0 if none */
	ufs_lbn_t lbn;
{
	struct newblk *newblk;
	struct allocindir *aip;
	struct freefrag *freefrag;
	struct jnewblk *jnewblk;

	if (oldblkno)
		freefrag = newfreefrag(ip, oldblkno, ip->i_fs->fs_bsize, lbn);
	else
		freefrag = NULL;
	ACQUIRE_LOCK(&lk);
	if (newblk_lookup(UFSTOVFS(ip->i_ump), newblkno, 0, &newblk) == 0)
		panic("new_allocindir: lost block");
	KASSERT(newblk->nb_list.wk_type == D_NEWBLK,
	    ("newallocindir: newblk already initialized"));
	newblk->nb_list.wk_type = D_ALLOCINDIR;
	newblk->nb_freefrag = freefrag;
	aip = (struct allocindir *)newblk;
	aip->ai_offset = ptrno;
	aip->ai_oldblkno = oldblkno;
	if ((jnewblk = newblk->nb_jnewblk) != NULL) {
		jnewblk->jn_ino = ip->i_number;
		jnewblk->jn_lbn = lbn;
		add_to_journal(&jnewblk->jn_list);
	}
	if (freefrag && freefrag->ff_jfreefrag != NULL)
		add_to_journal(&freefrag->ff_jfreefrag->fr_list);
	return (aip);
}

/*
 * Called just before setting an indirect block pointer
 * to a newly allocated file page.
 */
void
softdep_setup_allocindir_page(ip, lbn, bp, ptrno, newblkno, oldblkno, nbp)
	struct inode *ip;	/* inode for file being extended */
	ufs_lbn_t lbn;		/* allocated block number within file */
	struct buf *bp;		/* buffer with indirect blk referencing page */
	int ptrno;		/* offset of pointer in indirect block */
	ufs2_daddr_t newblkno;	/* disk block number being added */
	ufs2_daddr_t oldblkno;	/* previous block number, 0 if none */
	struct buf *nbp;	/* buffer holding allocated page */
{
	struct inodedep *inodedep;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct mount *mp;

	if (lbn != nbp->b_lblkno)
		panic("softdep_setup_allocindir_page: lbn %jd != lblkno %jd",
		    lbn, bp->b_lblkno);
	ASSERT_VOP_LOCKED(ITOV(ip), "softdep_setup_allocindir_page");
	mp = UFSTOVFS(ip->i_ump);
	aip = newallocindir(ip, ptrno, newblkno, oldblkno, lbn);
	(void) inodedep_lookup(mp, ip->i_number, DEPALLOC, &inodedep);
	/*
	 * If we are allocating a directory page, then we must
	 * allocate an associated pagedep to track additions and
	 * deletions.
	 */
	if ((ip->i_mode & IFMT) == IFDIR &&
	    pagedep_lookup(mp, ip->i_number, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&nbp->b_dep, &pagedep->pd_list);
	WORKLIST_INSERT(&nbp->b_dep, &aip->ai_block.nb_list);
	setup_allocindir_phase2(bp, ip, inodedep, aip, lbn);
	FREE_LOCK(&lk);
}

/*
 * Called just before setting an indirect block pointer to a
 * newly allocated indirect block.
 */
void
softdep_setup_allocindir_meta(nbp, ip, bp, ptrno, newblkno)
	struct buf *nbp;	/* newly allocated indirect block */
	struct inode *ip;	/* inode for file being extended */
	struct buf *bp;		/* indirect block referencing allocated block */
	int ptrno;		/* offset of pointer in indirect block */
	ufs2_daddr_t newblkno;	/* disk block number being added */
{
	struct inodedep *inodedep;
	struct allocindir *aip;
	ufs_lbn_t lbn;

	lbn = nbp->b_lblkno;
	ASSERT_VOP_LOCKED(ITOV(ip), "softdep_setup_allocindir_meta");
	aip = newallocindir(ip, ptrno, newblkno, 0, lbn);
	inodedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number, DEPALLOC, &inodedep);
	WORKLIST_INSERT(&nbp->b_dep, &aip->ai_block.nb_list);
	setup_allocindir_phase2(bp, ip, inodedep, aip, lbn);
	FREE_LOCK(&lk);
}

static void
indirdep_complete(indirdep)
	struct indirdep *indirdep;
{
	struct allocindir *aip;

	LIST_REMOVE(indirdep, ir_next);
	indirdep->ir_state &= ~ONDEPLIST;

	while ((aip = LIST_FIRST(&indirdep->ir_completehd)) != NULL) {
		LIST_REMOVE(aip, ai_next);
		free_newblk(&aip->ai_block);
	}
	/*
	 * If this indirdep is not attached to a buf it was simply waiting
	 * on completion to clear completehd.  free_indirdep() asserts
	 * that nothing is dangling.
	 */
	if ((indirdep->ir_state & ONWORKLIST) == 0)
		free_indirdep(indirdep);
}

/*
 * Called to finish the allocation of the "aip" allocated
 * by one of the two routines above.
 */
static void 
setup_allocindir_phase2(bp, ip, inodedep, aip, lbn)
	struct buf *bp;		/* in-memory copy of the indirect block */
	struct inode *ip;	/* inode for file being extended */
	struct inodedep *inodedep; /* Inodedep for ip */
	struct allocindir *aip;	/* allocindir allocated by the above routines */
	ufs_lbn_t lbn;		/* Logical block number for this block. */
{
	struct worklist *wk;
	struct fs *fs;
	struct newblk *newblk;
	struct indirdep *indirdep, *newindirdep;
	struct allocindir *oldaip;
	struct freefrag *freefrag;
	struct mount *mp;
	ufs2_daddr_t blkno;

	mp = UFSTOVFS(ip->i_ump);
	fs = ip->i_fs;
	mtx_assert(&lk, MA_OWNED);
	if (bp->b_lblkno >= 0)
		panic("setup_allocindir_phase2: not indir blk");
	for (freefrag = NULL, indirdep = NULL, newindirdep = NULL; ; ) {
		LIST_FOREACH(wk, &bp->b_dep, wk_list) {
			if (wk->wk_type != D_INDIRDEP)
				continue;
			indirdep = WK_INDIRDEP(wk);
			break;
		}
		if (indirdep == NULL && newindirdep) {
			indirdep = newindirdep;
			newindirdep = NULL;
			WORKLIST_INSERT(&bp->b_dep, &indirdep->ir_list);
			if (newblk_lookup(mp, dbtofsb(fs, bp->b_blkno), 0,
			    &newblk)) {
				indirdep->ir_state |= ONDEPLIST;
				LIST_INSERT_HEAD(&newblk->nb_indirdeps,
				    indirdep, ir_next);
			} else
				indirdep->ir_state |= DEPCOMPLETE;
		}
		if (indirdep) {
			aip->ai_indirdep = indirdep;
			/*
			 * Check to see if there is an existing dependency
			 * for this block. If there is, merge the old
			 * dependency into the new one.  This happens
			 * as a result of reallocblk only.
			 */
			if (aip->ai_oldblkno == 0)
				oldaip = NULL;
			else

				LIST_FOREACH(oldaip, &indirdep->ir_deplisthd,
				    ai_next)
					if (oldaip->ai_offset == aip->ai_offset)
						break;
			if (oldaip != NULL)
				freefrag = allocindir_merge(aip, oldaip);
			LIST_INSERT_HEAD(&indirdep->ir_deplisthd, aip, ai_next);
			KASSERT(aip->ai_offset >= 0 &&
			    aip->ai_offset < NINDIR(ip->i_ump->um_fs),
			    ("setup_allocindir_phase2: Bad offset %d",
			    aip->ai_offset));
			KASSERT(indirdep->ir_savebp != NULL,
			    ("setup_allocindir_phase2 NULL ir_savebp"));
			if (ip->i_ump->um_fstype == UFS1)
				((ufs1_daddr_t *)indirdep->ir_savebp->b_data)
				    [aip->ai_offset] = aip->ai_oldblkno;
			else
				((ufs2_daddr_t *)indirdep->ir_savebp->b_data)
				    [aip->ai_offset] = aip->ai_oldblkno;
			FREE_LOCK(&lk);
			if (freefrag != NULL)
				handle_workitem_freefrag(freefrag);
		} else
			FREE_LOCK(&lk);
		if (newindirdep) {
			newindirdep->ir_savebp->b_flags |= B_INVAL | B_NOCACHE;
			brelse(newindirdep->ir_savebp);
			ACQUIRE_LOCK(&lk);
			WORKITEM_FREE((caddr_t)newindirdep, D_INDIRDEP);
			if (indirdep)
				break;
			FREE_LOCK(&lk);
		}
		if (indirdep) {
			ACQUIRE_LOCK(&lk);
			break;
		}
		newindirdep = malloc(sizeof(struct indirdep),
			M_INDIRDEP, M_SOFTDEP_FLAGS);
		workitem_alloc(&newindirdep->ir_list, D_INDIRDEP, mp);
		newindirdep->ir_state = ATTACHED;
		if (ip->i_ump->um_fstype == UFS1)
			newindirdep->ir_state |= UFS1FMT;
		newindirdep->ir_saveddata = NULL;
		LIST_INIT(&newindirdep->ir_deplisthd);
		LIST_INIT(&newindirdep->ir_donehd);
		LIST_INIT(&newindirdep->ir_writehd);
		LIST_INIT(&newindirdep->ir_completehd);
		LIST_INIT(&newindirdep->ir_jwork);
		if (bp->b_blkno == bp->b_lblkno) {
			ufs_bmaparray(bp->b_vp, bp->b_lblkno, &blkno, bp,
			    NULL, NULL);
			bp->b_blkno = blkno;
		}
		newindirdep->ir_savebp =
		    getblk(ip->i_devvp, bp->b_blkno, bp->b_bcount, 0, 0, 0);
		BUF_KERNPROC(newindirdep->ir_savebp);
		bcopy(bp->b_data, newindirdep->ir_savebp->b_data, bp->b_bcount);
		ACQUIRE_LOCK(&lk);
	}
}

/*
 * Merge two allocindirs which refer to the same block.  Move newblock
 * dependencies and setup the freefrags appropriately.
 */
static struct freefrag *
allocindir_merge(aip, oldaip)
	struct allocindir *aip;
	struct allocindir *oldaip;
{
	struct newdirblk *newdirblk;
	struct freefrag *freefrag;
	struct worklist *wk;

	if (oldaip->ai_newblkno != aip->ai_oldblkno)
		panic("allocindir_merge: blkno");
	aip->ai_oldblkno = oldaip->ai_oldblkno;
	freefrag = aip->ai_freefrag;
	aip->ai_freefrag = oldaip->ai_freefrag;
	oldaip->ai_freefrag = NULL;
	KASSERT(freefrag != NULL, ("setup_allocindir_phase2: No freefrag"));
	/*
	 * If we are tracking a new directory-block allocation,
	 * move it from the old allocindir to the new allocindir.
	 */
	if ((wk = LIST_FIRST(&oldaip->ai_newdirblk)) != NULL) {
		newdirblk = WK_NEWDIRBLK(wk);
		WORKLIST_REMOVE(&newdirblk->db_list);
		if (!LIST_EMPTY(&oldaip->ai_newdirblk))
			panic("allocindir_merge: extra newdirblk");
		WORKLIST_INSERT(&aip->ai_newdirblk, &newdirblk->db_list);
	}
	/*
	 * We can skip journaling for this freefrag and just complete
	 * any pending journal work for the allocindir that is being
	 * removed after the freefrag completes.
	 */
	if (freefrag->ff_jfreefrag)
		cancel_jfreefrag(freefrag->ff_jfreefrag);
	LIST_REMOVE(oldaip, ai_next);
	cancel_newblk(&oldaip->ai_block, &freefrag->ff_jwork);
	free_newblk(&oldaip->ai_block);

	return (freefrag);
}

/*
 * Block de-allocation dependencies.
 * 
 * When blocks are de-allocated, the on-disk pointers must be nullified before
 * the blocks are made available for use by other files.  (The true
 * requirement is that old pointers must be nullified before new on-disk
 * pointers are set.  We chose this slightly more stringent requirement to
 * reduce complexity.) Our implementation handles this dependency by updating
 * the inode (or indirect block) appropriately but delaying the actual block
 * de-allocation (i.e., freemap and free space count manipulation) until
 * after the updated versions reach stable storage.  After the disk is
 * updated, the blocks can be safely de-allocated whenever it is convenient.
 * This implementation handles only the common case of reducing a file's
 * length to zero. Other cases are handled by the conventional synchronous
 * write approach.
 *
 * The ffs implementation with which we worked double-checks
 * the state of the block pointers and file size as it reduces
 * a file's length.  Some of this code is replicated here in our
 * soft updates implementation.  The freeblks->fb_chkcnt field is
 * used to transfer a part of this information to the procedure
 * that eventually de-allocates the blocks.
 *
 * This routine should be called from the routine that shortens
 * a file's length, before the inode's size or block pointers
 * are modified. It will save the block pointer information for
 * later release and zero the inode so that the calling routine
 * can release it.
 */
void
softdep_setup_freeblocks(ip, length, flags)
	struct inode *ip;	/* The inode whose length is to be reduced */
	off_t length;		/* The new length for the file */
	int flags;		/* IO_EXT and/or IO_NORMAL */
{
	struct ufs1_dinode *dp1;
	struct ufs2_dinode *dp2;
	struct freeblks *freeblks;
	struct inodedep *inodedep;
	struct allocdirect *adp;
	struct jfreeblk *jfreeblk;
	struct bufobj *bo;
	struct vnode *vp;
	struct buf *bp;
	struct fs *fs;
	ufs2_daddr_t extblocks, datablocks;
	struct mount *mp;
	int i, delay, error;
	ufs2_daddr_t blkno;
	ufs_lbn_t tmpval;
	ufs_lbn_t lbn;
	long oldextsize;
	long oldsize;
	int frags;
	int needj;

	fs = ip->i_fs;
	mp = UFSTOVFS(ip->i_ump);
	if (length != 0)
		panic("softdep_setup_freeblocks: non-zero length");
	freeblks = malloc(sizeof(struct freeblks),
		M_FREEBLKS, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&freeblks->fb_list, D_FREEBLKS, mp);
	LIST_INIT(&freeblks->fb_jfreeblkhd);
	LIST_INIT(&freeblks->fb_jwork);
	freeblks->fb_state = ATTACHED;
	freeblks->fb_uid = ip->i_uid;
	freeblks->fb_previousinum = ip->i_number;
	freeblks->fb_devvp = ip->i_devvp;
	freeblks->fb_chkcnt = 0;
	ACQUIRE_LOCK(&lk);
	/*
	 * If we're truncating a removed file that will never be written
	 * we don't need to journal the block frees.  The canceled journals
	 * for the allocations will suffice.
	 */
	inodedep_lookup(mp, ip->i_number, DEPALLOC, &inodedep);
	if ((inodedep->id_state & (UNLINKED | DEPCOMPLETE)) == UNLINKED ||
	    (fs->fs_flags & FS_SUJ) == 0)
		needj = 0;
	else
		needj = 1;
	num_freeblkdep++;
	FREE_LOCK(&lk);
	extblocks = 0;
	if (fs->fs_magic == FS_UFS2_MAGIC)
		extblocks = btodb(fragroundup(fs, ip->i_din2->di_extsize));
	datablocks = DIP(ip, i_blocks) - extblocks;
	if ((flags & IO_NORMAL) != 0) {
		oldsize = ip->i_size;
		ip->i_size = 0;
		DIP_SET(ip, i_size, 0);
		freeblks->fb_chkcnt = datablocks;
		for (i = 0; i < NDADDR; i++) {
			blkno = DIP(ip, i_db[i]);
			DIP_SET(ip, i_db[i], 0);
			if (blkno == 0)
				continue;
			frags = sblksize(fs, oldsize, i);
			frags = numfrags(fs, frags);
			newfreework(ip->i_ump, freeblks, NULL, i, blkno, frags,
			    needj);
		}
		for (i = 0, tmpval = NINDIR(fs), lbn = NDADDR; i < NIADDR;
		    i++, tmpval *= NINDIR(fs)) {
			blkno = DIP(ip, i_ib[i]);
			DIP_SET(ip, i_ib[i], 0);
			if (blkno)
				newfreework(ip->i_ump, freeblks, NULL, -lbn - i,
				    blkno, fs->fs_frag, needj);
			lbn += tmpval;
		}
		UFS_LOCK(ip->i_ump);
		fs->fs_pendingblocks += datablocks;
		UFS_UNLOCK(ip->i_ump);
	}
	if ((flags & IO_EXT) != 0) {
		oldextsize = ip->i_din2->di_extsize;
		ip->i_din2->di_extsize = 0;
		freeblks->fb_chkcnt += extblocks;
		for (i = 0; i < NXADDR; i++) {
			blkno = ip->i_din2->di_extb[i];
			ip->i_din2->di_extb[i] = 0;
			if (blkno == 0)
				continue;
			frags = sblksize(fs, oldextsize, i);
			frags = numfrags(fs, frags);
			newfreework(ip->i_ump, freeblks, NULL, -1 - i, blkno,
			    frags, needj);
		}
	}
	if (LIST_EMPTY(&freeblks->fb_jfreeblkhd))
		needj = 0;
	DIP_SET(ip, i_blocks, DIP(ip, i_blocks) - freeblks->fb_chkcnt);
	/*
	 * Push the zero'ed inode to to its disk buffer so that we are free
	 * to delete its dependencies below. Once the dependencies are gone
	 * the buffer can be safely released.
	 */
	if ((error = bread(ip->i_devvp,
	    fsbtodb(fs, ino_to_fsba(fs, ip->i_number)),
	    (int)fs->fs_bsize, NOCRED, &bp)) != 0) {
		brelse(bp);
		softdep_error("softdep_setup_freeblocks", error);
	}
	if (ip->i_ump->um_fstype == UFS1) {
		dp1 = ((struct ufs1_dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number));
		ip->i_din1->di_freelink = dp1->di_freelink;
		*dp1 = *ip->i_din1;
	} else {
		dp2 = ((struct ufs2_dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number));
		ip->i_din2->di_freelink = dp2->di_freelink;
		*dp2 = *ip->i_din2;
	}
	/*
	 * Find and eliminate any inode dependencies.
	 */
	ACQUIRE_LOCK(&lk);
	(void) inodedep_lookup(mp, ip->i_number, DEPALLOC, &inodedep);
	if ((inodedep->id_state & IOSTARTED) != 0)
		panic("softdep_setup_freeblocks: inode busy");
	/*
	 * Add the freeblks structure to the list of operations that
	 * must await the zero'ed inode being written to disk. If we
	 * still have a bitmap dependency (delay == 0), then the inode
	 * has never been written to disk, so we can process the
	 * freeblks below once we have deleted the dependencies.
	 */
	delay = (inodedep->id_state & DEPCOMPLETE);
	if (delay)
		WORKLIST_INSERT(&bp->b_dep, &freeblks->fb_list);
	else if (needj)
		freeblks->fb_state |= COMPLETE;
	/*
	 * Because the file length has been truncated to zero, any
	 * pending block allocation dependency structures associated
	 * with this inode are obsolete and can simply be de-allocated.
	 * We must first merge the two dependency lists to get rid of
	 * any duplicate freefrag structures, then purge the merged list.
	 * If we still have a bitmap dependency, then the inode has never
	 * been written to disk, so we can free any fragments without delay.
	 */
	if (flags & IO_NORMAL) {
		merge_inode_lists(&inodedep->id_newinoupdt,
		    &inodedep->id_inoupdt);
		while ((adp = TAILQ_FIRST(&inodedep->id_inoupdt)) != 0)
			cancel_allocdirect(&inodedep->id_inoupdt, adp,
			    freeblks, delay);
	}
	if (flags & IO_EXT) {
		merge_inode_lists(&inodedep->id_newextupdt,
		    &inodedep->id_extupdt);
		while ((adp = TAILQ_FIRST(&inodedep->id_extupdt)) != 0)
			cancel_allocdirect(&inodedep->id_extupdt, adp,
			    freeblks, delay);
	}
	LIST_FOREACH(jfreeblk, &freeblks->fb_jfreeblkhd, jf_deps)
		add_to_journal(&jfreeblk->jf_list);

	FREE_LOCK(&lk);
	bdwrite(bp);
	/*
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 * Once they are all there, walk the list and get rid of
	 * any dependencies.
	 */
	vp = ITOV(ip);
	bo = &vp->v_bufobj;
	BO_LOCK(bo);
	drain_output(vp);
restart:
	TAILQ_FOREACH(bp, &bo->bo_dirty.bv_hd, b_bobufs) {
		if (((flags & IO_EXT) == 0 && (bp->b_xflags & BX_ALTDATA)) ||
		    ((flags & IO_NORMAL) == 0 &&
		      (bp->b_xflags & BX_ALTDATA) == 0))
			continue;
		if ((bp = getdirtybuf(bp, BO_MTX(bo), MNT_WAIT)) == NULL)
			goto restart;
		BO_UNLOCK(bo);
		ACQUIRE_LOCK(&lk);
		(void) inodedep_lookup(mp, ip->i_number, 0, &inodedep);
		if (deallocate_dependencies(bp, inodedep, freeblks))
			bp->b_flags |= B_INVAL | B_NOCACHE;
		FREE_LOCK(&lk);
		brelse(bp);
		BO_LOCK(bo);
		goto restart;
	}
	BO_UNLOCK(bo);
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) != 0)
		(void) free_inodedep(inodedep);

	if (delay || needj)
		freeblks->fb_state |= DEPCOMPLETE;
	if (delay) {
		/*
		 * If the inode with zeroed block pointers is now on disk
		 * we can start freeing blocks. Add freeblks to the worklist
		 * instead of calling  handle_workitem_freeblocks directly as
		 * it is more likely that additional IO is needed to complete
		 * the request here than in the !delay case.
		 */  
		if ((freeblks->fb_state & ALLCOMPLETE) == ALLCOMPLETE)
			add_to_worklist(&freeblks->fb_list, 1);
	}
	if (needj && LIST_EMPTY(&freeblks->fb_jfreeblkhd))
		needj = 0;

	FREE_LOCK(&lk);
	/*
	 * If the inode has never been written to disk (delay == 0) and
	 * we're not waiting on any journal writes, then we can process the
	 * freeblks now that we have deleted the dependencies.
	 */
	if (!delay && !needj)
		handle_workitem_freeblocks(freeblks, 0);
}

/*
 * Reclaim any dependency structures from a buffer that is about to
 * be reallocated to a new vnode. The buffer must be locked, thus,
 * no I/O completion operations can occur while we are manipulating
 * its associated dependencies. The mutex is held so that other I/O's
 * associated with related dependencies do not occur.  Returns 1 if
 * all dependencies were cleared, 0 otherwise.
 */
static int
deallocate_dependencies(bp, inodedep, freeblks)
	struct buf *bp;
	struct inodedep *inodedep;
	struct freeblks *freeblks;
{
	struct worklist *wk;
	struct indirdep *indirdep;
	struct newdirblk *newdirblk;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct jremref *jremref;
	struct jmvref *jmvref;
	struct dirrem *dirrem;
	int i;

	mtx_assert(&lk, MA_OWNED);
	while ((wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		switch (wk->wk_type) {

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);
			if (bp->b_lblkno >= 0 ||
			    bp->b_blkno != indirdep->ir_savebp->b_lblkno)
				panic("deallocate_dependencies: not indir");
			cancel_indirdep(indirdep, bp, inodedep, freeblks);
			continue;

		case D_PAGEDEP:
			pagedep = WK_PAGEDEP(wk);
			/*
			 * There should be no directory add dependencies present
			 * as the directory could not be truncated until all
			 * children were removed.
			 */
			KASSERT(LIST_FIRST(&pagedep->pd_pendinghd) == NULL,
			    ("deallocate_dependencies: pendinghd != NULL"));
			for (i = 0; i < DAHASHSZ; i++)
				KASSERT(LIST_FIRST(&pagedep->pd_diraddhd[i]) == NULL,
				    ("deallocate_dependencies: diraddhd != NULL"));
			/*
			 * Copy any directory remove dependencies to the list
			 * to be processed after the zero'ed inode is written.
			 * If the inode has already been written, then they 
			 * can be dumped directly onto the work list.
			 */
			LIST_FOREACH(dirrem, &pagedep->pd_dirremhd, dm_next) {
				/*
				 * If there are any dirrems we wait for
				 * the journal write to complete and
				 * then restart the buf scan as the lock
				 * has been dropped.
				 */
				while ((jremref =
				    LIST_FIRST(&dirrem->dm_jremrefhd))
				    != NULL) {
					stat_jwait_filepage++;
					jwait(&jremref->jr_list);
					return (0);
				}
				LIST_REMOVE(dirrem, dm_next);
				dirrem->dm_dirinum = pagedep->pd_ino;
				if (inodedep == NULL ||
				    (inodedep->id_state & ALLCOMPLETE) ==
				     ALLCOMPLETE) {
					dirrem->dm_state |= COMPLETE;
					add_to_worklist(&dirrem->dm_list, 0);
				} else
					WORKLIST_INSERT(&inodedep->id_bufwait,
					    &dirrem->dm_list);
			}
			if ((pagedep->pd_state & NEWBLOCK) != 0) {
				newdirblk = pagedep->pd_newdirblk;
				WORKLIST_REMOVE(&newdirblk->db_list);
				free_newdirblk(newdirblk);
			}
			while ((jmvref = LIST_FIRST(&pagedep->pd_jmvrefhd))
			    != NULL) {
				stat_jwait_filepage++;
				jwait(&jmvref->jm_list);
				return (0);
			}
			WORKLIST_REMOVE(&pagedep->pd_list);
			LIST_REMOVE(pagedep, pd_hash);
			WORKITEM_FREE(pagedep, D_PAGEDEP);
			continue;

		case D_ALLOCINDIR:
			aip = WK_ALLOCINDIR(wk);
			cancel_allocindir(aip, inodedep, freeblks);
			continue;

		case D_ALLOCDIRECT:
		case D_INODEDEP:
			panic("deallocate_dependencies: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */

		default:
			panic("deallocate_dependencies: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}

	return (1);
}

/*
 * An allocdirect is being canceled due to a truncate.  We must make sure
 * the journal entry is released in concert with the blkfree that releases
 * the storage.  Completed journal entries must not be released until the
 * space is no longer pointed to by the inode or in the bitmap.
 */
static void
cancel_allocdirect(adphead, adp, freeblks, delay)
	struct allocdirectlst *adphead;
	struct allocdirect *adp;
	struct freeblks *freeblks;
	int delay;
{
	struct freework *freework;
	struct newblk *newblk;
	struct worklist *wk;
	ufs_lbn_t lbn;

	TAILQ_REMOVE(adphead, adp, ad_next);
	newblk = (struct newblk *)adp;
	/*
	 * If the journal hasn't been written the jnewblk must be passed
	 * to the call to ffs_blkfree that reclaims the space.  We accomplish
	 * this by linking the journal dependency into the freework to be
	 * freed when freework_freeblock() is called.  If the journal has
	 * been written we can simply reclaim the journal space when the
	 * freeblks work is complete.
	 */
	if (newblk->nb_jnewblk == NULL) {
		cancel_newblk(newblk, &freeblks->fb_jwork);
		goto found;
	}
	lbn = newblk->nb_jnewblk->jn_lbn;
	/*
	 * Find the correct freework structure so it releases the canceled
	 * journal when the bitmap is cleared.  This preserves rollback
	 * until the allocation is reverted.
	 */
	LIST_FOREACH(wk, &freeblks->fb_freeworkhd, wk_list) {
		freework = WK_FREEWORK(wk);
		if (freework->fw_lbn != lbn)
			continue;
		cancel_newblk(newblk, &freework->fw_jwork);
		goto found;
	}
	panic("cancel_allocdirect: Freework not found for lbn %jd\n", lbn);
found:
	if (delay)
		WORKLIST_INSERT(&adp->ad_inodedep->id_bufwait,
		    &newblk->nb_list);
	else
		free_newblk(newblk);
	return;
}


static void
cancel_newblk(newblk, wkhd)
	struct newblk *newblk;
	struct workhead *wkhd;
{
	struct indirdep *indirdep;
	struct allocindir *aip;

	while ((indirdep = LIST_FIRST(&newblk->nb_indirdeps)) != NULL) {
		indirdep->ir_state &= ~ONDEPLIST;
		LIST_REMOVE(indirdep, ir_next);
		/*
		 * If an indirdep is not on the buf worklist we need to
		 * free it here as deallocate_dependencies() will never
		 * find it.  These pointers were never visible on disk and
		 * can be discarded immediately.
		 */
		while ((aip = LIST_FIRST(&indirdep->ir_completehd)) != NULL) {
			LIST_REMOVE(aip, ai_next);
			cancel_newblk(&aip->ai_block, wkhd);
			free_newblk(&aip->ai_block);
		}
		/*
		 * If this indirdep is not attached to a buf it was simply
		 * waiting on completion to clear completehd.  free_indirdep()
		 * asserts that nothing is dangling.
		 */
		if ((indirdep->ir_state & ONWORKLIST) == 0)
			free_indirdep(indirdep);
	}
	if (newblk->nb_state & ONDEPLIST) {
		newblk->nb_state &= ~ONDEPLIST;
		LIST_REMOVE(newblk, nb_deps);
	}
	if (newblk->nb_state & ONWORKLIST)
		WORKLIST_REMOVE(&newblk->nb_list);
	/*
	 * If the journal entry hasn't been written we hold onto the dep
	 * until it is safe to free along with the other journal work.
	 */
	if (newblk->nb_jnewblk != NULL) {
		cancel_jnewblk(newblk->nb_jnewblk, wkhd);
		newblk->nb_jnewblk = NULL;
	}
	if (!LIST_EMPTY(&newblk->nb_jwork))
		jwork_move(wkhd, &newblk->nb_jwork);
}

/*
 * Free a newblk. Generate a new freefrag work request if appropriate.
 * This must be called after the inode pointer and any direct block pointers
 * are valid or fully removed via truncate or frag extension.
 */
static void
free_newblk(newblk)
	struct newblk *newblk;
{
	struct indirdep *indirdep;
	struct newdirblk *newdirblk;
	struct freefrag *freefrag;
	struct worklist *wk;

	mtx_assert(&lk, MA_OWNED);
	if (newblk->nb_state & ONDEPLIST)
		LIST_REMOVE(newblk, nb_deps);
	if (newblk->nb_state & ONWORKLIST)
		WORKLIST_REMOVE(&newblk->nb_list);
	LIST_REMOVE(newblk, nb_hash);
	if ((freefrag = newblk->nb_freefrag) != NULL) {
		freefrag->ff_state |= COMPLETE;
		if ((freefrag->ff_state & ALLCOMPLETE) == ALLCOMPLETE)
			add_to_worklist(&freefrag->ff_list, 0);
	}
	if ((wk = LIST_FIRST(&newblk->nb_newdirblk)) != NULL) {
		newdirblk = WK_NEWDIRBLK(wk);
		WORKLIST_REMOVE(&newdirblk->db_list);
		if (!LIST_EMPTY(&newblk->nb_newdirblk))
			panic("free_newblk: extra newdirblk");
		free_newdirblk(newdirblk);
	}
	while ((indirdep = LIST_FIRST(&newblk->nb_indirdeps)) != NULL) {
		indirdep->ir_state |= DEPCOMPLETE;
		indirdep_complete(indirdep);
	}
	KASSERT(newblk->nb_jnewblk == NULL,
	    ("free_newblk; jnewblk %p still attached", newblk->nb_jnewblk));
	handle_jwork(&newblk->nb_jwork);
	newblk->nb_list.wk_type = D_NEWBLK;
	WORKITEM_FREE(newblk, D_NEWBLK);
}

/*
 * Free a newdirblk. Clear the NEWBLOCK flag on its associated pagedep.
 * This routine must be called with splbio interrupts blocked.
 */
static void
free_newdirblk(newdirblk)
	struct newdirblk *newdirblk;
{
	struct pagedep *pagedep;
	struct diradd *dap;
	struct worklist *wk;
	int i;

	mtx_assert(&lk, MA_OWNED);
	/*
	 * If the pagedep is still linked onto the directory buffer
	 * dependency chain, then some of the entries on the
	 * pd_pendinghd list may not be committed to disk yet. In
	 * this case, we will simply clear the NEWBLOCK flag and
	 * let the pd_pendinghd list be processed when the pagedep
	 * is next written. If the pagedep is no longer on the buffer
	 * dependency chain, then all the entries on the pd_pending
	 * list are committed to disk and we can free them here.
	 */
	pagedep = newdirblk->db_pagedep;
	pagedep->pd_state &= ~NEWBLOCK;
	if ((pagedep->pd_state & ONWORKLIST) == 0)
		while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != NULL)
			free_diradd(dap, NULL);
	/*
	 * If no dependencies remain, the pagedep will be freed.
	 */
	for (i = 0; i < DAHASHSZ; i++)
		if (!LIST_EMPTY(&pagedep->pd_diraddhd[i]))
			break;
	if (i == DAHASHSZ && (pagedep->pd_state & ONWORKLIST) == 0 &&
	    LIST_EMPTY(&pagedep->pd_jmvrefhd)) {
		KASSERT(LIST_FIRST(&pagedep->pd_dirremhd) == NULL,
		    ("free_newdirblk: Freeing non-free pagedep %p", pagedep));
		LIST_REMOVE(pagedep, pd_hash);
		WORKITEM_FREE(pagedep, D_PAGEDEP);
	}
	/* Should only ever be one item in the list. */
	while ((wk = LIST_FIRST(&newdirblk->db_mkdir)) != NULL) {
		WORKLIST_REMOVE(wk);
		handle_written_mkdir(WK_MKDIR(wk), MKDIR_BODY);
	}
	WORKITEM_FREE(newdirblk, D_NEWDIRBLK);
}

/*
 * Prepare an inode to be freed. The actual free operation is not
 * done until the zero'ed inode has been written to disk.
 */
void
softdep_freefile(pvp, ino, mode)
	struct vnode *pvp;
	ino_t ino;
	int mode;
{
	struct inode *ip = VTOI(pvp);
	struct inodedep *inodedep;
	struct freefile *freefile;

	/*
	 * This sets up the inode de-allocation dependency.
	 */
	freefile = malloc(sizeof(struct freefile),
		M_FREEFILE, M_SOFTDEP_FLAGS);
	workitem_alloc(&freefile->fx_list, D_FREEFILE, pvp->v_mount);
	freefile->fx_mode = mode;
	freefile->fx_oldinum = ino;
	freefile->fx_devvp = ip->i_devvp;
	LIST_INIT(&freefile->fx_jwork);
	UFS_LOCK(ip->i_ump);
	ip->i_fs->fs_pendinginodes += 1;
	UFS_UNLOCK(ip->i_ump);

	/*
	 * If the inodedep does not exist, then the zero'ed inode has
	 * been written to disk. If the allocated inode has never been
	 * written to disk, then the on-disk inode is zero'ed. In either
	 * case we can free the file immediately.  If the journal was
	 * canceled before being written the inode will never make it to
	 * disk and we must send the canceled journal entrys to
	 * ffs_freefile() to be cleared in conjunction with the bitmap.
	 * Any blocks waiting on the inode to write can be safely freed
	 * here as it will never been written.
	 */
	ACQUIRE_LOCK(&lk);
	inodedep_lookup(pvp->v_mount, ino, 0, &inodedep);
	/*
	 * Remove this inode from the unlinked list and set
	 * GOINGAWAY as appropriate to indicate that this inode
	 * will never be written.
	 */
	if (inodedep && inodedep->id_state & UNLINKED) {
		/*
		 * Save the journal work to be freed with the bitmap
		 * before we clear UNLINKED.  Otherwise it can be lost
		 * if the inode block is written.
		 */
		handle_bufwait(inodedep, &freefile->fx_jwork);
		clear_unlinked_inodedep(inodedep);
		/* Re-acquire inodedep as we've dropped lk. */
		inodedep_lookup(pvp->v_mount, ino, 0, &inodedep);
		if (inodedep && (inodedep->id_state & DEPCOMPLETE) == 0)
			inodedep->id_state |= GOINGAWAY;
	}
	if (inodedep == NULL || check_inode_unwritten(inodedep)) {
		FREE_LOCK(&lk);
		handle_workitem_freefile(freefile);
		return;
	}
	WORKLIST_INSERT(&inodedep->id_inowait, &freefile->fx_list);
	FREE_LOCK(&lk);
	if (ip->i_number == ino)
		ip->i_flag |= IN_MODIFIED;
}

/*
 * Check to see if an inode has never been written to disk. If
 * so free the inodedep and return success, otherwise return failure.
 * This routine must be called with splbio interrupts blocked.
 *
 * If we still have a bitmap dependency, then the inode has never
 * been written to disk. Drop the dependency as it is no longer
 * necessary since the inode is being deallocated. We set the
 * ALLCOMPLETE flags since the bitmap now properly shows that the
 * inode is not allocated. Even if the inode is actively being
 * written, it has been rolled back to its zero'ed state, so we
 * are ensured that a zero inode is what is on the disk. For short
 * lived files, this change will usually result in removing all the
 * dependencies from the inode so that it can be freed immediately.
 */
static int
check_inode_unwritten(inodedep)
	struct inodedep *inodedep;
{

	mtx_assert(&lk, MA_OWNED);

	if ((inodedep->id_state & (DEPCOMPLETE | UNLINKED)) != 0 ||
	    !LIST_EMPTY(&inodedep->id_pendinghd) ||
	    !LIST_EMPTY(&inodedep->id_bufwait) ||
	    !LIST_EMPTY(&inodedep->id_inowait) ||
	    !TAILQ_EMPTY(&inodedep->id_inoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newinoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_extupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newextupdt) ||
	    inodedep->id_mkdiradd != NULL || 
	    inodedep->id_nlinkdelta != 0)
		return (0);
	/*
	 * Another process might be in initiate_write_inodeblock_ufs[12]
	 * trying to allocate memory without holding "Softdep Lock".
	 */
	if ((inodedep->id_state & IOSTARTED) != 0 &&
	    inodedep->id_savedino1 == NULL)
		return (0);

	if (inodedep->id_state & ONDEPLIST)
		LIST_REMOVE(inodedep, id_deps);
	inodedep->id_state &= ~ONDEPLIST;
	inodedep->id_state |= ALLCOMPLETE;
	inodedep->id_bmsafemap = NULL;
	if (inodedep->id_state & ONWORKLIST)
		WORKLIST_REMOVE(&inodedep->id_list);
	if (inodedep->id_savedino1 != NULL) {
		free(inodedep->id_savedino1, M_SAVEDINO);
		inodedep->id_savedino1 = NULL;
	}
	if (free_inodedep(inodedep) == 0)
		panic("check_inode_unwritten: busy inode");
	return (1);
}

/*
 * Try to free an inodedep structure. Return 1 if it could be freed.
 */
static int
free_inodedep(inodedep)
	struct inodedep *inodedep;
{

	mtx_assert(&lk, MA_OWNED);
	if ((inodedep->id_state & (ONWORKLIST | UNLINKED)) != 0 ||
	    (inodedep->id_state & ALLCOMPLETE) != ALLCOMPLETE ||
	    !LIST_EMPTY(&inodedep->id_dirremhd) ||
	    !LIST_EMPTY(&inodedep->id_pendinghd) ||
	    !LIST_EMPTY(&inodedep->id_bufwait) ||
	    !LIST_EMPTY(&inodedep->id_inowait) ||
	    !TAILQ_EMPTY(&inodedep->id_inoreflst) ||
	    !TAILQ_EMPTY(&inodedep->id_inoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newinoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_extupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newextupdt) ||
	    inodedep->id_mkdiradd != NULL ||
	    inodedep->id_nlinkdelta != 0 ||
	    inodedep->id_savedino1 != NULL)
		return (0);
	if (inodedep->id_state & ONDEPLIST)
		LIST_REMOVE(inodedep, id_deps);
	LIST_REMOVE(inodedep, id_hash);
	WORKITEM_FREE(inodedep, D_INODEDEP);
	num_inodedep -= 1;
	return (1);
}

/*
 * Free the block referenced by a freework structure.  The parent freeblks
 * structure is released and completed when the final cg bitmap reaches
 * the disk.  This routine may be freeing a jnewblk which never made it to
 * disk in which case we do not have to wait as the operation is undone
 * in memory immediately.
 */
static void
freework_freeblock(freework)
	struct freework *freework;
{
	struct freeblks *freeblks;
	struct ufsmount *ump;
	struct workhead wkhd;
	struct fs *fs;
	int complete;
	int pending;
	int bsize;
	int needj;

	freeblks = freework->fw_freeblks;
	ump = VFSTOUFS(freeblks->fb_list.wk_mp);
	fs = ump->um_fs;
	needj = freeblks->fb_list.wk_mp->mnt_kern_flag & MNTK_SUJ;
	complete = 0;
	LIST_INIT(&wkhd);
	/*
	 * If we are canceling an existing jnewblk pass it to the free
	 * routine, otherwise pass the freeblk which will ultimately
	 * release the freeblks.  If we're not journaling, we can just
	 * free the freeblks immediately.
	 */
	if (!LIST_EMPTY(&freework->fw_jwork)) {
		LIST_SWAP(&wkhd, &freework->fw_jwork, worklist, wk_list);
		complete = 1;
	} else if (needj)
		WORKLIST_INSERT_UNLOCKED(&wkhd, &freework->fw_list);
	bsize = lfragtosize(fs, freework->fw_frags);
	pending = btodb(bsize);
	ACQUIRE_LOCK(&lk);
	freeblks->fb_chkcnt -= pending;
	FREE_LOCK(&lk);
	/*
	 * extattr blocks don't show up in pending blocks.  XXX why?
	 */
	if (freework->fw_lbn >= 0 || freework->fw_lbn <= -NDADDR) {
		UFS_LOCK(ump);
		fs->fs_pendingblocks -= pending;
		UFS_UNLOCK(ump);
	}
	ffs_blkfree(ump, fs, freeblks->fb_devvp, freework->fw_blkno,
	    bsize, freeblks->fb_previousinum, &wkhd);
	if (complete == 0 && needj)
		return;
	/*
	 * The jnewblk will be discarded and the bits in the map never
	 * made it to disk.  We can immediately free the freeblk.
	 */
	ACQUIRE_LOCK(&lk);
	handle_written_freework(freework);
	FREE_LOCK(&lk);
}

/*
 * Start, continue, or finish the process of freeing an indirect block tree.
 * The free operation may be paused at any point with fw_off containing the
 * offset to restart from.  This enables us to implement some flow control
 * for large truncates which may fan out and generate a huge number of
 * dependencies.
 */
static void
handle_workitem_indirblk(freework)
	struct freework *freework;
{
	struct freeblks *freeblks;
	struct ufsmount *ump;
	struct fs *fs;


	freeblks = freework->fw_freeblks;
	ump = VFSTOUFS(freeblks->fb_list.wk_mp);
	fs = ump->um_fs;
	if (freework->fw_off == NINDIR(fs))
		freework_freeblock(freework);
	else
		indir_trunc(freework, fsbtodb(fs, freework->fw_blkno),
		    freework->fw_lbn);
}

/*
 * Called when a freework structure attached to a cg buf is written.  The
 * ref on either the parent or the freeblks structure is released and
 * either may be added to the worklist if it is the final ref.
 */
static void
handle_written_freework(freework)
	struct freework *freework;
{
	struct freeblks *freeblks;
	struct freework *parent;

	freeblks = freework->fw_freeblks;
	parent = freework->fw_parent;
	if (parent) {
		if (--parent->fw_ref != 0)
			parent = NULL;
		freeblks = NULL;
	} else if (--freeblks->fb_ref != 0)
		freeblks = NULL;
	WORKITEM_FREE(freework, D_FREEWORK);
	/*
	 * Don't delay these block frees or it takes an intolerable amount
	 * of time to process truncates and free their journal entries.
	 */
	if (freeblks)
		add_to_worklist(&freeblks->fb_list, 1);
	if (parent)
		add_to_worklist(&parent->fw_list, 1);
}

/*
 * This workitem routine performs the block de-allocation.
 * The workitem is added to the pending list after the updated
 * inode block has been written to disk.  As mentioned above,
 * checks regarding the number of blocks de-allocated (compared
 * to the number of blocks allocated for the file) are also
 * performed in this function.
 */
static void
handle_workitem_freeblocks(freeblks, flags)
	struct freeblks *freeblks;
	int flags;
{
	struct freework *freework;
	struct worklist *wk;

	KASSERT(LIST_EMPTY(&freeblks->fb_jfreeblkhd),
	    ("handle_workitem_freeblocks: Journal entries not written."));
	if (LIST_EMPTY(&freeblks->fb_freeworkhd)) {
		handle_complete_freeblocks(freeblks);
		return;
	}
	freeblks->fb_ref++;
	while ((wk = LIST_FIRST(&freeblks->fb_freeworkhd)) != NULL) {
		KASSERT(wk->wk_type == D_FREEWORK,
		    ("handle_workitem_freeblocks: Unknown type %s",
		    TYPENAME(wk->wk_type)));
		WORKLIST_REMOVE_UNLOCKED(wk);
		freework = WK_FREEWORK(wk);
		if (freework->fw_lbn <= -NDADDR)
			handle_workitem_indirblk(freework);
		else
			freework_freeblock(freework);
	}
	ACQUIRE_LOCK(&lk);
	if (--freeblks->fb_ref != 0)
		freeblks = NULL;
	FREE_LOCK(&lk);
	if (freeblks)
		handle_complete_freeblocks(freeblks);
}

/*
 * Once all of the freework workitems are complete we can retire the
 * freeblocks dependency and any journal work awaiting completion.  This
 * can not be called until all other dependencies are stable on disk.
 */
static void
handle_complete_freeblocks(freeblks)
	struct freeblks *freeblks;
{
	struct inode *ip;
	struct vnode *vp;
	struct fs *fs;
	struct ufsmount *ump;
	int flags;

	ump = VFSTOUFS(freeblks->fb_list.wk_mp);
	fs = ump->um_fs;
	flags = LK_NOWAIT;

	/*
	 * If we still have not finished background cleanup, then check
	 * to see if the block count needs to be adjusted.
	 */
	if (freeblks->fb_chkcnt != 0 && (fs->fs_flags & FS_UNCLEAN) != 0 &&
	    ffs_vgetf(freeblks->fb_list.wk_mp, freeblks->fb_previousinum,
	    (flags & LK_NOWAIT) | LK_EXCLUSIVE, &vp, FFSV_FORCEINSMQ) == 0) {
		ip = VTOI(vp);
		DIP_SET(ip, i_blocks, DIP(ip, i_blocks) + freeblks->fb_chkcnt);
		ip->i_flag |= IN_CHANGE;
		vput(vp);
	}

	if (!(freeblks->fb_chkcnt == 0 ||
	    ((fs->fs_flags & FS_UNCLEAN) != 0 && (flags & LK_NOWAIT) == 0)))
	        printf(
	"handle_workitem_freeblocks: inode %ju block count %jd\n",
		   (uintmax_t)freeblks->fb_previousinum,
		   (intmax_t)freeblks->fb_chkcnt);

	ACQUIRE_LOCK(&lk);
	/*
	 * All of the freeblock deps must be complete prior to this call
	 * so it's now safe to complete earlier outstanding journal entries.
	 */
	handle_jwork(&freeblks->fb_jwork);
	WORKITEM_FREE(freeblks, D_FREEBLKS);
	num_freeblkdep--;
	FREE_LOCK(&lk);
}

/*
 * Release blocks associated with the inode ip and stored in the indirect
 * block dbn. If level is greater than SINGLE, the block is an indirect block
 * and recursive calls to indirtrunc must be used to cleanse other indirect
 * blocks.
 */
static void
indir_trunc(freework, dbn, lbn)
	struct freework *freework;
	ufs2_daddr_t dbn;
	ufs_lbn_t lbn;
{
	struct freework *nfreework;
	struct workhead wkhd;
	struct jnewblk *jnewblk;
	struct freeblks *freeblks;
	struct buf *bp;
	struct fs *fs;
	struct worklist *wkn;
	struct worklist *wk;
	struct indirdep *indirdep;
	struct ufsmount *ump;
	ufs1_daddr_t *bap1 = 0;
	ufs2_daddr_t nb, nnb, *bap2 = 0;
	ufs_lbn_t lbnadd;
	int i, nblocks, ufs1fmt;
	int fs_pendingblocks;
	int freedeps;
	int needj;
	int level;
	int cnt;

	LIST_INIT(&wkhd);
	level = lbn_level(lbn);
	if (level == -1)
		panic("indir_trunc: Invalid lbn %jd\n", lbn);
	freeblks = freework->fw_freeblks;
	ump = VFSTOUFS(freeblks->fb_list.wk_mp);
	fs = ump->um_fs;
	fs_pendingblocks = 0;
	freedeps = 0;
	needj = UFSTOVFS(ump)->mnt_kern_flag & MNTK_SUJ;
	lbnadd = lbn_offset(fs, level);
	/*
	 * Get buffer of block pointers to be freed. This routine is not
	 * called until the zero'ed inode has been written, so it is safe
	 * to free blocks as they are encountered. Because the inode has
	 * been zero'ed, calls to bmap on these blocks will fail. So, we
	 * have to use the on-disk address and the block device for the
	 * filesystem to look them up. If the file was deleted before its
	 * indirect blocks were all written to disk, the routine that set
	 * us up (deallocate_dependencies) will have arranged to leave
	 * a complete copy of the indirect block in memory for our use.
	 * Otherwise we have to read the blocks in from the disk.
	 */
#ifdef notyet
	bp = getblk(freeblks->fb_devvp, dbn, (int)fs->fs_bsize, 0, 0,
	    GB_NOCREAT);
#else
	bp = incore(&freeblks->fb_devvp->v_bufobj, dbn);
#endif
	ACQUIRE_LOCK(&lk);
	if (bp != NULL && (wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		if (wk->wk_type != D_INDIRDEP ||
		    (wk->wk_state & GOINGAWAY) == 0)
			panic("indir_trunc: lost indirdep %p", wk);
		indirdep = WK_INDIRDEP(wk);
		LIST_SWAP(&wkhd, &indirdep->ir_jwork, worklist, wk_list);
		free_indirdep(indirdep);
		if (!LIST_EMPTY(&bp->b_dep))
			panic("indir_trunc: dangling dep %p",
			    LIST_FIRST(&bp->b_dep));
		ump->um_numindirdeps -= 1;
		FREE_LOCK(&lk);
	} else {
#ifdef notyet
		if (bp)
			brelse(bp);
#endif
		FREE_LOCK(&lk);
		if (bread(freeblks->fb_devvp, dbn, (int)fs->fs_bsize,
		    NOCRED, &bp) != 0) {
			brelse(bp);
			return;
		}
	}
	/*
	 * Recursively free indirect blocks.
	 */
	if (ump->um_fstype == UFS1) {
		ufs1fmt = 1;
		bap1 = (ufs1_daddr_t *)bp->b_data;
	} else {
		ufs1fmt = 0;
		bap2 = (ufs2_daddr_t *)bp->b_data;
	}

	/*
	 * Reclaim indirect blocks which never made it to disk.
	 */
	cnt = 0;
	LIST_FOREACH_SAFE(wk, &wkhd, wk_list, wkn) {
		if (wk->wk_type != D_JNEWBLK)
			continue;
		ACQUIRE_LOCK(&lk);
		WORKLIST_REMOVE(wk);
		FREE_LOCK(&lk);
		jnewblk = WK_JNEWBLK(wk);
		if (jnewblk->jn_lbn > 0)
			i = (jnewblk->jn_lbn - -lbn) / lbnadd;
		else
			i = (-(jnewblk->jn_lbn + level - 1) - -(lbn + level)) /
			    lbnadd;
		KASSERT(i >= 0 && i < NINDIR(fs),
		    ("indir_trunc: Index out of range %d parent %jd lbn %jd level %d",
		    i, lbn, jnewblk->jn_lbn, level));
		/* Clear the pointer so it isn't found below. */
		if (ufs1fmt) {
			nb = bap1[i];
			bap1[i] = 0;
		} else {
			nb = bap2[i];
			bap2[i] = 0;
		}
		KASSERT(nb == jnewblk->jn_blkno,
		    ("indir_trunc: Block mismatch %jd != %jd",
		    nb, jnewblk->jn_blkno));
		if (level != 0) {
			ufs_lbn_t nlbn;

			nlbn = (lbn + 1) - (i * lbnadd);
			nfreework = newfreework(ump, freeblks, freework,
			    nlbn, nb, fs->fs_frag, 0);
			WORKLIST_INSERT_UNLOCKED(&nfreework->fw_jwork, wk);
			freedeps++;
			indir_trunc(nfreework, fsbtodb(fs, nb), nlbn);
		} else {
			struct workhead freewk;

			LIST_INIT(&freewk);
			ACQUIRE_LOCK(&lk);
			WORKLIST_INSERT(&freewk, wk);
			FREE_LOCK(&lk);
			ffs_blkfree(ump, fs, freeblks->fb_devvp,
			    jnewblk->jn_blkno, fs->fs_bsize,
			    freeblks->fb_previousinum, &freewk);
		}
		cnt++;
	}
	ACQUIRE_LOCK(&lk);
	/* Any remaining journal work can be completed with freeblks. */
	jwork_move(&freeblks->fb_jwork, &wkhd);
	FREE_LOCK(&lk);
	nblocks = btodb(fs->fs_bsize);
	if (ufs1fmt)
		nb = bap1[0];
	else
		nb = bap2[0];
	nfreework = freework;
	/*
	 * Reclaim on disk blocks.
	 */
	for (i = freework->fw_off; i < NINDIR(fs); i++, nb = nnb) {
		if (i != NINDIR(fs) - 1) {
			if (ufs1fmt)
				nnb = bap1[i+1];
			else
				nnb = bap2[i+1];
		} else
			nnb = 0;
		if (nb == 0)
			continue;
		cnt++;
		if (level != 0) {
			ufs_lbn_t nlbn;

			nlbn = (lbn + 1) - (i * lbnadd);
			if (needj != 0) {
				nfreework = newfreework(ump, freeblks, freework,
				    nlbn, nb, fs->fs_frag, 0);
				freedeps++;
			}
			indir_trunc(nfreework, fsbtodb(fs, nb), nlbn);
		} else {
			struct freedep *freedep;

			/*
			 * Attempt to aggregate freedep dependencies for
			 * all blocks being released to the same CG.
			 */
			LIST_INIT(&wkhd);
			if (needj != 0 &&
			    (nnb == 0 || (dtog(fs, nb) != dtog(fs, nnb)))) {
				freedep = newfreedep(freework);
				WORKLIST_INSERT_UNLOCKED(&wkhd,
				    &freedep->fd_list);
				freedeps++;
			}
			ffs_blkfree(ump, fs, freeblks->fb_devvp, nb,
			    fs->fs_bsize, freeblks->fb_previousinum, &wkhd);
		}
	}
	if (level == 0)
		fs_pendingblocks = (nblocks * cnt);
	/*
	 * If we're not journaling we can free the indirect now.  Otherwise
	 * setup the ref counts and offset so this indirect can be completed
	 * when its children are free.
	 */
	if (needj == 0) {
		fs_pendingblocks += nblocks;
		dbn = dbtofsb(fs, dbn);
		ffs_blkfree(ump, fs, freeblks->fb_devvp, dbn, fs->fs_bsize,
		    freeblks->fb_previousinum, NULL);
		ACQUIRE_LOCK(&lk);
		freeblks->fb_chkcnt -= fs_pendingblocks;
		if (freework->fw_blkno == dbn)
			handle_written_freework(freework);
		FREE_LOCK(&lk);
		freework = NULL;
	} else {
		ACQUIRE_LOCK(&lk);
		freework->fw_off = i;
		freework->fw_ref += freedeps;
		freework->fw_ref -= NINDIR(fs) + 1;
		if (freework->fw_ref != 0)
			freework = NULL;
		freeblks->fb_chkcnt -= fs_pendingblocks;
		FREE_LOCK(&lk);
	}
	if (fs_pendingblocks) {
		UFS_LOCK(ump);
		fs->fs_pendingblocks -= fs_pendingblocks;
		UFS_UNLOCK(ump);
	}
	bp->b_flags |= B_INVAL | B_NOCACHE;
	brelse(bp);
	if (freework)
		handle_workitem_indirblk(freework);
	return;
}

/*
 * Cancel an allocindir when it is removed via truncation.
 */
static void
cancel_allocindir(aip, inodedep, freeblks)
	struct allocindir *aip;
	struct inodedep *inodedep;
	struct freeblks *freeblks;
{
	struct newblk *newblk;

	/*
	 * If the journal hasn't been written the jnewblk must be passed
	 * to the call to ffs_blkfree that reclaims the space.  We accomplish
	 * this by linking the journal dependency into the indirdep to be
	 * freed when indir_trunc() is called.  If the journal has already
	 * been written we can simply reclaim the journal space when the
	 * freeblks work is complete.
	 */
	LIST_REMOVE(aip, ai_next);
	newblk = (struct newblk *)aip;
	if (newblk->nb_jnewblk == NULL)
		cancel_newblk(newblk, &freeblks->fb_jwork);
	else
		cancel_newblk(newblk, &aip->ai_indirdep->ir_jwork);
	if (inodedep && inodedep->id_state & DEPCOMPLETE)
		WORKLIST_INSERT(&inodedep->id_bufwait, &newblk->nb_list);
	else
		free_newblk(newblk);
}

/*
 * Create the mkdir dependencies for . and .. in a new directory.  Link them
 * in to a newdirblk so any subsequent additions are tracked properly.  The
 * caller is responsible for adding the mkdir1 dependency to the journal
 * and updating id_mkdiradd.  This function returns with lk held.
 */
static struct mkdir *
setup_newdir(dap, newinum, dinum, newdirbp, mkdirp)
	struct diradd *dap;
	ino_t newinum;
	ino_t dinum;
	struct buf *newdirbp;
	struct mkdir **mkdirp;
{
	struct newblk *newblk;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct newdirblk *newdirblk = 0;
	struct mkdir *mkdir1, *mkdir2;
	struct worklist *wk;
	struct jaddref *jaddref;
	struct mount *mp;

	mp = dap->da_list.wk_mp;
	newdirblk = malloc(sizeof(struct newdirblk), M_NEWDIRBLK,
	    M_SOFTDEP_FLAGS);
	workitem_alloc(&newdirblk->db_list, D_NEWDIRBLK, mp);
	LIST_INIT(&newdirblk->db_mkdir);
	mkdir1 = malloc(sizeof(struct mkdir), M_MKDIR, M_SOFTDEP_FLAGS);
	workitem_alloc(&mkdir1->md_list, D_MKDIR, mp);
	mkdir1->md_state = ATTACHED | MKDIR_BODY;
	mkdir1->md_diradd = dap;
	mkdir1->md_jaddref = NULL;
	mkdir2 = malloc(sizeof(struct mkdir), M_MKDIR, M_SOFTDEP_FLAGS);
	workitem_alloc(&mkdir2->md_list, D_MKDIR, mp);
	mkdir2->md_state = ATTACHED | MKDIR_PARENT;
	mkdir2->md_diradd = dap;
	mkdir2->md_jaddref = NULL;
	if ((mp->mnt_kern_flag & MNTK_SUJ) == 0) {
		mkdir1->md_state |= DEPCOMPLETE;
		mkdir2->md_state |= DEPCOMPLETE;
	}
	/*
	 * Dependency on "." and ".." being written to disk.
	 */
	mkdir1->md_buf = newdirbp;
	ACQUIRE_LOCK(&lk);
	LIST_INSERT_HEAD(&mkdirlisthd, mkdir1, md_mkdirs);
	/*
	 * We must link the pagedep, allocdirect, and newdirblk for
	 * the initial file page so the pointer to the new directory
	 * is not written until the directory contents are live and
	 * any subsequent additions are not marked live until the
	 * block is reachable via the inode.
	 */
	if (pagedep_lookup(mp, newinum, 0, 0, &pagedep) == 0)
		panic("setup_newdir: lost pagedep");
	LIST_FOREACH(wk, &newdirbp->b_dep, wk_list)
		if (wk->wk_type == D_ALLOCDIRECT)
			break;
	if (wk == NULL)
		panic("setup_newdir: lost allocdirect");
	newblk = WK_NEWBLK(wk);
	pagedep->pd_state |= NEWBLOCK;
	pagedep->pd_newdirblk = newdirblk;
	newdirblk->db_pagedep = pagedep;
	WORKLIST_INSERT(&newblk->nb_newdirblk, &newdirblk->db_list);
	WORKLIST_INSERT(&newdirblk->db_mkdir, &mkdir1->md_list);
	/*
	 * Look up the inodedep for the parent directory so that we
	 * can link mkdir2 into the pending dotdot jaddref or
	 * the inode write if there is none.  If the inode is
	 * ALLCOMPLETE and no jaddref is present all dependencies have
	 * been satisfied and mkdir2 can be freed.
	 */
	inodedep_lookup(mp, dinum, 0, &inodedep);
	if (mp->mnt_kern_flag & MNTK_SUJ) {
		if (inodedep == NULL)
			panic("setup_newdir: Lost parent.");
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref != NULL && jaddref->ja_parent == newinum &&
		    (jaddref->ja_state & MKDIR_PARENT),
		    ("setup_newdir: bad dotdot jaddref %p", jaddref));
		LIST_INSERT_HEAD(&mkdirlisthd, mkdir2, md_mkdirs);
		mkdir2->md_jaddref = jaddref;
		jaddref->ja_mkdir = mkdir2;
	} else if (inodedep == NULL ||
	    (inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE) {
		dap->da_state &= ~MKDIR_PARENT;
		WORKITEM_FREE(mkdir2, D_MKDIR);
	} else {
		LIST_INSERT_HEAD(&mkdirlisthd, mkdir2, md_mkdirs);
		WORKLIST_INSERT(&inodedep->id_bufwait,&mkdir2->md_list);
	}
	*mkdirp = mkdir2;

	return (mkdir1);
}

/*
 * Directory entry addition dependencies.
 * 
 * When adding a new directory entry, the inode (with its incremented link
 * count) must be written to disk before the directory entry's pointer to it.
 * Also, if the inode is newly allocated, the corresponding freemap must be
 * updated (on disk) before the directory entry's pointer. These requirements
 * are met via undo/redo on the directory entry's pointer, which consists
 * simply of the inode number.
 * 
 * As directory entries are added and deleted, the free space within a
 * directory block can become fragmented.  The ufs filesystem will compact
 * a fragmented directory block to make space for a new entry. When this
 * occurs, the offsets of previously added entries change. Any "diradd"
 * dependency structures corresponding to these entries must be updated with
 * the new offsets.
 */

/*
 * This routine is called after the in-memory inode's link
 * count has been incremented, but before the directory entry's
 * pointer to the inode has been set.
 */
int
softdep_setup_directory_add(bp, dp, diroffset, newinum, newdirbp, isnewblk)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for directory */
	off_t diroffset;	/* offset of new entry in directory */
	ino_t newinum;		/* inode referenced by new directory entry */
	struct buf *newdirbp;	/* non-NULL => contents of new mkdir */
	int isnewblk;		/* entry is in a newly allocated block */
{
	int offset;		/* offset of new entry within directory block */
	ufs_lbn_t lbn;		/* block in directory containing new entry */
	struct fs *fs;
	struct diradd *dap;
	struct newblk *newblk;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct newdirblk *newdirblk = 0;
	struct mkdir *mkdir1, *mkdir2;
	struct jaddref *jaddref;
	struct mount *mp;
	int isindir;

	/*
	 * Whiteouts have no dependencies.
	 */
	if (newinum == WINO) {
		if (newdirbp != NULL)
			bdwrite(newdirbp);
		return (0);
	}
	jaddref = NULL;
	mkdir1 = mkdir2 = NULL;
	mp = UFSTOVFS(dp->i_ump);
	fs = dp->i_fs;
	lbn = lblkno(fs, diroffset);
	offset = blkoff(fs, diroffset);
	dap = malloc(sizeof(struct diradd), M_DIRADD,
		M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&dap->da_list, D_DIRADD, mp);
	dap->da_offset = offset;
	dap->da_newinum = newinum;
	dap->da_state = ATTACHED;
	LIST_INIT(&dap->da_jwork);
	isindir = bp->b_lblkno >= NDADDR;
	if (isnewblk &&
	    (isindir ? blkoff(fs, diroffset) : fragoff(fs, diroffset)) == 0) {
		newdirblk = malloc(sizeof(struct newdirblk),
		    M_NEWDIRBLK, M_SOFTDEP_FLAGS);
		workitem_alloc(&newdirblk->db_list, D_NEWDIRBLK, mp);
		LIST_INIT(&newdirblk->db_mkdir);
	}
	/*
	 * If we're creating a new directory setup the dependencies and set
	 * the dap state to wait for them.  Otherwise it's COMPLETE and
	 * we can move on.
	 */
	if (newdirbp == NULL) {
		dap->da_state |= DEPCOMPLETE;
		ACQUIRE_LOCK(&lk);
	} else {
		dap->da_state |= MKDIR_BODY | MKDIR_PARENT;
		mkdir1 = setup_newdir(dap, newinum, dp->i_number, newdirbp,
		    &mkdir2);
	}
	/*
	 * Link into parent directory pagedep to await its being written.
	 */
	if (pagedep_lookup(mp, dp->i_number, lbn, DEPALLOC, &pagedep) == 0)
		WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
#ifdef DEBUG
	if (diradd_lookup(pagedep, offset) != NULL)
		panic("softdep_setup_directory_add: %p already at off %d\n",
		    diradd_lookup(pagedep, offset), offset);
#endif
	dap->da_pagedep = pagedep;
	LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)], dap,
	    da_pdlist);
	inodedep_lookup(mp, newinum, DEPALLOC, &inodedep);
	/*
	 * If we're journaling, link the diradd into the jaddref so it
	 * may be completed after the journal entry is written.  Otherwise,
	 * link the diradd into its inodedep.  If the inode is not yet
	 * written place it on the bufwait list, otherwise do the post-inode
	 * write processing to put it on the id_pendinghd list.
	 */
	if (mp->mnt_kern_flag & MNTK_SUJ) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref != NULL && jaddref->ja_parent == dp->i_number,
		    ("softdep_setup_directory_add: bad jaddref %p", jaddref));
		jaddref->ja_diroff = diroffset;
		jaddref->ja_diradd = dap;
		add_to_journal(&jaddref->ja_list);
	} else if ((inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE)
		diradd_inode_written(dap, inodedep);
	else
		WORKLIST_INSERT(&inodedep->id_bufwait, &dap->da_list);
	/*
	 * Add the journal entries for . and .. links now that the primary
	 * link is written.
	 */
	if (mkdir1 != NULL && mp->mnt_kern_flag & MNTK_SUJ) {
		jaddref = (struct jaddref *)TAILQ_PREV(&jaddref->ja_ref,
		    inoreflst, if_deps);
		KASSERT(jaddref != NULL &&
		    jaddref->ja_ino == jaddref->ja_parent &&
		    (jaddref->ja_state & MKDIR_BODY),
		    ("softdep_setup_directory_add: bad dot jaddref %p",
		    jaddref));
		mkdir1->md_jaddref = jaddref;
		jaddref->ja_mkdir = mkdir1;
		/*
		 * It is important that the dotdot journal entry
		 * is added prior to the dot entry since dot writes
		 * both the dot and dotdot links.  These both must
		 * be added after the primary link for the journal
		 * to remain consistent.
		 */
		add_to_journal(&mkdir2->md_jaddref->ja_list);
		add_to_journal(&jaddref->ja_list);
	}
	/*
	 * If we are adding a new directory remember this diradd so that if
	 * we rename it we can keep the dot and dotdot dependencies.  If
	 * we are adding a new name for an inode that has a mkdiradd we
	 * must be in rename and we have to move the dot and dotdot
	 * dependencies to this new name.  The old name is being orphaned
	 * soon.
	 */
	if (mkdir1 != NULL) {
		if (inodedep->id_mkdiradd != NULL)
			panic("softdep_setup_directory_add: Existing mkdir");
		inodedep->id_mkdiradd = dap;
	} else if (inodedep->id_mkdiradd)
		merge_diradd(inodedep, dap);
	if (newdirblk) {
		/*
		 * There is nothing to do if we are already tracking
		 * this block.
		 */
		if ((pagedep->pd_state & NEWBLOCK) != 0) {
			WORKITEM_FREE(newdirblk, D_NEWDIRBLK);
			FREE_LOCK(&lk);
			return (0);
		}
		if (newblk_lookup(mp, dbtofsb(fs, bp->b_blkno), 0, &newblk)
		    == 0)
			panic("softdep_setup_directory_add: lost entry");
		WORKLIST_INSERT(&newblk->nb_newdirblk, &newdirblk->db_list);
		pagedep->pd_state |= NEWBLOCK;
		pagedep->pd_newdirblk = newdirblk;
		newdirblk->db_pagedep = pagedep;
		FREE_LOCK(&lk);
		/*
		 * If we extended into an indirect signal direnter to sync.
		 */
		if (isindir)
			return (1);
		return (0);
	}
	FREE_LOCK(&lk);
	return (0);
}

/*
 * This procedure is called to change the offset of a directory
 * entry when compacting a directory block which must be owned
 * exclusively by the caller. Note that the actual entry movement
 * must be done in this procedure to ensure that no I/O completions
 * occur while the move is in progress.
 */
void 
softdep_change_directoryentry_offset(bp, dp, base, oldloc, newloc, entrysize)
	struct buf *bp;		/* Buffer holding directory block. */
	struct inode *dp;	/* inode for directory */
	caddr_t base;		/* address of dp->i_offset */
	caddr_t oldloc;		/* address of old directory location */
	caddr_t newloc;		/* address of new directory location */
	int entrysize;		/* size of directory entry */
{
	int offset, oldoffset, newoffset;
	struct pagedep *pagedep;
	struct jmvref *jmvref;
	struct diradd *dap;
	struct direct *de;
	struct mount *mp;
	ufs_lbn_t lbn;
	int flags;

	mp = UFSTOVFS(dp->i_ump);
	de = (struct direct *)oldloc;
	jmvref = NULL;
	flags = 0;
	/*
	 * Moves are always journaled as it would be too complex to
	 * determine if any affected adds or removes are present in the
	 * journal.
	 */
	if (mp->mnt_kern_flag & MNTK_SUJ)  {
		flags = DEPALLOC;
		jmvref = newjmvref(dp, de->d_ino,
		    dp->i_offset + (oldloc - base),
		    dp->i_offset + (newloc - base));
	}
	lbn = lblkno(dp->i_fs, dp->i_offset);
	offset = blkoff(dp->i_fs, dp->i_offset);
	oldoffset = offset + (oldloc - base);
	newoffset = offset + (newloc - base);
	ACQUIRE_LOCK(&lk);
	if (pagedep_lookup(mp, dp->i_number, lbn, flags, &pagedep) == 0) {
		if (pagedep)
			WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
		goto done;
	}
	dap = diradd_lookup(pagedep, oldoffset);
	if (dap) {
		dap->da_offset = newoffset;
		newoffset = DIRADDHASH(newoffset);
		oldoffset = DIRADDHASH(oldoffset);
		if ((dap->da_state & ALLCOMPLETE) != ALLCOMPLETE &&
		    newoffset != oldoffset) {
			LIST_REMOVE(dap, da_pdlist);
			LIST_INSERT_HEAD(&pagedep->pd_diraddhd[newoffset],
			    dap, da_pdlist);
		}
	}
done:
	if (jmvref) {
		jmvref->jm_pagedep = pagedep;
		LIST_INSERT_HEAD(&pagedep->pd_jmvrefhd, jmvref, jm_deps);
		add_to_journal(&jmvref->jm_list);
	}
	bcopy(oldloc, newloc, entrysize);
	FREE_LOCK(&lk);
}

/*
 * Move the mkdir dependencies and journal work from one diradd to another
 * when renaming a directory.  The new name must depend on the mkdir deps
 * completing as the old name did.  Directories can only have one valid link
 * at a time so one must be canonical.
 */
static void
merge_diradd(inodedep, newdap)
	struct inodedep *inodedep;
	struct diradd *newdap;
{
	struct diradd *olddap;
	struct mkdir *mkdir, *nextmd;
	short state;

	olddap = inodedep->id_mkdiradd;
	inodedep->id_mkdiradd = newdap;
	if ((olddap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0) {
		newdap->da_state &= ~DEPCOMPLETE;
		for (mkdir = LIST_FIRST(&mkdirlisthd); mkdir; mkdir = nextmd) {
			nextmd = LIST_NEXT(mkdir, md_mkdirs);
			if (mkdir->md_diradd != olddap)
				continue;
			mkdir->md_diradd = newdap;
			state = mkdir->md_state & (MKDIR_PARENT | MKDIR_BODY);
			newdap->da_state |= state;
			olddap->da_state &= ~state;
			if ((olddap->da_state &
			    (MKDIR_PARENT | MKDIR_BODY)) == 0)
				break;
		}
		if ((olddap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0)
			panic("merge_diradd: unfound ref");
	}
	/*
	 * Any mkdir related journal items are not safe to be freed until
	 * the new name is stable.
	 */
	jwork_move(&newdap->da_jwork, &olddap->da_jwork);
	olddap->da_state |= DEPCOMPLETE;
	complete_diradd(olddap);
}

/*
 * Move the diradd to the pending list when all diradd dependencies are
 * complete.
 */
static void
complete_diradd(dap)
	struct diradd *dap;
{
	struct pagedep *pagedep;

	if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		LIST_REMOVE(dap, da_pdlist);
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
	}
}

/*
 * Cancel a diradd when a dirrem overlaps with it.  We must cancel the journal
 * add entries and conditonally journal the remove.
 */
static void
cancel_diradd(dap, dirrem, jremref, dotremref, dotdotremref)
	struct diradd *dap;
	struct dirrem *dirrem;
	struct jremref *jremref;
	struct jremref *dotremref;
	struct jremref *dotdotremref;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct inoref *inoref;
	struct mkdir *mkdir;

	/*
	 * If no remove references were allocated we're on a non-journaled
	 * filesystem and can skip the cancel step.
	 */
	if (jremref == NULL) {
		free_diradd(dap, NULL);
		return;
	}
	/*
	 * Cancel the primary name an free it if it does not require
	 * journaling.
	 */
	if (inodedep_lookup(dap->da_list.wk_mp, dap->da_newinum,
	    0, &inodedep) != 0) {
		/* Abort the addref that reference this diradd.  */
		TAILQ_FOREACH(inoref, &inodedep->id_inoreflst, if_deps) {
			if (inoref->if_list.wk_type != D_JADDREF)
				continue;
			jaddref = (struct jaddref *)inoref;
			if (jaddref->ja_diradd != dap)
				continue;
			if (cancel_jaddref(jaddref, inodedep,
			    &dirrem->dm_jwork) == 0) {
				free_jremref(jremref);
				jremref = NULL;
			}
			break;
		}
	}
	/*
	 * Cancel subordinate names and free them if they do not require
	 * journaling.
	 */
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0) {
		LIST_FOREACH(mkdir, &mkdirlisthd, md_mkdirs) {
			if (mkdir->md_diradd != dap)
				continue;
			if ((jaddref = mkdir->md_jaddref) == NULL)
				continue;
			mkdir->md_jaddref = NULL;
			if (mkdir->md_state & MKDIR_PARENT) {
				if (cancel_jaddref(jaddref, NULL,
				    &dirrem->dm_jwork) == 0) {
					free_jremref(dotdotremref);
					dotdotremref = NULL;
				}
			} else {
				if (cancel_jaddref(jaddref, inodedep,
				    &dirrem->dm_jwork) == 0) {
					free_jremref(dotremref);
					dotremref = NULL;
				}
			}
		}
	}

	if (jremref)
		journal_jremref(dirrem, jremref, inodedep);
	if (dotremref)
		journal_jremref(dirrem, dotremref, inodedep);
	if (dotdotremref)
		journal_jremref(dirrem, dotdotremref, NULL);
	jwork_move(&dirrem->dm_jwork, &dap->da_jwork);
	free_diradd(dap, &dirrem->dm_jwork);
}

/*
 * Free a diradd dependency structure. This routine must be called
 * with splbio interrupts blocked.
 */
static void
free_diradd(dap, wkhd)
	struct diradd *dap;
	struct workhead *wkhd;
{
	struct dirrem *dirrem;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct mkdir *mkdir, *nextmd;

	mtx_assert(&lk, MA_OWNED);
	LIST_REMOVE(dap, da_pdlist);
	if (dap->da_state & ONWORKLIST)
		WORKLIST_REMOVE(&dap->da_list);
	if ((dap->da_state & DIRCHG) == 0) {
		pagedep = dap->da_pagedep;
	} else {
		dirrem = dap->da_previous;
		pagedep = dirrem->dm_pagedep;
		dirrem->dm_dirinum = pagedep->pd_ino;
		dirrem->dm_state |= COMPLETE;
		if (LIST_EMPTY(&dirrem->dm_jremrefhd))
			add_to_worklist(&dirrem->dm_list, 0);
	}
	if (inodedep_lookup(pagedep->pd_list.wk_mp, dap->da_newinum,
	    0, &inodedep) != 0)
		if (inodedep->id_mkdiradd == dap)
			inodedep->id_mkdiradd = NULL;
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0) {
		for (mkdir = LIST_FIRST(&mkdirlisthd); mkdir; mkdir = nextmd) {
			nextmd = LIST_NEXT(mkdir, md_mkdirs);
			if (mkdir->md_diradd != dap)
				continue;
			dap->da_state &=
			    ~(mkdir->md_state & (MKDIR_PARENT | MKDIR_BODY));
			LIST_REMOVE(mkdir, md_mkdirs);
			if (mkdir->md_state & ONWORKLIST)
				WORKLIST_REMOVE(&mkdir->md_list);
			if (mkdir->md_jaddref != NULL)
				panic("free_diradd: Unexpected jaddref");
			WORKITEM_FREE(mkdir, D_MKDIR);
			if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) == 0)
				break;
		}
		if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) != 0)
			panic("free_diradd: unfound ref");
	}
	if (inodedep)
		free_inodedep(inodedep);
	/*
	 * Free any journal segments waiting for the directory write.
	 */
	handle_jwork(&dap->da_jwork);
	WORKITEM_FREE(dap, D_DIRADD);
}

/*
 * Directory entry removal dependencies.
 * 
 * When removing a directory entry, the entry's inode pointer must be
 * zero'ed on disk before the corresponding inode's link count is decremented
 * (possibly freeing the inode for re-use). This dependency is handled by
 * updating the directory entry but delaying the inode count reduction until
 * after the directory block has been written to disk. After this point, the
 * inode count can be decremented whenever it is convenient.
 */

/*
 * This routine should be called immediately after removing
 * a directory entry.  The inode's link count should not be
 * decremented by the calling procedure -- the soft updates
 * code will do this task when it is safe.
 */
void 
softdep_setup_remove(bp, dp, ip, isrmdir)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	int isrmdir;		/* indicates if doing RMDIR */
{
	struct dirrem *dirrem, *prevdirrem;
	struct inodedep *inodedep;
	int direct;

	/*
	 * Allocate a new dirrem if appropriate and ACQUIRE_LOCK.  We want
	 * newdirrem() to setup the full directory remove which requires
	 * isrmdir > 1.
	 */
	dirrem = newdirrem(bp, dp, ip, isrmdir, &prevdirrem);
	/*
	 * Add the dirrem to the inodedep's pending remove list for quick
	 * discovery later.
	 */
	if (inodedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number, 0,
	    &inodedep) == 0)
		panic("softdep_setup_remove: Lost inodedep.");
	dirrem->dm_state |= ONDEPLIST;
	LIST_INSERT_HEAD(&inodedep->id_dirremhd, dirrem, dm_inonext);

	/*
	 * If the COMPLETE flag is clear, then there were no active
	 * entries and we want to roll back to a zeroed entry until
	 * the new inode is committed to disk. If the COMPLETE flag is
	 * set then we have deleted an entry that never made it to
	 * disk. If the entry we deleted resulted from a name change,
	 * then the old name still resides on disk. We cannot delete
	 * its inode (returned to us in prevdirrem) until the zeroed
	 * directory entry gets to disk. The new inode has never been
	 * referenced on the disk, so can be deleted immediately.
	 */
	if ((dirrem->dm_state & COMPLETE) == 0) {
		LIST_INSERT_HEAD(&dirrem->dm_pagedep->pd_dirremhd, dirrem,
		    dm_next);
		FREE_LOCK(&lk);
	} else {
		if (prevdirrem != NULL)
			LIST_INSERT_HEAD(&dirrem->dm_pagedep->pd_dirremhd,
			    prevdirrem, dm_next);
		dirrem->dm_dirinum = dirrem->dm_pagedep->pd_ino;
		direct = LIST_EMPTY(&dirrem->dm_jremrefhd);
		FREE_LOCK(&lk);
		if (direct)
			handle_workitem_remove(dirrem, NULL);
	}
}

/*
 * Check for an entry matching 'offset' on both the pd_dirraddhd list and the
 * pd_pendinghd list of a pagedep.
 */
static struct diradd *
diradd_lookup(pagedep, offset)
	struct pagedep *pagedep;
	int offset;
{
	struct diradd *dap;

	LIST_FOREACH(dap, &pagedep->pd_diraddhd[DIRADDHASH(offset)], da_pdlist)
		if (dap->da_offset == offset)
			return (dap);
	LIST_FOREACH(dap, &pagedep->pd_pendinghd, da_pdlist)
		if (dap->da_offset == offset)
			return (dap);
	return (NULL);
}

/*
 * Search for a .. diradd dependency in a directory that is being removed.
 * If the directory was renamed to a new parent we have a diradd rather
 * than a mkdir for the .. entry.  We need to cancel it now before
 * it is found in truncate().
 */
static struct jremref *
cancel_diradd_dotdot(ip, dirrem, jremref)
	struct inode *ip;
	struct dirrem *dirrem;
	struct jremref *jremref;
{
	struct pagedep *pagedep;
	struct diradd *dap;
	struct worklist *wk;

	if (pagedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number, 0, 0,
	    &pagedep) == 0)
		return (jremref);
	dap = diradd_lookup(pagedep, DOTDOT_OFFSET);
	if (dap == NULL)
		return (jremref);
	cancel_diradd(dap, dirrem, jremref, NULL, NULL);
	/*
	 * Mark any journal work as belonging to the parent so it is freed
	 * with the .. reference.
	 */
	LIST_FOREACH(wk, &dirrem->dm_jwork, wk_list)
		wk->wk_state |= MKDIR_PARENT;
	return (NULL);
}

/*
 * Cancel the MKDIR_PARENT mkdir component of a diradd when we're going to
 * replace it with a dirrem/diradd pair as a result of re-parenting a
 * directory.  This ensures that we don't simultaneously have a mkdir and
 * a diradd for the same .. entry.
 */
static struct jremref *
cancel_mkdir_dotdot(ip, dirrem, jremref)
	struct inode *ip;
	struct dirrem *dirrem;
	struct jremref *jremref;
{
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct mkdir *mkdir;
	struct diradd *dap;

	if (inodedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number, 0,
	    &inodedep) == 0)
		panic("cancel_mkdir_dotdot: Lost inodedep");
	dap = inodedep->id_mkdiradd;
	if (dap == NULL || (dap->da_state & MKDIR_PARENT) == 0)
		return (jremref);
	for (mkdir = LIST_FIRST(&mkdirlisthd); mkdir;
	    mkdir = LIST_NEXT(mkdir, md_mkdirs))
		if (mkdir->md_diradd == dap && mkdir->md_state & MKDIR_PARENT)
			break;
	if (mkdir == NULL)
		panic("cancel_mkdir_dotdot: Unable to find mkdir\n");
	if ((jaddref = mkdir->md_jaddref) != NULL) {
		mkdir->md_jaddref = NULL;
		jaddref->ja_state &= ~MKDIR_PARENT;
		if (inodedep_lookup(UFSTOVFS(ip->i_ump), jaddref->ja_ino, 0,
		    &inodedep) == 0)
			panic("cancel_mkdir_dotdot: Lost parent inodedep");
		if (cancel_jaddref(jaddref, inodedep, &dirrem->dm_jwork)) {
			journal_jremref(dirrem, jremref, inodedep);
			jremref = NULL;
		}
	}
	if (mkdir->md_state & ONWORKLIST)
		WORKLIST_REMOVE(&mkdir->md_list);
	mkdir->md_state |= ALLCOMPLETE;
	complete_mkdir(mkdir);
	return (jremref);
}

static void
journal_jremref(dirrem, jremref, inodedep)
	struct dirrem *dirrem;
	struct jremref *jremref;
	struct inodedep *inodedep;
{

	if (inodedep == NULL)
		if (inodedep_lookup(jremref->jr_list.wk_mp,
		    jremref->jr_ref.if_ino, 0, &inodedep) == 0)
			panic("journal_jremref: Lost inodedep");
	LIST_INSERT_HEAD(&dirrem->dm_jremrefhd, jremref, jr_deps);
	TAILQ_INSERT_TAIL(&inodedep->id_inoreflst, &jremref->jr_ref, if_deps);
	add_to_journal(&jremref->jr_list);
}

static void
dirrem_journal(dirrem, jremref, dotremref, dotdotremref)
	struct dirrem *dirrem;
	struct jremref *jremref;
	struct jremref *dotremref;
	struct jremref *dotdotremref;
{
	struct inodedep *inodedep;


	if (inodedep_lookup(jremref->jr_list.wk_mp, jremref->jr_ref.if_ino, 0,
	    &inodedep) == 0)
		panic("dirrem_journal: Lost inodedep");
	journal_jremref(dirrem, jremref, inodedep);
	if (dotremref)
		journal_jremref(dirrem, dotremref, inodedep);
	if (dotdotremref)
		journal_jremref(dirrem, dotdotremref, NULL);
}

/*
 * Allocate a new dirrem if appropriate and return it along with
 * its associated pagedep. Called without a lock, returns with lock.
 */
static long num_dirrem;		/* number of dirrem allocated */
static struct dirrem *
newdirrem(bp, dp, ip, isrmdir, prevdirremp)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	int isrmdir;		/* indicates if doing RMDIR */
	struct dirrem **prevdirremp; /* previously referenced inode, if any */
{
	int offset;
	ufs_lbn_t lbn;
	struct diradd *dap;
	struct dirrem *dirrem;
	struct pagedep *pagedep;
	struct jremref *jremref;
	struct jremref *dotremref;
	struct jremref *dotdotremref;
	struct vnode *dvp;

	/*
	 * Whiteouts have no deletion dependencies.
	 */
	if (ip == NULL)
		panic("newdirrem: whiteout");
	dvp = ITOV(dp);
	/*
	 * If we are over our limit, try to improve the situation.
	 * Limiting the number of dirrem structures will also limit
	 * the number of freefile and freeblks structures.
	 */
	ACQUIRE_LOCK(&lk);
	if (!(ip->i_flags & SF_SNAPSHOT) && num_dirrem > max_softdeps / 2)
		(void) request_cleanup(ITOV(dp)->v_mount, FLUSH_REMOVE);
	num_dirrem += 1;
	FREE_LOCK(&lk);
	dirrem = malloc(sizeof(struct dirrem),
		M_DIRREM, M_SOFTDEP_FLAGS|M_ZERO);
	workitem_alloc(&dirrem->dm_list, D_DIRREM, dvp->v_mount);
	LIST_INIT(&dirrem->dm_jremrefhd);
	LIST_INIT(&dirrem->dm_jwork);
	dirrem->dm_state = isrmdir ? RMDIR : 0;
	dirrem->dm_oldinum = ip->i_number;
	*prevdirremp = NULL;
	/*
	 * Allocate remove reference structures to track journal write
	 * dependencies.  We will always have one for the link and
	 * when doing directories we will always have one more for dot.
	 * When renaming a directory we skip the dotdot link change so
	 * this is not needed.
	 */
	jremref = dotremref = dotdotremref = NULL;
	if (DOINGSUJ(dvp)) {
		if (isrmdir) {
			jremref = newjremref(dirrem, dp, ip, dp->i_offset,
			    ip->i_effnlink + 2);
			dotremref = newjremref(dirrem, ip, ip, DOT_OFFSET,
			    ip->i_effnlink + 1);
			dotdotremref = newjremref(dirrem, ip, dp, DOTDOT_OFFSET,
			    dp->i_effnlink + 1);
			dotdotremref->jr_state |= MKDIR_PARENT;
		} else
			jremref = newjremref(dirrem, dp, ip, dp->i_offset,
			    ip->i_effnlink + 1);
	}
	ACQUIRE_LOCK(&lk);
	lbn = lblkno(dp->i_fs, dp->i_offset);
	offset = blkoff(dp->i_fs, dp->i_offset);
	if (pagedep_lookup(UFSTOVFS(dp->i_ump), dp->i_number, lbn, DEPALLOC,
	    &pagedep) == 0)
		WORKLIST_INSERT(&bp->b_dep, &pagedep->pd_list);
	dirrem->dm_pagedep = pagedep;
	/*
	 * If we're renaming a .. link to a new directory, cancel any
	 * existing MKDIR_PARENT mkdir.  If it has already been canceled
	 * the jremref is preserved for any potential diradd in this
	 * location.  This can not coincide with a rmdir.
	 */
	if (dp->i_offset == DOTDOT_OFFSET) {
		if (isrmdir)
			panic("newdirrem: .. directory change during remove?");
		jremref = cancel_mkdir_dotdot(dp, dirrem, jremref);
	}
	/*
	 * If we're removing a directory search for the .. dependency now and
	 * cancel it.  Any pending journal work will be added to the dirrem
	 * to be completed when the workitem remove completes.
	 */
	if (isrmdir)
		dotdotremref = cancel_diradd_dotdot(ip, dirrem, dotdotremref);
	/*
	 * Check for a diradd dependency for the same directory entry.
	 * If present, then both dependencies become obsolete and can
	 * be de-allocated.
	 */
	dap = diradd_lookup(pagedep, offset);
	if (dap == NULL) {
		/*
		 * Link the jremref structures into the dirrem so they are
		 * written prior to the pagedep.
		 */
		if (jremref)
			dirrem_journal(dirrem, jremref, dotremref,
			    dotdotremref);
		return (dirrem);
	}
	/*
	 * Must be ATTACHED at this point.
	 */
	if ((dap->da_state & ATTACHED) == 0)
		panic("newdirrem: not ATTACHED");
	if (dap->da_newinum != ip->i_number)
		panic("newdirrem: inum %d should be %d",
		    ip->i_number, dap->da_newinum);
	/*
	 * If we are deleting a changed name that never made it to disk,
	 * then return the dirrem describing the previous inode (which
	 * represents the inode currently referenced from this entry on disk).
	 */
	if ((dap->da_state & DIRCHG) != 0) {
		*prevdirremp = dap->da_previous;
		dap->da_state &= ~DIRCHG;
		dap->da_pagedep = pagedep;
	}
	/*
	 * We are deleting an entry that never made it to disk.
	 * Mark it COMPLETE so we can delete its inode immediately.
	 */
	dirrem->dm_state |= COMPLETE;
	cancel_diradd(dap, dirrem, jremref, dotremref, dotdotremref);
#ifdef SUJ_DEBUG
	if (isrmdir == 0) {
		struct worklist *wk;

		LIST_FOREACH(wk, &dirrem->dm_jwork, wk_list)
			if (wk->wk_state & (MKDIR_BODY | MKDIR_PARENT))
				panic("bad wk %p (0x%X)\n", wk, wk->wk_state);
	}
#endif

	return (dirrem);
}

/*
 * Directory entry change dependencies.
 * 
 * Changing an existing directory entry requires that an add operation
 * be completed first followed by a deletion. The semantics for the addition
 * are identical to the description of adding a new entry above except
 * that the rollback is to the old inode number rather than zero. Once
 * the addition dependency is completed, the removal is done as described
 * in the removal routine above.
 */

/*
 * This routine should be called immediately after changing
 * a directory entry.  The inode's link count should not be
 * decremented by the calling procedure -- the soft updates
 * code will perform this task when it is safe.
 */
void 
softdep_setup_directory_change(bp, dp, ip, newinum, isrmdir)
	struct buf *bp;		/* buffer containing directory block */
	struct inode *dp;	/* inode for the directory being modified */
	struct inode *ip;	/* inode for directory entry being removed */
	ino_t newinum;		/* new inode number for changed entry */
	int isrmdir;		/* indicates if doing RMDIR */
{
	int offset;
	struct diradd *dap = NULL;
	struct dirrem *dirrem, *prevdirrem;
	struct pagedep *pagedep;
	struct inodedep *inodedep;
	struct jaddref *jaddref;
	struct mount *mp;

	offset = blkoff(dp->i_fs, dp->i_offset);
	mp = UFSTOVFS(dp->i_ump);

	/*
	 * Whiteouts do not need diradd dependencies.
	 */
	if (newinum != WINO) {
		dap = malloc(sizeof(struct diradd),
		    M_DIRADD, M_SOFTDEP_FLAGS|M_ZERO);
		workitem_alloc(&dap->da_list, D_DIRADD, mp);
		dap->da_state = DIRCHG | ATTACHED | DEPCOMPLETE;
		dap->da_offset = offset;
		dap->da_newinum = newinum;
		LIST_INIT(&dap->da_jwork);
	}

	/*
	 * Allocate a new dirrem and ACQUIRE_LOCK.
	 */
	dirrem = newdirrem(bp, dp, ip, isrmdir, &prevdirrem);
	pagedep = dirrem->dm_pagedep;
	/*
	 * The possible values for isrmdir:
	 *	0 - non-directory file rename
	 *	1 - directory rename within same directory
	 *   inum - directory rename to new directory of given inode number
	 * When renaming to a new directory, we are both deleting and
	 * creating a new directory entry, so the link count on the new
	 * directory should not change. Thus we do not need the followup
	 * dirrem which is usually done in handle_workitem_remove. We set
	 * the DIRCHG flag to tell handle_workitem_remove to skip the 
	 * followup dirrem.
	 */
	if (isrmdir > 1)
		dirrem->dm_state |= DIRCHG;

	/*
	 * Whiteouts have no additional dependencies,
	 * so just put the dirrem on the correct list.
	 */
	if (newinum == WINO) {
		if ((dirrem->dm_state & COMPLETE) == 0) {
			LIST_INSERT_HEAD(&pagedep->pd_dirremhd, dirrem,
			    dm_next);
		} else {
			dirrem->dm_dirinum = pagedep->pd_ino;
			if (LIST_EMPTY(&dirrem->dm_jremrefhd))
				add_to_worklist(&dirrem->dm_list, 0);
		}
		FREE_LOCK(&lk);
		return;
	}
	/*
	 * Add the dirrem to the inodedep's pending remove list for quick
	 * discovery later.  A valid nlinkdelta ensures that this lookup
	 * will not fail.
	 */
	if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) == 0)
		panic("softdep_setup_directory_change: Lost inodedep.");
	dirrem->dm_state |= ONDEPLIST;
	LIST_INSERT_HEAD(&inodedep->id_dirremhd, dirrem, dm_inonext);

	/*
	 * If the COMPLETE flag is clear, then there were no active
	 * entries and we want to roll back to the previous inode until
	 * the new inode is committed to disk. If the COMPLETE flag is
	 * set, then we have deleted an entry that never made it to disk.
	 * If the entry we deleted resulted from a name change, then the old
	 * inode reference still resides on disk. Any rollback that we do
	 * needs to be to that old inode (returned to us in prevdirrem). If
	 * the entry we deleted resulted from a create, then there is
	 * no entry on the disk, so we want to roll back to zero rather
	 * than the uncommitted inode. In either of the COMPLETE cases we
	 * want to immediately free the unwritten and unreferenced inode.
	 */
	if ((dirrem->dm_state & COMPLETE) == 0) {
		dap->da_previous = dirrem;
	} else {
		if (prevdirrem != NULL) {
			dap->da_previous = prevdirrem;
		} else {
			dap->da_state &= ~DIRCHG;
			dap->da_pagedep = pagedep;
		}
		dirrem->dm_dirinum = pagedep->pd_ino;
		if (LIST_EMPTY(&dirrem->dm_jremrefhd))
			add_to_worklist(&dirrem->dm_list, 0);
	}
	/*
	 * Lookup the jaddref for this journal entry.  We must finish
	 * initializing it and make the diradd write dependent on it.
	 * If we're not journaling Put it on the id_bufwait list if the inode
	 * is not yet written. If it is written, do the post-inode write
	 * processing to put it on the id_pendinghd list.
	 */
	inodedep_lookup(mp, newinum, DEPALLOC, &inodedep);
	if (mp->mnt_kern_flag & MNTK_SUJ) {
		jaddref = (struct jaddref *)TAILQ_LAST(&inodedep->id_inoreflst,
		    inoreflst);
		KASSERT(jaddref != NULL && jaddref->ja_parent == dp->i_number,
		    ("softdep_setup_directory_change: bad jaddref %p",
		    jaddref));
		jaddref->ja_diroff = dp->i_offset;
		jaddref->ja_diradd = dap;
		LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)],
		    dap, da_pdlist);
		add_to_journal(&jaddref->ja_list);
	} else if ((inodedep->id_state & ALLCOMPLETE) == ALLCOMPLETE) {
		dap->da_state |= COMPLETE;
		LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap, da_pdlist);
		WORKLIST_INSERT(&inodedep->id_pendinghd, &dap->da_list);
	} else {
		LIST_INSERT_HEAD(&pagedep->pd_diraddhd[DIRADDHASH(offset)],
		    dap, da_pdlist);
		WORKLIST_INSERT(&inodedep->id_bufwait, &dap->da_list);
	}
	/*
	 * If we're making a new name for a directory that has not been
	 * committed when need to move the dot and dotdot references to
	 * this new name.
	 */
	if (inodedep->id_mkdiradd && dp->i_offset != DOTDOT_OFFSET)
		merge_diradd(inodedep, dap);
	FREE_LOCK(&lk);
}

/*
 * Called whenever the link count on an inode is changed.
 * It creates an inode dependency so that the new reference(s)
 * to the inode cannot be committed to disk until the updated
 * inode has been written.
 */
void
softdep_change_linkcnt(ip)
	struct inode *ip;	/* the inode with the increased link count */
{
	struct inodedep *inodedep;

	ACQUIRE_LOCK(&lk);
	inodedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number, DEPALLOC, &inodedep);
	if (ip->i_nlink < ip->i_effnlink)
		panic("softdep_change_linkcnt: bad delta");
	inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
	FREE_LOCK(&lk);
}

/*
 * Attach a sbdep dependency to the superblock buf so that we can keep
 * track of the head of the linked list of referenced but unlinked inodes.
 */
void
softdep_setup_sbupdate(ump, fs, bp)
	struct ufsmount *ump;
	struct fs *fs;
	struct buf *bp;
{
	struct sbdep *sbdep;
	struct worklist *wk;

	if ((fs->fs_flags & FS_SUJ) == 0)
		return;
	LIST_FOREACH(wk, &bp->b_dep, wk_list)
		if (wk->wk_type == D_SBDEP)
			break;
	if (wk != NULL)
		return;
	sbdep = malloc(sizeof(struct sbdep), M_SBDEP, M_SOFTDEP_FLAGS);
	workitem_alloc(&sbdep->sb_list, D_SBDEP, UFSTOVFS(ump));
	sbdep->sb_fs = fs;
	sbdep->sb_ump = ump;
	ACQUIRE_LOCK(&lk);
	WORKLIST_INSERT(&bp->b_dep, &sbdep->sb_list);
	FREE_LOCK(&lk);
}

/*
 * Return the first unlinked inodedep which is ready to be the head of the
 * list.  The inodedep and all those after it must have valid next pointers.
 */
static struct inodedep *
first_unlinked_inodedep(ump)
	struct ufsmount *ump;
{
	struct inodedep *inodedep;
	struct inodedep *idp;

	for (inodedep = TAILQ_LAST(&ump->softdep_unlinked, inodedeplst);
	    inodedep; inodedep = idp) {
		if ((inodedep->id_state & UNLINKNEXT) == 0)
			return (NULL);
		idp = TAILQ_PREV(inodedep, inodedeplst, id_unlinked);
		if (idp == NULL || (idp->id_state & UNLINKNEXT) == 0)
			break;
		if ((inodedep->id_state & UNLINKPREV) == 0)
			panic("first_unlinked_inodedep: prev != next");
	}
	if (inodedep == NULL)
		return (NULL);

	return (inodedep);
}

/*
 * Set the sujfree unlinked head pointer prior to writing a superblock.
 */
static void
initiate_write_sbdep(sbdep)
	struct sbdep *sbdep;
{
	struct inodedep *inodedep;
	struct fs *bpfs;
	struct fs *fs;

	bpfs = sbdep->sb_fs;
	fs = sbdep->sb_ump->um_fs;
	inodedep = first_unlinked_inodedep(sbdep->sb_ump);
	if (inodedep) {
		fs->fs_sujfree = inodedep->id_ino;
		inodedep->id_state |= UNLINKPREV;
	} else
		fs->fs_sujfree = 0;
	bpfs->fs_sujfree = fs->fs_sujfree;
}

/*
 * After a superblock is written determine whether it must be written again
 * due to a changing unlinked list head.
 */
static int
handle_written_sbdep(sbdep, bp)
	struct sbdep *sbdep;
	struct buf *bp;
{
	struct inodedep *inodedep;
	struct mount *mp;
	struct fs *fs;

	fs = sbdep->sb_fs;
	mp = UFSTOVFS(sbdep->sb_ump);
	inodedep = first_unlinked_inodedep(sbdep->sb_ump);
	if ((inodedep && fs->fs_sujfree != inodedep->id_ino) ||
	    (inodedep == NULL && fs->fs_sujfree != 0)) {
		bdirty(bp);
		return (1);
	}
	WORKITEM_FREE(sbdep, D_SBDEP);
	if (fs->fs_sujfree == 0)
		return (0);
	if (inodedep_lookup(mp, fs->fs_sujfree, 0, &inodedep) == 0)
		panic("handle_written_sbdep: lost inodedep");
	/*
	 * Now that we have a record of this inode in stable store allow it
	 * to be written to free up pending work.  Inodes may see a lot of
	 * write activity after they are unlinked which we must not hold up.
	 */
	for (; inodedep != NULL; inodedep = TAILQ_NEXT(inodedep, id_unlinked)) {
		if ((inodedep->id_state & UNLINKLINKS) != UNLINKLINKS)
			panic("handle_written_sbdep: Bad inodedep %p (0x%X)",
			    inodedep, inodedep->id_state);
		if (inodedep->id_state & UNLINKONLIST)
			break;
		inodedep->id_state |= DEPCOMPLETE | UNLINKONLIST;
	}

	return (0);
}

/*
 * Mark an inodedep as unlinked and insert it into the in-memory unlinked list.
 */
static void
unlinked_inodedep(mp, inodedep)
	struct mount *mp;
	struct inodedep *inodedep;
{
	struct ufsmount *ump;

	if ((mp->mnt_kern_flag & MNTK_SUJ) == 0)
		return;
	ump = VFSTOUFS(mp);
	ump->um_fs->fs_fmod = 1;
	inodedep->id_state |= UNLINKED;
	TAILQ_INSERT_HEAD(&ump->softdep_unlinked, inodedep, id_unlinked);
}

/*
 * Remove an inodedep from the unlinked inodedep list.  This may require
 * disk writes if the inode has made it that far.
 */
static void
clear_unlinked_inodedep(inodedep)
	struct inodedep *inodedep;
{
	struct ufsmount *ump;
	struct inodedep *idp;
	struct inodedep *idn;
	struct fs *fs;
	struct buf *bp;
	ino_t ino;
	ino_t nino;
	ino_t pino;
	int error;

	ump = VFSTOUFS(inodedep->id_list.wk_mp);
	fs = ump->um_fs;
	ino = inodedep->id_ino;
	error = 0;
	for (;;) {
		/*
		 * If nothing has yet been written simply remove us from
		 * the in memory list and return.  This is the most common
		 * case where handle_workitem_remove() loses the final
		 * reference.
		 */
		if ((inodedep->id_state & UNLINKLINKS) == 0)
			break;
		/*
		 * If we have a NEXT pointer and no PREV pointer we can simply
		 * clear NEXT's PREV and remove ourselves from the list.  Be
		 * careful not to clear PREV if the superblock points at
		 * next as well.
		 */
		idn = TAILQ_NEXT(inodedep, id_unlinked);
		if ((inodedep->id_state & UNLINKLINKS) == UNLINKNEXT) {
			if (idn && fs->fs_sujfree != idn->id_ino)
				idn->id_state &= ~UNLINKPREV;
			break;
		}
		/*
		 * Here we have an inodedep which is actually linked into
		 * the list.  We must remove it by forcing a write to the
		 * link before us, whether it be the superblock or an inode.
		 * Unfortunately the list may change while we're waiting
		 * on the buf lock for either resource so we must loop until
		 * we lock the right one.  If both the superblock and an
		 * inode point to this inode we must clear the inode first
		 * followed by the superblock.
		 */
		idp = TAILQ_PREV(inodedep, inodedeplst, id_unlinked);
		pino = 0;
		if (idp && (idp->id_state & UNLINKNEXT))
			pino = idp->id_ino;
		FREE_LOCK(&lk);
		if (pino == 0)
			bp = getblk(ump->um_devvp, btodb(fs->fs_sblockloc),
			    (int)fs->fs_sbsize, 0, 0, 0);
		else
			error = bread(ump->um_devvp,
			    fsbtodb(fs, ino_to_fsba(fs, pino)),
			    (int)fs->fs_bsize, NOCRED, &bp);
		ACQUIRE_LOCK(&lk);
		if (error)
			break;
		/* If the list has changed restart the loop. */
		idp = TAILQ_PREV(inodedep, inodedeplst, id_unlinked);
		nino = 0;
		if (idp && (idp->id_state & UNLINKNEXT))
			nino = idp->id_ino;
		if (nino != pino ||
		    (inodedep->id_state & UNLINKPREV) != UNLINKPREV) {
			FREE_LOCK(&lk);
			brelse(bp);
			ACQUIRE_LOCK(&lk);
			continue;
		}
		/*
		 * Remove us from the in memory list.  After this we cannot
		 * access the inodedep.
		 */
		idn = TAILQ_NEXT(inodedep, id_unlinked);
		inodedep->id_state &= ~(UNLINKED | UNLINKLINKS);
		TAILQ_REMOVE(&ump->softdep_unlinked, inodedep, id_unlinked);
		/*
		 * Determine the next inode number.
		 */
		nino = 0;
		if (idn) {
			/*
			 * If next isn't on the list we can just clear prev's
			 * state and schedule it to be fixed later.  No need
			 * to synchronously write if we're not in the real
			 * list.
			 */
			if ((idn->id_state & UNLINKPREV) == 0 && pino != 0) {
				idp->id_state &= ~UNLINKNEXT;
				if ((idp->id_state & ONWORKLIST) == 0)
					WORKLIST_INSERT(&bp->b_dep,
					    &idp->id_list);
				FREE_LOCK(&lk);
				bawrite(bp);
				ACQUIRE_LOCK(&lk);
				return;
			}
			nino = idn->id_ino;
		}
		FREE_LOCK(&lk);
		/*
		 * The predecessor's next pointer is manually updated here
		 * so that the NEXT flag is never cleared for an element
		 * that is in the list.
		 */
		if (pino == 0) {
			bcopy((caddr_t)fs, bp->b_data, (u_int)fs->fs_sbsize);
			ffs_oldfscompat_write((struct fs *)bp->b_data, ump);
			softdep_setup_sbupdate(ump, (struct fs *)bp->b_data,
			    bp);
		} else if (fs->fs_magic == FS_UFS1_MAGIC)
			((struct ufs1_dinode *)bp->b_data +
			    ino_to_fsbo(fs, pino))->di_freelink = nino;
		else
			((struct ufs2_dinode *)bp->b_data +
			    ino_to_fsbo(fs, pino))->di_freelink = nino;
		/*
		 * If the bwrite fails we have no recourse to recover.  The
		 * filesystem is corrupted already.
		 */
		bwrite(bp);
		ACQUIRE_LOCK(&lk);
		/*
		 * If the superblock pointer still needs to be cleared force
		 * a write here.
		 */
		if (fs->fs_sujfree == ino) {
			FREE_LOCK(&lk);
			bp = getblk(ump->um_devvp, btodb(fs->fs_sblockloc),
			    (int)fs->fs_sbsize, 0, 0, 0);
			bcopy((caddr_t)fs, bp->b_data, (u_int)fs->fs_sbsize);
			ffs_oldfscompat_write((struct fs *)bp->b_data, ump);
			softdep_setup_sbupdate(ump, (struct fs *)bp->b_data,
			    bp);
			bwrite(bp);
			ACQUIRE_LOCK(&lk);
		}
		if (fs->fs_sujfree != ino)
			return;
		panic("clear_unlinked_inodedep: Failed to clear free head");
	}
	if (inodedep->id_ino == fs->fs_sujfree)
		panic("clear_unlinked_inodedep: Freeing head of free list");
	inodedep->id_state &= ~(UNLINKED | UNLINKLINKS);
	TAILQ_REMOVE(&ump->softdep_unlinked, inodedep, id_unlinked);
	return;
}

/*
 * This workitem decrements the inode's link count.
 * If the link count reaches zero, the file is removed.
 */
static void 
handle_workitem_remove(dirrem, xp)
	struct dirrem *dirrem;
	struct vnode *xp;
{
	struct inodedep *inodedep;
	struct workhead dotdotwk;
	struct worklist *wk;
	struct ufsmount *ump;
	struct mount *mp;
	struct vnode *vp;
	struct inode *ip;
	ino_t oldinum;
	int error;

	if (dirrem->dm_state & ONWORKLIST)
		panic("handle_workitem_remove: dirrem %p still on worklist",
		    dirrem);
	oldinum = dirrem->dm_oldinum;
	mp = dirrem->dm_list.wk_mp;
	ump = VFSTOUFS(mp);
	if ((vp = xp) == NULL &&
	    (error = ffs_vgetf(mp, oldinum, LK_EXCLUSIVE, &vp,
	    FFSV_FORCEINSMQ)) != 0) {
		softdep_error("handle_workitem_remove: vget", error);
		return;
	}
	ip = VTOI(vp);
	ACQUIRE_LOCK(&lk);
	if ((inodedep_lookup(mp, oldinum, 0, &inodedep)) == 0)
		panic("handle_workitem_remove: lost inodedep");
	if (dirrem->dm_state & ONDEPLIST)
		LIST_REMOVE(dirrem, dm_inonext);
	KASSERT(LIST_EMPTY(&dirrem->dm_jremrefhd),
	    ("handle_workitem_remove:  Journal entries not written."));

	/*
	 * Move all dependencies waiting on the remove to complete
	 * from the dirrem to the inode inowait list to be completed
	 * after the inode has been updated and written to disk.  Any
	 * marked MKDIR_PARENT are saved to be completed when the .. ref
	 * is removed.
	 */
	LIST_INIT(&dotdotwk);
	while ((wk = LIST_FIRST(&dirrem->dm_jwork)) != NULL) {
		WORKLIST_REMOVE(wk);
		if (wk->wk_state & MKDIR_PARENT) {
			wk->wk_state &= ~MKDIR_PARENT;
			WORKLIST_INSERT(&dotdotwk, wk);
			continue;
		}
		WORKLIST_INSERT(&inodedep->id_inowait, wk);
	}
	LIST_SWAP(&dirrem->dm_jwork, &dotdotwk, worklist, wk_list);
	/*
	 * Normal file deletion.
	 */
	if ((dirrem->dm_state & RMDIR) == 0) {
		ip->i_nlink--;
		DIP_SET(ip, i_nlink, ip->i_nlink);
		ip->i_flag |= IN_CHANGE;
		if (ip->i_nlink < ip->i_effnlink)
			panic("handle_workitem_remove: bad file delta");
		if (ip->i_nlink == 0) 
			unlinked_inodedep(mp, inodedep);
		inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
		num_dirrem -= 1;
		KASSERT(LIST_EMPTY(&dirrem->dm_jwork),
		    ("handle_workitem_remove: worklist not empty. %s",
		    TYPENAME(LIST_FIRST(&dirrem->dm_jwork)->wk_type)));
		WORKITEM_FREE(dirrem, D_DIRREM);
		FREE_LOCK(&lk);
		goto out;
	}
	/*
	 * Directory deletion. Decrement reference count for both the
	 * just deleted parent directory entry and the reference for ".".
	 * Arrange to have the reference count on the parent decremented
	 * to account for the loss of "..".
	 */
	ip->i_nlink -= 2;
	DIP_SET(ip, i_nlink, ip->i_nlink);
	ip->i_flag |= IN_CHANGE;
	if (ip->i_nlink < ip->i_effnlink)
		panic("handle_workitem_remove: bad dir delta");
	if (ip->i_nlink == 0)
		unlinked_inodedep(mp, inodedep);
	inodedep->id_nlinkdelta = ip->i_nlink - ip->i_effnlink;
	/*
	 * Rename a directory to a new parent. Since, we are both deleting
	 * and creating a new directory entry, the link count on the new
	 * directory should not change. Thus we skip the followup dirrem.
	 */
	if (dirrem->dm_state & DIRCHG) {
		KASSERT(LIST_EMPTY(&dirrem->dm_jwork),
		    ("handle_workitem_remove: DIRCHG and worklist not empty."));
		num_dirrem -= 1;
		WORKITEM_FREE(dirrem, D_DIRREM);
		FREE_LOCK(&lk);
		goto out;
	}
	dirrem->dm_state = ONDEPLIST;
	dirrem->dm_oldinum = dirrem->dm_dirinum;
	/*
	 * Place the dirrem on the parent's diremhd list.
	 */
	if (inodedep_lookup(mp, dirrem->dm_oldinum, 0, &inodedep) == 0)
		panic("handle_workitem_remove: lost dir inodedep");
	LIST_INSERT_HEAD(&inodedep->id_dirremhd, dirrem, dm_inonext);
	/*
	 * If the allocated inode has never been written to disk, then
	 * the on-disk inode is zero'ed and we can remove the file
	 * immediately.  When journaling if the inode has been marked
	 * unlinked and not DEPCOMPLETE we know it can never be written.
	 */
	inodedep_lookup(mp, oldinum, 0, &inodedep);
	if (inodedep == NULL ||
	    (inodedep->id_state & (DEPCOMPLETE | UNLINKED)) == UNLINKED ||
	    check_inode_unwritten(inodedep)) {
		if (xp != NULL)
			add_to_worklist(&dirrem->dm_list, 0);
		FREE_LOCK(&lk);
		if (xp == NULL) {
			vput(vp);
			handle_workitem_remove(dirrem, NULL);
		}
		return;
	}
	WORKLIST_INSERT(&inodedep->id_inowait, &dirrem->dm_list);
	FREE_LOCK(&lk);
	ip->i_flag |= IN_CHANGE;
out:
	ffs_update(vp, 0);
	if (xp == NULL)
		vput(vp);
}

/*
 * Inode de-allocation dependencies.
 * 
 * When an inode's link count is reduced to zero, it can be de-allocated. We
 * found it convenient to postpone de-allocation until after the inode is
 * written to disk with its new link count (zero).  At this point, all of the
 * on-disk inode's block pointers are nullified and, with careful dependency
 * list ordering, all dependencies related to the inode will be satisfied and
 * the corresponding dependency structures de-allocated.  So, if/when the
 * inode is reused, there will be no mixing of old dependencies with new
 * ones.  This artificial dependency is set up by the block de-allocation
 * procedure above (softdep_setup_freeblocks) and completed by the
 * following procedure.
 */
static void 
handle_workitem_freefile(freefile)
	struct freefile *freefile;
{
	struct workhead wkhd;
	struct fs *fs;
	struct inodedep *idp;
	struct ufsmount *ump;
	int error;

	ump = VFSTOUFS(freefile->fx_list.wk_mp);
	fs = ump->um_fs;
#ifdef DEBUG
	ACQUIRE_LOCK(&lk);
	error = inodedep_lookup(UFSTOVFS(ump), freefile->fx_oldinum, 0, &idp);
	FREE_LOCK(&lk);
	if (error)
		panic("handle_workitem_freefile: inodedep %p survived", idp);
#endif
	UFS_LOCK(ump);
	fs->fs_pendinginodes -= 1;
	UFS_UNLOCK(ump);
	LIST_INIT(&wkhd);
	LIST_SWAP(&freefile->fx_jwork, &wkhd, worklist, wk_list);
	if ((error = ffs_freefile(ump, fs, freefile->fx_devvp,
	    freefile->fx_oldinum, freefile->fx_mode, &wkhd)) != 0)
		softdep_error("handle_workitem_freefile", error);
	ACQUIRE_LOCK(&lk);
	WORKITEM_FREE(freefile, D_FREEFILE);
	FREE_LOCK(&lk);
}


/*
 * Helper function which unlinks marker element from work list and returns
 * the next element on the list.
 */
static __inline struct worklist *
markernext(struct worklist *marker)
{
	struct worklist *next;
	
	next = LIST_NEXT(marker, wk_list);
	LIST_REMOVE(marker, wk_list);
	return next;
}

/*
 * Disk writes.
 * 
 * The dependency structures constructed above are most actively used when file
 * system blocks are written to disk.  No constraints are placed on when a
 * block can be written, but unsatisfied update dependencies are made safe by
 * modifying (or replacing) the source memory for the duration of the disk
 * write.  When the disk write completes, the memory block is again brought
 * up-to-date.
 *
 * In-core inode structure reclamation.
 * 
 * Because there are a finite number of "in-core" inode structures, they are
 * reused regularly.  By transferring all inode-related dependencies to the
 * in-memory inode block and indexing them separately (via "inodedep"s), we
 * can allow "in-core" inode structures to be reused at any time and avoid
 * any increase in contention.
 *
 * Called just before entering the device driver to initiate a new disk I/O.
 * The buffer must be locked, thus, no I/O completion operations can occur
 * while we are manipulating its associated dependencies.
 */
static void 
softdep_disk_io_initiation(bp)
	struct buf *bp;		/* structure describing disk write to occur */
{
	struct worklist *wk;
	struct worklist marker;
	struct inodedep *inodedep;
	struct freeblks *freeblks;
	struct jfreeblk *jfreeblk;
	struct newblk *newblk;

	/*
	 * We only care about write operations. There should never
	 * be dependencies for reads.
	 */
	if (bp->b_iocmd != BIO_WRITE)
		panic("softdep_disk_io_initiation: not write");

	if (bp->b_vflags & BV_BKGRDINPROG)
		panic("softdep_disk_io_initiation: Writing buffer with "
		    "background write in progress: %p", bp);

	marker.wk_type = D_LAST + 1;	/* Not a normal workitem */
	PHOLD(curproc);			/* Don't swap out kernel stack */

	ACQUIRE_LOCK(&lk);
	/*
	 * Do any necessary pre-I/O processing.
	 */
	for (wk = LIST_FIRST(&bp->b_dep); wk != NULL;
	     wk = markernext(&marker)) {
		LIST_INSERT_AFTER(wk, &marker, wk_list);
		switch (wk->wk_type) {

		case D_PAGEDEP:
			initiate_write_filepage(WK_PAGEDEP(wk), bp);
			continue;

		case D_INODEDEP:
			inodedep = WK_INODEDEP(wk);
			if (inodedep->id_fs->fs_magic == FS_UFS1_MAGIC)
				initiate_write_inodeblock_ufs1(inodedep, bp);
			else
				initiate_write_inodeblock_ufs2(inodedep, bp);
			continue;

		case D_INDIRDEP:
			initiate_write_indirdep(WK_INDIRDEP(wk), bp);
			continue;

		case D_BMSAFEMAP:
			initiate_write_bmsafemap(WK_BMSAFEMAP(wk), bp);
			continue;

		case D_JSEG:
			WK_JSEG(wk)->js_buf = NULL;
			continue;

		case D_FREEBLKS:
			freeblks = WK_FREEBLKS(wk);
			jfreeblk = LIST_FIRST(&freeblks->fb_jfreeblkhd);
			/*
			 * We have to wait for the jfreeblks to be journaled
			 * before we can write an inodeblock with updated
			 * pointers.  Be careful to arrange the marker so
			 * we revisit the jfreeblk if it's not removed by
			 * the first jwait().
			 */
			if (jfreeblk != NULL) {
				LIST_REMOVE(&marker, wk_list);
				LIST_INSERT_BEFORE(wk, &marker, wk_list);
				jwait(&jfreeblk->jf_list);
			}
			continue;
		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
			/*
			 * We have to wait for the jnewblk to be journaled
			 * before we can write to a block otherwise the
			 * contents may be confused with an earlier file
			 * at recovery time.  Handle the marker as described
			 * above.
			 */
			newblk = WK_NEWBLK(wk);
			if (newblk->nb_jnewblk != NULL) {
				LIST_REMOVE(&marker, wk_list);
				LIST_INSERT_BEFORE(wk, &marker, wk_list);
				jwait(&newblk->nb_jnewblk->jn_list);
			}
			continue;

		case D_SBDEP:
			initiate_write_sbdep(WK_SBDEP(wk));
			continue;

		case D_MKDIR:
		case D_FREEWORK:
		case D_FREEDEP:
		case D_JSEGDEP:
			continue;

		default:
			panic("handle_disk_io_initiation: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	FREE_LOCK(&lk);
	PRELE(curproc);			/* Allow swapout of kernel stack */
}

/*
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in a directory. The buffer must be locked,
 * thus, no I/O completion operations can occur while we are
 * manipulating its associated dependencies.
 */
static void
initiate_write_filepage(pagedep, bp)
	struct pagedep *pagedep;
	struct buf *bp;
{
	struct jremref *jremref;
	struct jmvref *jmvref;
	struct dirrem *dirrem;
	struct diradd *dap;
	struct direct *ep;
	int i;

	if (pagedep->pd_state & IOSTARTED) {
		/*
		 * This can only happen if there is a driver that does not
		 * understand chaining. Here biodone will reissue the call
		 * to strategy for the incomplete buffers.
		 */
		printf("initiate_write_filepage: already started\n");
		return;
	}
	pagedep->pd_state |= IOSTARTED;
	/*
	 * Wait for all journal remove dependencies to hit the disk.
	 * We can not allow any potentially conflicting directory adds
	 * to be visible before removes and rollback is too difficult.
	 * lk may be dropped and re-acquired, however we hold the buf
	 * locked so the dependency can not go away.
	 */
	LIST_FOREACH(dirrem, &pagedep->pd_dirremhd, dm_next)
		while ((jremref = LIST_FIRST(&dirrem->dm_jremrefhd)) != NULL) {
			stat_jwait_filepage++;
			jwait(&jremref->jr_list);
		}
	while ((jmvref = LIST_FIRST(&pagedep->pd_jmvrefhd)) != NULL) {
		stat_jwait_filepage++;
		jwait(&jmvref->jm_list);
	}
	for (i = 0; i < DAHASHSZ; i++) {
		LIST_FOREACH(dap, &pagedep->pd_diraddhd[i], da_pdlist) {
			ep = (struct direct *)
			    ((char *)bp->b_data + dap->da_offset);
			if (ep->d_ino != dap->da_newinum)
				panic("%s: dir inum %d != new %d",
				    "initiate_write_filepage",
				    ep->d_ino, dap->da_newinum);
			if (dap->da_state & DIRCHG)
				ep->d_ino = dap->da_previous->dm_oldinum;
			else
				ep->d_ino = 0;
			dap->da_state &= ~ATTACHED;
			dap->da_state |= UNDONE;
		}
	}
}

/*
 * Version of initiate_write_inodeblock that handles UFS1 dinodes.
 * Note that any bug fixes made to this routine must be done in the
 * version found below.
 *
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in an inodeblock. The buffer must be
 * locked, thus, no I/O completion operations can occur while we
 * are manipulating its associated dependencies.
 */
static void 
initiate_write_inodeblock_ufs1(inodedep, bp)
	struct inodedep *inodedep;
	struct buf *bp;			/* The inode block */
{
	struct allocdirect *adp, *lastadp;
	struct ufs1_dinode *dp;
	struct ufs1_dinode *sip;
	struct inoref *inoref;
	struct fs *fs;
	ufs_lbn_t i;
#ifdef INVARIANTS
	ufs_lbn_t prevlbn = 0;
#endif
	int deplist;

	if (inodedep->id_state & IOSTARTED)
		panic("initiate_write_inodeblock_ufs1: already started");
	inodedep->id_state |= IOSTARTED;
	fs = inodedep->id_fs;
	dp = (struct ufs1_dinode *)bp->b_data +
	    ino_to_fsbo(fs, inodedep->id_ino);

	/*
	 * If we're on the unlinked list but have not yet written our
	 * next pointer initialize it here.
	 */
	if ((inodedep->id_state & (UNLINKED | UNLINKNEXT)) == UNLINKED) {
		struct inodedep *inon;

		inon = TAILQ_NEXT(inodedep, id_unlinked);
		dp->di_freelink = inon ? inon->id_ino : 0;
	}
	/*
	 * If the bitmap is not yet written, then the allocated
	 * inode cannot be written to disk.
	 */
	if ((inodedep->id_state & DEPCOMPLETE) == 0) {
		if (inodedep->id_savedino1 != NULL)
			panic("initiate_write_inodeblock_ufs1: I/O underway");
		FREE_LOCK(&lk);
		sip = malloc(sizeof(struct ufs1_dinode),
		    M_SAVEDINO, M_SOFTDEP_FLAGS);
		ACQUIRE_LOCK(&lk);
		inodedep->id_savedino1 = sip;
		*inodedep->id_savedino1 = *dp;
		bzero((caddr_t)dp, sizeof(struct ufs1_dinode));
		dp->di_gen = inodedep->id_savedino1->di_gen;
		dp->di_freelink = inodedep->id_savedino1->di_freelink;
		return;
	}
	/*
	 * If no dependencies, then there is nothing to roll back.
	 */
	inodedep->id_savedsize = dp->di_size;
	inodedep->id_savedextsize = 0;
	inodedep->id_savednlink = dp->di_nlink;
	if (TAILQ_EMPTY(&inodedep->id_inoupdt) &&
	    TAILQ_EMPTY(&inodedep->id_inoreflst))
		return;
	/*
	 * Revert the link count to that of the first unwritten journal entry.
	 */
	inoref = TAILQ_FIRST(&inodedep->id_inoreflst);
	if (inoref)
		dp->di_nlink = inoref->if_nlink;
	/*
	 * Set the dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef INVARIANTS
		if (deplist != 0 && prevlbn >= adp->ad_offset)
			panic("softdep_write_inodeblock: lbn order");
		prevlbn = adp->ad_offset;
		if (adp->ad_offset < NDADDR &&
		    dp->di_db[adp->ad_offset] != adp->ad_newblkno)
			panic("%s: direct pointer #%jd mismatch %d != %jd",
			    "softdep_write_inodeblock",
			    (intmax_t)adp->ad_offset,
			    dp->di_db[adp->ad_offset],
			    (intmax_t)adp->ad_newblkno);
		if (adp->ad_offset >= NDADDR &&
		    dp->di_ib[adp->ad_offset - NDADDR] != adp->ad_newblkno)
			panic("%s: indirect pointer #%jd mismatch %d != %jd",
			    "softdep_write_inodeblock",
			    (intmax_t)adp->ad_offset - NDADDR,
			    dp->di_ib[adp->ad_offset - NDADDR],
			    (intmax_t)adp->ad_newblkno);
		deplist |= 1 << adp->ad_offset;
		if ((adp->ad_state & ATTACHED) == 0)
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
#endif /* INVARIANTS */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		if (adp->ad_offset >= NDADDR)
			break;
		dp->di_db[adp->ad_offset] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_size = fs->fs_bsize * adp->ad_offset + adp->ad_oldsize;
		for (i = adp->ad_offset + 1; i < NDADDR; i++) {
#ifdef INVARIANTS
			if (dp->di_db[i] != 0 && (deplist & (1 << i)) == 0)
				panic("softdep_write_inodeblock: lost dep1");
#endif /* INVARIANTS */
			dp->di_db[i] = 0;
		}
		for (i = 0; i < NIADDR; i++) {
#ifdef INVARIANTS
			if (dp->di_ib[i] != 0 &&
			    (deplist & ((1 << NDADDR) << i)) == 0)
				panic("softdep_write_inodeblock: lost dep2");
#endif /* INVARIANTS */
			dp->di_ib[i] = 0;
		}
		return;
	}
	/*
	 * If we have zero'ed out the last allocated block of the file,
	 * roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_size <= (lastadp->ad_offset + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_offset; i >= 0; i--)
			if (dp->di_db[i] != 0)
				break;
		dp->di_size = (i + 1) * fs->fs_bsize;
	}
	/*
	 * The only dependencies are for indirect blocks.
	 *
	 * The file size for indirect block additions is not guaranteed.
	 * Such a guarantee would be non-trivial to achieve. The conventional
	 * synchronous write implementation also does not make this guarantee.
	 * Fsck should catch and fix discrepancies. Arguably, the file size
	 * can be over-estimated without destroying integrity when the file
	 * moves into the indirect blocks (i.e., is large). If we want to
	 * postpone fsck, we are stuck with this argument.
	 */
	for (; adp; adp = TAILQ_NEXT(adp, ad_next))
		dp->di_ib[adp->ad_offset - NDADDR] = 0;
}
		
/*
 * Version of initiate_write_inodeblock that handles UFS2 dinodes.
 * Note that any bug fixes made to this routine must be done in the
 * version found above.
 *
 * Called from within the procedure above to deal with unsatisfied
 * allocation dependencies in an inodeblock. The buffer must be
 * locked, thus, no I/O completion operations can occur while we
 * are manipulating its associated dependencies.
 */
static void 
initiate_write_inodeblock_ufs2(inodedep, bp)
	struct inodedep *inodedep;
	struct buf *bp;			/* The inode block */
{
	struct allocdirect *adp, *lastadp;
	struct ufs2_dinode *dp;
	struct ufs2_dinode *sip;
	struct inoref *inoref;
	struct fs *fs;
	ufs_lbn_t i;
#ifdef INVARIANTS
	ufs_lbn_t prevlbn = 0;
#endif
	int deplist;

	if (inodedep->id_state & IOSTARTED)
		panic("initiate_write_inodeblock_ufs2: already started");
	inodedep->id_state |= IOSTARTED;
	fs = inodedep->id_fs;
	dp = (struct ufs2_dinode *)bp->b_data +
	    ino_to_fsbo(fs, inodedep->id_ino);

	/*
	 * If we're on the unlinked list but have not yet written our
	 * next pointer initialize it here.
	 */
	if ((inodedep->id_state & (UNLINKED | UNLINKNEXT)) == UNLINKED) {
		struct inodedep *inon;

		inon = TAILQ_NEXT(inodedep, id_unlinked);
		dp->di_freelink = inon ? inon->id_ino : 0;
	}
	if ((inodedep->id_state & (UNLINKED | UNLINKNEXT)) ==
	    (UNLINKED | UNLINKNEXT)) {
		struct inodedep *inon;
		ino_t freelink;

		inon = TAILQ_NEXT(inodedep, id_unlinked);
		freelink = inon ? inon->id_ino : 0;
		if (freelink != dp->di_freelink)
			panic("ino %p(0x%X) %d, %d != %d",
			    inodedep, inodedep->id_state, inodedep->id_ino,
			    freelink, dp->di_freelink);
	}
	/*
	 * If the bitmap is not yet written, then the allocated
	 * inode cannot be written to disk.
	 */
	if ((inodedep->id_state & DEPCOMPLETE) == 0) {
		if (inodedep->id_savedino2 != NULL)
			panic("initiate_write_inodeblock_ufs2: I/O underway");
		FREE_LOCK(&lk);
		sip = malloc(sizeof(struct ufs2_dinode),
		    M_SAVEDINO, M_SOFTDEP_FLAGS);
		ACQUIRE_LOCK(&lk);
		inodedep->id_savedino2 = sip;
		*inodedep->id_savedino2 = *dp;
		bzero((caddr_t)dp, sizeof(struct ufs2_dinode));
		dp->di_gen = inodedep->id_savedino2->di_gen;
		dp->di_freelink = inodedep->id_savedino2->di_freelink;
		return;
	}
	/*
	 * If no dependencies, then there is nothing to roll back.
	 */
	inodedep->id_savedsize = dp->di_size;
	inodedep->id_savedextsize = dp->di_extsize;
	inodedep->id_savednlink = dp->di_nlink;
	if (TAILQ_EMPTY(&inodedep->id_inoupdt) &&
	    TAILQ_EMPTY(&inodedep->id_extupdt) &&
	    TAILQ_EMPTY(&inodedep->id_inoreflst))
		return;
	/*
	 * Revert the link count to that of the first unwritten journal entry.
	 */
	inoref = TAILQ_FIRST(&inodedep->id_inoreflst);
	if (inoref)
		dp->di_nlink = inoref->if_nlink;

	/*
	 * Set the ext data dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_extupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef INVARIANTS
		if (deplist != 0 && prevlbn >= adp->ad_offset)
			panic("softdep_write_inodeblock: lbn order");
		prevlbn = adp->ad_offset;
		if (dp->di_extb[adp->ad_offset] != adp->ad_newblkno)
			panic("%s: direct pointer #%jd mismatch %jd != %jd",
			    "softdep_write_inodeblock",
			    (intmax_t)adp->ad_offset,
			    (intmax_t)dp->di_extb[adp->ad_offset],
			    (intmax_t)adp->ad_newblkno);
		deplist |= 1 << adp->ad_offset;
		if ((adp->ad_state & ATTACHED) == 0)
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
#endif /* INVARIANTS */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the ext
	 * data which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_extupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		dp->di_extb[adp->ad_offset] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_extsize = fs->fs_bsize * adp->ad_offset + adp->ad_oldsize;
		for (i = adp->ad_offset + 1; i < NXADDR; i++) {
#ifdef INVARIANTS
			if (dp->di_extb[i] != 0 && (deplist & (1 << i)) == 0)
				panic("softdep_write_inodeblock: lost dep1");
#endif /* INVARIANTS */
			dp->di_extb[i] = 0;
		}
		lastadp = NULL;
		break;
	}
	/*
	 * If we have zero'ed out the last allocated block of the ext
	 * data, roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_extsize <= (lastadp->ad_offset + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_offset; i >= 0; i--)
			if (dp->di_extb[i] != 0)
				break;
		dp->di_extsize = (i + 1) * fs->fs_bsize;
	}
	/*
	 * Set the file data dependencies to busy.
	 */
	for (deplist = 0, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     adp = TAILQ_NEXT(adp, ad_next)) {
#ifdef INVARIANTS
		if (deplist != 0 && prevlbn >= adp->ad_offset)
			panic("softdep_write_inodeblock: lbn order");
		prevlbn = adp->ad_offset;
		if (adp->ad_offset < NDADDR &&
		    dp->di_db[adp->ad_offset] != adp->ad_newblkno)
			panic("%s: direct pointer #%jd mismatch %jd != %jd",
			    "softdep_write_inodeblock",
			    (intmax_t)adp->ad_offset,
			    (intmax_t)dp->di_db[adp->ad_offset],
			    (intmax_t)adp->ad_newblkno);
		if (adp->ad_offset >= NDADDR &&
		    dp->di_ib[adp->ad_offset - NDADDR] != adp->ad_newblkno)
			panic("%s indirect pointer #%jd mismatch %jd != %jd",
			    "softdep_write_inodeblock:",
			    (intmax_t)adp->ad_offset - NDADDR,
			    (intmax_t)dp->di_ib[adp->ad_offset - NDADDR],
			    (intmax_t)adp->ad_newblkno);
		deplist |= 1 << adp->ad_offset;
		if ((adp->ad_state & ATTACHED) == 0)
			panic("softdep_write_inodeblock: Unknown state 0x%x",
			    adp->ad_state);
#endif /* INVARIANTS */
		adp->ad_state &= ~ATTACHED;
		adp->ad_state |= UNDONE;
	}
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem.
	 */
	for (lastadp = NULL, adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp;
	     lastadp = adp, adp = TAILQ_NEXT(adp, ad_next)) {
		if (adp->ad_offset >= NDADDR)
			break;
		dp->di_db[adp->ad_offset] = adp->ad_oldblkno;
		/* keep going until hitting a rollback to a frag */
		if (adp->ad_oldsize == 0 || adp->ad_oldsize == fs->fs_bsize)
			continue;
		dp->di_size = fs->fs_bsize * adp->ad_offset + adp->ad_oldsize;
		for (i = adp->ad_offset + 1; i < NDADDR; i++) {
#ifdef INVARIANTS
			if (dp->di_db[i] != 0 && (deplist & (1 << i)) == 0)
				panic("softdep_write_inodeblock: lost dep2");
#endif /* INVARIANTS */
			dp->di_db[i] = 0;
		}
		for (i = 0; i < NIADDR; i++) {
#ifdef INVARIANTS
			if (dp->di_ib[i] != 0 &&
			    (deplist & ((1 << NDADDR) << i)) == 0)
				panic("softdep_write_inodeblock: lost dep3");
#endif /* INVARIANTS */
			dp->di_ib[i] = 0;
		}
		return;
	}
	/*
	 * If we have zero'ed out the last allocated block of the file,
	 * roll back the size to the last currently allocated block.
	 * We know that this last allocated block is a full-sized as
	 * we already checked for fragments in the loop above.
	 */
	if (lastadp != NULL &&
	    dp->di_size <= (lastadp->ad_offset + 1) * fs->fs_bsize) {
		for (i = lastadp->ad_offset; i >= 0; i--)
			if (dp->di_db[i] != 0)
				break;
		dp->di_size = (i + 1) * fs->fs_bsize;
	}
	/*
	 * The only dependencies are for indirect blocks.
	 *
	 * The file size for indirect block additions is not guaranteed.
	 * Such a guarantee would be non-trivial to achieve. The conventional
	 * synchronous write implementation also does not make this guarantee.
	 * Fsck should catch and fix discrepancies. Arguably, the file size
	 * can be over-estimated without destroying integrity when the file
	 * moves into the indirect blocks (i.e., is large). If we want to
	 * postpone fsck, we are stuck with this argument.
	 */
	for (; adp; adp = TAILQ_NEXT(adp, ad_next))
		dp->di_ib[adp->ad_offset - NDADDR] = 0;
}

/*
 * Cancel an indirdep as a result of truncation.  Release all of the
 * children allocindirs and place their journal work on the appropriate
 * list.
 */
static void
cancel_indirdep(indirdep, bp, inodedep, freeblks)
	struct indirdep *indirdep;
	struct buf *bp;
	struct inodedep *inodedep;
	struct freeblks *freeblks;
{
	struct allocindir *aip;

	/*
	 * None of the indirect pointers will ever be visible,
	 * so they can simply be tossed. GOINGAWAY ensures
	 * that allocated pointers will be saved in the buffer
	 * cache until they are freed. Note that they will
	 * only be able to be found by their physical address
	 * since the inode mapping the logical address will
	 * be gone. The save buffer used for the safe copy
	 * was allocated in setup_allocindir_phase2 using
	 * the physical address so it could be used for this
	 * purpose. Hence we swap the safe copy with the real
	 * copy, allowing the safe copy to be freed and holding
	 * on to the real copy for later use in indir_trunc.
	 */
	if (indirdep->ir_state & GOINGAWAY)
		panic("cancel_indirdep: already gone");
	if (indirdep->ir_state & ONDEPLIST) {
		indirdep->ir_state &= ~ONDEPLIST;
		LIST_REMOVE(indirdep, ir_next);
	}
	indirdep->ir_state |= GOINGAWAY;
	VFSTOUFS(indirdep->ir_list.wk_mp)->um_numindirdeps += 1;
	while ((aip = LIST_FIRST(&indirdep->ir_deplisthd)) != 0)
		cancel_allocindir(aip, inodedep, freeblks);
	while ((aip = LIST_FIRST(&indirdep->ir_donehd)) != 0)
		cancel_allocindir(aip, inodedep, freeblks);
	while ((aip = LIST_FIRST(&indirdep->ir_writehd)) != 0)
		cancel_allocindir(aip, inodedep, freeblks);
	while ((aip = LIST_FIRST(&indirdep->ir_completehd)) != 0)
		cancel_allocindir(aip, inodedep, freeblks);
	bcopy(bp->b_data, indirdep->ir_savebp->b_data, bp->b_bcount);
	WORKLIST_REMOVE(&indirdep->ir_list);
	WORKLIST_INSERT(&indirdep->ir_savebp->b_dep, &indirdep->ir_list);
	indirdep->ir_savebp = NULL;
}

/*
 * Free an indirdep once it no longer has new pointers to track.
 */
static void
free_indirdep(indirdep)
	struct indirdep *indirdep;
{

	KASSERT(LIST_EMPTY(&indirdep->ir_jwork),
	    ("free_indirdep: Journal work not empty."));
	KASSERT(LIST_EMPTY(&indirdep->ir_completehd),
	    ("free_indirdep: Complete head not empty."));
	KASSERT(LIST_EMPTY(&indirdep->ir_writehd),
	    ("free_indirdep: write head not empty."));
	KASSERT(LIST_EMPTY(&indirdep->ir_donehd),
	    ("free_indirdep: done head not empty."));
	KASSERT(LIST_EMPTY(&indirdep->ir_deplisthd),
	    ("free_indirdep: deplist head not empty."));
	KASSERT(indirdep->ir_savebp == NULL,
	    ("free_indirdep: %p ir_savebp != NULL", indirdep));
	KASSERT((indirdep->ir_state & ONDEPLIST) == 0,
	    ("free_indirdep: %p still on deplist.", indirdep));
	if (indirdep->ir_state & ONWORKLIST)
		WORKLIST_REMOVE(&indirdep->ir_list);
	WORKITEM_FREE(indirdep, D_INDIRDEP);
}

/*
 * Called before a write to an indirdep.  This routine is responsible for
 * rolling back pointers to a safe state which includes only those
 * allocindirs which have been completed.
 */
static void
initiate_write_indirdep(indirdep, bp)
	struct indirdep *indirdep;
	struct buf *bp;
{

	if (indirdep->ir_state & GOINGAWAY)
		panic("disk_io_initiation: indirdep gone");

	/*
	 * If there are no remaining dependencies, this will be writing
	 * the real pointers.
	 */
	if (LIST_EMPTY(&indirdep->ir_deplisthd))
		return;
	/*
	 * Replace up-to-date version with safe version.
	 */
	FREE_LOCK(&lk);
	indirdep->ir_saveddata = malloc(bp->b_bcount, M_INDIRDEP,
	    M_SOFTDEP_FLAGS);
	ACQUIRE_LOCK(&lk);
	indirdep->ir_state &= ~ATTACHED;
	indirdep->ir_state |= UNDONE;
	bcopy(bp->b_data, indirdep->ir_saveddata, bp->b_bcount);
	bcopy(indirdep->ir_savebp->b_data, bp->b_data,
	    bp->b_bcount);
}

/*
 * Called when an inode has been cleared in a cg bitmap.  This finally
 * eliminates any canceled jaddrefs
 */
void
softdep_setup_inofree(mp, bp, ino, wkhd)
	struct mount *mp;
	struct buf *bp;
	ino_t ino;
	struct workhead *wkhd;
{
	struct worklist *wk, *wkn;
	struct inodedep *inodedep;
	uint8_t *inosused;
	struct cg *cgp;
	struct fs *fs;

	ACQUIRE_LOCK(&lk);
	fs = VFSTOUFS(mp)->um_fs;
	cgp = (struct cg *)bp->b_data;
	inosused = cg_inosused(cgp);
	if (isset(inosused, ino % fs->fs_ipg))
		panic("softdep_setup_inofree: inode %d not freed.", ino);
	if (inodedep_lookup(mp, ino, 0, &inodedep))
		panic("softdep_setup_inofree: ino %d has existing inodedep %p",
		    ino, inodedep);
	if (wkhd) {
		LIST_FOREACH_SAFE(wk, wkhd, wk_list, wkn) {
			if (wk->wk_type != D_JADDREF)
				continue;
			WORKLIST_REMOVE(wk);
			/*
			 * We can free immediately even if the jaddref
			 * isn't attached in a background write as now
			 * the bitmaps are reconciled.
		 	 */
			wk->wk_state |= COMPLETE | ATTACHED;
			free_jaddref(WK_JADDREF(wk));
		}
		jwork_move(&bp->b_dep, wkhd);
	}
	FREE_LOCK(&lk);
}


/*
 * Called via ffs_blkfree() after a set of frags has been cleared from a cg
 * map.  Any dependencies waiting for the write to clear are added to the
 * buf's list and any jnewblks that are being canceled are discarded
 * immediately.
 */
void
softdep_setup_blkfree(mp, bp, blkno, frags, wkhd)
	struct mount *mp;
	struct buf *bp;
	ufs2_daddr_t blkno;
	int frags;
	struct workhead *wkhd;
{
	struct jnewblk *jnewblk;
	struct worklist *wk, *wkn;
#ifdef SUJ_DEBUG
	struct bmsafemap *bmsafemap;
	struct fs *fs;
	uint8_t *blksfree;
	struct cg *cgp;
	ufs2_daddr_t jstart;
	ufs2_daddr_t jend;
	ufs2_daddr_t end;
	long bno;
	int i;
#endif

	ACQUIRE_LOCK(&lk);
	/*
	 * Detach any jnewblks which have been canceled.  They must linger
	 * until the bitmap is cleared again by ffs_blkfree() to prevent
	 * an unjournaled allocation from hitting the disk.
	 */
	if (wkhd) {
		LIST_FOREACH_SAFE(wk, wkhd, wk_list, wkn) {
			if (wk->wk_type != D_JNEWBLK)
				continue;
			jnewblk = WK_JNEWBLK(wk);
			KASSERT(jnewblk->jn_state & GOINGAWAY,
			    ("softdep_setup_blkfree: jnewblk not canceled."));
			WORKLIST_REMOVE(wk);
#ifdef SUJ_DEBUG
			/*
			 * Assert that this block is free in the bitmap
			 * before we discard the jnewblk.
			 */
			fs = VFSTOUFS(mp)->um_fs;
			cgp = (struct cg *)bp->b_data;
			blksfree = cg_blksfree(cgp);
			bno = dtogd(fs, jnewblk->jn_blkno);
			for (i = jnewblk->jn_oldfrags;
			    i < jnewblk->jn_frags; i++) {
				if (isset(blksfree, bno + i))
					continue;
				panic("softdep_setup_blkfree: not free");
			}
#endif
			/*
			 * Even if it's not attached we can free immediately
			 * as the new bitmap is correct.
			 */
			wk->wk_state |= COMPLETE | ATTACHED;
			free_jnewblk(jnewblk);
		}
		/*
		 * The buf must be locked by the caller otherwise these could
		 * be added while it's being written and the write would
		 * complete them before they made it to disk.
		 */
		jwork_move(&bp->b_dep, wkhd);
	}

#ifdef SUJ_DEBUG
	/*
	 * Assert that we are not freeing a block which has an outstanding
	 * allocation dependency.
	 */
	fs = VFSTOUFS(mp)->um_fs;
	bmsafemap = bmsafemap_lookup(mp, bp, dtog(fs, blkno));
	end = blkno + frags;
	LIST_FOREACH(jnewblk, &bmsafemap->sm_jnewblkhd, jn_deps) {
		/*
		 * Don't match against blocks that will be freed when the
		 * background write is done.
		 */
		if ((jnewblk->jn_state & (ATTACHED | COMPLETE | DEPCOMPLETE)) ==
		    (COMPLETE | DEPCOMPLETE))
			continue;
		jstart = jnewblk->jn_blkno + jnewblk->jn_oldfrags;
		jend = jnewblk->jn_blkno + jnewblk->jn_frags;
		if ((blkno >= jstart && blkno < jend) ||
		    (end > jstart && end <= jend)) {
			printf("state 0x%X %jd - %d %d dep %p\n",
			    jnewblk->jn_state, jnewblk->jn_blkno,
			    jnewblk->jn_oldfrags, jnewblk->jn_frags,
			    jnewblk->jn_newblk);
			panic("softdep_setup_blkfree: "
			    "%jd-%jd(%d) overlaps with %jd-%jd",
			    blkno, end, frags, jstart, jend);
		}
	}
#endif
	FREE_LOCK(&lk);
}

static void 
initiate_write_bmsafemap(bmsafemap, bp)
	struct bmsafemap *bmsafemap;
	struct buf *bp;			/* The cg block. */
{
	struct jaddref *jaddref;
	struct jnewblk *jnewblk;
	uint8_t *inosused;
	uint8_t *blksfree;
	struct cg *cgp;
	struct fs *fs;
	int cleared;
	ino_t ino;
	long bno;
	int i;

	if (bmsafemap->sm_state & IOSTARTED)
		panic("initiate_write_bmsafemap: Already started\n");
	bmsafemap->sm_state |= IOSTARTED;
	/*
	 * Clear any inode allocations which are pending journal writes.
	 */
	if (LIST_FIRST(&bmsafemap->sm_jaddrefhd) != NULL) {
		cgp = (struct cg *)bp->b_data;
		fs = VFSTOUFS(bmsafemap->sm_list.wk_mp)->um_fs;
		inosused = cg_inosused(cgp);
		LIST_FOREACH(jaddref, &bmsafemap->sm_jaddrefhd, ja_bmdeps) {
			ino = jaddref->ja_ino % fs->fs_ipg;
			/*
			 * If this is a background copy the inode may not
			 * be marked used yet.
			 */
			if (isset(inosused, ino)) {
				if ((jaddref->ja_mode & IFMT) == IFDIR)
					cgp->cg_cs.cs_ndir--;
				cgp->cg_cs.cs_nifree++;
				clrbit(inosused, ino);
				jaddref->ja_state &= ~ATTACHED;
				jaddref->ja_state |= UNDONE;
				stat_jaddref++;
			} else if ((bp->b_xflags & BX_BKGRDMARKER) == 0)
				panic("initiate_write_bmsafemap: inode %d "
				    "marked free", jaddref->ja_ino);
		}
	}
	/*
	 * Clear any block allocations which are pending journal writes.
	 */
	if (LIST_FIRST(&bmsafemap->sm_jnewblkhd) != NULL) {
		cgp = (struct cg *)bp->b_data;
		fs = VFSTOUFS(bmsafemap->sm_list.wk_mp)->um_fs;
		blksfree = cg_blksfree(cgp);
		LIST_FOREACH(jnewblk, &bmsafemap->sm_jnewblkhd, jn_deps) {
			bno = dtogd(fs, jnewblk->jn_blkno);
			cleared = 0;
			for (i = jnewblk->jn_oldfrags; i < jnewblk->jn_frags;
			    i++) {
				if (isclr(blksfree, bno + i)) {
					cleared = 1;
					setbit(blksfree, bno + i);
				}
			}
			/*
			 * We may not clear the block if it's a background
			 * copy.  In that case there is no reason to detach
			 * it.
			 */
			if (cleared) {
				stat_jnewblk++;
				jnewblk->jn_state &= ~ATTACHED;
				jnewblk->jn_state |= UNDONE;
			} else if ((bp->b_xflags & BX_BKGRDMARKER) == 0)
				panic("initiate_write_bmsafemap: block %jd "
				    "marked free", jnewblk->jn_blkno);
		}
	}
	/*
	 * Move allocation lists to the written lists so they can be
	 * cleared once the block write is complete.
	 */
	LIST_SWAP(&bmsafemap->sm_inodedephd, &bmsafemap->sm_inodedepwr,
	    inodedep, id_deps);
	LIST_SWAP(&bmsafemap->sm_newblkhd, &bmsafemap->sm_newblkwr,
	    newblk, nb_deps);
}

/*
 * This routine is called during the completion interrupt
 * service routine for a disk write (from the procedure called
 * by the device driver to inform the filesystem caches of
 * a request completion).  It should be called early in this
 * procedure, before the block is made available to other
 * processes or other routines are called.
 *
 */
static void 
softdep_disk_write_complete(bp)
	struct buf *bp;		/* describes the completed disk write */
{
	struct worklist *wk;
	struct worklist *owk;
	struct workhead reattach;
	struct buf *sbp;

	/*
	 * If an error occurred while doing the write, then the data
	 * has not hit the disk and the dependencies cannot be unrolled.
	 */
	if ((bp->b_ioflags & BIO_ERROR) != 0 && (bp->b_flags & B_INVAL) == 0)
		return;
	LIST_INIT(&reattach);
	/*
	 * This lock must not be released anywhere in this code segment.
	 */
	sbp = NULL;
	owk = NULL;
	ACQUIRE_LOCK(&lk);
	while ((wk = LIST_FIRST(&bp->b_dep)) != NULL) {
		WORKLIST_REMOVE(wk);
		if (wk == owk)
			panic("duplicate worklist: %p\n", wk);
		owk = wk;
		switch (wk->wk_type) {

		case D_PAGEDEP:
			if (handle_written_filepage(WK_PAGEDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_INODEDEP:
			if (handle_written_inodeblock(WK_INODEDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_BMSAFEMAP:
			if (handle_written_bmsafemap(WK_BMSAFEMAP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_MKDIR:
			handle_written_mkdir(WK_MKDIR(wk), MKDIR_BODY);
			continue;

		case D_ALLOCDIRECT:
			wk->wk_state |= COMPLETE;
			handle_allocdirect_partdone(WK_ALLOCDIRECT(wk), NULL);
			continue;

		case D_ALLOCINDIR:
			wk->wk_state |= COMPLETE;
			handle_allocindir_partdone(WK_ALLOCINDIR(wk));
			continue;

		case D_INDIRDEP:
			if (handle_written_indirdep(WK_INDIRDEP(wk), bp, &sbp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		case D_FREEBLKS:
			wk->wk_state |= COMPLETE;
			if ((wk->wk_state & ALLCOMPLETE) == ALLCOMPLETE)
				add_to_worklist(wk, 1);
			continue;

		case D_FREEWORK:
			handle_written_freework(WK_FREEWORK(wk));
			break;

		case D_FREEDEP:
			free_freedep(WK_FREEDEP(wk));
			continue;

		case D_JSEGDEP:
			free_jsegdep(WK_JSEGDEP(wk));
			continue;

		case D_JSEG:
			handle_written_jseg(WK_JSEG(wk), bp);
			continue;

		case D_SBDEP:
			if (handle_written_sbdep(WK_SBDEP(wk), bp))
				WORKLIST_INSERT(&reattach, wk);
			continue;

		default:
			panic("handle_disk_write_complete: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	/*
	 * Reattach any requests that must be redone.
	 */
	while ((wk = LIST_FIRST(&reattach)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(&bp->b_dep, wk);
	}
	FREE_LOCK(&lk);
	if (sbp)
		brelse(sbp);
}

/*
 * Called from within softdep_disk_write_complete above. Note that
 * this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
static void 
handle_allocdirect_partdone(adp, wkhd)
	struct allocdirect *adp;	/* the completed allocdirect */
	struct workhead *wkhd;		/* Work to do when inode is writtne. */
{
	struct allocdirectlst *listhead;
	struct allocdirect *listadp;
	struct inodedep *inodedep;
	long bsize;

	if ((adp->ad_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	/*
	 * The on-disk inode cannot claim to be any larger than the last
	 * fragment that has been written. Otherwise, the on-disk inode
	 * might have fragments that were not the last block in the file
	 * which would corrupt the filesystem. Thus, we cannot free any
	 * allocdirects after one whose ad_oldblkno claims a fragment as
	 * these blocks must be rolled back to zero before writing the inode.
	 * We check the currently active set of allocdirects in id_inoupdt
	 * or id_extupdt as appropriate.
	 */
	inodedep = adp->ad_inodedep;
	bsize = inodedep->id_fs->fs_bsize;
	if (adp->ad_state & EXTDATA)
		listhead = &inodedep->id_extupdt;
	else
		listhead = &inodedep->id_inoupdt;
	TAILQ_FOREACH(listadp, listhead, ad_next) {
		/* found our block */
		if (listadp == adp)
			break;
		/* continue if ad_oldlbn is not a fragment */
		if (listadp->ad_oldsize == 0 ||
		    listadp->ad_oldsize == bsize)
			continue;
		/* hit a fragment */
		return;
	}
	/*
	 * If we have reached the end of the current list without
	 * finding the just finished dependency, then it must be
	 * on the future dependency list. Future dependencies cannot
	 * be freed until they are moved to the current list.
	 */
	if (listadp == NULL) {
#ifdef DEBUG
		if (adp->ad_state & EXTDATA)
			listhead = &inodedep->id_newextupdt;
		else
			listhead = &inodedep->id_newinoupdt;
		TAILQ_FOREACH(listadp, listhead, ad_next)
			/* found our block */
			if (listadp == adp)
				break;
		if (listadp == NULL)
			panic("handle_allocdirect_partdone: lost dep");
#endif /* DEBUG */
		return;
	}
	/*
	 * If we have found the just finished dependency, then queue
	 * it along with anything that follows it that is complete.
	 * Since the pointer has not yet been written in the inode
	 * as the dependency prevents it, place the allocdirect on the
	 * bufwait list where it will be freed once the pointer is
	 * valid.
	 */
	if (wkhd == NULL)
		wkhd = &inodedep->id_bufwait;
	for (; adp; adp = listadp) {
		listadp = TAILQ_NEXT(adp, ad_next);
		if ((adp->ad_state & ALLCOMPLETE) != ALLCOMPLETE)
			return;
		TAILQ_REMOVE(listhead, adp, ad_next);
		WORKLIST_INSERT(wkhd, &adp->ad_block.nb_list);
	}
}

/*
 * Called from within softdep_disk_write_complete above.  This routine
 * completes successfully written allocindirs.
 */
static void
handle_allocindir_partdone(aip)
	struct allocindir *aip;		/* the completed allocindir */
{
	struct indirdep *indirdep;

	if ((aip->ai_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	indirdep = aip->ai_indirdep;
	LIST_REMOVE(aip, ai_next);
	if (indirdep->ir_state & UNDONE) {
		LIST_INSERT_HEAD(&indirdep->ir_donehd, aip, ai_next);
		return;
	}
	if (indirdep->ir_state & UFS1FMT)
		((ufs1_daddr_t *)indirdep->ir_savebp->b_data)[aip->ai_offset] =
		    aip->ai_newblkno;
	else
		((ufs2_daddr_t *)indirdep->ir_savebp->b_data)[aip->ai_offset] =
		    aip->ai_newblkno;
	/*
	 * Await the pointer write before freeing the allocindir.
	 */
	LIST_INSERT_HEAD(&indirdep->ir_writehd, aip, ai_next);
}

/*
 * Release segments held on a jwork list.
 */
static void
handle_jwork(wkhd)
	struct workhead *wkhd;
{
	struct worklist *wk;

	while ((wk = LIST_FIRST(wkhd)) != NULL) {
		WORKLIST_REMOVE(wk);
		switch (wk->wk_type) {
		case D_JSEGDEP:
			free_jsegdep(WK_JSEGDEP(wk));
			continue;
		default:
			panic("handle_jwork: Unknown type %s\n",
			    TYPENAME(wk->wk_type));
		}
	}
}

/*
 * Handle the bufwait list on an inode when it is safe to release items
 * held there.  This normally happens after an inode block is written but
 * may be delayed and handled later if there are pending journal items that
 * are not yet safe to be released.
 */
static struct freefile *
handle_bufwait(inodedep, refhd)
	struct inodedep *inodedep;
	struct workhead *refhd;
{
	struct jaddref *jaddref;
	struct freefile *freefile;
	struct worklist *wk;

	freefile = NULL;
	while ((wk = LIST_FIRST(&inodedep->id_bufwait)) != NULL) {
		WORKLIST_REMOVE(wk);
		switch (wk->wk_type) {
		case D_FREEFILE:
			/*
			 * We defer adding freefile to the worklist
			 * until all other additions have been made to
			 * ensure that it will be done after all the
			 * old blocks have been freed.
			 */
			if (freefile != NULL)
				panic("handle_bufwait: freefile");
			freefile = WK_FREEFILE(wk);
			continue;

		case D_MKDIR:
			handle_written_mkdir(WK_MKDIR(wk), MKDIR_PARENT);
			continue;

		case D_DIRADD:
			diradd_inode_written(WK_DIRADD(wk), inodedep);
			continue;

		case D_FREEFRAG:
			wk->wk_state |= COMPLETE;
			if ((wk->wk_state & ALLCOMPLETE) == ALLCOMPLETE)
				add_to_worklist(wk, 0);
			continue;

		case D_DIRREM:
			wk->wk_state |= COMPLETE;
			add_to_worklist(wk, 0);
			continue;

		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
			free_newblk(WK_NEWBLK(wk));
			continue;

		case D_JNEWBLK:
			wk->wk_state |= COMPLETE;
			free_jnewblk(WK_JNEWBLK(wk));
			continue;

		/*
		 * Save freed journal segments and add references on
		 * the supplied list which will delay their release
		 * until the cg bitmap is cleared on disk.
		 */
		case D_JSEGDEP:
			if (refhd == NULL)
				free_jsegdep(WK_JSEGDEP(wk));
			else
				WORKLIST_INSERT(refhd, wk);
			continue;

		case D_JADDREF:
			jaddref = WK_JADDREF(wk);
			TAILQ_REMOVE(&inodedep->id_inoreflst, &jaddref->ja_ref,
			    if_deps);
			/*
			 * Transfer any jaddrefs to the list to be freed with
			 * the bitmap if we're handling a removed file.
			 */
			if (refhd == NULL) {
				wk->wk_state |= COMPLETE;
				free_jaddref(jaddref);
			} else
				WORKLIST_INSERT(refhd, wk);
			continue;

		default:
			panic("handle_bufwait: Unknown type %p(%s)",
			    wk, TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
	return (freefile);
}
/*
 * Called from within softdep_disk_write_complete above to restore
 * in-memory inode block contents to their most up-to-date state. Note
 * that this routine is always called from interrupt level with further
 * splbio interrupts blocked.
 */
static int 
handle_written_inodeblock(inodedep, bp)
	struct inodedep *inodedep;
	struct buf *bp;		/* buffer containing the inode block */
{
	struct freefile *freefile;
	struct allocdirect *adp, *nextadp;
	struct ufs1_dinode *dp1 = NULL;
	struct ufs2_dinode *dp2 = NULL;
	struct workhead wkhd;
	int hadchanges, fstype;
	ino_t freelink;

	LIST_INIT(&wkhd);
	hadchanges = 0;
	freefile = NULL;
	if ((inodedep->id_state & IOSTARTED) == 0)
		panic("handle_written_inodeblock: not started");
	inodedep->id_state &= ~IOSTARTED;
	if (inodedep->id_fs->fs_magic == FS_UFS1_MAGIC) {
		fstype = UFS1;
		dp1 = (struct ufs1_dinode *)bp->b_data +
		    ino_to_fsbo(inodedep->id_fs, inodedep->id_ino);
		freelink = dp1->di_freelink;
	} else {
		fstype = UFS2;
		dp2 = (struct ufs2_dinode *)bp->b_data +
		    ino_to_fsbo(inodedep->id_fs, inodedep->id_ino);
		freelink = dp2->di_freelink;
	}
	/*
	 * If we wrote a valid freelink pointer during the last write
	 * record it here.
	 */
	if ((inodedep->id_state & (UNLINKED | UNLINKNEXT)) == UNLINKED) {
		struct inodedep *inon;

		inon = TAILQ_NEXT(inodedep, id_unlinked);
		if ((inon == NULL && freelink == 0) ||
		    (inon && inon->id_ino == freelink)) {
			if (inon)
				inon->id_state |= UNLINKPREV;
			inodedep->id_state |= UNLINKNEXT;
		} else
			hadchanges = 1;
	}
	/* Leave this inodeblock dirty until it's in the list. */
	if ((inodedep->id_state & (UNLINKED | UNLINKONLIST)) == UNLINKED)
		hadchanges = 1;
	/*
	 * If we had to rollback the inode allocation because of
	 * bitmaps being incomplete, then simply restore it.
	 * Keep the block dirty so that it will not be reclaimed until
	 * all associated dependencies have been cleared and the
	 * corresponding updates written to disk.
	 */
	if (inodedep->id_savedino1 != NULL) {
		hadchanges = 1;
		if (fstype == UFS1)
			*dp1 = *inodedep->id_savedino1;
		else
			*dp2 = *inodedep->id_savedino2;
		free(inodedep->id_savedino1, M_SAVEDINO);
		inodedep->id_savedino1 = NULL;
		if ((bp->b_flags & B_DELWRI) == 0)
			stat_inode_bitmap++;
		bdirty(bp);
		/*
		 * If the inode is clear here and GOINGAWAY it will never
		 * be written.  Process the bufwait and clear any pending
		 * work which may include the freefile.
		 */
		if (inodedep->id_state & GOINGAWAY)
			goto bufwait;
		return (1);
	}
	inodedep->id_state |= COMPLETE;
	/*
	 * Roll forward anything that had to be rolled back before 
	 * the inode could be updated.
	 */
	for (adp = TAILQ_FIRST(&inodedep->id_inoupdt); adp; adp = nextadp) {
		nextadp = TAILQ_NEXT(adp, ad_next);
		if (adp->ad_state & ATTACHED)
			panic("handle_written_inodeblock: new entry");
		if (fstype == UFS1) {
			if (adp->ad_offset < NDADDR) {
				if (dp1->di_db[adp->ad_offset]!=adp->ad_oldblkno)
					panic("%s %s #%jd mismatch %d != %jd",
					    "handle_written_inodeblock:",
					    "direct pointer",
					    (intmax_t)adp->ad_offset,
					    dp1->di_db[adp->ad_offset],
					    (intmax_t)adp->ad_oldblkno);
				dp1->di_db[adp->ad_offset] = adp->ad_newblkno;
			} else {
				if (dp1->di_ib[adp->ad_offset - NDADDR] != 0)
					panic("%s: %s #%jd allocated as %d",
					    "handle_written_inodeblock",
					    "indirect pointer",
					    (intmax_t)adp->ad_offset - NDADDR,
					    dp1->di_ib[adp->ad_offset - NDADDR]);
				dp1->di_ib[adp->ad_offset - NDADDR] =
				    adp->ad_newblkno;
			}
		} else {
			if (adp->ad_offset < NDADDR) {
				if (dp2->di_db[adp->ad_offset]!=adp->ad_oldblkno)
					panic("%s: %s #%jd %s %jd != %jd",
					    "handle_written_inodeblock",
					    "direct pointer",
					    (intmax_t)adp->ad_offset, "mismatch",
					    (intmax_t)dp2->di_db[adp->ad_offset],
					    (intmax_t)adp->ad_oldblkno);
				dp2->di_db[adp->ad_offset] = adp->ad_newblkno;
			} else {
				if (dp2->di_ib[adp->ad_offset - NDADDR] != 0)
					panic("%s: %s #%jd allocated as %jd",
					    "handle_written_inodeblock",
					    "indirect pointer",
					    (intmax_t)adp->ad_offset - NDADDR,
					    (intmax_t)
					    dp2->di_ib[adp->ad_offset - NDADDR]);
				dp2->di_ib[adp->ad_offset - NDADDR] =
				    adp->ad_newblkno;
			}
		}
		adp->ad_state &= ~UNDONE;
		adp->ad_state |= ATTACHED;
		hadchanges = 1;
	}
	for (adp = TAILQ_FIRST(&inodedep->id_extupdt); adp; adp = nextadp) {
		nextadp = TAILQ_NEXT(adp, ad_next);
		if (adp->ad_state & ATTACHED)
			panic("handle_written_inodeblock: new entry");
		if (dp2->di_extb[adp->ad_offset] != adp->ad_oldblkno)
			panic("%s: direct pointers #%jd %s %jd != %jd",
			    "handle_written_inodeblock",
			    (intmax_t)adp->ad_offset, "mismatch",
			    (intmax_t)dp2->di_extb[adp->ad_offset],
			    (intmax_t)adp->ad_oldblkno);
		dp2->di_extb[adp->ad_offset] = adp->ad_newblkno;
		adp->ad_state &= ~UNDONE;
		adp->ad_state |= ATTACHED;
		hadchanges = 1;
	}
	if (hadchanges && (bp->b_flags & B_DELWRI) == 0)
		stat_direct_blk_ptrs++;
	/*
	 * Reset the file size to its most up-to-date value.
	 */
	if (inodedep->id_savedsize == -1 || inodedep->id_savedextsize == -1)
		panic("handle_written_inodeblock: bad size");
	if (inodedep->id_savednlink > LINK_MAX)
		panic("handle_written_inodeblock: Invalid link count "
		    "%d for inodedep %p", inodedep->id_savednlink, inodedep);
	if (fstype == UFS1) {
		if (dp1->di_nlink != inodedep->id_savednlink) { 
			dp1->di_nlink = inodedep->id_savednlink;
			hadchanges = 1;
		}
		if (dp1->di_size != inodedep->id_savedsize) {
			dp1->di_size = inodedep->id_savedsize;
			hadchanges = 1;
		}
	} else {
		if (dp2->di_nlink != inodedep->id_savednlink) { 
			dp2->di_nlink = inodedep->id_savednlink;
			hadchanges = 1;
		}
		if (dp2->di_size != inodedep->id_savedsize) {
			dp2->di_size = inodedep->id_savedsize;
			hadchanges = 1;
		}
		if (dp2->di_extsize != inodedep->id_savedextsize) {
			dp2->di_extsize = inodedep->id_savedextsize;
			hadchanges = 1;
		}
	}
	inodedep->id_savedsize = -1;
	inodedep->id_savedextsize = -1;
	inodedep->id_savednlink = -1;
	/*
	 * If there were any rollbacks in the inode block, then it must be
	 * marked dirty so that its will eventually get written back in
	 * its correct form.
	 */
	if (hadchanges)
		bdirty(bp);
bufwait:
	/*
	 * Process any allocdirects that completed during the update.
	 */
	if ((adp = TAILQ_FIRST(&inodedep->id_inoupdt)) != NULL)
		handle_allocdirect_partdone(adp, &wkhd);
	if ((adp = TAILQ_FIRST(&inodedep->id_extupdt)) != NULL)
		handle_allocdirect_partdone(adp, &wkhd);
	/*
	 * Process deallocations that were held pending until the
	 * inode had been written to disk. Freeing of the inode
	 * is delayed until after all blocks have been freed to
	 * avoid creation of new <vfsid, inum, lbn> triples
	 * before the old ones have been deleted.  Completely
	 * unlinked inodes are not processed until the unlinked
	 * inode list is written or the last reference is removed.
	 */
	if ((inodedep->id_state & (UNLINKED | UNLINKONLIST)) != UNLINKED) {
		freefile = handle_bufwait(inodedep, NULL);
		if (freefile && !LIST_EMPTY(&wkhd)) {
			WORKLIST_INSERT(&wkhd, &freefile->fx_list);
			freefile = NULL;
		}
	}
	/*
	 * Move rolled forward dependency completions to the bufwait list
	 * now that those that were already written have been processed.
	 */
	if (!LIST_EMPTY(&wkhd) && hadchanges == 0)
		panic("handle_written_inodeblock: bufwait but no changes");
	jwork_move(&inodedep->id_bufwait, &wkhd);

	if (freefile != NULL) {
		/*
		 * If the inode is goingaway it was never written.  Fake up
		 * the state here so free_inodedep() can succeed.
		 */
		if (inodedep->id_state & GOINGAWAY)
			inodedep->id_state |= COMPLETE | DEPCOMPLETE;
		if (free_inodedep(inodedep) == 0)
			panic("handle_written_inodeblock: live inodedep %p",
			    inodedep);
		add_to_worklist(&freefile->fx_list, 0);
		return (0);
	}

	/*
	 * If no outstanding dependencies, free it.
	 */
	if (free_inodedep(inodedep) ||
	    (TAILQ_FIRST(&inodedep->id_inoreflst) == 0 &&
	     TAILQ_FIRST(&inodedep->id_inoupdt) == 0 &&
	     TAILQ_FIRST(&inodedep->id_extupdt) == 0 &&
	     LIST_FIRST(&inodedep->id_bufwait) == 0))
		return (0);
	return (hadchanges);
}

static int
handle_written_indirdep(indirdep, bp, bpp)
	struct indirdep *indirdep;
	struct buf *bp;
	struct buf **bpp;
{
	struct allocindir *aip;
	int chgs;

	if (indirdep->ir_state & GOINGAWAY)
		panic("disk_write_complete: indirdep gone");
	chgs = 0;
	/*
	 * If there were rollbacks revert them here.
	 */
	if (indirdep->ir_saveddata) {
		bcopy(indirdep->ir_saveddata, bp->b_data, bp->b_bcount);
		free(indirdep->ir_saveddata, M_INDIRDEP);
		indirdep->ir_saveddata = 0;
		chgs = 1;
	}
	indirdep->ir_state &= ~UNDONE;
	indirdep->ir_state |= ATTACHED;
	/*
	 * Move allocindirs with written pointers to the completehd if
	 * the indirdep's pointer is not yet written.  Otherwise
	 * free them here.
	 */
	while ((aip = LIST_FIRST(&indirdep->ir_writehd)) != 0) {
		LIST_REMOVE(aip, ai_next);
		if ((indirdep->ir_state & DEPCOMPLETE) == 0) {
			LIST_INSERT_HEAD(&indirdep->ir_completehd, aip,
			    ai_next);
			continue;
		}
		free_newblk(&aip->ai_block);
	}
	/*
	 * Move allocindirs that have finished dependency processing from
	 * the done list to the write list after updating the pointers.
	 */
	while ((aip = LIST_FIRST(&indirdep->ir_donehd)) != 0) {
		handle_allocindir_partdone(aip);
		if (aip == LIST_FIRST(&indirdep->ir_donehd))
			panic("disk_write_complete: not gone");
		chgs = 1;
	}
	/*
	 * If this indirdep has been detached from its newblk during
	 * I/O we need to keep this dep attached to the buffer so
	 * deallocate_dependencies can find it and properly resolve
	 * any outstanding dependencies.
	 */
	if ((indirdep->ir_state & (ONDEPLIST | DEPCOMPLETE)) == 0)
		chgs = 1;
	if ((bp->b_flags & B_DELWRI) == 0)
		stat_indir_blk_ptrs++;
	/*
	 * If there were no changes we can discard the savedbp and detach
	 * ourselves from the buf.  We are only carrying completed pointers
	 * in this case.
	 */
	if (chgs == 0) {
		struct buf *sbp;

		sbp = indirdep->ir_savebp;
		sbp->b_flags |= B_INVAL | B_NOCACHE;
		indirdep->ir_savebp = NULL;
		if (*bpp != NULL)
			panic("handle_written_indirdep: bp already exists.");
		*bpp = sbp;
	} else
		bdirty(bp);
	/*
	 * If there are no fresh dependencies and none waiting on writes
	 * we can free the indirdep. 
	 */
	if ((indirdep->ir_state & DEPCOMPLETE) && chgs == 0) {
		if (indirdep->ir_state & ONDEPLIST)
			LIST_REMOVE(indirdep, ir_next);
		free_indirdep(indirdep);
		return (0);
	}

	return (chgs);
}

/*
 * Process a diradd entry after its dependent inode has been written.
 * This routine must be called with splbio interrupts blocked.
 */
static void
diradd_inode_written(dap, inodedep)
	struct diradd *dap;
	struct inodedep *inodedep;
{

	dap->da_state |= COMPLETE;
	complete_diradd(dap);
	WORKLIST_INSERT(&inodedep->id_pendinghd, &dap->da_list);
}

/*
 * Returns true if the bmsafemap will have rollbacks when written.  Must
 * only be called with lk and the buf lock on the cg held.
 */
static int
bmsafemap_rollbacks(bmsafemap)
	struct bmsafemap *bmsafemap;
{

	return (!LIST_EMPTY(&bmsafemap->sm_jaddrefhd) | 
	    !LIST_EMPTY(&bmsafemap->sm_jnewblkhd));
}

/*
 * Complete a write to a bmsafemap structure.  Roll forward any bitmap
 * changes if it's not a background write.  Set all written dependencies 
 * to DEPCOMPLETE and free the structure if possible.
 */
static int
handle_written_bmsafemap(bmsafemap, bp)
	struct bmsafemap *bmsafemap;
	struct buf *bp;
{
	struct newblk *newblk;
	struct inodedep *inodedep;
	struct jaddref *jaddref, *jatmp;
	struct jnewblk *jnewblk, *jntmp;
	uint8_t *inosused;
	uint8_t *blksfree;
	struct cg *cgp;
	struct fs *fs;
	ino_t ino;
	long bno;
	int chgs;
	int i;

	if ((bmsafemap->sm_state & IOSTARTED) == 0)
		panic("initiate_write_bmsafemap: Not started\n");
	chgs = 0;
	bmsafemap->sm_state &= ~IOSTARTED;
	/*
	 * Restore unwritten inode allocation pending jaddref writes.
	 */
	if (!LIST_EMPTY(&bmsafemap->sm_jaddrefhd)) {
		cgp = (struct cg *)bp->b_data;
		fs = VFSTOUFS(bmsafemap->sm_list.wk_mp)->um_fs;
		inosused = cg_inosused(cgp);
		LIST_FOREACH_SAFE(jaddref, &bmsafemap->sm_jaddrefhd,
		    ja_bmdeps, jatmp) {
			if ((jaddref->ja_state & UNDONE) == 0)
				continue;
			ino = jaddref->ja_ino % fs->fs_ipg;
			if (isset(inosused, ino))
				panic("handle_written_bmsafemap: "
				    "re-allocated inode");
			if ((bp->b_xflags & BX_BKGRDMARKER) == 0) {
				if ((jaddref->ja_mode & IFMT) == IFDIR)
					cgp->cg_cs.cs_ndir++;
				cgp->cg_cs.cs_nifree--;
				setbit(inosused, ino);
				chgs = 1;
			}
			jaddref->ja_state &= ~UNDONE;
			jaddref->ja_state |= ATTACHED;
			free_jaddref(jaddref);
		}
	}
	/*
	 * Restore any block allocations which are pending journal writes.
	 */
	if (LIST_FIRST(&bmsafemap->sm_jnewblkhd) != NULL) {
		cgp = (struct cg *)bp->b_data;
		fs = VFSTOUFS(bmsafemap->sm_list.wk_mp)->um_fs;
		blksfree = cg_blksfree(cgp);
		LIST_FOREACH_SAFE(jnewblk, &bmsafemap->sm_jnewblkhd, jn_deps,
		    jntmp) {
			if ((jnewblk->jn_state & UNDONE) == 0)
				continue;
			bno = dtogd(fs, jnewblk->jn_blkno);
			for (i = jnewblk->jn_oldfrags; i < jnewblk->jn_frags;
			    i++) {
				if (bp->b_xflags & BX_BKGRDMARKER)
					break;
				if ((jnewblk->jn_state & NEWBLOCK) == 0 &&
				    isclr(blksfree, bno + i))
					panic("handle_written_bmsafemap: "
					    "re-allocated fragment");
				clrbit(blksfree, bno + i);
				chgs = 1;
			}
			jnewblk->jn_state &= ~(UNDONE | NEWBLOCK);
			jnewblk->jn_state |= ATTACHED;
			free_jnewblk(jnewblk);
		}
	}
	while ((newblk = LIST_FIRST(&bmsafemap->sm_newblkwr))) {
		newblk->nb_state |= DEPCOMPLETE;
		newblk->nb_state &= ~ONDEPLIST;
		newblk->nb_bmsafemap = NULL;
		LIST_REMOVE(newblk, nb_deps);
		if (newblk->nb_list.wk_type == D_ALLOCDIRECT)
			handle_allocdirect_partdone(
			    WK_ALLOCDIRECT(&newblk->nb_list), NULL);
		else if (newblk->nb_list.wk_type == D_ALLOCINDIR)
			handle_allocindir_partdone(
			    WK_ALLOCINDIR(&newblk->nb_list));
		else if (newblk->nb_list.wk_type != D_NEWBLK)
			panic("handle_written_bmsafemap: Unexpected type: %s",
			    TYPENAME(newblk->nb_list.wk_type));
	}
	while ((inodedep = LIST_FIRST(&bmsafemap->sm_inodedepwr)) != NULL) {
		inodedep->id_state |= DEPCOMPLETE;
		inodedep->id_state &= ~ONDEPLIST;
		LIST_REMOVE(inodedep, id_deps);
		inodedep->id_bmsafemap = NULL;
	}
	if (LIST_EMPTY(&bmsafemap->sm_jaddrefhd) &&
	    LIST_EMPTY(&bmsafemap->sm_jnewblkhd) &&
	    LIST_EMPTY(&bmsafemap->sm_newblkhd) &&
	    LIST_EMPTY(&bmsafemap->sm_inodedephd)) {
		if (chgs)
			bdirty(bp);
		LIST_REMOVE(bmsafemap, sm_hash);
		WORKITEM_FREE(bmsafemap, D_BMSAFEMAP);
		return (0);
	}
	bdirty(bp);
	return (1);
}

/*
 * Try to free a mkdir dependency.
 */
static void
complete_mkdir(mkdir)
	struct mkdir *mkdir;
{
	struct diradd *dap;

	if ((mkdir->md_state & ALLCOMPLETE) != ALLCOMPLETE)
		return;
	LIST_REMOVE(mkdir, md_mkdirs);
	dap = mkdir->md_diradd;
	dap->da_state &= ~(mkdir->md_state & (MKDIR_PARENT | MKDIR_BODY));
	if ((dap->da_state & (MKDIR_PARENT | MKDIR_BODY)) == 0) {
		dap->da_state |= DEPCOMPLETE;
		complete_diradd(dap);
	}
	WORKITEM_FREE(mkdir, D_MKDIR);
}

/*
 * Handle the completion of a mkdir dependency.
 */
static void
handle_written_mkdir(mkdir, type)
	struct mkdir *mkdir;
	int type;
{

	if ((mkdir->md_state & (MKDIR_PARENT | MKDIR_BODY)) != type)
		panic("handle_written_mkdir: bad type");
	mkdir->md_state |= COMPLETE;
	complete_mkdir(mkdir);
}

static void
free_pagedep(pagedep)
	struct pagedep *pagedep;
{
	int i;

	if (pagedep->pd_state & (NEWBLOCK | ONWORKLIST))
		return;
	for (i = 0; i < DAHASHSZ; i++)
		if (!LIST_EMPTY(&pagedep->pd_diraddhd[i]))
			return;
	if (!LIST_EMPTY(&pagedep->pd_jmvrefhd))
		return;
	if (!LIST_EMPTY(&pagedep->pd_dirremhd))
		return;
	if (!LIST_EMPTY(&pagedep->pd_pendinghd))
		return;
	LIST_REMOVE(pagedep, pd_hash);
	WORKITEM_FREE(pagedep, D_PAGEDEP);
}

/*
 * Called from within softdep_disk_write_complete above.
 * A write operation was just completed. Removed inodes can
 * now be freed and associated block pointers may be committed.
 * Note that this routine is always called from interrupt level
 * with further splbio interrupts blocked.
 */
static int 
handle_written_filepage(pagedep, bp)
	struct pagedep *pagedep;
	struct buf *bp;		/* buffer containing the written page */
{
	struct dirrem *dirrem;
	struct diradd *dap, *nextdap;
	struct direct *ep;
	int i, chgs;

	if ((pagedep->pd_state & IOSTARTED) == 0)
		panic("handle_written_filepage: not started");
	pagedep->pd_state &= ~IOSTARTED;
	/*
	 * Process any directory removals that have been committed.
	 */
	while ((dirrem = LIST_FIRST(&pagedep->pd_dirremhd)) != NULL) {
		LIST_REMOVE(dirrem, dm_next);
		dirrem->dm_state |= COMPLETE;
		dirrem->dm_dirinum = pagedep->pd_ino;
		KASSERT(LIST_EMPTY(&dirrem->dm_jremrefhd),
		    ("handle_written_filepage: Journal entries not written."));
		add_to_worklist(&dirrem->dm_list, 0);
	}
	/*
	 * Free any directory additions that have been committed.
	 * If it is a newly allocated block, we have to wait until
	 * the on-disk directory inode claims the new block.
	 */
	if ((pagedep->pd_state & NEWBLOCK) == 0)
		while ((dap = LIST_FIRST(&pagedep->pd_pendinghd)) != NULL)
			free_diradd(dap, NULL);
	/*
	 * Uncommitted directory entries must be restored.
	 */
	for (chgs = 0, i = 0; i < DAHASHSZ; i++) {
		for (dap = LIST_FIRST(&pagedep->pd_diraddhd[i]); dap;
		     dap = nextdap) {
			nextdap = LIST_NEXT(dap, da_pdlist);
			if (dap->da_state & ATTACHED)
				panic("handle_written_filepage: attached");
			ep = (struct direct *)
			    ((char *)bp->b_data + dap->da_offset);
			ep->d_ino = dap->da_newinum;
			dap->da_state &= ~UNDONE;
			dap->da_state |= ATTACHED;
			chgs = 1;
			/*
			 * If the inode referenced by the directory has
			 * been written out, then the dependency can be
			 * moved to the pending list.
			 */
			if ((dap->da_state & ALLCOMPLETE) == ALLCOMPLETE) {
				LIST_REMOVE(dap, da_pdlist);
				LIST_INSERT_HEAD(&pagedep->pd_pendinghd, dap,
				    da_pdlist);
			}
		}
	}
	/*
	 * If there were any rollbacks in the directory, then it must be
	 * marked dirty so that its will eventually get written back in
	 * its correct form.
	 */
	if (chgs) {
		if ((bp->b_flags & B_DELWRI) == 0)
			stat_dir_entry++;
		bdirty(bp);
		return (1);
	}
	/*
	 * If we are not waiting for a new directory block to be
	 * claimed by its inode, then the pagedep will be freed.
	 * Otherwise it will remain to track any new entries on
	 * the page in case they are fsync'ed.
	 */
	if ((pagedep->pd_state & NEWBLOCK) == 0 &&
	    LIST_EMPTY(&pagedep->pd_jmvrefhd)) {
		LIST_REMOVE(pagedep, pd_hash);
		WORKITEM_FREE(pagedep, D_PAGEDEP);
	}
	return (0);
}

/*
 * Writing back in-core inode structures.
 * 
 * The filesystem only accesses an inode's contents when it occupies an
 * "in-core" inode structure.  These "in-core" structures are separate from
 * the page frames used to cache inode blocks.  Only the latter are
 * transferred to/from the disk.  So, when the updated contents of the
 * "in-core" inode structure are copied to the corresponding in-memory inode
 * block, the dependencies are also transferred.  The following procedure is
 * called when copying a dirty "in-core" inode to a cached inode block.
 */

/*
 * Called when an inode is loaded from disk. If the effective link count
 * differed from the actual link count when it was last flushed, then we
 * need to ensure that the correct effective link count is put back.
 */
void 
softdep_load_inodeblock(ip)
	struct inode *ip;	/* the "in_core" copy of the inode */
{
	struct inodedep *inodedep;

	/*
	 * Check for alternate nlink count.
	 */
	ip->i_effnlink = ip->i_nlink;
	ACQUIRE_LOCK(&lk);
	if (inodedep_lookup(UFSTOVFS(ip->i_ump), ip->i_number, 0,
	    &inodedep) == 0) {
		FREE_LOCK(&lk);
		return;
	}
	ip->i_effnlink -= inodedep->id_nlinkdelta;
	FREE_LOCK(&lk);
}

/*
 * This routine is called just before the "in-core" inode
 * information is to be copied to the in-memory inode block.
 * Recall that an inode block contains several inodes. If
 * the force flag is set, then the dependencies will be
 * cleared so that the update can always be made. Note that
 * the buffer is locked when this routine is called, so we
 * will never be in the middle of writing the inode block 
 * to disk.
 */
void 
softdep_update_inodeblock(ip, bp, waitfor)
	struct inode *ip;	/* the "in_core" copy of the inode */
	struct buf *bp;		/* the buffer containing the inode block */
	int waitfor;		/* nonzero => update must be allowed */
{
	struct inodedep *inodedep;
	struct inoref *inoref;
	struct worklist *wk;
	struct mount *mp;
	struct buf *ibp;
	struct fs *fs;
	int error;

	mp = UFSTOVFS(ip->i_ump);
	fs = ip->i_fs;
	/*
	 * Preserve the freelink that is on disk.  clear_unlinked_inodedep()
	 * does not have access to the in-core ip so must write directly into
	 * the inode block buffer when setting freelink.
	 */
	if (fs->fs_magic == FS_UFS1_MAGIC)
		DIP_SET(ip, i_freelink, ((struct ufs1_dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number))->di_freelink);
	else
		DIP_SET(ip, i_freelink, ((struct ufs2_dinode *)bp->b_data +
		    ino_to_fsbo(fs, ip->i_number))->di_freelink);
	/*
	 * If the effective link count is not equal to the actual link
	 * count, then we must track the difference in an inodedep while
	 * the inode is (potentially) tossed out of the cache. Otherwise,
	 * if there is no existing inodedep, then there are no dependencies
	 * to track.
	 */
	ACQUIRE_LOCK(&lk);
again:
	if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		if (ip->i_effnlink != ip->i_nlink)
			panic("softdep_update_inodeblock: bad link count");
		return;
	}
	if (inodedep->id_nlinkdelta != ip->i_nlink - ip->i_effnlink)
		panic("softdep_update_inodeblock: bad delta");
	/*
	 * If we're flushing all dependencies we must also move any waiting
	 * for journal writes onto the bufwait list prior to I/O.
	 */
	if (waitfor) {
		TAILQ_FOREACH(inoref, &inodedep->id_inoreflst, if_deps) {
			if ((inoref->if_state & (DEPCOMPLETE | GOINGAWAY))
			    == DEPCOMPLETE) {
				stat_jwait_inode++;
				jwait(&inoref->if_list);
				goto again;
			}
		}
	}
	/*
	 * Changes have been initiated. Anything depending on these
	 * changes cannot occur until this inode has been written.
	 */
	inodedep->id_state &= ~COMPLETE;
	if ((inodedep->id_state & ONWORKLIST) == 0)
		WORKLIST_INSERT(&bp->b_dep, &inodedep->id_list);
	/*
	 * Any new dependencies associated with the incore inode must 
	 * now be moved to the list associated with the buffer holding
	 * the in-memory copy of the inode. Once merged process any
	 * allocdirects that are completed by the merger.
	 */
	merge_inode_lists(&inodedep->id_newinoupdt, &inodedep->id_inoupdt);
	if (!TAILQ_EMPTY(&inodedep->id_inoupdt))
		handle_allocdirect_partdone(TAILQ_FIRST(&inodedep->id_inoupdt),
		    NULL);
	merge_inode_lists(&inodedep->id_newextupdt, &inodedep->id_extupdt);
	if (!TAILQ_EMPTY(&inodedep->id_extupdt))
		handle_allocdirect_partdone(TAILQ_FIRST(&inodedep->id_extupdt),
		    NULL);
	/*
	 * Now that the inode has been pushed into the buffer, the
	 * operations dependent on the inode being written to disk
	 * can be moved to the id_bufwait so that they will be
	 * processed when the buffer I/O completes.
	 */
	while ((wk = LIST_FIRST(&inodedep->id_inowait)) != NULL) {
		WORKLIST_REMOVE(wk);
		WORKLIST_INSERT(&inodedep->id_bufwait, wk);
	}
	/*
	 * Newly allocated inodes cannot be written until the bitmap
	 * that allocates them have been written (indicated by
	 * DEPCOMPLETE being set in id_state). If we are doing a
	 * forced sync (e.g., an fsync on a file), we force the bitmap
	 * to be written so that the update can be done.
	 */
	if (waitfor == 0) {
		FREE_LOCK(&lk);
		return;
	}
retry:
	if ((inodedep->id_state & (DEPCOMPLETE | GOINGAWAY)) != 0) {
		FREE_LOCK(&lk);
		return;
	}
	ibp = inodedep->id_bmsafemap->sm_buf;
	ibp = getdirtybuf(ibp, &lk, MNT_WAIT);
	if (ibp == NULL) {
		/*
		 * If ibp came back as NULL, the dependency could have been
		 * freed while we slept.  Look it up again, and check to see
		 * that it has completed.
		 */
		if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) != 0)
			goto retry;
		FREE_LOCK(&lk);
		return;
	}
	FREE_LOCK(&lk);
	if ((error = bwrite(ibp)) != 0)
		softdep_error("softdep_update_inodeblock: bwrite", error);
}

/*
 * Merge the a new inode dependency list (such as id_newinoupdt) into an
 * old inode dependency list (such as id_inoupdt). This routine must be
 * called with splbio interrupts blocked.
 */
static void
merge_inode_lists(newlisthead, oldlisthead)
	struct allocdirectlst *newlisthead;
	struct allocdirectlst *oldlisthead;
{
	struct allocdirect *listadp, *newadp;

	newadp = TAILQ_FIRST(newlisthead);
	for (listadp = TAILQ_FIRST(oldlisthead); listadp && newadp;) {
		if (listadp->ad_offset < newadp->ad_offset) {
			listadp = TAILQ_NEXT(listadp, ad_next);
			continue;
		}
		TAILQ_REMOVE(newlisthead, newadp, ad_next);
		TAILQ_INSERT_BEFORE(listadp, newadp, ad_next);
		if (listadp->ad_offset == newadp->ad_offset) {
			allocdirect_merge(oldlisthead, newadp,
			    listadp);
			listadp = newadp;
		}
		newadp = TAILQ_FIRST(newlisthead);
	}
	while ((newadp = TAILQ_FIRST(newlisthead)) != NULL) {
		TAILQ_REMOVE(newlisthead, newadp, ad_next);
		TAILQ_INSERT_TAIL(oldlisthead, newadp, ad_next);
	}
}

/*
 * If we are doing an fsync, then we must ensure that any directory
 * entries for the inode have been written after the inode gets to disk.
 */
int
softdep_fsync(vp)
	struct vnode *vp;	/* the "in_core" copy of the inode */
{
	struct inodedep *inodedep;
	struct pagedep *pagedep;
	struct inoref *inoref;
	struct worklist *wk;
	struct diradd *dap;
	struct mount *mp;
	struct vnode *pvp;
	struct inode *ip;
	struct buf *bp;
	struct fs *fs;
	struct thread *td = curthread;
	int error, flushparent, pagedep_new_block;
	ino_t parentino;
	ufs_lbn_t lbn;

	ip = VTOI(vp);
	fs = ip->i_fs;
	mp = vp->v_mount;
	ACQUIRE_LOCK(&lk);
restart:
	if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) == 0) {
		FREE_LOCK(&lk);
		return (0);
	}
	TAILQ_FOREACH(inoref, &inodedep->id_inoreflst, if_deps) {
		if ((inoref->if_state & (DEPCOMPLETE | GOINGAWAY))
		    == DEPCOMPLETE) {
			stat_jwait_inode++;
			jwait(&inoref->if_list);
			goto restart;
		}
	}
	if (!LIST_EMPTY(&inodedep->id_inowait) ||
	    !TAILQ_EMPTY(&inodedep->id_extupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newextupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_inoupdt) ||
	    !TAILQ_EMPTY(&inodedep->id_newinoupdt))
		panic("softdep_fsync: pending ops %p", inodedep);
	for (error = 0, flushparent = 0; ; ) {
		if ((wk = LIST_FIRST(&inodedep->id_pendinghd)) == NULL)
			break;
		if (wk->wk_type != D_DIRADD)
			panic("softdep_fsync: Unexpected type %s",
			    TYPENAME(wk->wk_type));
		dap = WK_DIRADD(wk);
		/*
		 * Flush our parent if this directory entry has a MKDIR_PARENT
		 * dependency or is contained in a newly allocated block.
		 */
		if (dap->da_state & DIRCHG)
			pagedep = dap->da_previous->dm_pagedep;
		else
			pagedep = dap->da_pagedep;
		parentino = pagedep->pd_ino;
		lbn = pagedep->pd_lbn;
		if ((dap->da_state & (MKDIR_BODY | COMPLETE)) != COMPLETE)
			panic("softdep_fsync: dirty");
		if ((dap->da_state & MKDIR_PARENT) ||
		    (pagedep->pd_state & NEWBLOCK))
			flushparent = 1;
		else
			flushparent = 0;
		/*
		 * If we are being fsync'ed as part of vgone'ing this vnode,
		 * then we will not be able to release and recover the
		 * vnode below, so we just have to give up on writing its
		 * directory entry out. It will eventually be written, just
		 * not now, but then the user was not asking to have it
		 * written, so we are not breaking any promises.
		 */
		if (vp->v_iflag & VI_DOOMED)
			break;
		/*
		 * We prevent deadlock by always fetching inodes from the
		 * root, moving down the directory tree. Thus, when fetching
		 * our parent directory, we first try to get the lock. If
		 * that fails, we must unlock ourselves before requesting
		 * the lock on our parent. See the comment in ufs_lookup
		 * for details on possible races.
		 */
		FREE_LOCK(&lk);
		if (ffs_vgetf(mp, parentino, LK_NOWAIT | LK_EXCLUSIVE, &pvp,
		    FFSV_FORCEINSMQ)) {
			error = vfs_busy(mp, MBF_NOWAIT);
			if (error != 0) {
				vfs_ref(mp);
				VOP_UNLOCK(vp, 0);
				error = vfs_busy(mp, 0);
				vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
				vfs_rel(mp);
				if (error != 0)
					return (ENOENT);
				if (vp->v_iflag & VI_DOOMED) {
					vfs_unbusy(mp);
					return (ENOENT);
				}
			}
			VOP_UNLOCK(vp, 0);
			error = ffs_vgetf(mp, parentino, LK_EXCLUSIVE,
			    &pvp, FFSV_FORCEINSMQ);
			vfs_unbusy(mp);
			vn_lock(vp, LK_EXCLUSIVE | LK_RETRY);
			if (vp->v_iflag & VI_DOOMED) {
				if (error == 0)
					vput(pvp);
				error = ENOENT;
			}
			if (error != 0)
				return (error);
		}
		/*
		 * All MKDIR_PARENT dependencies and all the NEWBLOCK pagedeps
		 * that are contained in direct blocks will be resolved by 
		 * doing a ffs_update. Pagedeps contained in indirect blocks
		 * may require a complete sync'ing of the directory. So, we
		 * try the cheap and fast ffs_update first, and if that fails,
		 * then we do the slower ffs_syncvnode of the directory.
		 */
		if (flushparent) {
			int locked;

			if ((error = ffs_update(pvp, 1)) != 0) {
				vput(pvp);
				return (error);
			}
			ACQUIRE_LOCK(&lk);
			locked = 1;
			if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) != 0) {
				if ((wk = LIST_FIRST(&inodedep->id_pendinghd)) != NULL) {
					if (wk->wk_type != D_DIRADD)
						panic("softdep_fsync: Unexpected type %s",
						      TYPENAME(wk->wk_type));
					dap = WK_DIRADD(wk);
					if (dap->da_state & DIRCHG)
						pagedep = dap->da_previous->dm_pagedep;
					else
						pagedep = dap->da_pagedep;
					pagedep_new_block = pagedep->pd_state & NEWBLOCK;
					FREE_LOCK(&lk);
					locked = 0;
					if (pagedep_new_block &&
					    (error = ffs_syncvnode(pvp, MNT_WAIT))) {
						vput(pvp);
						return (error);
					}
				}
			}
			if (locked)
				FREE_LOCK(&lk);
		}
		/*
		 * Flush directory page containing the inode's name.
		 */
		error = bread(pvp, lbn, blksize(fs, VTOI(pvp), lbn), td->td_ucred,
		    &bp);
		if (error == 0)
			error = bwrite(bp);
		else
			brelse(bp);
		vput(pvp);
		if (error != 0)
			return (error);
		ACQUIRE_LOCK(&lk);
		if (inodedep_lookup(mp, ip->i_number, 0, &inodedep) == 0)
			break;
	}
	FREE_LOCK(&lk);
	return (0);
}

/*
 * Flush all the dirty bitmaps associated with the block device
 * before flushing the rest of the dirty blocks so as to reduce
 * the number of dependencies that will have to be rolled back.
 */
void
softdep_fsync_mountdev(vp)
	struct vnode *vp;
{
	struct buf *bp, *nbp;
	struct worklist *wk;
	struct bufobj *bo;

	if (!vn_isdisk(vp, NULL))
		panic("softdep_fsync_mountdev: vnode not a disk");
	bo = &vp->v_bufobj;
restart:
	BO_LOCK(bo);
	ACQUIRE_LOCK(&lk);
	TAILQ_FOREACH_SAFE(bp, &bo->bo_dirty.bv_hd, b_bobufs, nbp) {
		/* 
		 * If it is already scheduled, skip to the next buffer.
		 */
		if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL))
			continue;

		if ((bp->b_flags & B_DELWRI) == 0)
			panic("softdep_fsync_mountdev: not dirty");
		/*
		 * We are only interested in bitmaps with outstanding
		 * dependencies.
		 */
		if ((wk = LIST_FIRST(&bp->b_dep)) == NULL ||
		    wk->wk_type != D_BMSAFEMAP ||
		    (bp->b_vflags & BV_BKGRDINPROG)) {
			BUF_UNLOCK(bp);
			continue;
		}
		FREE_LOCK(&lk);
		BO_UNLOCK(bo);
		bremfree(bp);
		(void) bawrite(bp);
		goto restart;
	}
	FREE_LOCK(&lk);
	drain_output(vp);
	BO_UNLOCK(bo);
}

/*
 * This routine is called when we are trying to synchronously flush a
 * file. This routine must eliminate any filesystem metadata dependencies
 * so that the syncing routine can succeed by pushing the dirty blocks
 * associated with the file. If any I/O errors occur, they are returned.
 */
int
softdep_sync_metadata(struct vnode *vp)
{
	struct pagedep *pagedep;
	struct allocindir *aip;
	struct newblk *newblk;
	struct buf *bp, *nbp;
	struct worklist *wk;
	struct bufobj *bo;
	int i, error, waitfor;

	if (!DOINGSOFTDEP(vp))
		return (0);
	/*
	 * Ensure that any direct block dependencies have been cleared.
	 */
	ACQUIRE_LOCK(&lk);
	if ((error = flush_inodedep_deps(vp->v_mount, VTOI(vp)->i_number))) {
		FREE_LOCK(&lk);
		return (error);
	}
	FREE_LOCK(&lk);
	/*
	 * For most files, the only metadata dependencies are the
	 * cylinder group maps that allocate their inode or blocks.
	 * The block allocation dependencies can be found by traversing
	 * the dependency lists for any buffers that remain on their
	 * dirty buffer list. The inode allocation dependency will
	 * be resolved when the inode is updated with MNT_WAIT.
	 * This work is done in two passes. The first pass grabs most
	 * of the buffers and begins asynchronously writing them. The
	 * only way to wait for these asynchronous writes is to sleep
	 * on the filesystem vnode which may stay busy for a long time
	 * if the filesystem is active. So, instead, we make a second
	 * pass over the dependencies blocking on each write. In the
	 * usual case we will be blocking against a write that we
	 * initiated, so when it is done the dependency will have been
	 * resolved. Thus the second pass is expected to end quickly.
	 */
	waitfor = MNT_NOWAIT;
	bo = &vp->v_bufobj;

top:
	/*
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 */
	BO_LOCK(bo);
	drain_output(vp);
	while ((bp = TAILQ_FIRST(&bo->bo_dirty.bv_hd)) != NULL) {
		bp = getdirtybuf(bp, BO_MTX(bo), MNT_WAIT);
		if (bp)
			break;
	}
	BO_UNLOCK(bo);
	if (bp == NULL)
		return (0);
loop:
	/* While syncing snapshots, we must allow recursive lookups */
	BUF_AREC(bp);
	ACQUIRE_LOCK(&lk);
	/*
	 * As we hold the buffer locked, none of its dependencies
	 * will disappear.
	 */
	LIST_FOREACH(wk, &bp->b_dep, wk_list) {
		switch (wk->wk_type) {

		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
			newblk = WK_NEWBLK(wk);
			if (newblk->nb_jnewblk != NULL) {
				stat_jwait_newblk++;
				jwait(&newblk->nb_jnewblk->jn_list);
				goto restart;
			}
			if (newblk->nb_state & DEPCOMPLETE)
				continue;
			nbp = newblk->nb_bmsafemap->sm_buf;
			nbp = getdirtybuf(nbp, &lk, waitfor);
			if (nbp == NULL)
				continue;
			FREE_LOCK(&lk);
			if (waitfor == MNT_NOWAIT) {
				bawrite(nbp);
			} else if ((error = bwrite(nbp)) != 0) {
				break;
			}
			ACQUIRE_LOCK(&lk);
			continue;

		case D_INDIRDEP:
		restart:

			LIST_FOREACH(aip,
			    &WK_INDIRDEP(wk)->ir_deplisthd, ai_next) {
				newblk = (struct newblk *)aip;
				if (newblk->nb_jnewblk != NULL) {
					stat_jwait_newblk++;
					jwait(&newblk->nb_jnewblk->jn_list);
					goto restart;
				}
				if (newblk->nb_state & DEPCOMPLETE)
					continue;
				nbp = newblk->nb_bmsafemap->sm_buf;
				nbp = getdirtybuf(nbp, &lk, MNT_WAIT);
				if (nbp == NULL)
					goto restart;
				FREE_LOCK(&lk);
				if ((error = bwrite(nbp)) != 0) {
					goto loop_end;
				}
				ACQUIRE_LOCK(&lk);
				goto restart;
			}
			continue;

		case D_PAGEDEP:
			/*
			 * We are trying to sync a directory that may
			 * have dependencies on both its own metadata
			 * and/or dependencies on the inodes of any
			 * recently allocated files. We walk its diradd
			 * lists pushing out the associated inode.
			 */
			pagedep = WK_PAGEDEP(wk);
			for (i = 0; i < DAHASHSZ; i++) {
				if (LIST_FIRST(&pagedep->pd_diraddhd[i]) == 0)
					continue;
				if ((error =
				    flush_pagedep_deps(vp, wk->wk_mp,
						&pagedep->pd_diraddhd[i]))) {
					FREE_LOCK(&lk);
					goto loop_end;
				}
			}
			continue;

		default:
			panic("softdep_sync_metadata: Unknown type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	loop_end:
		/* We reach here only in error and unlocked */
		if (error == 0)
			panic("softdep_sync_metadata: zero error");
		BUF_NOREC(bp);
		bawrite(bp);
		return (error);
	}
	FREE_LOCK(&lk);
	BO_LOCK(bo);
	while ((nbp = TAILQ_NEXT(bp, b_bobufs)) != NULL) {
		nbp = getdirtybuf(nbp, BO_MTX(bo), MNT_WAIT);
		if (nbp)
			break;
	}
	BO_UNLOCK(bo);
	BUF_NOREC(bp);
	bawrite(bp);
	if (nbp != NULL) {
		bp = nbp;
		goto loop;
	}
	/*
	 * The brief unlock is to allow any pent up dependency
	 * processing to be done. Then proceed with the second pass.
	 */
	if (waitfor == MNT_NOWAIT) {
		waitfor = MNT_WAIT;
		goto top;
	}

	/*
	 * If we have managed to get rid of all the dirty buffers,
	 * then we are done. For certain directories and block
	 * devices, we may need to do further work.
	 *
	 * We must wait for any I/O in progress to finish so that
	 * all potential buffers on the dirty list will be visible.
	 */
	BO_LOCK(bo);
	drain_output(vp);
	BO_UNLOCK(bo);
	return ffs_update(vp, 1);
	/* return (0); */
}

/*
 * Flush the dependencies associated with an inodedep.
 * Called with splbio blocked.
 */
static int
flush_inodedep_deps(mp, ino)
	struct mount *mp;
	ino_t ino;
{
	struct inodedep *inodedep;
	struct inoref *inoref;
	int error, waitfor;

	/*
	 * This work is done in two passes. The first pass grabs most
	 * of the buffers and begins asynchronously writing them. The
	 * only way to wait for these asynchronous writes is to sleep
	 * on the filesystem vnode which may stay busy for a long time
	 * if the filesystem is active. So, instead, we make a second
	 * pass over the dependencies blocking on each write. In the
	 * usual case we will be blocking against a write that we
	 * initiated, so when it is done the dependency will have been
	 * resolved. Thus the second pass is expected to end quickly.
	 * We give a brief window at the top of the loop to allow
	 * any pending I/O to complete.
	 */
	for (error = 0, waitfor = MNT_NOWAIT; ; ) {
		if (error)
			return (error);
		FREE_LOCK(&lk);
		ACQUIRE_LOCK(&lk);
restart:
		if (inodedep_lookup(mp, ino, 0, &inodedep) == 0)
			return (0);
		TAILQ_FOREACH(inoref, &inodedep->id_inoreflst, if_deps) {
			if ((inoref->if_state & (DEPCOMPLETE | GOINGAWAY))
			    == DEPCOMPLETE) {
				stat_jwait_inode++;
				jwait(&inoref->if_list);
				goto restart;
			}
		}
		if (flush_deplist(&inodedep->id_inoupdt, waitfor, &error) ||
		    flush_deplist(&inodedep->id_newinoupdt, waitfor, &error) ||
		    flush_deplist(&inodedep->id_extupdt, waitfor, &error) ||
		    flush_deplist(&inodedep->id_newextupdt, waitfor, &error))
			continue;
		/*
		 * If pass2, we are done, otherwise do pass 2.
		 */
		if (waitfor == MNT_WAIT)
			break;
		waitfor = MNT_WAIT;
	}
	/*
	 * Try freeing inodedep in case all dependencies have been removed.
	 */
	if (inodedep_lookup(mp, ino, 0, &inodedep) != 0)
		(void) free_inodedep(inodedep);
	return (0);
}

/*
 * Flush an inode dependency list.
 * Called with splbio blocked.
 */
static int
flush_deplist(listhead, waitfor, errorp)
	struct allocdirectlst *listhead;
	int waitfor;
	int *errorp;
{
	struct allocdirect *adp;
	struct newblk *newblk;
	struct buf *bp;

	mtx_assert(&lk, MA_OWNED);
	TAILQ_FOREACH(adp, listhead, ad_next) {
		newblk = (struct newblk *)adp;
		if (newblk->nb_jnewblk != NULL) {
			stat_jwait_newblk++;
			jwait(&newblk->nb_jnewblk->jn_list);
			return (1);
		}
		if (newblk->nb_state & DEPCOMPLETE)
			continue;
		bp = newblk->nb_bmsafemap->sm_buf;
		bp = getdirtybuf(bp, &lk, waitfor);
		if (bp == NULL) {
			if (waitfor == MNT_NOWAIT)
				continue;
			return (1);
		}
		FREE_LOCK(&lk);
		if (waitfor == MNT_NOWAIT) {
			bawrite(bp);
		} else if ((*errorp = bwrite(bp)) != 0) {
			ACQUIRE_LOCK(&lk);
			return (1);
		}
		ACQUIRE_LOCK(&lk);
		return (1);
	}
	return (0);
}

/*
 * Flush dependencies associated with an allocdirect block.
 */
static int
flush_newblk_dep(vp, mp, lbn)
	struct vnode *vp;
	struct mount *mp;
	ufs_lbn_t lbn;
{
	struct newblk *newblk;
	struct bufobj *bo;
	struct inode *ip;
	struct buf *bp;
	ufs2_daddr_t blkno;
	int error;

	error = 0;
	bo = &vp->v_bufobj;
	ip = VTOI(vp);
	blkno = DIP(ip, i_db[lbn]);
	if (blkno == 0)
		panic("flush_newblk_dep: Missing block");
	ACQUIRE_LOCK(&lk);
	/*
	 * Loop until all dependencies related to this block are satisfied.
	 * We must be careful to restart after each sleep in case a write
	 * completes some part of this process for us.
	 */
	for (;;) {
		if (newblk_lookup(mp, blkno, 0, &newblk) == 0) {
			FREE_LOCK(&lk);
			break;
		}
		if (newblk->nb_list.wk_type != D_ALLOCDIRECT)
			panic("flush_newblk_deps: Bad newblk %p", newblk);
		/*
		 * Flush the journal.
		 */
		if (newblk->nb_jnewblk != NULL) {
			stat_jwait_newblk++;
			jwait(&newblk->nb_jnewblk->jn_list);
			continue;
		}
		/*
		 * Write the bitmap dependency.
		 */
		if ((newblk->nb_state & DEPCOMPLETE) == 0) {
			bp = newblk->nb_bmsafemap->sm_buf;
			bp = getdirtybuf(bp, &lk, MNT_WAIT);
			if (bp == NULL)
				continue;
			FREE_LOCK(&lk);
			error = bwrite(bp);
			if (error)
				break;
			ACQUIRE_LOCK(&lk);
			continue;
		}
		/*
		 * Write the buffer.
		 */
		FREE_LOCK(&lk);
		BO_LOCK(bo);
		bp = gbincore(bo, lbn);
		if (bp != NULL) {
			error = BUF_LOCK(bp, LK_EXCLUSIVE | LK_SLEEPFAIL |
			    LK_INTERLOCK, BO_MTX(bo));
			if (error == ENOLCK) {
				ACQUIRE_LOCK(&lk);
				continue; /* Slept, retry */
			}
			if (error != 0)
				break;	/* Failed */
			if (bp->b_flags & B_DELWRI) {
				bremfree(bp);
				error = bwrite(bp);
				if (error)
					break;
			} else
				BUF_UNLOCK(bp);
		} else
			BO_UNLOCK(bo);
		/*
		 * We have to wait for the direct pointers to
		 * point at the newdirblk before the dependency
		 * will go away.
		 */
		error = ffs_update(vp, MNT_WAIT);
		if (error)
			break;
		ACQUIRE_LOCK(&lk);
	}
	return (error);
}

/*
 * Eliminate a pagedep dependency by flushing out all its diradd dependencies.
 * Called with splbio blocked.
 */
static int
flush_pagedep_deps(pvp, mp, diraddhdp)
	struct vnode *pvp;
	struct mount *mp;
	struct diraddhd *diraddhdp;
{
	struct inodedep *inodedep;
	struct inoref *inoref;
	struct ufsmount *ump;
	struct diradd *dap;
	struct vnode *vp;
	int error = 0;
	struct buf *bp;
	ino_t inum;

	ump = VFSTOUFS(mp);
restart:
	while ((dap = LIST_FIRST(diraddhdp)) != NULL) {
		/*
		 * Flush ourselves if this directory entry
		 * has a MKDIR_PARENT dependency.
		 */
		if (dap->da_state & MKDIR_PARENT) {
			FREE_LOCK(&lk);
			if ((error = ffs_update(pvp, MNT_WAIT)) != 0)
				break;
			ACQUIRE_LOCK(&lk);
			/*
			 * If that cleared dependencies, go on to next.
			 */
			if (dap != LIST_FIRST(diraddhdp))
				continue;
			if (dap->da_state & MKDIR_PARENT)
				panic("flush_pagedep_deps: MKDIR_PARENT");
		}
		/*
		 * A newly allocated directory must have its "." and
		 * ".." entries written out before its name can be
		 * committed in its parent. 
		 */
		inum = dap->da_newinum;
		if (inodedep_lookup(UFSTOVFS(ump), inum, 0, &inodedep) == 0)
			panic("flush_pagedep_deps: lost inode1");
		/*
		 * Wait for any pending journal adds to complete so we don't
		 * cause rollbacks while syncing.
		 */
		TAILQ_FOREACH(inoref, &inodedep->id_inoreflst, if_deps) {
			if ((inoref->if_state & (DEPCOMPLETE | GOINGAWAY))
			    == DEPCOMPLETE) {
				stat_jwait_inode++;
				jwait(&inoref->if_list);
				goto restart;
			}
		}
		if (dap->da_state & MKDIR_BODY) {
			FREE_LOCK(&lk);
			if ((error = ffs_vgetf(mp, inum, LK_EXCLUSIVE, &vp,
			    FFSV_FORCEINSMQ)))
				break;
			error = flush_newblk_dep(vp, mp, 0);
			/*
			 * If we still have the dependency we might need to
			 * update the vnode to sync the new link count to
			 * disk.
			 */
			if (error == 0 && dap == LIST_FIRST(diraddhdp))
				error = ffs_update(vp, MNT_WAIT);
			vput(vp);
			if (error != 0)
				break;
			ACQUIRE_LOCK(&lk);
			/*
			 * If that cleared dependencies, go on to next.
			 */
			if (dap != LIST_FIRST(diraddhdp))
				continue;
			if (dap->da_state & MKDIR_BODY) {
				inodedep_lookup(UFSTOVFS(ump), inum, 0,
				    &inodedep);
				panic("flush_pagedep_deps: MKDIR_BODY "
				    "inodedep %p dap %p vp %p",
				    inodedep, dap, vp);
			}
		}
		/*
		 * Flush the inode on which the directory entry depends.
		 * Having accounted for MKDIR_PARENT and MKDIR_BODY above,
		 * the only remaining dependency is that the updated inode
		 * count must get pushed to disk. The inode has already
		 * been pushed into its inode buffer (via VOP_UPDATE) at
		 * the time of the reference count change. So we need only
		 * locate that buffer, ensure that there will be no rollback
		 * caused by a bitmap dependency, then write the inode buffer.
		 */
retry:
		if (inodedep_lookup(UFSTOVFS(ump), inum, 0, &inodedep) == 0)
			panic("flush_pagedep_deps: lost inode");
		/*
		 * If the inode still has bitmap dependencies,
		 * push them to disk.
		 */
		if ((inodedep->id_state & (DEPCOMPLETE | GOINGAWAY)) == 0) {
			bp = inodedep->id_bmsafemap->sm_buf;
			bp = getdirtybuf(bp, &lk, MNT_WAIT);
			if (bp == NULL)
				goto retry;
			FREE_LOCK(&lk);
			if ((error = bwrite(bp)) != 0)
				break;
			ACQUIRE_LOCK(&lk);
			if (dap != LIST_FIRST(diraddhdp))
				continue;
		}
		/*
		 * If the inode is still sitting in a buffer waiting
		 * to be written or waiting for the link count to be
		 * adjusted update it here to flush it to disk.
		 */
		if (dap == LIST_FIRST(diraddhdp)) {
			FREE_LOCK(&lk);
			if ((error = ffs_vgetf(mp, inum, LK_EXCLUSIVE, &vp,
			    FFSV_FORCEINSMQ)))
				break;
			error = ffs_update(vp, MNT_WAIT);
			vput(vp);
			if (error)
				break;
			ACQUIRE_LOCK(&lk);
		}
		/*
		 * If we have failed to get rid of all the dependencies
		 * then something is seriously wrong.
		 */
		if (dap == LIST_FIRST(diraddhdp)) {
			inodedep_lookup(UFSTOVFS(ump), inum, 0, &inodedep);
			panic("flush_pagedep_deps: failed to flush " 
			    "inodedep %p ino %d dap %p", inodedep, inum, dap);
		}
	}
	if (error)
		ACQUIRE_LOCK(&lk);
	return (error);
}

/*
 * A large burst of file addition or deletion activity can drive the
 * memory load excessively high. First attempt to slow things down
 * using the techniques below. If that fails, this routine requests
 * the offending operations to fall back to running synchronously
 * until the memory load returns to a reasonable level.
 */
int
softdep_slowdown(vp)
	struct vnode *vp;
{
	struct ufsmount *ump;
	int jlow;
	int max_softdeps_hard;

	ACQUIRE_LOCK(&lk);
	jlow = 0;
	/*
	 * Check for journal space if needed.
	 */
	if (DOINGSUJ(vp)) {
		ump = VFSTOUFS(vp->v_mount);
		if (journal_space(ump, 0) == 0)
			jlow = 1;
	}
	max_softdeps_hard = max_softdeps * 11 / 10;
	if (num_dirrem < max_softdeps_hard / 2 &&
	    num_inodedep < max_softdeps_hard &&
	    VFSTOUFS(vp->v_mount)->um_numindirdeps < maxindirdeps &&
	    num_freeblkdep < max_softdeps_hard && jlow == 0) {
		FREE_LOCK(&lk);
  		return (0);
	}
	if (VFSTOUFS(vp->v_mount)->um_numindirdeps >= maxindirdeps || jlow)
		softdep_speedup();
	stat_sync_limit_hit += 1;
	FREE_LOCK(&lk);
	return (1);
}

/*
 * Called by the allocation routines when they are about to fail
 * in the hope that we can free up some disk space.
 * 
 * First check to see if the work list has anything on it. If it has,
 * clean up entries until we successfully free some space. Because this
 * process holds inodes locked, we cannot handle any remove requests
 * that might block on a locked inode as that could lead to deadlock.
 * If the worklist yields no free space, encourage the syncer daemon
 * to help us. In no event will we try for longer than tickdelay seconds.
 */
int
softdep_request_cleanup(fs, vp)
	struct fs *fs;
	struct vnode *vp;
{
	struct ufsmount *ump;
	long starttime;
	ufs2_daddr_t needed;
	int error;

	ump = VTOI(vp)->i_ump;
	mtx_assert(UFS_MTX(ump), MA_OWNED);
	needed = fs->fs_cstotal.cs_nbfree + fs->fs_contigsumsize;
	starttime = time_second + tickdelay;
	/*
	 * If we are being called because of a process doing a
	 * copy-on-write, then it is not safe to update the vnode
	 * as we may recurse into the copy-on-write routine.
	 */
	if (!(curthread->td_pflags & TDP_COWINPROGRESS)) {
		UFS_UNLOCK(ump);
		error = ffs_update(vp, 1);
		UFS_LOCK(ump);
		if (error != 0)
			return (0);
	}
	while (fs->fs_pendingblocks > 0 && fs->fs_cstotal.cs_nbfree <= needed) {
		if (time_second > starttime)
			return (0);
		UFS_UNLOCK(ump);
		ACQUIRE_LOCK(&lk);
		process_removes(vp);
		if (ump->softdep_on_worklist > 0 &&
		    process_worklist_item(UFSTOVFS(ump), LK_NOWAIT) != -1) {
			stat_worklist_push += 1;
			FREE_LOCK(&lk);
			UFS_LOCK(ump);
			continue;
		}
		request_cleanup(UFSTOVFS(ump), FLUSH_REMOVE_WAIT);
		FREE_LOCK(&lk);
		UFS_LOCK(ump);
	}
	return (1);
}

/*
 * If memory utilization has gotten too high, deliberately slow things
 * down and speed up the I/O processing.
 */
extern struct thread *syncertd;
static int
request_cleanup(mp, resource)
	struct mount *mp;
	int resource;
{
	struct thread *td = curthread;
	struct ufsmount *ump;

	mtx_assert(&lk, MA_OWNED);
	/*
	 * We never hold up the filesystem syncer or buf daemon.
	 */
	if (td->td_pflags & (TDP_SOFTDEP|TDP_NORUNNINGBUF))
		return (0);
	ump = VFSTOUFS(mp);
	/*
	 * First check to see if the work list has gotten backlogged.
	 * If it has, co-opt this process to help clean up two entries.
	 * Because this process may hold inodes locked, we cannot
	 * handle any remove requests that might block on a locked
	 * inode as that could lead to deadlock.  We set TDP_SOFTDEP
	 * to avoid recursively processing the worklist.
	 */
	if (ump->softdep_on_worklist > max_softdeps / 10) {
		td->td_pflags |= TDP_SOFTDEP;
		process_worklist_item(mp, LK_NOWAIT);
		process_worklist_item(mp, LK_NOWAIT);
		td->td_pflags &= ~TDP_SOFTDEP;
		stat_worklist_push += 2;
		return(1);
	}
	/*
	 * Next, we attempt to speed up the syncer process. If that
	 * is successful, then we allow the process to continue.
	 */
	if (softdep_speedup() && resource != FLUSH_REMOVE_WAIT)
		return(0);
	/*
	 * If we are resource constrained on inode dependencies, try
	 * flushing some dirty inodes. Otherwise, we are constrained
	 * by file deletions, so try accelerating flushes of directories
	 * with removal dependencies. We would like to do the cleanup
	 * here, but we probably hold an inode locked at this point and 
	 * that might deadlock against one that we try to clean. So,
	 * the best that we can do is request the syncer daemon to do
	 * the cleanup for us.
	 */
	switch (resource) {

	case FLUSH_INODES:
		stat_ino_limit_push += 1;
		req_clear_inodedeps += 1;
		stat_countp = &stat_ino_limit_hit;
		break;

	case FLUSH_REMOVE:
	case FLUSH_REMOVE_WAIT:
		stat_blk_limit_push += 1;
		req_clear_remove += 1;
		stat_countp = &stat_blk_limit_hit;
		break;

	default:
		panic("request_cleanup: unknown type");
	}
	/*
	 * Hopefully the syncer daemon will catch up and awaken us.
	 * We wait at most tickdelay before proceeding in any case.
	 */
	proc_waiting += 1;
	if (callout_pending(&softdep_callout) == FALSE)
		callout_reset(&softdep_callout, tickdelay > 2 ? tickdelay : 2,
		    pause_timer, 0);

	msleep((caddr_t)&proc_waiting, &lk, PPAUSE, "softupdate", 0);
	proc_waiting -= 1;
	return (1);
}

/*
 * Awaken processes pausing in request_cleanup and clear proc_waiting
 * to indicate that there is no longer a timer running.
 */
static void
pause_timer(arg)
	void *arg;
{

	/*
	 * The callout_ API has acquired mtx and will hold it around this
	 * function call.
	 */
	*stat_countp += 1;
	wakeup_one(&proc_waiting);
	if (proc_waiting > 0)
		callout_reset(&softdep_callout, tickdelay > 2 ? tickdelay : 2,
		    pause_timer, 0);
}

/*
 * Flush out a directory with at least one removal dependency in an effort to
 * reduce the number of dirrem, freefile, and freeblks dependency structures.
 */
static void
clear_remove(td)
	struct thread *td;
{
	struct pagedep_hashhead *pagedephd;
	struct pagedep *pagedep;
	static int next = 0;
	struct mount *mp;
	struct vnode *vp;
	struct bufobj *bo;
	int error, cnt;
	ino_t ino;

	mtx_assert(&lk, MA_OWNED);

	for (cnt = 0; cnt < pagedep_hash; cnt++) {
		pagedephd = &pagedep_hashtbl[next++];
		if (next >= pagedep_hash)
			next = 0;
		LIST_FOREACH(pagedep, pagedephd, pd_hash) {
			if (LIST_EMPTY(&pagedep->pd_dirremhd))
				continue;
			mp = pagedep->pd_list.wk_mp;
			ino = pagedep->pd_ino;
			if (vn_start_write(NULL, &mp, V_NOWAIT) != 0)
				continue;
			FREE_LOCK(&lk);

			/*
			 * Let unmount clear deps
			 */
			error = vfs_busy(mp, MBF_NOWAIT);
			if (error != 0)
				goto finish_write;
			error = ffs_vgetf(mp, ino, LK_EXCLUSIVE, &vp,
			     FFSV_FORCEINSMQ);
			vfs_unbusy(mp);
			if (error != 0) {
				softdep_error("clear_remove: vget", error);
				goto finish_write;
			}
			if ((error = ffs_syncvnode(vp, MNT_NOWAIT)))
				softdep_error("clear_remove: fsync", error);
			bo = &vp->v_bufobj;
			BO_LOCK(bo);
			drain_output(vp);
			BO_UNLOCK(bo);
			vput(vp);
		finish_write:
			vn_finished_write(mp);
			ACQUIRE_LOCK(&lk);
			return;
		}
	}
}

/*
 * Clear out a block of dirty inodes in an effort to reduce
 * the number of inodedep dependency structures.
 */
static void
clear_inodedeps(td)
	struct thread *td;
{
	struct inodedep_hashhead *inodedephd;
	struct inodedep *inodedep;
	static int next = 0;
	struct mount *mp;
	struct vnode *vp;
	struct fs *fs;
	int error, cnt;
	ino_t firstino, lastino, ino;

	mtx_assert(&lk, MA_OWNED);
	/*
	 * Pick a random inode dependency to be cleared.
	 * We will then gather up all the inodes in its block 
	 * that have dependencies and flush them out.
	 */
	for (cnt = 0; cnt < inodedep_hash; cnt++) {
		inodedephd = &inodedep_hashtbl[next++];
		if (next >= inodedep_hash)
			next = 0;
		if ((inodedep = LIST_FIRST(inodedephd)) != NULL)
			break;
	}
	if (inodedep == NULL)
		return;
	fs = inodedep->id_fs;
	mp = inodedep->id_list.wk_mp;
	/*
	 * Find the last inode in the block with dependencies.
	 */
	firstino = inodedep->id_ino & ~(INOPB(fs) - 1);
	for (lastino = firstino + INOPB(fs) - 1; lastino > firstino; lastino--)
		if (inodedep_lookup(mp, lastino, 0, &inodedep) != 0)
			break;
	/*
	 * Asynchronously push all but the last inode with dependencies.
	 * Synchronously push the last inode with dependencies to ensure
	 * that the inode block gets written to free up the inodedeps.
	 */
	for (ino = firstino; ino <= lastino; ino++) {
		if (inodedep_lookup(mp, ino, 0, &inodedep) == 0)
			continue;
		if (vn_start_write(NULL, &mp, V_NOWAIT) != 0)
			continue;
		FREE_LOCK(&lk);
		error = vfs_busy(mp, MBF_NOWAIT); /* Let unmount clear deps */
		if (error != 0) {
			vn_finished_write(mp);
			ACQUIRE_LOCK(&lk);
			return;
		}
		if ((error = ffs_vgetf(mp, ino, LK_EXCLUSIVE, &vp,
		    FFSV_FORCEINSMQ)) != 0) {
			softdep_error("clear_inodedeps: vget", error);
			vfs_unbusy(mp);
			vn_finished_write(mp);
			ACQUIRE_LOCK(&lk);
			return;
		}
		vfs_unbusy(mp);
		if (ino == lastino) {
			if ((error = ffs_syncvnode(vp, MNT_WAIT)))
				softdep_error("clear_inodedeps: fsync1", error);
		} else {
			if ((error = ffs_syncvnode(vp, MNT_NOWAIT)))
				softdep_error("clear_inodedeps: fsync2", error);
			BO_LOCK(&vp->v_bufobj);
			drain_output(vp);
			BO_UNLOCK(&vp->v_bufobj);
		}
		vput(vp);
		vn_finished_write(mp);
		ACQUIRE_LOCK(&lk);
	}
}

/*
 * Function to determine if the buffer has outstanding dependencies
 * that will cause a roll-back if the buffer is written. If wantcount
 * is set, return number of dependencies, otherwise just yes or no.
 */
static int
softdep_count_dependencies(bp, wantcount)
	struct buf *bp;
	int wantcount;
{
	struct worklist *wk;
	struct bmsafemap *bmsafemap;
	struct inodedep *inodedep;
	struct indirdep *indirdep;
	struct freeblks *freeblks;
	struct allocindir *aip;
	struct pagedep *pagedep;
	struct dirrem *dirrem;
	struct newblk *newblk;
	struct mkdir *mkdir;
	struct diradd *dap;
	int i, retval;

	retval = 0;
	ACQUIRE_LOCK(&lk);
	LIST_FOREACH(wk, &bp->b_dep, wk_list) {
		switch (wk->wk_type) {

		case D_INODEDEP:
			inodedep = WK_INODEDEP(wk);
			if ((inodedep->id_state & DEPCOMPLETE) == 0) {
				/* bitmap allocation dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			if (TAILQ_FIRST(&inodedep->id_inoupdt)) {
				/* direct block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			if (TAILQ_FIRST(&inodedep->id_extupdt)) {
				/* direct block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			if (TAILQ_FIRST(&inodedep->id_inoreflst)) {
				/* Add reference dependency. */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_INDIRDEP:
			indirdep = WK_INDIRDEP(wk);

			LIST_FOREACH(aip, &indirdep->ir_deplisthd, ai_next) {
				/* indirect block pointer dependency */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_PAGEDEP:
			pagedep = WK_PAGEDEP(wk);
			LIST_FOREACH(dirrem, &pagedep->pd_dirremhd, dm_next) {
				if (LIST_FIRST(&dirrem->dm_jremrefhd)) {
					/* Journal remove ref dependency. */
					retval += 1;
					if (!wantcount)
						goto out;
				}
			}
			for (i = 0; i < DAHASHSZ; i++) {

				LIST_FOREACH(dap, &pagedep->pd_diraddhd[i], da_pdlist) {
					/* directory entry dependency */
					retval += 1;
					if (!wantcount)
						goto out;
				}
			}
			continue;

		case D_BMSAFEMAP:
			bmsafemap = WK_BMSAFEMAP(wk);
			if (LIST_FIRST(&bmsafemap->sm_jaddrefhd)) {
				/* Add reference dependency. */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			if (LIST_FIRST(&bmsafemap->sm_jnewblkhd)) {
				/* Allocate block dependency. */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_FREEBLKS:
			freeblks = WK_FREEBLKS(wk);
			if (LIST_FIRST(&freeblks->fb_jfreeblkhd)) {
				/* Freeblk journal dependency. */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_ALLOCDIRECT:
		case D_ALLOCINDIR:
			newblk = WK_NEWBLK(wk);
			if (newblk->nb_jnewblk) {
				/* Journal allocate dependency. */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_MKDIR:
			mkdir = WK_MKDIR(wk);
			if (mkdir->md_jaddref) {
				/* Journal reference dependency. */
				retval += 1;
				if (!wantcount)
					goto out;
			}
			continue;

		case D_FREEWORK:
		case D_FREEDEP:
		case D_JSEGDEP:
		case D_JSEG:
		case D_SBDEP:
			/* never a dependency on these blocks */
			continue;

		default:
			panic("softdep_count_dependencies: Unexpected type %s",
			    TYPENAME(wk->wk_type));
			/* NOTREACHED */
		}
	}
out:
	FREE_LOCK(&lk);
	return retval;
}

/*
 * Acquire exclusive access to a buffer.
 * Must be called with a locked mtx parameter.
 * Return acquired buffer or NULL on failure.
 */
static struct buf *
getdirtybuf(bp, mtx, waitfor)
	struct buf *bp;
	struct mtx *mtx;
	int waitfor;
{
	int error;

	mtx_assert(mtx, MA_OWNED);
	if (BUF_LOCK(bp, LK_EXCLUSIVE | LK_NOWAIT, NULL) != 0) {
		if (waitfor != MNT_WAIT)
			return (NULL);
		error = BUF_LOCK(bp,
		    LK_EXCLUSIVE | LK_SLEEPFAIL | LK_INTERLOCK, mtx);
		/*
		 * Even if we sucessfully acquire bp here, we have dropped
		 * mtx, which may violates our guarantee.
		 */
		if (error == 0)
			BUF_UNLOCK(bp);
		else if (error != ENOLCK)
			panic("getdirtybuf: inconsistent lock: %d", error);
		mtx_lock(mtx);
		return (NULL);
	}
	if ((bp->b_vflags & BV_BKGRDINPROG) != 0) {
		if (mtx == &lk && waitfor == MNT_WAIT) {
			mtx_unlock(mtx);
			BO_LOCK(bp->b_bufobj);
			BUF_UNLOCK(bp);
			if ((bp->b_vflags & BV_BKGRDINPROG) != 0) {
				bp->b_vflags |= BV_BKGRDWAIT;
				msleep(&bp->b_xflags, BO_MTX(bp->b_bufobj),
				       PRIBIO | PDROP, "getbuf", 0);
			} else
				BO_UNLOCK(bp->b_bufobj);
			mtx_lock(mtx);
			return (NULL);
		}
		BUF_UNLOCK(bp);
		if (waitfor != MNT_WAIT)
			return (NULL);
		/*
		 * The mtx argument must be bp->b_vp's mutex in
		 * this case.
		 */
#ifdef	DEBUG_VFS_LOCKS
		if (bp->b_vp->v_type != VCHR)
			ASSERT_BO_LOCKED(bp->b_bufobj);
#endif
		bp->b_vflags |= BV_BKGRDWAIT;
		msleep(&bp->b_xflags, mtx, PRIBIO, "getbuf", 0);
		return (NULL);
	}
	if ((bp->b_flags & B_DELWRI) == 0) {
		BUF_UNLOCK(bp);
		return (NULL);
	}
	bremfree(bp);
	return (bp);
}


/*
 * Check if it is safe to suspend the file system now.  On entry,
 * the vnode interlock for devvp should be held.  Return 0 with
 * the mount interlock held if the file system can be suspended now,
 * otherwise return EAGAIN with the mount interlock held.
 */
int
softdep_check_suspend(struct mount *mp,
		      struct vnode *devvp,
		      int softdep_deps,
		      int softdep_accdeps,
		      int secondary_writes,
		      int secondary_accwrites)
{
	struct bufobj *bo;
	struct ufsmount *ump;
	int error;

	ump = VFSTOUFS(mp);
	bo = &devvp->v_bufobj;
	ASSERT_BO_LOCKED(bo);

	for (;;) {
		if (!TRY_ACQUIRE_LOCK(&lk)) {
			BO_UNLOCK(bo);
			ACQUIRE_LOCK(&lk);
			FREE_LOCK(&lk);
			BO_LOCK(bo);
			continue;
		}
		MNT_ILOCK(mp);
		if (mp->mnt_secondary_writes != 0) {
			FREE_LOCK(&lk);
			BO_UNLOCK(bo);
			msleep(&mp->mnt_secondary_writes,
			       MNT_MTX(mp),
			       (PUSER - 1) | PDROP, "secwr", 0);
			BO_LOCK(bo);
			continue;
		}
		break;
	}

	/*
	 * Reasons for needing more work before suspend:
	 * - Dirty buffers on devvp.
	 * - Softdep activity occurred after start of vnode sync loop
	 * - Secondary writes occurred after start of vnode sync loop
	 */
	error = 0;
	if (bo->bo_numoutput > 0 ||
	    bo->bo_dirty.bv_cnt > 0 ||
	    softdep_deps != 0 ||
	    ump->softdep_deps != 0 ||
	    softdep_accdeps != ump->softdep_accdeps ||
	    secondary_writes != 0 ||
	    mp->mnt_secondary_writes != 0 ||
	    secondary_accwrites != mp->mnt_secondary_accwrites)
		error = EAGAIN;
	FREE_LOCK(&lk);
	BO_UNLOCK(bo);
	return (error);
}


/*
 * Get the number of dependency structures for the file system, both
 * the current number and the total number allocated.  These will
 * later be used to detect that softdep processing has occurred.
 */
void
softdep_get_depcounts(struct mount *mp,
		      int *softdep_depsp,
		      int *softdep_accdepsp)
{
	struct ufsmount *ump;

	ump = VFSTOUFS(mp);
	ACQUIRE_LOCK(&lk);
	*softdep_depsp = ump->softdep_deps;
	*softdep_accdepsp = ump->softdep_accdeps;
	FREE_LOCK(&lk);
}

/*
 * Wait for pending output on a vnode to complete.
 * Must be called with vnode lock and interlock locked.
 *
 * XXX: Should just be a call to bufobj_wwait().
 */
static void
drain_output(vp)
	struct vnode *vp;
{
	struct bufobj *bo;

	bo = &vp->v_bufobj;
	ASSERT_VOP_LOCKED(vp, "drain_output");
	ASSERT_BO_LOCKED(bo);

	while (bo->bo_numoutput) {
		bo->bo_flag |= BO_WWAIT;
		msleep((caddr_t)&bo->bo_numoutput,
		    BO_MTX(bo), PRIBIO + 1, "drainvp", 0);
	}
}

/*
 * Called whenever a buffer that is being invalidated or reallocated
 * contains dependencies. This should only happen if an I/O error has
 * occurred. The routine is called with the buffer locked.
 */ 
static void
softdep_deallocate_dependencies(bp)
	struct buf *bp;
{

	if ((bp->b_ioflags & BIO_ERROR) == 0)
		panic("softdep_deallocate_dependencies: dangling deps");
	softdep_error(bp->b_vp->v_mount->mnt_stat.f_mntonname, bp->b_error);
	panic("softdep_deallocate_dependencies: unrecovered I/O error");
}

/*
 * Function to handle asynchronous write errors in the filesystem.
 */
static void
softdep_error(func, error)
	char *func;
	int error;
{

	/* XXX should do something better! */
	printf("%s: got error %d while accessing filesystem\n", func, error);
}

#ifdef DDB

static void
inodedep_print(struct inodedep *inodedep, int verbose)
{
	db_printf("%p fs %p st %x ino %jd inoblk %jd delta %d nlink %d"
	    " saveino %p\n",
	    inodedep, inodedep->id_fs, inodedep->id_state,
	    (intmax_t)inodedep->id_ino,
	    (intmax_t)fsbtodb(inodedep->id_fs,
	    ino_to_fsba(inodedep->id_fs, inodedep->id_ino)),
	    inodedep->id_nlinkdelta, inodedep->id_savednlink,
	    inodedep->id_savedino1);

	if (verbose == 0)
		return;

	db_printf("\tpendinghd %p, bufwait %p, inowait %p, inoreflst %p, "
	    "mkdiradd %p\n",
	    LIST_FIRST(&inodedep->id_pendinghd),
	    LIST_FIRST(&inodedep->id_bufwait),
	    LIST_FIRST(&inodedep->id_inowait),
	    TAILQ_FIRST(&inodedep->id_inoreflst),
	    inodedep->id_mkdiradd);
	db_printf("\tinoupdt %p, newinoupdt %p, extupdt %p, newextupdt %p\n",
	    TAILQ_FIRST(&inodedep->id_inoupdt),
	    TAILQ_FIRST(&inodedep->id_newinoupdt),
	    TAILQ_FIRST(&inodedep->id_extupdt),
	    TAILQ_FIRST(&inodedep->id_newextupdt));
}

DB_SHOW_COMMAND(inodedep, db_show_inodedep)
{

	if (have_addr == 0) {
		db_printf("Address required\n");
		return;
	}
	inodedep_print((struct inodedep*)addr, 1);
}

DB_SHOW_COMMAND(inodedeps, db_show_inodedeps)
{
	struct inodedep_hashhead *inodedephd;
	struct inodedep *inodedep;
	struct fs *fs;
	int cnt;

	fs = have_addr ? (struct fs *)addr : NULL;
	for (cnt = 0; cnt < inodedep_hash; cnt++) {
		inodedephd = &inodedep_hashtbl[cnt];
		LIST_FOREACH(inodedep, inodedephd, id_hash) {
			if (fs != NULL && fs != inodedep->id_fs)
				continue;
			inodedep_print(inodedep, 0);
		}
	}
}

DB_SHOW_COMMAND(worklist, db_show_worklist)
{
	struct worklist *wk;

	if (have_addr == 0) {
		db_printf("Address required\n");
		return;
	}
	wk = (struct worklist *)addr;
	printf("worklist: %p type %s state 0x%X\n",
	    wk, TYPENAME(wk->wk_type), wk->wk_state);
}

DB_SHOW_COMMAND(workhead, db_show_workhead)
{
	struct workhead *wkhd;
	struct worklist *wk;
	int i;

	if (have_addr == 0) {
		db_printf("Address required\n");
		return;
	}
	wkhd = (struct workhead *)addr;
	wk = LIST_FIRST(wkhd);
	for (i = 0; i < 100 && wk != NULL; i++, wk = LIST_NEXT(wk, wk_list))
		db_printf("worklist: %p type %s state 0x%X",
		    wk, TYPENAME(wk->wk_type), wk->wk_state);
	if (i == 100)
		db_printf("workhead overflow");
	printf("\n");
}


DB_SHOW_COMMAND(mkdirs, db_show_mkdirs)
{
	struct jaddref *jaddref;
	struct diradd *diradd;
	struct mkdir *mkdir;

	LIST_FOREACH(mkdir, &mkdirlisthd, md_mkdirs) {
		diradd = mkdir->md_diradd;
		db_printf("mkdir: %p state 0x%X dap %p state 0x%X",
		    mkdir, mkdir->md_state, diradd, diradd->da_state);
		if ((jaddref = mkdir->md_jaddref) != NULL)
			db_printf(" jaddref %p jaddref state 0x%X",
			    jaddref, jaddref->ja_state);
		db_printf("\n");
	}
}

#endif /* DDB */

#endif /* SOFTUPDATES */
