/*-
 * Copyright 1998, 2000 Marshall Kirk McKusick. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY MARSHALL KIRK MCKUSICK ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL MARSHALL KIRK MCKUSICK BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)softdep.h	9.7 (McKusick) 6/21/00
 * $FreeBSD: src/sys/ufs/ffs/softdep.h,v 1.19 2006/03/02 05:50:23 jeff Exp $
 */

#include <sys/queue.h>

/*
 * Allocation dependencies are handled with undo/redo on the in-memory
 * copy of the data. A particular data dependency is eliminated when
 * it is ALLCOMPLETE: that is ATTACHED, DEPCOMPLETE, and COMPLETE.
 * 
 * ATTACHED means that the data is not currently being written to
 * disk. UNDONE means that the data has been rolled back to a safe
 * state for writing to the disk. When the I/O completes, the data is
 * restored to its current form and the state reverts to ATTACHED.
 * The data must be locked throughout the rollback, I/O, and roll
 * forward so that the rolled back information is never visible to
 * user processes. The COMPLETE flag indicates that the item has been
 * written. For example, a dependency that requires that an inode be
 * written will be marked COMPLETE after the inode has been written
 * to disk. The DEPCOMPLETE flag indicates the completion of any other
 * dependencies such as the writing of a cylinder group map has been
 * completed. A dependency structure may be freed only when both it
 * and its dependencies have completed and any rollbacks that are in
 * progress have finished as indicated by the set of ALLCOMPLETE flags
 * all being set. The two MKDIR flags indicate additional dependencies
 * that must be done when creating a new directory. MKDIR_BODY is
 * cleared when the directory data block containing the "." and ".."
 * entries has been written. MKDIR_PARENT is cleared when the parent
 * inode with the increased link count for ".." has been written. When
 * both MKDIR flags have been cleared, the DEPCOMPLETE flag is set to
 * indicate that the directory dependencies have been completed. The
 * writing of the directory inode itself sets the COMPLETE flag which
 * then allows the directory entry for the new directory to be written
 * to disk. The RMDIR flag marks a dirrem structure as representing
 * the removal of a directory rather than a file. When the removal
 * dependencies are completed, additional work needs to be done
 * (truncation of the "." and ".." entries, an additional decrement
 * of the associated inode, and a decrement of the parent inode). The
 * DIRCHG flag marks a diradd structure as representing the changing
 * of an existing entry rather than the addition of a new one. When
 * the update is complete the dirrem associated with the inode for
 * the old name must be added to the worklist to do the necessary
 * reference count decrement. The GOINGAWAY flag indicates that the
 * data structure is frozen from further change until its dependencies
 * have been completed and its resources freed after which it will be
 * discarded. The IOSTARTED flag prevents multiple calls to the I/O
 * start routine from doing multiple rollbacks. The SPACECOUNTED flag
 * says that the files space has been accounted to the pending free
 * space count. The NEWBLOCK flag marks pagedep structures that have
 * just been allocated, so must be claimed by the inode before all
 * dependencies are complete. The INPROGRESS flag marks worklist
 * structures that are still on the worklist, but are being considered
 * for action by some process. The UFS1FMT flag indicates that the
 * inode being processed is a ufs1 format. The EXTDATA flag indicates
 * that the allocdirect describes an extended-attributes dependency.
 * The ONWORKLIST flag shows whether the structure is currently linked
 * onto a worklist.
 */
#define	ATTACHED	0x0001
#define	UNDONE		0x0002
#define	COMPLETE	0x0004
#define	DEPCOMPLETE	0x0008
#define	MKDIR_PARENT	0x0010	/* diradd & mkdir only */
#define	MKDIR_BODY	0x0020	/* diradd & mkdir only */
#define	RMDIR		0x0040	/* dirrem only */
#define	DIRCHG		0x0080	/* diradd & dirrem only */
#define	GOINGAWAY	0x0100	/* indirdep only */
#define	IOSTARTED	0x0200	/* inodedep & pagedep only */
#define	SPACECOUNTED	0x0400	/* inodedep only */
#define	NEWBLOCK	0x0800	/* pagedep only */
#define	INPROGRESS	0x1000	/* dirrem, freeblks, freefrag, freefile only */
#define	UFS1FMT		0x2000	/* indirdep only */
#define	EXTDATA		0x4000	/* allocdirect only */
#define ONWORKLIST	0x8000

#define	ALLCOMPLETE	(ATTACHED | COMPLETE | DEPCOMPLETE)

/*
 * The workitem queue.
 * 
 * It is sometimes useful and/or necessary to clean up certain dependencies
 * in the background rather than during execution of an application process
 * or interrupt service routine. To realize this, we append dependency
 * structures corresponding to such tasks to a "workitem" queue. In a soft
 * updates implementation, most pending workitems should not wait for more
 * than a couple of seconds, so the filesystem syncer process awakens once
 * per second to process the items on the queue.
 */

/* LIST_HEAD(workhead, worklist);	-- declared in buf.h */

/*
 * Each request can be linked onto a work queue through its worklist structure.
 * To avoid the need for a pointer to the structure itself, this structure
 * MUST be declared FIRST in each type in which it appears! If more than one
 * worklist is needed in the structure, then a wk_data field must be added
 * and the macros below changed to use it.
 */
struct worklist {
	struct mount		*wk_mp;		/* Mount we live in */
	LIST_ENTRY(worklist)	wk_list;	/* list of work requests */
	unsigned short		wk_type;	/* type of request */
	unsigned short		wk_state;	/* state flags */
};
#define WK_DATA(wk) ((void *)(wk))
#define WK_PAGEDEP(wk) ((struct pagedep *)(wk))
#define WK_INODEDEP(wk) ((struct inodedep *)(wk))
#define WK_BMSAFEMAP(wk) ((struct bmsafemap *)(wk))
#define WK_ALLOCDIRECT(wk) ((struct allocdirect *)(wk))
#define WK_INDIRDEP(wk) ((struct indirdep *)(wk))
#define WK_ALLOCINDIR(wk) ((struct allocindir *)(wk))
#define WK_FREEFRAG(wk) ((struct freefrag *)(wk))
#define WK_FREEBLKS(wk) ((struct freeblks *)(wk))
#define WK_FREEFILE(wk) ((struct freefile *)(wk))
#define WK_DIRADD(wk) ((struct diradd *)(wk))
#define WK_MKDIR(wk) ((struct mkdir *)(wk))
#define WK_DIRREM(wk) ((struct dirrem *)(wk))
#define WK_NEWDIRBLK(wk) ((struct newdirblk *)(wk))

/*
 * Various types of lists
 */
LIST_HEAD(dirremhd, dirrem);
LIST_HEAD(diraddhd, diradd);
LIST_HEAD(newblkhd, newblk);
LIST_HEAD(inodedephd, inodedep);
LIST_HEAD(allocindirhd, allocindir);
LIST_HEAD(allocdirecthd, allocdirect);
TAILQ_HEAD(allocdirectlst, allocdirect);

/*
 * The "pagedep" structure tracks the various dependencies related to
 * a particular directory page. If a directory page has any dependencies,
 * it will have a pagedep linked to its associated buffer. The
 * pd_dirremhd list holds the list of dirrem requests which decrement
 * inode reference counts. These requests are processed after the
 * directory page with the corresponding zero'ed entries has been
 * written. The pd_diraddhd list maintains the list of diradd requests
 * which cannot be committed until their corresponding inode has been
 * written to disk. Because a directory may have many new entries
 * being created, several lists are maintained hashed on bits of the
 * offset of the entry into the directory page to keep the lists from
 * getting too long. Once a new directory entry has been cleared to
 * be written, it is moved to the pd_pendinghd list. After the new
 * entry has been written to disk it is removed from the pd_pendinghd
 * list, any removed operations are done, and the dependency structure
 * is freed.
 */
#define DAHASHSZ 5
#define DIRADDHASH(offset) (((offset) >> 2) % DAHASHSZ)
struct pagedep {
	struct	worklist pd_list;	/* page buffer */
#	define	pd_state pd_list.wk_state /* check for multiple I/O starts */
	LIST_ENTRY(pagedep) pd_hash;	/* hashed lookup */
	ino_t	pd_ino;			/* associated file */
	ufs_lbn_t pd_lbn;		/* block within file */
	struct	dirremhd pd_dirremhd;	/* dirrem's waiting for page */
	struct	diraddhd pd_diraddhd[DAHASHSZ]; /* diradd dir entry updates */
	struct	diraddhd pd_pendinghd;	/* directory entries awaiting write */
};

/*
 * The "inodedep" structure tracks the set of dependencies associated
 * with an inode. One task that it must manage is delayed operations
 * (i.e., work requests that must be held until the inodedep's associated
 * inode has been written to disk). Getting an inode from its incore 
 * state to the disk requires two steps to be taken by the filesystem
 * in this order: first the inode must be copied to its disk buffer by
 * the VOP_UPDATE operation; second the inode's buffer must be written
 * to disk. To ensure that both operations have happened in the required
 * order, the inodedep maintains two lists. Delayed operations are
 * placed on the id_inowait list. When the VOP_UPDATE is done, all
 * operations on the id_inowait list are moved to the id_bufwait list.
 * When the buffer is written, the items on the id_bufwait list can be
 * safely moved to the work queue to be processed. A second task of the
 * inodedep structure is to track the status of block allocation within
 * the inode.  Each block that is allocated is represented by an
 * "allocdirect" structure (see below). It is linked onto the id_newinoupdt
 * list until both its contents and its allocation in the cylinder
 * group map have been written to disk. Once these dependencies have been
 * satisfied, it is removed from the id_newinoupdt list and any followup
 * actions such as releasing the previous block or fragment are placed
 * on the id_inowait list. When an inode is updated (a VOP_UPDATE is
 * done), the "inodedep" structure is linked onto the buffer through
 * its worklist. Thus, it will be notified when the buffer is about
 * to be written and when it is done. At the update time, all the
 * elements on the id_newinoupdt list are moved to the id_inoupdt list
 * since those changes are now relevant to the copy of the inode in the
 * buffer. Also at update time, the tasks on the id_inowait list are
 * moved to the id_bufwait list so that they will be executed when
 * the updated inode has been written to disk. When the buffer containing
 * the inode is written to disk, any updates listed on the id_inoupdt
 * list are rolled back as they are not yet safe. Following the write,
 * the changes are once again rolled forward and any actions on the
 * id_bufwait list are processed (since those actions are now safe).
 * The entries on the id_inoupdt and id_newinoupdt lists must be kept
 * sorted by logical block number to speed the calculation of the size
 * of the rolled back inode (see explanation in initiate_write_inodeblock).
 * When a directory entry is created, it is represented by a diradd.
 * The diradd is added to the id_inowait list as it cannot be safely
 * written to disk until the inode that it represents is on disk. After
 * the inode is written, the id_bufwait list is processed and the diradd
 * entries are moved to the id_pendinghd list where they remain until
 * the directory block containing the name has been written to disk.
 * The purpose of keeping the entries on the id_pendinghd list is so that
 * the softdep_fsync function can find and push the inode's directory
 * name(s) as part of the fsync operation for that file.
 */
struct inodedep {
	struct	worklist id_list;	/* buffer holding inode block */
#	define	id_state id_list.wk_state /* inode dependency state */
	LIST_ENTRY(inodedep) id_hash;	/* hashed lookup */
	struct	fs *id_fs;		/* associated filesystem */
	ino_t	id_ino;			/* dependent inode */
	nlink_t	id_nlinkdelta;		/* saved effective link count */
	LIST_ENTRY(inodedep) id_deps;	/* bmsafemap's list of inodedep's */
	struct	buf *id_buf;		/* related bmsafemap (if pending) */
	long	id_savedextsize;	/* ext size saved during rollback */
	off_t	id_savedsize;		/* file size saved during rollback */
	struct	workhead id_pendinghd;	/* entries awaiting directory write */
	struct	workhead id_bufwait;	/* operations after inode written */
	struct	workhead id_inowait;	/* operations waiting inode update */
	struct	allocdirectlst id_inoupdt; /* updates before inode written */
	struct	allocdirectlst id_newinoupdt; /* updates when inode written */
	struct	allocdirectlst id_extupdt; /* extdata updates pre-inode write */
	struct	allocdirectlst id_newextupdt; /* extdata updates at ino write */
	union {
	struct	ufs1_dinode *idu_savedino1; /* saved ufs1_dinode contents */
	struct	ufs2_dinode *idu_savedino2; /* saved ufs2_dinode contents */
	} id_un;
};
#define id_savedino1 id_un.idu_savedino1
#define id_savedino2 id_un.idu_savedino2

/*
 * A "newblk" structure is attached to a bmsafemap structure when a block
 * or fragment is allocated from a cylinder group. Its state is set to
 * DEPCOMPLETE when its cylinder group map is written. It is consumed by
 * an associated allocdirect or allocindir allocation which will attach
 * themselves to the bmsafemap structure if the newblk's DEPCOMPLETE flag
 * is not set (i.e., its cylinder group map has not been written).
 */ 
struct newblk {
	LIST_ENTRY(newblk) nb_hash;	/* hashed lookup */
	struct	fs *nb_fs;		/* associated filesystem */
	int	nb_state;		/* state of bitmap dependency */
	ufs2_daddr_t nb_newblkno;	/* allocated block number */
	LIST_ENTRY(newblk) nb_deps;	/* bmsafemap's list of newblk's */
	struct	bmsafemap *nb_bmsafemap; /* associated bmsafemap */
};

/*
 * A "bmsafemap" structure maintains a list of dependency structures
 * that depend on the update of a particular cylinder group map.
 * It has lists for newblks, allocdirects, allocindirs, and inodedeps.
 * It is attached to the buffer of a cylinder group block when any of
 * these things are allocated from the cylinder group. It is freed
 * after the cylinder group map is written and the state of its
 * dependencies are updated with DEPCOMPLETE to indicate that it has
 * been processed.
 */
struct bmsafemap {
	struct	worklist sm_list;	/* cylgrp buffer */
	struct	buf *sm_buf;		/* associated buffer */
	struct	allocdirecthd sm_allocdirecthd; /* allocdirect deps */
	struct	allocindirhd sm_allocindirhd; /* allocindir deps */
	struct	inodedephd sm_inodedephd; /* inodedep deps */
	struct	newblkhd sm_newblkhd;	/* newblk deps */
};

/*
 * An "allocdirect" structure is attached to an "inodedep" when a new block
 * or fragment is allocated and pointed to by the inode described by
 * "inodedep". The worklist is linked to the buffer that holds the block.
 * When the block is first allocated, it is linked to the bmsafemap
 * structure associated with the buffer holding the cylinder group map
 * from which it was allocated. When the cylinder group map is written
 * to disk, ad_state has the DEPCOMPLETE flag set. When the block itself
 * is written, the COMPLETE flag is set. Once both the cylinder group map
 * and the data itself have been written, it is safe to write the inode
 * that claims the block. If there was a previous fragment that had been
 * allocated before the file was increased in size, the old fragment may
 * be freed once the inode claiming the new block is written to disk.
 * This ad_fragfree request is attached to the id_inowait list of the
 * associated inodedep (pointed to by ad_inodedep) for processing after
 * the inode is written. When a block is allocated to a directory, an
 * fsync of a file whose name is within that block must ensure not only
 * that the block containing the file name has been written, but also
 * that the on-disk inode references that block. When a new directory
 * block is created, we allocate a newdirblk structure which is linked
 * to the associated allocdirect (on its ad_newdirblk list). When the
 * allocdirect has been satisfied, the newdirblk structure is moved to
 * the inodedep id_bufwait list of its directory to await the inode
 * being written. When the inode is written, the directory entries are
 * fully committed and can be deleted from their pagedep->id_pendinghd
 * and inodedep->id_pendinghd lists.
 */
struct allocdirect {
	struct	worklist ad_list;	/* buffer holding block */
#	define	ad_state ad_list.wk_state /* block pointer state */
	TAILQ_ENTRY(allocdirect) ad_next; /* inodedep's list of allocdirect's */
	ufs_lbn_t ad_lbn;		/* block within file */
	ufs2_daddr_t ad_newblkno;	/* new value of block pointer */
	ufs2_daddr_t ad_oldblkno;	/* old value of block pointer */
	long	ad_newsize;		/* size of new block */
	long	ad_oldsize;		/* size of old block */
	LIST_ENTRY(allocdirect) ad_deps; /* bmsafemap's list of allocdirect's */
	struct	buf *ad_buf;		/* cylgrp buffer (if pending) */
	struct	inodedep *ad_inodedep;	/* associated inodedep */
	struct	freefrag *ad_freefrag;	/* fragment to be freed (if any) */
	struct	workhead ad_newdirblk;	/* dir block to notify when written */
};

/*
 * A single "indirdep" structure manages all allocation dependencies for
 * pointers in an indirect block. The up-to-date state of the indirect
 * block is stored in ir_savedata. The set of pointers that may be safely
 * written to the disk is stored in ir_safecopy. The state field is used
 * only to track whether the buffer is currently being written (in which
 * case it is not safe to update ir_safecopy). Ir_deplisthd contains the
 * list of allocindir structures, one for each block that needs to be
 * written to disk. Once the block and its bitmap allocation have been
 * written the safecopy can be updated to reflect the allocation and the
 * allocindir structure freed. If ir_state indicates that an I/O on the
 * indirect block is in progress when ir_safecopy is to be updated, the
 * update is deferred by placing the allocindir on the ir_donehd list.
 * When the I/O on the indirect block completes, the entries on the
 * ir_donehd list are processed by updating their corresponding ir_safecopy
 * pointers and then freeing the allocindir structure.
 */
struct indirdep {
	struct	worklist ir_list;	/* buffer holding indirect block */
#	define	ir_state ir_list.wk_state /* indirect block pointer state */
	caddr_t ir_saveddata;		/* buffer cache contents */
	struct	buf *ir_savebp;		/* buffer holding safe copy */
	struct	allocindirhd ir_donehd;	/* done waiting to update safecopy */
	struct	allocindirhd ir_deplisthd; /* allocindir deps for this block */
};

/*
 * An "allocindir" structure is attached to an "indirdep" when a new block
 * is allocated and pointed to by the indirect block described by the
 * "indirdep". The worklist is linked to the buffer that holds the new block.
 * When the block is first allocated, it is linked to the bmsafemap
 * structure associated with the buffer holding the cylinder group map
 * from which it was allocated. When the cylinder group map is written
 * to disk, ai_state has the DEPCOMPLETE flag set. When the block itself
 * is written, the COMPLETE flag is set. Once both the cylinder group map
 * and the data itself have been written, it is safe to write the entry in
 * the indirect block that claims the block; the "allocindir" dependency 
 * can then be freed as it is no longer applicable.
 */
struct allocindir {
	struct	worklist ai_list;	/* buffer holding indirect block */
#	define	ai_state ai_list.wk_state /* indirect block pointer state */
	LIST_ENTRY(allocindir) ai_next;	/* indirdep's list of allocindir's */
	int	ai_offset;		/* pointer offset in indirect block */
	ufs2_daddr_t ai_newblkno;	/* new block pointer value */
	ufs2_daddr_t ai_oldblkno;	/* old block pointer value */
	struct	freefrag *ai_freefrag;	/* block to be freed when complete */
	struct	indirdep *ai_indirdep;	/* address of associated indirdep */
	LIST_ENTRY(allocindir) ai_deps;	/* bmsafemap's list of allocindir's */
	struct	buf *ai_buf;		/* cylgrp buffer (if pending) */
};

/*
 * A "freefrag" structure is attached to an "inodedep" when a previously
 * allocated fragment is replaced with a larger fragment, rather than extended.
 * The "freefrag" structure is constructed and attached when the replacement
 * block is first allocated. It is processed after the inode claiming the
 * bigger block that replaces it has been written to disk. Note that the
 * ff_state field is is used to store the uid, so may lose data. However,
 * the uid is used only in printing an error message, so is not critical.
 * Keeping it in a short keeps the data structure down to 32 bytes.
 */
struct freefrag {
	struct	worklist ff_list;	/* id_inowait or delayed worklist */
#	define	ff_state ff_list.wk_state /* owning user; should be uid_t */
	ufs2_daddr_t ff_blkno;		/* fragment physical block number */
	long	ff_fragsize;		/* size of fragment being deleted */
	ino_t	ff_inum;		/* owning inode number */
};

/*
 * A "freeblks" structure is attached to an "inodedep" when the
 * corresponding file's length is reduced to zero. It records all
 * the information needed to free the blocks of a file after its
 * zero'ed inode has been written to disk.
 */
struct freeblks {
	struct	worklist fb_list;	/* id_inowait or delayed worklist */
#	define	fb_state fb_list.wk_state /* inode and dirty block state */
	ino_t	fb_previousinum;	/* inode of previous owner of blocks */
	uid_t	fb_uid;			/* uid of previous owner of blocks */
	struct	vnode *fb_devvp;	/* filesystem device vnode */
	long	fb_oldextsize;		/* previous ext data size */
	off_t	fb_oldsize;		/* previous file size */
	ufs2_daddr_t fb_chkcnt;		/* used to check cnt of blks released */
	ufs2_daddr_t fb_dblks[NDADDR];	/* direct blk ptrs to deallocate */
	ufs2_daddr_t fb_iblks[NIADDR];	/* indirect blk ptrs to deallocate */
	ufs2_daddr_t fb_eblks[NXADDR];	/* indirect blk ptrs to deallocate */
};

/*
 * A "freefile" structure is attached to an inode when its
 * link count is reduced to zero. It marks the inode as free in
 * the cylinder group map after the zero'ed inode has been written
 * to disk and any associated blocks and fragments have been freed.
 */
struct freefile {
	struct	worklist fx_list;	/* id_inowait or delayed worklist */
	mode_t	fx_mode;		/* mode of inode */
	ino_t	fx_oldinum;		/* inum of the unlinked file */
	struct	vnode *fx_devvp;	/* filesystem device vnode */
};

/*
 * A "diradd" structure is linked to an "inodedep" id_inowait list when a
 * new directory entry is allocated that references the inode described
 * by "inodedep". When the inode itself is written (either the initial
 * allocation for new inodes or with the increased link count for
 * existing inodes), the COMPLETE flag is set in da_state. If the entry
 * is for a newly allocated inode, the "inodedep" structure is associated
 * with a bmsafemap which prevents the inode from being written to disk
 * until the cylinder group has been updated. Thus the da_state COMPLETE
 * flag cannot be set until the inode bitmap dependency has been removed.
 * When creating a new file, it is safe to write the directory entry that
 * claims the inode once the referenced inode has been written. Since
 * writing the inode clears the bitmap dependencies, the DEPCOMPLETE flag
 * in the diradd can be set unconditionally when creating a file. When
 * creating a directory, there are two additional dependencies described by
 * mkdir structures (see their description below). When these dependencies
 * are resolved the DEPCOMPLETE flag is set in the diradd structure.
 * If there are multiple links created to the same inode, there will be
 * a separate diradd structure created for each link. The diradd is
 * linked onto the pg_diraddhd list of the pagedep for the directory
 * page that contains the entry. When a directory page is written,
 * the pg_diraddhd list is traversed to rollback any entries that are
 * not yet ready to be written to disk. If a directory entry is being
 * changed (by rename) rather than added, the DIRCHG flag is set and
 * the da_previous entry points to the entry that will be "removed"
 * once the new entry has been committed. During rollback, entries
 * with da_previous are replaced with the previous inode number rather
 * than zero.
 *
 * The overlaying of da_pagedep and da_previous is done to keep the
 * structure down to 32 bytes in size on a 32-bit machine. If a
 * da_previous entry is present, the pointer to its pagedep is available
 * in the associated dirrem entry. If the DIRCHG flag is set, the
 * da_previous entry is valid; if not set the da_pagedep entry is valid.
 * The DIRCHG flag never changes; it is set when the structure is created
 * if appropriate and is never cleared.
 */
struct diradd {
	struct	worklist da_list;	/* id_inowait or id_pendinghd list */
#	define	da_state da_list.wk_state /* state of the new directory entry */
	LIST_ENTRY(diradd) da_pdlist;	/* pagedep holding directory block */
	doff_t	da_offset;		/* offset of new dir entry in dir blk */
	ino_t	da_newinum;		/* inode number for the new dir entry */
	union {
	struct	dirrem *dau_previous;	/* entry being replaced in dir change */
	struct	pagedep *dau_pagedep;	/* pagedep dependency for addition */
	} da_un;
};
#define da_previous da_un.dau_previous
#define da_pagedep da_un.dau_pagedep

/*
 * Two "mkdir" structures are needed to track the additional dependencies
 * associated with creating a new directory entry. Normally a directory
 * addition can be committed as soon as the newly referenced inode has been
 * written to disk with its increased link count. When a directory is
 * created there are two additional dependencies: writing the directory
 * data block containing the "." and ".." entries (MKDIR_BODY) and writing
 * the parent inode with the increased link count for ".." (MKDIR_PARENT).
 * These additional dependencies are tracked by two mkdir structures that
 * reference the associated "diradd" structure. When they have completed,
 * they set the DEPCOMPLETE flag on the diradd so that it knows that its
 * extra dependencies have been completed. The md_state field is used only
 * to identify which type of dependency the mkdir structure is tracking.
 * It is not used in the mainline code for any purpose other than consistency
 * checking. All the mkdir structures in the system are linked together on
 * a list. This list is needed so that a diradd can find its associated
 * mkdir structures and deallocate them if it is prematurely freed (as for
 * example if a mkdir is immediately followed by a rmdir of the same directory).
 * Here, the free of the diradd must traverse the list to find the associated
 * mkdir structures that reference it. The deletion would be faster if the
 * diradd structure were simply augmented to have two pointers that referenced
 * the associated mkdir's. However, this would increase the size of the diradd
 * structure from 32 to 64-bits to speed a very infrequent operation.
 */
struct mkdir {
	struct	worklist md_list;	/* id_inowait or buffer holding dir */
#	define	md_state md_list.wk_state /* type: MKDIR_PARENT or MKDIR_BODY */
	struct	diradd *md_diradd;	/* associated diradd */
	struct	buf *md_buf;		/* MKDIR_BODY: buffer holding dir */
	LIST_ENTRY(mkdir) md_mkdirs;	/* list of all mkdirs */
};
LIST_HEAD(mkdirlist, mkdir) mkdirlisthd;

/*
 * A "dirrem" structure describes an operation to decrement the link
 * count on an inode. The dirrem structure is attached to the pg_dirremhd
 * list of the pagedep for the directory page that contains the entry.
 * It is processed after the directory page with the deleted entry has
 * been written to disk.
 *
 * The overlaying of dm_pagedep and dm_dirinum is done to keep the
 * structure down to 32 bytes in size on a 32-bit machine. It works
 * because they are never used concurrently.
 */
struct dirrem {
	struct	worklist dm_list;	/* delayed worklist */
#	define	dm_state dm_list.wk_state /* state of the old directory entry */
	LIST_ENTRY(dirrem) dm_next;	/* pagedep's list of dirrem's */
	ino_t	dm_oldinum;		/* inum of the removed dir entry */
	union {
	struct	pagedep *dmu_pagedep;	/* pagedep dependency for remove */
	ino_t	dmu_dirinum;		/* parent inode number (for rmdir) */
	} dm_un;
};
#define dm_pagedep dm_un.dmu_pagedep
#define dm_dirinum dm_un.dmu_dirinum

/*
 * A "newdirblk" structure tracks the progress of a newly allocated
 * directory block from its creation until it is claimed by its on-disk
 * inode. When a block is allocated to a directory, an fsync of a file
 * whose name is within that block must ensure not only that the block
 * containing the file name has been written, but also that the on-disk
 * inode references that block. When a new directory block is created,
 * we allocate a newdirblk structure which is linked to the associated
 * allocdirect (on its ad_newdirblk list). When the allocdirect has been
 * satisfied, the newdirblk structure is moved to the inodedep id_bufwait
 * list of its directory to await the inode being written. When the inode
 * is written, the directory entries are fully committed and can be
 * deleted from their pagedep->id_pendinghd and inodedep->id_pendinghd
 * lists. Note that we could track directory blocks allocated to indirect
 * blocks using a similar scheme with the allocindir structures. Rather
 * than adding this level of complexity, we simply write those newly 
 * allocated indirect blocks synchronously as such allocations are rare.
 */
struct newdirblk {
	struct	worklist db_list;	/* id_inowait or pg_newdirblk */
#	define	db_state db_list.wk_state /* unused */
	struct	pagedep *db_pagedep;	/* associated pagedep */
};
