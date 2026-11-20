/*-
 *  modified for EXT2FS support in Lites 1.1
 *
 *  Aug 1995, Godmar Back (gback@cs.utah.edu)
 *  University of Utah, Department of Computer Science
 */
/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1991, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 */

#ifndef _FS_EXT2FS_EXT2_EXTERN_H_
#define	_FS_EXT2FS_EXT2_EXTERN_H_

struct ext2fs_dinode;
struct ext2fs_direct_2;
struct ext2fs_direct_tail;
struct ext2fs_searchslot;
struct indir;
struct inode;
struct mount;
struct vfsconf;
struct vnode;

/*
 * Physical allocation run descriptor.
 *
 * Represents a contiguous range of physical filesystem blocks
 * allocated by the physical allocator.  This abstraction keeps the
 * physical allocator independent of filesystem mapping structures
 * (indirect blocks, extent trees, etc.).
 */
struct ext2_alloc_run {
	e4fs_daddr_t	par_physical_start;	/* first physical block */
	uint32_t	par_length;			/* count of blocks */
};

/*
 * Allocation lifecycle state for an allocated run.
 *
 * A runtime-only (in-memory) state carried by the allocation context —
 * it is NOT written to disk and requires no new on-disk ext2 fields.
 *
 * The state describes only the publication boundary:
 *   Before PUBLISHED, rollback is ordinary cleanup.
 *   After PUBLISHED, blocks must not be synchronously returned to the
 *   free map.
 *
 *   ALLOCATED     Physical blocks reserved in the bitmap.
 *                 Run is unpublished.
 *
 *   MAPPED        Block pointer installed in in-memory inode
 *                 metadata (bap[], i_db[], or an extent record).
 *                 NOT yet durably written.  Run is unpublished.
 *
 *   PUBLISHED     Metadata containing the block pointer has been
 *                 durably written via bwrite().  On-disk inode
 *                 metadata references the run.
 *
 *   ROLLED_BACK   Allocation was completely undone.  No further
 *                 rollback is permitted.
 *
 * Legal transitions:
 *   ALLOCATED   -> MAPPED        (block ptr installed in-memory)
 *   ALLOCATED   -> ROLLED_BACK   (rollback before mapping)
 *   MAPPED      -> PUBLISHED     (metadata durably written)
 *   MAPPED      -> ROLLED_BACK   (rollback after in-memory map, pre-bwrite)
 *
 * Rejected (KASSERT-only, all builds):
 *   PUBLISHED  -> ROLLED_BACK   (blocks may be referenced by on-disk metadata)
 *   ROLLED_BACK -> any state    (terminal failure)
 */
enum ext2_alloc_state {
	EXT2_ALLOC_ALLOCATED = 0,
	EXT2_ALLOC_MAPPED,
	EXT2_ALLOC_PUBLISHED,
	EXT2_ALLOC_ROLLED_BACK,
};

/*
 * Allocation context — runtime-only tracking of an allocation run's
 * lifecycle through the publication pipeline.
 *
 * This struct is never written to disk.  It exists solely to let
 * rollback distinguish unpublished from published blocks at runtime.
 * Contexts are single-use: once consumed by ext2_rollback_allocation()
 * they must not be passed to another cleanup path.
 *
 * Fields:
 *   ip               Owning inode.
 *   run              The physical run (start + length).
 *   logical_start    Logical block number (for diagnostics).
 *   logical_length   Requested logical blocks.
 *   state            Current lifecycle state (see enum above).
 *   accounting_applied  Whether i_blocks (and i_flag) has been
 *                       incremented for this run by
 *                       ext2_commit_allocated_block().  Needed for
 *                       the ALLOCATED -> ROLLED_BACK path; for
 *                       MAPPED or higher, accounting is always
 *                       applied by definition.
 *
 * mapping_installed is implied by state >= MAPPED.
 * metadata_published is implied by state >= PUBLISHED.
 * physically_freed need not be tracked: contexts are single-use.
 * buffers_initialized is guaranteed by the buffer-allocation helper;
 * rollback does not need to inspect it.
 */
struct ext2_alloc_context {
	struct inode		*ip;		/* owning inode */
	struct ext2_alloc_run	run;		/* physical run (start + length) */
	e2fs_lbn_t		logical_start;	/* logical block number */
	uint32_t		logical_length;	/* requested logical blocks */
	enum ext2_alloc_state	state;		/* lifecycle state */
	bool			accounting_applied; /* i_blocks incremented */
};

/*
 * Allocation class for the physical allocator.
 *
 * These classes let the allocation policy tune itself for the kind
 * of data being allocated.
 */
enum ext2_alloc_class {
	EXT2_ALLOC_DATA_SEQ,		/* sequential data write */
	EXT2_ALLOC_DATA_RAND,		/* random data write */
	EXT2_ALLOC_DIRECTORY,		/* directory */
	EXT2_ALLOC_INDIR_METADATA,	/* indirect block metadata */
	EXT2_ALLOC_EXTENT_METADATA,	/* extent tree metadata */
};

int	ext2_add_entry(struct vnode *, struct ext2fs_direct_2 *);
int	ext2_alloc(struct inode *, daddr_t, e4fs_daddr_t, int,
	    struct ucred *, e4fs_daddr_t *);
e4fs_daddr_t ext2_alloc_meta(struct inode *ip);
int	ext2_alloc_run(struct inode *, e2fs_lbn_t, uint32_t, e4fs_daddr_t,
	    enum ext2_alloc_class, struct ucred *, struct ext2_alloc_context *);
void	ext2_abort_allocated_runs(struct inode *, struct ext2_alloc_run *);

/*
 * ext2_rollback_inode_accounting — revert i_blocks and i_flag
 *   for an unpublished allocation run.
 *
 * This is the accounting-only half of ext2_rollback_unpublished(),
 * separated so it can be unit-tested independently from physical
 * block freeing.
 *
 * Validation is active in ALL builds (debug and production).  This is
 * a cheap check (comparisons) on the error path, and omitting it in
 * production would leave corrupted runs silently corrupting i_blocks.
 *
 * Contract:
 *   - Called WITHOUT EXT2_LOCK held (returns EDEADLK if held)
 *   - phys_start != 0 (returns EINVAL)
 *   - length > 0 (returns EINVAL)
 *   - length <= EXT4_MAX_LEN (returns EINVAL)
 *   - phys_start within filesystem block range
 *     [e2fs_first_dblock, e2fs_bcount) (returns EINVAL)
 *   - phys_start + length - 1 does not overflow uint64_t
 *     and does not exceed e2fs_bcount - 1 (returns EFBIG or EINVAL)
 *   - i_blocks does not underflow (returns EIO)
 *
 * NOTE: range validation proves only that the run lies inside the
 * filesystem's legal block range.  It does NOT prove ownership —
 * that the blocks were allocated by this operation, are still
 * allocated, or match the recorded allocation.  The scalar
 * (phys_start, length) interface cannot provide these guarantees.
 *
 * The context-based ext2_rollback_allocation() provides stronger
 * validation by tracking:
 *   - whether blocks were allocated by this context (not a stale
 *     or unrelated range);
 *   - whether accounting was applied (accounting_applied flag);
 *   - the publication state (state >= PUBLISHED forbids rollback).
 */
void	ext2_rollback_inode_accounting(struct inode *, e4fs_daddr_t,
	    uint32_t);

/*
 * ext2_rollback_inode_accounting_checked — same as
 *   ext2_rollback_inode_accounting() but returns errno instead of
 *   panicking, for use with externally derived or potentially
 *   corrupted allocation-run arguments.
 *
 * Performs ALL validation in EVERY build (not compiled out in
 * production).  Returns 0 on success, or one of:
 *   EDEADLK — EXT2_LOCK held on entry
 *   EINVAL  — invalid phys_start, length, overflow, or range
 *   EFBIG   — run extends past end of filesystem
 *   EIO     — i_blocks underflow.  This is NOT a normal caller error;
 *             it indicates filesystem corruption, double rollback, or
 *             an accounting bug.  The checked variant returns EIO so
 *             the wrapper can panic with context.  Under INVARIANTS,
 *             the checked variant panics directly before returning.
 */
int	ext2_rollback_inode_accounting_checked(struct inode *, e4fs_daddr_t,
	    uint32_t);

/*
 * ext2_rollback_unpublished — revert a block run that was allocated
 *   but never published into on-disk inode metadata.
 *
 * TRANSITIONAL INTERFACE:
 *   New code should call ext2_rollback_allocation() with an
 *   ext2_alloc_context instead of this scalar variant.  This wrapper
 *   is retained for caller sites that have not yet been converted.
 *
 * Delegates to:
 *   1. ext2_rollback_inode_accounting() — validates all preconditions
 *      and adjusts i_blocks/i_flag under EXT2_LOCK.
 *   2. ext2_abort_allocated_runs() — frees the physical blocks via
 *      ext2_blkfree() (manages its own locking).
 *
 * Contract (all enforced by ext2_rollback_inode_accounting):
 *   - Called WITHOUT EXT2_LOCK held — callers that hold the lock
 *     (e.g. ext2_balloc.c multi-block EFBIG path) must release it
 *     before calling.
 *   - phys_start != 0
 *   - length > 0
 *   - length <= EXT4_MAX_LEN
 *   - phys_start within filesystem block range
 *   - phys_start + length - 1 does not overflow and does not exceed
 *     e2fs_bcount
 *   - i_blocks does not underflow
 *
 * Publication state:
 *   The blocks MUST be unpublished — i.e., no on-disk inode metadata
 *   (direct/indirect block pointers, extent records) references them.
 *   If a pointer has been committed to storage via bwrite(), the
 *   operation is a consistency repair, not an allocation rollback.
 *
 * Buffer ownership:
 *   The caller is responsible for releasing any held buffers (e.g.
 *   bp from bread() or getblk()) before or after calling this
 *   helper.  The helper operates only on the inode and the
 *   physical extent; it does not touch caller-owned buffers.
 */
void	ext2_rollback_unpublished(struct inode *, e4fs_daddr_t, uint32_t);

/*
 * ext2_rollback_allocation — revert an allocation tracked by an
 *   ext2_alloc_context.
 *
 * Preferred rollback interface.  Verifies state and guards against
 * double-rollback.
 *
 * Contract:
 *   - ctx->state must be ALLOCATED or MAPPED.  KASSERT-panics otherwise
 *     (PUBLISHED, ROLLED_BACK are terminal).
 *   - On success: undoes i_blocks (if accounting_applied), frees
 *     physical blocks, sets state to ROLLED_BACK, clears stale
 *     allocation hints.
 *   - Consumes the context.  The context must not be reused after call.
 *   - Does NOT require EXT2_LOCK held on entry.
 */
void	ext2_rollback_allocation(struct ext2_alloc_context *);

/*
 * ext2_commit_allocated_block — finalize inode accounting and
 *   allocation hints for a successfully allocated block run.
 *
 * Called by the mapping layer after ext2_alloc_run() succeeds and
 * before the physical block pointer is installed into on-disk inode
 * metadata.  Updates:
 *   - i_blocks (sector count for the allocated run)
 *   - i_next_alloc_block / i_next_alloc_goal (sequential allocation hints)
 *   - i_flag (IN_CHANGE | IN_UPDATE)
 *
 * Sets accounting_applied=true so that rollback knows to reverse the
 * i_blocks update.  The context remains in the ALLOCATED state after
 * this call; it transitions to MAPPED when the block pointer is
 * installed, and to PUBLISHED when the metadata is durably written.
 *
 * Contract:
 *   - Called WITH EXT2_LOCK held (caller is responsible for locking).
 *   - ctx->state should be ALLOCATED on entry.
 *   - Does NOT free blocks or unlock.
 */
void	ext2_commit_allocated_block(struct inode *, struct ext2_alloc_context *);

/*
 * ext2_alloc_transition — enforce a state-machine transition on an
 * allocation context.
 *
 * Transitions are the only sanctioned way to advance the state.
 * Direct assignment of ctx->state by callers is forbidden except for
 * the initial ALLOCATED state set by ext2_alloc_run().
 *
 * Permitted transitions:
 *   ALLOCATED -> MAPPED        (block ptr installed in-memory)
 *   ALLOCATED -> ROLLED_BACK   (rollback before mapping)
 *   MAPPED -> PUBLISHED        (metadata durably written)
 *   MAPPED -> ROLLED_BACK      (rollback after in-memory map, pre-bwrite)
 *
 * Panics (KASSERT, all builds) if the transition is not in the
 * permitted set.  Broken internal contracts are assertions, not
 * production error paths.
 */
void	ext2_alloc_transition(struct ext2_alloc_context *,
	    enum ext2_alloc_state);

/*
 * ext2_doreallocblks — sysctl gate for VOP_REALLOCBLKS.  Non-zero lets the
 * buffer cache ask us to relocate a cluster onto contiguous blocks.  Both
 * ext2_reallocblks() and ext4_reallocblks() honour it.
 */
extern int	ext2_doreallocblks;

int	ext2_balloc(struct inode *,
	    e2fs_lbn_t, int, struct ucred *, struct buf **, int);
int	ext2_blkatoff(struct vnode *, off_t, char **, struct buf **);
void	ext2_blkfree(struct inode *,  e4fs_daddr_t, long);
e4fs_daddr_t	ext2_blkpref(struct inode *, e2fs_lbn_t, int, e2fs_daddr_t *,
	    e2fs_daddr_t);
int	ext2_bmap(struct vop_bmap_args *);
int	ext2_bmaparray(struct vnode *, daddr_t, daddr_t *, int *, int *);
int	ext4_bmapext(struct vnode *, int32_t, int64_t *, int *, int *);
int	ext2_bmap_seekdata(struct vnode *, off_t *);
void	ext2_clusteracct(struct m_ext2fs *, char *, int, e4fs_daddr_t, int);
void	ext2_dirbad(struct inode *ip, doff_t offset, char *how);
int	ext2_ei2i(struct ext2fs_dinode *, struct inode *);
int	ext2_getlbns(struct vnode *, daddr_t, struct indir *, int *);
int	ext2_i2ei(struct inode *, struct ext2fs_dinode *);
void	ext2_itimes(struct vnode *vp);
int	ext2_reallocblks(struct vop_reallocblks_args *);
int	ext4_reallocblks(struct vop_reallocblks_args *);
int	ext2_reclaim(struct vop_reclaim_args *);
int	ext2_truncate(struct vnode *, off_t, int, struct ucred *, struct thread *);
int	ext2_update(struct vnode *, int);
int	ext2_valloc(struct vnode *, int, struct ucred *, struct vnode **);
int	ext2_vfree(struct vnode *, ino_t, int);
int	ext2_vinit(struct mount *, struct vop_vector *, struct vnode **vpp);
int	ext2_lookup(struct vop_cachedlookup_args *);
int	ext2_readdir(struct vop_readdir_args *);
#ifdef EXT2FS_PRINT_EXTENTS
void	ext2_print_inode(struct inode *);
#endif
int	ext2_direnter(struct inode *, 
		struct vnode *, struct componentname *);
int	ext2_dirremove(struct vnode *, struct componentname *);
int	ext2_dirrewrite(struct inode *,
		struct inode *, struct componentname *);
int	ext2_dirempty(struct inode *, ino_t, struct ucred *);
int	ext2_checkpath(struct inode *, struct inode *, struct ucred *);
int	ext2_cg_has_sb(struct m_ext2fs *fs, int cg);
uint64_t	ext2_cg_number_gdb(struct m_ext2fs *fs, int cg);
int	ext2_inactive(struct vop_inactive_args *);
int	ext2_htree_add_entry(struct vnode *, struct ext2fs_direct_2 *,
	    struct componentname *);
int	ext2_htree_create_index(struct vnode *, struct componentname *,
	    struct ext2fs_direct_2 *);
int	ext2_htree_has_idx(struct inode *);
int	ext2_htree_hash(const char *, int, uint32_t *, int, uint32_t *,
	    uint32_t *);
int	ext2_htree_lookup(struct inode *, const char *, int, struct buf **,
	    int *, doff_t *, doff_t *, doff_t *, struct ext2fs_searchslot *);
int	ext2_search_dirblock(struct inode *, void *, int *, const char *, int,
	    int *, doff_t *, doff_t *, doff_t *, struct ext2fs_searchslot *);
uint32_t	e2fs_gd_get_ndirs(struct ext2_gd *gd);
uint64_t	e2fs_gd_get_b_bitmap(struct ext2_gd *);
uint64_t	e2fs_gd_get_i_bitmap(struct ext2_gd *);
uint64_t	e2fs_gd_get_i_tables(struct ext2_gd *);
void	ext2_sb_csum_set_seed(struct m_ext2fs *);
int	ext2_sb_csum_verify(struct m_ext2fs *);
void	ext2_sb_csum_set(struct m_ext2fs *);
int	ext2_extattr_blk_csum_verify(struct inode *, struct buf *);
void	ext2_extattr_blk_csum_set(struct inode *, struct buf *);
int	ext2_dir_blk_csum_verify(struct inode *, struct buf *);
struct ext2fs_direct_tail	*ext2_dirent_get_tail(struct inode *ip,
    struct ext2fs_direct_2 *ep);
void	ext2_dirent_csum_set(struct inode *, struct ext2fs_direct_2 *);
int	ext2_dirent_csum_verify(struct inode *ip, struct ext2fs_direct_2 *ep);
void	ext2_dx_csum_set(struct inode *, struct ext2fs_direct_2 *);
int	ext2_dx_csum_verify(struct inode *ip, struct ext2fs_direct_2 *ep);
int	ext2_extent_blk_csum_verify(struct inode *, void *);
void	ext2_extent_blk_csum_set(struct inode *, void *);
void	ext2_init_dirent_tail(struct ext2fs_direct_tail *);
int	ext2_is_dirent_tail(struct inode *, struct ext2fs_direct_2 *);
int	ext2_gd_i_bitmap_csum_verify(struct m_ext2fs *, int, struct buf *);
void	ext2_gd_i_bitmap_csum_set(struct m_ext2fs *, int, struct buf *);
int	ext2_gd_b_bitmap_csum_verify(struct m_ext2fs *, int, struct buf *);
void	ext2_gd_b_bitmap_csum_set(struct m_ext2fs *, int, struct buf *);
int	ext2_ei_csum_verify(struct inode *, struct ext2fs_dinode *);
void	ext2_ei_csum_set(struct inode *, struct ext2fs_dinode *);
int	ext2_gd_csum_verify(struct m_ext2fs *, struct cdev *);
void	ext2_gd_csum_set(struct m_ext2fs *);
/* Flags to low-level allocation routines.
 * The low 16-bits are reserved for IO_ flags from vnode.h.
 *
 * The BA_CLRBUF flag specifies that the existing content of the block
 * will not be completely overwritten by the caller, so buffers for new
 * blocks must be cleared and buffers for existing blocks must be read.
 * When BA_CLRBUF is not set the buffer will be completely overwritten
 * and there is no reason to clear them or to spend I/O fetching existing
 * data. The BA_CLRBUF flag is handled in the ext2_balloc() functions.
 */
#define	BA_CLRBUF	0x00010000	/* Clear invalid areas of buffer. */
#define	BA_SEQMASK	0x7F000000	/* Bits holding seq heuristic. */
#define	BA_SEQSHIFT	24
#define	BA_SEQMAX	0x7F

extern struct vop_vector ext2_vnodeops;
extern struct vop_vector ext2_fifoops;

#endif	/* !_FS_EXT2FS_EXT2_EXTERN_H_ */
