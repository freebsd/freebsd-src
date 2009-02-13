/*
 * Copyright (c) 2000-2005 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it would be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write the Free Software Foundation,
 * Inc.,  51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */
#include "xfs.h"

#include "xfs_fs.h"
#include "xfs_types.h"
#include "xfs_bit.h"
#include "xfs_log.h"
#include "xfs_inum.h"
#include "xfs_trans.h"
#include "xfs_sb.h"
#include "xfs_ag.h"
#include "xfs_dir.h"
#include "xfs_dir2.h"
#include "xfs_dmapi.h"
#include "xfs_mount.h"
#include "xfs_alloc.h"
#include "xfs_da_btree.h"
#include "xfs_alloc_btree.h"
#include "xfs_bmap_btree.h"
#include "xfs_ialloc_btree.h"
#include "xfs_dir_sf.h"
#include "xfs_dir2_sf.h"
#include "xfs_attr_sf.h"
#include "xfs_dinode.h"
#include "xfs_inode.h"
#include "xfs_btree.h"
#include "xfs_buf_item.h"
#include "xfs_inode_item.h"
#include "xfs_extfree_item.h"
#include "xfs_rw.h"
#include "xfs_bmap.h"
#include "xfs_attr.h"
#include "xfs_attr_leaf.h"
#include "xfs_dir_leaf.h"
#include "xfs_dir2_data.h"
#include "xfs_dir2_leaf.h"
#include "xfs_dir2_block.h"
#include "xfs_dir2_node.h"
#include "xfs_dir2_trace.h"
#include "xfs_log_priv.h"
#include "xfs_log_recover.h"
#include "xfs_quota.h"
#include "quota/xfs_qm.h"
#include "xfs_iomap.h"

#include <support/kdb.h>

#define qprintf	kdb_printf

/*
 * Command table functions. (tracing)
 */
#ifdef XFS_ALLOC_TRACE
static void	xfsidbg_xalatrace(int);
static void	xfsidbg_xalbtrace(xfs_agblock_t);
static void	xfsidbg_xalgtrace(xfs_agnumber_t);
static void	xfsidbg_xalmtrace(xfs_mount_t *);
static void	xfsidbg_xalttrace(int);
#endif
#ifdef XFS_ATTR_TRACE
static void	xfsidbg_xattrtrace(int);
#endif
#ifdef XFS_BLI_TRACE
static void	xfsidbg_xblitrace(xfs_buf_log_item_t *);
#endif
#ifdef XFS_BMAP_TRACE
static void	xfsidbg_xbmatrace(int);
static void	xfsidbg_xbmitrace(xfs_inode_t *);
static void	xfsidbg_xbmstrace(xfs_inode_t *);
static void	xfsidbg_xbxatrace(int);
static void	xfsidbg_xbxitrace(xfs_inode_t *);
static void	xfsidbg_xbxstrace(xfs_inode_t *);
#endif
#ifdef XFS_ILOCK_TRACE
static void	xfsidbg_xilock_trace(xfs_inode_t *);
static void	xfsidbg_xailock_trace(int);
#endif
#ifdef XFS_DIR_TRACE
static void	xfsidbg_xdirtrace(int);
#endif
#ifdef XFS_DIR2_TRACE
static void	xfsidbg_xdir2atrace(int);
static void	xfsidbg_xdir2itrace(xfs_inode_t *);
#endif
#ifdef XFS_LOG_TRACE
static void	xfsidbg_xiclogtrace(xlog_in_core_t *);
static void	xfsidbg_xlog_granttrace(xlog_t *);
#endif
#ifdef XFS_DQUOT_TRACE
static void	xfsidbg_xqm_dqtrace(xfs_dquot_t *);
#endif


/*
 * Command table functions.
 */
static void	xfsidbg_xagf(xfs_agf_t *);
static void	xfsidbg_xagi(xfs_agi_t *);
static void	xfsidbg_xaildump(xfs_mount_t *);
static void	xfsidbg_xalloc(xfs_alloc_arg_t *);
static void	xfsidbg_xattrcontext(xfs_attr_list_context_t *);
static void	xfsidbg_xattrleaf(xfs_attr_leafblock_t *);
static void	xfsidbg_xattrsf(xfs_attr_shortform_t *);
static void	xfsidbg_xbirec(xfs_bmbt_irec_t *r);
static void	xfsidbg_xbmalla(xfs_bmalloca_t *);
static void	xfsidbg_xbrec(xfs_bmbt_rec_64_t *);
static void	xfsidbg_xbroot(xfs_inode_t *);
static void	xfsidbg_xbroota(xfs_inode_t *);
static void	xfsidbg_xbtcur(xfs_btree_cur_t *);
static void	xfsidbg_xbuf(xfs_buf_t *);
static void	xfsidbg_xbuf_real(xfs_buf_t *, int);
static void	xfsidbg_xarg(int);
static void	xfsidbg_xchksum(uint *);
static void	xfsidbg_xchash(xfs_mount_t *mp);
static void	xfsidbg_xchashlist(xfs_chashlist_t *chl);
static void	xfsidbg_xdaargs(xfs_da_args_t *);
static void	xfsidbg_xdabuf(xfs_dabuf_t *);
static void	xfsidbg_xdanode(xfs_da_intnode_t *);
static void	xfsidbg_xdastate(xfs_da_state_t *);
static void	xfsidbg_xdirleaf(xfs_dir_leafblock_t *);
static void	xfsidbg_xdirsf(xfs_dir_shortform_t *);
static void	xfsidbg_xdir2free(xfs_dir2_free_t *);
static void	xfsidbg_xdir2sf(xfs_dir2_sf_t *);
static void	xfsidbg_xexlist(xfs_inode_t *);
static void	xfsidbg_xflist(xfs_bmap_free_t *);
static void	xfsidbg_xhelp(void);
static void	xfsidbg_xiclog(xlog_in_core_t *);
static void	xfsidbg_xiclogall(xlog_in_core_t *);
static void	xfsidbg_xiclogcb(xlog_in_core_t *);
static void	xfsidbg_xihash(xfs_mount_t *mp);
static void	xfsidbg_xinodes(xfs_mount_t *);
static void	xfsidbg_delayed_blocks(xfs_mount_t *);
static void	xfsidbg_xinodes_quiesce(xfs_mount_t *);
static void	xfsidbg_xlog(xlog_t *);
static void	xfsidbg_xlog_ritem(xlog_recover_item_t *);
static void	xfsidbg_xlog_rtrans(xlog_recover_t *);
static void	xfsidbg_xlog_rtrans_entire(xlog_recover_t *);
static void	xfsidbg_xlog_tic(xlog_ticket_t *);
static void	xfsidbg_xlogitem(xfs_log_item_t *);
static void	xfsidbg_xmount(xfs_mount_t *);
static void	xfsidbg_xnode(xfs_inode_t *ip);
static void	xfsidbg_xcore(xfs_iocore_t *io);
static void	xfsidbg_xperag(xfs_mount_t *);
static void	xfsidbg_xqm_diskdq(xfs_disk_dquot_t *);
static void	xfsidbg_xqm_dqattached_inos(xfs_mount_t *);
static void	xfsidbg_xqm_dquot(xfs_dquot_t *);
static void	xfsidbg_xqm_mplist(xfs_mount_t *);
static void	xfsidbg_xqm_qinfo(xfs_mount_t *mp);
static void	xfsidbg_xqm_tpdqinfo(xfs_trans_t *tp);
static void	xfsidbg_xsb(xfs_sb_t *);
static void	xfsidbg_xsb_convert(xfs_sb_t *);
static void	xfsidbg_xtp(xfs_trans_t *);
static void	xfsidbg_xtrans_res(xfs_mount_t *);
#ifdef CONFIG_XFS_QUOTA
static void	xfsidbg_xqm(void);
static void	xfsidbg_xqm_htab(void);
static void	xfsidbg_xqm_freelist_print(xfs_frlist_t *qlist, char *title);
static void	xfsidbg_xqm_freelist(void);
#endif

#ifdef XFS_BMAP_TRACE
static void	xfs_convert_extent(xfs_bmbt_rec_32_t *, xfs_dfiloff_t *,
				xfs_dfsbno_t *, xfs_dfilblks_t *, int *);
#endif

/*
 * Prototypes for static functions.
 */
#ifdef XFS_ALLOC_TRACE
static int	xfs_alloc_trace_entry(ktrace_entry_t *ktep);
#endif
#ifdef XFS_ATTR_TRACE
static int	xfs_attr_trace_entry(ktrace_entry_t *ktep);
#endif
#ifdef XFS_BMAP_TRACE
static int	xfs_bmap_trace_entry(ktrace_entry_t *ktep);
#endif
#ifdef XFS_BMAP_TRACE
static int	xfs_bmbt_trace_entry(ktrace_entry_t *ktep);
#endif
#ifdef XFS_DIR_TRACE
static int	xfs_dir_trace_entry(ktrace_entry_t *ktep);
#endif
#ifdef XFS_DIR2_TRACE
static int	xfs_dir2_trace_entry(ktrace_entry_t *ktep);
#endif
#ifdef XFS_RW_TRACE
static void	xfs_bunmap_trace_entry(ktrace_entry_t   *ktep);
static void	xfs_rw_enter_trace_entry(ktrace_entry_t *ktep);
static void	xfs_page_trace_entry(ktrace_entry_t *ktep);
static int	xfs_rw_trace_entry(ktrace_entry_t *ktep);
#endif
static void	xfs_broot(xfs_inode_t *ip, xfs_ifork_t *f);
static void	xfs_btalloc(xfs_alloc_block_t *bt, int bsz);
static void	xfs_btbmap(xfs_bmbt_block_t *bt, int bsz);
static void	xfs_btino(xfs_inobt_block_t *bt, int bsz);
static void	xfs_buf_item_print(xfs_buf_log_item_t *blip, int summary);
static void	xfs_dastate_path(xfs_da_state_path_t *p);
static void	xfs_dir2data(void *addr, int size);
static void	xfs_dir2leaf(xfs_dir2_leaf_t *leaf, int size);
static void	xfs_dquot_item_print(xfs_dq_logitem_t *lip, int summary);
static void	xfs_efd_item_print(xfs_efd_log_item_t *efdp, int summary);
static void	xfs_efi_item_print(xfs_efi_log_item_t *efip, int summary);
static char *	xfs_fmtformat(xfs_dinode_fmt_t f);
char *		xfs_fmtfsblock(xfs_fsblock_t bno, xfs_mount_t *mp);
static char *	xfs_fmtino(xfs_ino_t ino, xfs_mount_t *mp);
static char *	xfs_fmtlsn(xfs_lsn_t *lsnp);
static char *	xfs_fmtmode(int m);
static char *	xfs_fmtsize(size_t i);
static char *	xfs_fmtuuid(uuid_t *);
static void	xfs_inode_item_print(xfs_inode_log_item_t *ilip, int summary);
static void	xfs_inodebuf(xfs_buf_t *bp);
static void	xfs_prdinode_core(xfs_dinode_core_t *dip);
static void	xfs_qoff_item_print(xfs_qoff_logitem_t *lip, int summary);
static void	xfs_xexlist_fork(xfs_inode_t *ip, int whichfork);
static void	xfs_xnode_fork(char *name, xfs_ifork_t *f);


/* kdb wrappers */

static int	kdbm_xfs_xagf(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xagf((xfs_agf_t *)addr);
	return 0;
}

static int	kdbm_xfs_xagi(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xagi((xfs_agi_t *)addr);
	return 0;
}

static int	kdbm_xfs_xaildump(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xaildump((xfs_mount_t *) addr);
	return 0;
}

#ifdef XFS_ALLOC_TRACE
static int	kdbm_xfs_xalatrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;
	
	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xalatrace((int) addr);
	return 0;
}

static int	kdbm_xfs_xalbtrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xalbtrace((xfs_agblock_t) addr);
	return 0;
}

static int	kdbm_xfs_xalgtrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xalgtrace((xfs_agnumber_t) addr);
	return 0;
}
#endif

#ifdef XFS_ATTR_TRACE
static int	kdbm_xfs_xattrtrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xattrtrace((int) addr);
	return 0;
}
#endif

#ifdef XFS_BLI_TRACE
static int	kdbm_xfs_xblitrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xblitrace((xfs_buf_log_item_t *) addr);
	return 0;
}
#endif

#ifdef XFS_BMAP_TRACE
static int	kdbm_xfs_xbmatrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbmatrace((int) addr);
	return 0;
}

static int	kdbm_xfs_xbmitrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbmitrace((xfs_inode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbmstrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbmstrace((xfs_inode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbxatrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbxatrace((int) addr);
	return 0;
}

static int	kdbm_xfs_xbxitrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbxitrace((xfs_inode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbxstrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbxstrace((xfs_inode_t *) addr);
	return 0;
}
#endif

#ifdef XFS_DIR2_TRACE
static int	kdbm_xfs_xdir2atrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdir2atrace((int) addr);
	return 0;
}

static int	kdbm_xfs_xdir2itrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdir2itrace((xfs_inode_t *) addr);
	return 0;
}
#endif

#ifdef XFS_DIR_TRACE
static int	kdbm_xfs_xdirtrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdirtrace((int) addr);
	return 0;
}
#endif

#ifdef XFS_LOG_TRACE
static int	kdbm_xfs_xiclogtrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xiclogtrace((xlog_in_core_t *) addr);
	return 0;
}
#endif

#ifdef XFS_ILOCK_TRACE
static int	kdbm_xfs_xilock_trace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xilock_trace((xfs_inode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xailock_trace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xailock_trace((int) addr);
	return 0;
}
#endif

#ifdef XFS_LOG_TRACE
static int	kdbm_xfs_xlog_granttrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xlog_granttrace((xlog_t *) addr);
	return 0;
}
#endif

#ifdef XFS_DQUOT_TRACE
static int	kdbm_xfs_xqm_dqtrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xqm_dqtrace((xfs_dquot_t *) addr);
	return 0;
}
#endif

#ifdef XFS_RW_TRACE
static int	kdbm_xfs_xrwtrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long	addr;
	int		nextarg = 1;
	long 		offset = 0;
	int 		diag;
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	xfs_inode_t	*ip;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	ip = (xfs_inode_t *) addr;
	if (ip->i_rwtrace == NULL) {
		qprintf("The inode trace buffer is not initialized\n");
		return 0;
	}
	qprintf("i_rwtrace = 0x%p\n", ip->i_rwtrace);
	ktep = ktrace_first(ip->i_rwtrace, &kts);
	while (ktep != NULL) {
		if (xfs_rw_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(ip->i_rwtrace, &kts);
	}
	return 0;
}
#endif

static int	kdbm_xfs_xalloc(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xalloc((xfs_alloc_arg_t *) addr);
	return 0;
}

#ifdef XFS_ALLOC_TRACE
static int	kdbm_xfs_xalmtrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xalmtrace((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xalttrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xalttrace((int) addr);
	return 0;
}
#endif /* XFS_ALLOC_TRACE */

static int	kdbm_xfs_xattrcontext(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xattrcontext((xfs_attr_list_context_t *) addr);
	return 0;
}

static int	kdbm_xfs_xattrleaf(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xattrleaf((xfs_attr_leafblock_t *) addr);
	return 0;
}

static int	kdbm_xfs_xattrsf(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xattrsf((xfs_attr_shortform_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbirec(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbirec((xfs_bmbt_irec_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbmalla(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbmalla((xfs_bmalloca_t *)addr);
	return 0;
}

static int	kdbm_xfs_xbrec(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbrec((xfs_bmbt_rec_64_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbroot(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbroot((xfs_inode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbroota(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbroota((xfs_inode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbtcur(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbtcur((xfs_btree_cur_t *) addr);
	return 0;
}

static int	kdbm_xfs_xbuf(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xbuf((xfs_buf_t *) addr);
	return 0;
}


static int	kdbm_xfs_xarg(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xarg((int) addr);
	return 0;
}

static int	kdbm_xfs_xchksum(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xchksum((uint *) addr);
	return 0;
}


static int	kdbm_xfs_xchash(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xchash((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xchashlist(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xchashlist((xfs_chashlist_t *) addr);
	return 0;
}


static int	kdbm_xfs_xdaargs(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdaargs((xfs_da_args_t *) addr);
	return 0;
}

static int	kdbm_xfs_xdabuf(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdabuf((xfs_dabuf_t *) addr);
	return 0;
}

static int	kdbm_xfs_xdanode(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdanode((xfs_da_intnode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xdastate(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdastate((xfs_da_state_t *) addr);
	return 0;
}

static int	kdbm_xfs_xdirleaf(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdirleaf((xfs_dir_leafblock_t *) addr);
	return 0;
}

static int	kdbm_xfs_xdirsf(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdirsf((xfs_dir_shortform_t *) addr);
	return 0;
}

static int	kdbm_xfs_xdir2free(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdir2free((xfs_dir2_free_t *) addr);
	return 0;
}

static int	kdbm_xfs_xdir2sf(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xdir2sf((xfs_dir2_sf_t *) addr);
	return 0;
}

static int	kdbm_xfs_xexlist(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xexlist((xfs_inode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xflist(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xflist((xfs_bmap_free_t *) addr);
	return 0;
}

static int	kdbm_xfs_xhelp(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	if (argc != 0)
		return KDB_ARGCOUNT;

	xfsidbg_xhelp();
	return 0;
}

static int	kdbm_xfs_xiclog(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xiclog((xlog_in_core_t *) addr);
	return 0;
}

static int	kdbm_xfs_xiclogall(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xiclogall((xlog_in_core_t *) addr);
	return 0;
}

static int	kdbm_xfs_xiclogcb(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xiclogcb((xlog_in_core_t *) addr);
	return 0;
}

static int	kdbm_xfs_xihash(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xihash((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xinodes(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xinodes((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_delayed_blocks(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_delayed_blocks((xfs_mount_t *) addr);
	return 0;
}


static int	kdbm_xfs_xinodes_quiesce(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xinodes_quiesce((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xlog(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xlog((xlog_t *) addr);
	return 0;
}

static int	kdbm_xfs_xlog_ritem(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xlog_ritem((xlog_recover_item_t *) addr);
	return 0;
}

static int	kdbm_xfs_xlog_rtrans(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xlog_rtrans((xlog_recover_t *) addr);
	return 0;
}

static int	kdbm_xfs_xlog_rtrans_entire(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xlog_rtrans_entire((xlog_recover_t *) addr);
	return 0;
}

static int	kdbm_xfs_xlog_tic(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xlog_tic((xlog_ticket_t *) addr);
	return 0;
}

static int	kdbm_xfs_xlogitem(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xlogitem((xfs_log_item_t *) addr);
	return 0;
}

static int	kdbm_xfs_xmount(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xmount((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xnode(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xnode((xfs_inode_t *) addr);
	return 0;
}

static int	kdbm_xfs_xcore(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xcore((xfs_iocore_t *) addr);
	return 0;
}

static int	kdbm_xfs_xperag(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xperag((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xqm_diskdq(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xqm_diskdq((xfs_disk_dquot_t *) addr);
	return 0;
}

static int	kdbm_xfs_xqm_dqattached_inos(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xqm_dqattached_inos((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xqm_dquot(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xqm_dquot((xfs_dquot_t *) addr);
	return 0;
}

#ifdef	CONFIG_XFS_QUOTA
static int	kdbm_xfs_xqm(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	if (argc != 0)
		return KDB_ARGCOUNT;

	xfsidbg_xqm();
	return 0;
}

static int	kdbm_xfs_xqm_freelist(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	if (argc != 0)
		return KDB_ARGCOUNT;

	xfsidbg_xqm_freelist();
	return 0;
}

static int	kdbm_xfs_xqm_htab(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	if (argc != 0)
		return KDB_ARGCOUNT;

	xfsidbg_xqm_htab();
	return 0;
}
#endif

static int	kdbm_xfs_xqm_mplist(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xqm_mplist((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xqm_qinfo(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xqm_qinfo((xfs_mount_t *) addr);
	return 0;
}

static int	kdbm_xfs_xqm_tpdqinfo(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xqm_tpdqinfo((xfs_trans_t *) addr);
	return 0;
}

static int	kdbm_xfs_xsb(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	unsigned long convert=0;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1 && argc!=2)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;
	if (argc==2) {
	    /* extra argument - conversion flag */
	    diag = kdbgetaddrarg(argc, argv, &nextarg, &convert, &offset, NULL, regs);
	    if (diag)
		    return diag;
	}

	if (convert)
		xfsidbg_xsb_convert((xfs_sb_t *) addr);
	else
		xfsidbg_xsb((xfs_sb_t *) addr);
	return 0;
}

static int	kdbm_xfs_xtp(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xtp((xfs_trans_t *) addr);
	return 0;
}

static int	kdbm_xfs_xtrans_res(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;
	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	xfsidbg_xtrans_res((xfs_mount_t *) addr);
	return 0;
}

/*
 * Vnode descriptor dump.
 * This table is a string version of all the flags defined in vnode.h.
 */
char *tab_vflags[] = {
	/* local only flags */
	"VINACT",		/*	 0x01 */
	"VRECLM",		/*	 0x02 */
	"VWAIT",		/*	 0x04 */
	"VMODIFIED",		/*	 0x08 */
	"INVALID0x10",		/*	 0x10 */
	"INVALID0x20",		/*	 0x20 */
	"INVALID0x40",		/*	 0x40 */
	"INVALID0x80",		/*	 0x80 */
	"INVALID0x100",		/*	0x100 */
	"INVALID0x200",		/*	0x200 */
	"INVALID0x400",		/*	0x400 */
	"INVALID0x800",		/*	0x800 */
	"INVALID0x1000",	/*     0x1000 */
	"INVALID0x2000",	/*     0x2000 */
	"INVALID0x4000",	/*     0x4000 */
	"INVALID0x8000",	/*     0x8000 */
	"INVALID0x10000",	/*    0x10000 */
	"INVALID0x20000",	/*    0x20000 */
	"INVALID0x40000",	/*    0x40000 */
	"INVALID0x80000",	/*    0x80000 */
	"VROOT",		/*   0x100000 */
	"INVALID0x200000",	/*   0x200000 */
	"INVALID00x400000",	/*   0x400000 */
	"INVALID0x800000",	/*   0x800000 */
	"INVALID0x1000000",	/*  0x1000000 */
	"INVALID0x2000000",	/*  0x2000000 */
	"VSHARE",		/*  0x4000000 */
	"INVALID0x8000000",     /*  0x8000000 */
	"VENF_LOCKING",		/* 0x10000000 */
	"VOPLOCK",		/* 0x20000000 */
	"VPURGE",		/* 0x40000000 */
	"INVALID0x80000000",	/* 0x80000000 */
	NULL
};

static void
printflags(register uint64_t flags,
	register char **strings,
	register char *name)
{
	register uint64_t mask = 1;

	if (name)
		kdb_printf("%s 0x%llx <", name, (unsigned long long)flags);

	while (flags != 0 && *strings) {
		if (mask & flags) {
			kdb_printf("%s ", *strings);
			flags &= ~mask;
		}
		mask <<= 1;
		strings++;
	}

	if (name)
		kdb_printf("> ");

	return;
}


static void printbhv(bhv_desc_t *bdp)
{
	int maxbhv = 20; /* if you get 20 bhvs you're in trouble already */
	kdb_symtab_t	 symtab;

	if (bdp == NULL) {
		kdb_printf("NULL bhv\n");
		return;
	}

	kdb_printf("bhv at 0x%p\n", bdp);
	while (bdp && maxbhv--) {
		if (kdbnearsym((unsigned long)bdp->bd_ops, &symtab))
			kdb_printf("  ops %s", symtab.sym_name);
		else
			kdb_printf("  ops %s/0x%p", "???", (void *)bdp->bd_ops);

		kdb_printf(" vobj 0x%p pdata 0x%p next 0x%p\n",
			   bdp->bd_vobj, bdp->bd_pdata, bdp->bd_next);

		bdp = bdp->bd_next;
	}
}


static void	printvnode(xfs_vnode_t *vp, unsigned long addr)
{
	kdb_printf("vnode: 0x%lx\n", addr);
	kdb_printf(" v_bh 0x%p\n", &vp->v_bh);

	printbhv(vp->v_fbhv);

	printflags((__psunsigned_t)vp->v_flag, tab_vflags, "flag =");
	kdb_printf("\n");

#ifdef	XFS_VNODE_TRACE
	kdb_printf("   v_trace 0x%p\n", vp->v_trace);
#endif	/* XFS_VNODE_TRACE */

	kdb_printf("   v_vfsp 0x%p v_number 0x%llx\n",
		vp->v_vfsp, (unsigned long long)vp->v_number);
}

static int	kdbm_vnode(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;
	xfs_vnode_t	vp;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);

	if (diag)
		return diag;

	if ((diag = kdb_getarea(vp, addr)))
		return diag;

	printvnode(&vp, addr);

	return 0;
}

static void
print_vfs(xfs_vfs_t	*vfs, unsigned long addr)
{
	kdb_printf("vfsp at 0x%lx", addr);
	kdb_printf(" vfs_flag 0x%x\n", vfs->vfs_flag);
	kdb_printf(" vfs_mp 0x%p", vfs->vfs_mp);
	kdb_printf(" vfs_bh 0x%p\n", &vfs->vfs_bh);

	printbhv(vfs->vfs_fbhv);
}

static int	kdbm_bhv(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;
	bhv_desc_t	*bh;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);

	if (diag)
		return diag;

	bh = (bhv_desc_t *)addr;

	printbhv(bh);

	return 0;
}

static int	kdbm_vfs(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	unsigned long addr;
	int nextarg = 1;
	long offset = 0;
	int diag;
	xfs_vfs_t		vfs;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);

	if (diag)
		return diag;

	if ((diag = kdb_getarea(vfs, addr)))
		return diag;

	print_vfs(&vfs, addr);

	return 0;
}


#ifdef	XFS_VNODE_TRACE
/*
 * Print a vnode trace entry.
 */
static int
vn_trace_pr_entry(ktrace_entry_t *ktep)
{
	char		funcname[128];
	kdb_symtab_t	symtab;


	if ((__psint_t)ktep->val[0] == 0)
		return 0;

	if (kdbnearsym((unsigned int)ktep->val[8], &symtab)) {
		unsigned long offval;

		offval = (unsigned int)ktep->val[8] - symtab.sym_start;

		if (offval)
			sprintf(funcname, "%s+0x%lx", symtab.sym_name, offval);
		else
			sprintf(funcname, "%s", symtab.sym_name);
	} else
		funcname[0] = '\0';


	switch ((__psint_t)ktep->val[0]) {
	case VNODE_KTRACE_ENTRY:
		kdb_printf("entry to %s i_count = %d",
						(char *)ktep->val[1],
						(__psint_t)ktep->val[3]);
		break;

	case VNODE_KTRACE_EXIT:
		kdb_printf("exit from %s i_count = %d",
						(char *)ktep->val[1],
						(__psint_t)ktep->val[3]);
		break;

	case VNODE_KTRACE_HOLD:
		if ((__psint_t)ktep->val[3] != 1)
			kdb_printf("hold @%s:%d(%s) i_count %d => %d ",
						(char *)ktep->val[1],
						(__psint_t)ktep->val[2],
						funcname,
						(__psint_t)ktep->val[3] - 1,
						(__psint_t)ktep->val[3]);
		else
			kdb_printf("get @%s:%d(%s) i_count = %d",
						(char *)ktep->val[1],
						(__psint_t)ktep->val[2],
						funcname,
						(__psint_t)ktep->val[3]);
		break;

	case VNODE_KTRACE_REF:
		kdb_printf("ref @%s:%d(%s) i_count = %d",
						(char *)ktep->val[1],
						(__psint_t)ktep->val[2],
						funcname,
						(__psint_t)ktep->val[3]);
		break;

	case VNODE_KTRACE_RELE:
		if ((__psint_t)ktep->val[3] != 1)
			kdb_printf("rele @%s:%d(%s) i_count %d => %d ",
						(char *)ktep->val[1],
						(__psint_t)ktep->val[2],
						funcname,
						(__psint_t)ktep->val[3],
						(__psint_t)ktep->val[3] - 1);
		else
			kdb_printf("free @%s:%d(%s) i_count = %d",
						(char *)ktep->val[1],
						(__psint_t)ktep->val[2],
						funcname,
						(__psint_t)ktep->val[3]);
		break;

	default:
		kdb_printf("unknown vntrace record\n");
		return 1;
	}

	kdb_printf("\n");

	kdb_printf("  cpu = %d pid = %d ",
			(__psint_t)ktep->val[6], (pid_t)ktep->val[7]);

	printflags((__psunsigned_t)ktep->val[5], tab_vflags, "flag =");

	if (kdbnearsym((unsigned int)ktep->val[4], &symtab)) {
		unsigned long offval;

		offval = (unsigned int)ktep->val[4] - symtab.sym_start;

		if (offval)
			kdb_printf("  ra = %s+0x%lx", symtab.sym_name, offval);
		else
			kdb_printf("  ra = %s", symtab.sym_name);
	} else
		kdb_printf("  ra = ?? 0x%p", (void *)ktep->val[4]);

	return 1;
}


/*
 * Print out the trace buffer attached to the given vnode.
 */
static int	kdbm_vntrace(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	int		diag;
	int		nextarg = 1;
	long		offset = 0;
	unsigned long	addr;
	xfs_vnode_t	*vp;
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;


	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);

	if (diag)
		return diag;

	vp = (xfs_vnode_t *)addr;

	if (vp->v_trace == NULL) {
		kdb_printf("The vnode trace buffer is not initialized\n");

		return 0;
	}

	kdb_printf("vntrace vp 0x%p\n", vp);

	ktep = ktrace_first(vp->v_trace, &kts);

	while (ktep != NULL) {
		if (vn_trace_pr_entry(ktep))
			kdb_printf("\n");

		ktep = ktrace_next(vp->v_trace, &kts);
	}

	return 0;
}
/*
 * Print out the trace buffer attached to the given vnode.
 */
static int	kdbm_vntraceaddr(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	int		diag;
	int		nextarg = 1;
	long		offset = 0;
	unsigned long	addr;
	struct ktrace	*kt;
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;


	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);

	if (diag)
		return diag;

	kt = (struct ktrace *)addr;

	kdb_printf("vntraceaddr kt 0x%p\n", kt);

	ktep = ktrace_first(kt, &kts);

	while (ktep != NULL) {
		if (vn_trace_pr_entry(ktep))
			kdb_printf("\n");

		ktep = ktrace_next(kt, &kts);
	}

	return 0;
}
#endif	/* XFS_VNODE_TRACE */

#ifdef __linux__
static void	printinode(struct inode *ip)
{
	unsigned long	addr;


	if (ip == NULL)
		return;

	kdb_printf(" i_ino = %lu i_count = %u i_size %Ld\n",
					ip->i_ino, atomic_read(&ip->i_count),
					ip->i_size);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
	kdb_printf(
		" i_mode = 0x%x  i_nlink = %d  i_rdev = 0x%x i_state = 0x%lx\n",
					ip->i_mode, ip->i_nlink,
					kdev_t_to_nr(ip->i_rdev), ip->i_state);
	kdb_printf(" i_hash.nxt = 0x%p i_hash.pprv = 0x%p\n",
                                        ip->i_hash.next, ip->i_hash.prev);
#else
	kdb_printf(
		" i_mode = 0x%x  i_nlink = %d  i_rdev = 0x%x i_state = 0x%lx\n",
					ip->i_mode, ip->i_nlink,
					ip->i_rdev, ip->i_state);
	kdb_printf(" i_hash.nxt = 0x%p i_hash.pprv = 0x%p\n",
                                        ip->i_hash.next, ip->i_hash.pprev);
#endif
	kdb_printf(" i_list.nxt = 0x%p i_list.prv = 0x%p\n",
					ip->i_list.next, ip->i_list.prev);
	kdb_printf(" i_dentry.nxt = 0x%p i_dentry.prv = 0x%p\n",
					ip->i_dentry.next,
					ip->i_dentry.prev);

	addr = (unsigned long)ip;

	kdb_printf(" i_sb = 0x%p i_op = 0x%p i_data = 0x%lx nrpages = %lu\n",
					ip->i_sb, ip->i_op,
					addr + offsetof(struct inode, i_data),
					ip->i_data.nrpages);

	kdb_printf("  vnode ptr 0x%p\n", vn_from_inode(ip));
}

#endif


static int	kdbm_vn(
	int	argc,
	const char **argv,
	const char **envp,
	struct pt_regs *regs)
{
	int		diag;
	int		nextarg = 1;
	long		offset = 0;
	unsigned long	addr;
#ifdef __linux__
	struct inode	*ip;
#endif
	xfs_vnode_t	vp;
#ifdef	XFS_VNODE_TRACE
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
#endif

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs);
	if (diag)
		return diag;

	if ((diag = kdb_getarea(vp, addr)))
		return diag;

#ifdef __linux__
	ip = vn_to_inode((vnode_t *)addr);
	kdb_printf("--> Inode @ 0x%p\n", ip);
	printinode(ip);
#endif

	kdb_printf("--> Vnode @ 0x%lx\n", addr);
	printvnode(&vp, addr);

#ifdef	XFS_VNODE_TRACE
	kdb_printf("--> Vntrace @ 0x%lx/0x%p\n", addr, vp.v_trace);
	if (vp.v_trace == NULL)
		return 0;
	ktep = ktrace_first(vp.v_trace, &kts);
	while (ktep != NULL) {
		if (vn_trace_pr_entry(ktep))
			kdb_printf("\n");

		ktep = ktrace_next(vp.v_trace, &kts);
	}
#endif	/* XFS_VNODE_TRACE */
	return 0;
}


#ifdef __linux__

static char	*bp_flag_vals[] = {
/*  0 */ "READ", "WRITE", "MAPPED", "PARTIAL", "ASYNC",
/*  5 */ "NONE", "DELWRI",  "STALE", "FS_MANAGED", "FS_DATAIOD",
/* 10 */ "FORCEIO", "FLUSH", "READ_AHEAD", "DIRECTIO", "LOCK",
/* 15 */ "TRYLOCK", "DONT_BLOCK", "PAGE_CACHE", "KMEM_ALLOC", "RUN_QUEUES",
/* 20 */ "PRIVATE_BH", "DELWRI_Q",
	 NULL };

#endif

static char	*iomap_flag_vals[] = {
	"EOF", "HOLE", "DELAY", "INVALID0x08",
	"INVALID0x10", "UNWRITTEN", "NEW", "INVALID0x80",
	NULL };


static char	*map_flags(unsigned long flags, char *mapping[])
{
	static	char	buffer[256];
	int	index;
	int	offset = 12;

	buffer[0] = '\0';

	for (index = 0; flags && mapping[index]; flags >>= 1, index++) {
		if (flags & 1) {
			if ((offset + strlen(mapping[index]) + 1) >= 80) {
				strcat(buffer, "\n	    ");
				offset = 12;
			} else if (offset > 12) {
				strcat(buffer, " ");
				offset++;
			}
			strcat(buffer, mapping[index]);
			offset += strlen(mapping[index]);
		}
	}

	return (buffer);
}

#ifdef __linux__

static char	*bp_flags(xfs_buf_flags_t bp_flag)
{
	return(map_flags((unsigned long) bp_flag, bp_flag_vals));
}

static int
kdbm_bp_flags(int argc, const char **argv, const char **envp, struct pt_regs *regs)
{
	unsigned long flags;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	diag = kdbgetularg(argv[1], &flags);
	if (diag)
		return diag;

	kdb_printf("bp flags 0x%lx = %s\n", flags, bp_flags(flags));

	return 0;
}

static void
print_xfs_buf(
	xfs_buf_t	*bp,
	unsigned long	addr)
{
	unsigned long	age = (xfs_buf_age_centisecs * HZ) / 100;

	kdb_printf("xfs_buf_t at 0x%lx\n", addr);
	kdb_printf("  b_flags %s\n", bp_flags(bp->b_flags));
	kdb_printf("  b_target 0x%p b_hold %d b_next 0x%p b_prev 0x%p\n",
		   bp->b_target, bp->b_hold.counter,
		   list_entry(bp->b_list.next, xfs_buf_t, b_list),
		   list_entry(bp->b_list.prev, xfs_buf_t, b_list));
	kdb_printf("  b_hash 0x%p b_hash_next 0x%p b_hash_prev 0x%p\n",
		   bp->b_hash,
		   list_entry(bp->b_hash_list.next, xfs_buf_t, b_hash_list),
		   list_entry(bp->b_hash_list.prev, xfs_buf_t, b_hash_list));
	kdb_printf("  b_file_offset 0x%llx b_buffer_length 0x%llx b_addr 0x%p\n",
		   (unsigned long long) bp->b_file_offset,
		   (unsigned long long) bp->b_buffer_length,
		   bp->b_addr);
	kdb_printf("  b_bn 0x%llx b_count_desired 0x%lx b_locked %d\n",
		   (unsigned long long)bp->b_bn,
		   (unsigned long) bp->b_count_desired, (int)bp->b_locked);
	kdb_printf("  b_queuetime %ld (now=%ld/age=%ld) b_io_remaining %d\n",
		   bp->b_queuetime, jiffies, bp->b_queuetime + age,
		   bp->b_io_remaining.counter);
	kdb_printf("  b_page_count %u b_offset 0x%x b_pages 0x%p b_error %u\n",
		   bp->b_page_count, bp->b_offset,
		   bp->b_pages, bp->b_error);
	kdb_printf("  b_iodonesema (%d,%d) b_sema (%d,%d) b_pincount (%d)\n",
		   bp->b_iodonesema.count.counter,
		   bp->b_iodonesema.sleepers,
		   bp->b_sema.count.counter, bp->b_sema.sleepers,
		   bp->b_pin_count.counter);
#ifdef XFS_BUF_LOCK_TRACKING
	kdb_printf("  last holder %d\n", bp->b_last_holder);
#endif
	if (bp->b_fspriv || bp->b_fspriv2) {
		kdb_printf(  "  b_fspriv 0x%p b_fspriv2 0x%p\n",
			   bp->b_fspriv, bp->b_fspriv2);
	}
}

static int
kdbm_bp(int argc, const char **argv, const char **envp, struct pt_regs *regs)
{
	xfs_buf_t bp;
	unsigned long addr;
	long	offset=0;
	int nextarg;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	nextarg = 1;
	if ((diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs)) ||
	    (diag = kdb_getarea(bp, addr)))
		return diag;

	print_xfs_buf(&bp, addr);

	return 0;
}

static int
kdbm_bpdelay(int argc, const char **argv, const char **envp,
	struct pt_regs *regs)
{
#ifdef DEBUG
	extern struct list_head xfs_buftarg_list;
	struct list_head	*curr, *next;
	xfs_buftarg_t		*tp, *n;
	xfs_buf_t		bp;
	unsigned long		addr, verbose = 0;
	int			diag, count = 0;

	if (argc > 1)
		return KDB_ARGCOUNT;

	if (argc == 1) {
		if ((diag = kdbgetularg(argv[1], &verbose))) {
			return diag;
		}
	}

	if (!verbose) {
		kdb_printf("index bp       pin   queuetime\n");
	}


	list_for_each_entry_safe(tp, n, &xfs_buftarg_list, bt_list) {
		list_for_each_safe(curr, next, &tp->bt_delwrite_queue) {
			addr = (unsigned long)list_entry(curr, xfs_buf_t, b_list);
			if ((diag = kdb_getarea(bp, addr)))
				return diag;

			if (verbose) {
				print_xfs_buf(&bp, addr);
			} else {
				kdb_printf("%4d  0x%lx   %d   %ld\n",
					count++, addr,
					bp.b_pin_count.counter,
					bp.b_queuetime);
			}
		}
	}
#else
	kdb_printf("bt_delwrite_queue inaccessible (non-debug)\n");
#endif
	return 0;
}
#endif

static int
kdbm_iomap(int argc, const char **argv, const char **envp,
	struct pt_regs *regs)
{
	xfs_iomap_t iomap;
	unsigned long addr;
	long offset=0;
	int nextarg;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	nextarg = 1;
	if ((diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs)))
		return diag;
	if ((diag = kdb_getarea(iomap, addr)))
		return diag;

	kdb_printf("iomap_t at 0x%lx\n", addr);
	kdb_printf("  bn 0x%llx offset 0x%Lx delta 0x%lx bsize 0x%llx\n",
		(long long) iomap.iomap_bn, iomap.iomap_offset,
		(unsigned long)iomap.iomap_delta, (long long)iomap.iomap_bsize);
	kdb_printf("  iomap_flags %s\n",
		map_flags(iomap.iomap_flags, iomap_flag_vals));

	return 0;
}

static int
kdbm_i2vnode(int argc, const char **argv, const char **envp,
	struct pt_regs *regs)
{
	struct vnode vp;
	struct inode *ip;
	unsigned long addr;
	long offset=0;
	int nextarg;
	int diag;

	if (argc != 1)
		return KDB_ARGCOUNT;

	nextarg = 1;
	if ((diag = kdbgetaddrarg(argc, argv, &nextarg, &addr, &offset, NULL, regs)))
		return diag;
	ip = (struct inode *)addr;
	if ((diag = kdb_getarea(vp, (unsigned long)vn_from_inode(ip))))
		return diag;

	kdb_printf("--> Inode @ 0x%p\n", ip);
	printinode(ip);

	kdb_printf("--> Vnode @ 0x%p\n", vn_from_inode(ip));
	printvnode(&vp, (unsigned long)vn_from_inode(ip));

	return 0;
}

#ifdef XFS_BUF_TRACE
static int xfs_buf_trace_entry(ktrace_entry_t *ktep)
{
	unsigned long long daddr;

	daddr = ((unsigned long long)(unsigned long)ktep->val[8] << 32)
		| ((unsigned long long)(unsigned long)ktep->val[9]);

	kdb_printf("bp 0x%p [%s] (hold %lu lock %ld) data 0x%p",
		ktep->val[0],
		(char *)ktep->val[1],
		(unsigned long)ktep->val[3],
		(long)ktep->val[4],
		ktep->val[6]);
	kdb_symbol_print((unsigned long)ktep->val[7], NULL,
		KDB_SP_SPACEB|KDB_SP_PAREN|KDB_SP_NEWLINE);
	kdb_printf("    offset 0x%llx size 0x%lx task 0x%p\n",
		daddr, (long)ktep->val[10], ktep->val[5]);
	kdb_printf("    flags: %s\n", bp_flags((int)(long)ktep->val[2]));
	return 1;
}

static int
kdbm_bptrace_offset(int argc, const char **argv, const char **envp,
	struct pt_regs *regs)
{
	long		mask = 0;
	unsigned long	got_offset = 0, offset = 0;
	int		diag;
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;

	if (argc > 2)
		return KDB_ARGCOUNT;

	if (argc > 0) {
		diag = kdbgetularg(argv[1], &offset);
		if (diag)
			return diag;
		got_offset = 1;		/* allows tracing offset zero */
	}

	if (argc > 1) {
		diag = kdbgetularg(argv[1], &mask);	/* sign extent mask */
		if (diag)
			return diag;
	}

	ktep = ktrace_first(xfs_buf_trace_buf, &kts);
	do {
		unsigned long long daddr;

		if (ktep == NULL)
			break;
		daddr = ((unsigned long long)(unsigned long)ktep->val[8] << 32)
			| ((unsigned long long)(unsigned long)ktep->val[9]);
		if (got_offset && ((daddr & ~mask) != offset))
			continue;
		if (xfs_buf_trace_entry(ktep))
			kdb_printf("\n");
	} while ((ktep = ktrace_next(xfs_buf_trace_buf, &kts)) != NULL);
	return 0;
}

static int
kdbm_bptrace(int argc, const char **argv, const char **envp,
	struct pt_regs *regs)
{
	unsigned long	addr = 0;
	int		diag, nextarg;
	long		offset = 0;
	char		*event_match = NULL;
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;

	if (argc > 1)
		return KDB_ARGCOUNT;

	if (argc == 1) {
		if (isupper(argv[1][0]) || islower(argv[1][0])) {
			event_match = (char *)argv[1];
			kdb_printf("event match on \"%s\"\n", event_match);
			argc = 0;
		} else {
			nextarg = 1;
			diag = kdbgetaddrarg(argc, argv,
					&nextarg, &addr, &offset, NULL, regs);
			if (diag) {
				kdb_printf("non-numeric arg: %s\n", argv[1]);
				return diag;
			}
		}
	}

	ktep = ktrace_first(xfs_buf_trace_buf, &kts);
	do {
		if (ktep == NULL)
			break;
		if (addr && (ktep->val[0] != (void *)addr))
			continue;
		if (event_match && strcmp((char *)ktep->val[1], event_match))
			continue;
		if (xfs_buf_trace_entry(ktep))
			qprintf("\n");
	} while ((ktep = ktrace_next(xfs_buf_trace_buf, &kts)) != NULL);

	return 0;
}
#endif

struct xif {
	char	*name;
	int	(*func)(int, const char **, const char **, struct pt_regs *);
	char	*args;
	char	*help;
};

static struct xif xfsidbg_funcs[] = {
  {  "bhv",	kdbm_bhv,	"<bhv>", "Dump bhv chain"},
  {  "vn",	kdbm_vn,	"<vnode>", "Dump inode/vnode/trace"},
  {  "vnode",	kdbm_vnode,	"<vnode>", "Dump vnode"},
  {  "vfs",	kdbm_vfs,	"<vfs>", "Dump vfs"},
#ifdef XFS_VNODE_TRACE
  {  "vntrace",	kdbm_vntrace,	"<vntrace>", "Dump vnode Trace"},
  {  "vntraceaddr",	kdbm_vntraceaddr, "<vntrace>",
				"Dump vnode Trace by Address"},
#endif
  {  "xagf",	kdbm_xfs_xagf,	"<agf>",
				"Dump XFS allocation group freespace" },
  {  "xagi",	kdbm_xfs_xagi,	"<agi>",
				"Dump XFS allocation group inode" },
  {  "xail",	kdbm_xfs_xaildump,	"<xfs_mount_t>",
				"Dump XFS AIL for a mountpoint" },
#ifdef XFS_ALLOC_TRACE
  {  "xalatrc",	kdbm_xfs_xalatrace,	"<count>",
				"Dump XFS alloc count trace" },
  {  "xalbtrc",	kdbm_xfs_xalbtrace,	"<xfs_agblock_t>",
				"Dump XFS alloc block trace" },
  {  "xalgtrc",	kdbm_xfs_xalgtrace,	"<xfs_agnumber_t>",
				"Dump XFS alloc alloc-group trace" },
#endif

  {  "xalloc",	kdbm_xfs_xalloc,	"<xfs_alloc_arg_t>",
				"Dump XFS allocation args structure" },
#ifdef XFS_ALLOC_TRACE
  {  "xalmtrc",	kdbm_xfs_xalmtrace,	"<xfs_mount_t>",
				"Dump XFS alloc mount-point trace" },
  {  "xalttrc",	kdbm_xfs_xalttrace,	"<tag>",
				"Dump XFS alloc trace by tag number" },
#endif
  {  "xarg",	kdbm_xfs_xarg,		"<value>",
				"Input XFS argument for next function" },
  {  "xattrcx",	kdbm_xfs_xattrcontext,	"<xfs_attr_list_context_t>",
				"Dump XFS attr_list context struct"},
  {  "xattrlf",	kdbm_xfs_xattrleaf,	"<xfs_attr_leafblock_t>",
				"Dump XFS attribute leaf block"},
  {  "xattrsf",	kdbm_xfs_xattrsf,	"<xfs_attr_shortform_t>",
				"Dump XFS attribute shortform"},
#ifdef XFS_ATTR_TRACE
  {  "xattrtr",	kdbm_xfs_xattrtrace,	"<count>",
				"Dump XFS attribute attr_list() trace" },
#endif
  {  "xbirec",	kdbm_xfs_xbirec,	"<xfs_bmbt_irec_t",
				"Dump XFS bmap incore record"},
#ifdef XFS_BLI_TRACE
  {  "xblitrc",	kdbm_xfs_xblitrace,	"<xfs_buf_log_item_t>",
				"Dump XFS buf log item trace" },
#endif
  {  "xbmalla",	kdbm_xfs_xbmalla,	"<xfs_bmalloca_t>",
				"Dump XFS bmalloc args structure"},
#ifdef XFS_BMAP_TRACE
  {  "xbmatrc",	kdbm_xfs_xbmatrace,	"<count>",
				"Dump XFS bmap btree count trace" },
  {  "xbmitrc",	kdbm_xfs_xbmitrace,	"<xfs_inode_t>",
				"Dump XFS bmap btree per-inode trace" },
  {  "xbmstrc",	kdbm_xfs_xbmstrace,	"<xfs_inode_t>",
				"Dump XFS bmap btree inode trace" },
#endif
  {  "xbrec",	kdbm_xfs_xbrec,		"<xfs_bmbt_rec_64_t>",
				"Dump XFS bmap record"},
  {  "xbroot",	kdbm_xfs_xbroot,	"<xfs_inode_t>",
				"Dump XFS bmap btree root (data)"},
  {  "xbroota",	kdbm_xfs_xbroota,	"<xfs_inode_t>",
				"Dump XFS bmap btree root (attr)"},
  {  "xbtcur",	kdbm_xfs_xbtcur,	"<xfs_btree_cur_t>",
				"Dump XFS btree cursor"},
  {  "xbuf",	kdbm_xfs_xbuf,		"<xfs_buf_t>",
				"Dump XFS data from a buffer"},
#ifdef XFS_BMAP_TRACE
  {  "xbxatrc",	kdbm_xfs_xbxatrace,	"<count>",
				"Dump XFS bmap extent count trace" },
  {  "xbxitrc",	kdbm_xfs_xbxitrace,	"<xfs_inode_t>",
				"Dump XFS bmap extent per-inode trace" },
  {  "xbxstrc",	kdbm_xfs_xbxstrace,	"<xfs_inode_t>",
				"Dump XFS bmap extent inode trace" },
#endif
  {  "xchash",	kdbm_xfs_xchash,	"<xfs_mount_t>",
				"Dump XFS cluster hash"},
  {  "xchlist",	kdbm_xfs_xchashlist,	"<xfs_chashlist_t>",
				"Dump XFS cluster hash list"},
  {  "xchksum",	kdbm_xfs_xchksum,	"<addr>", "Dump chksum" },
#ifdef XFS_DIR2_TRACE
  {  "xd2atrc",	kdbm_xfs_xdir2atrace,	"<count>",
				"Dump XFS directory v2 count trace" },
#endif
  {  "xd2free",	kdbm_xfs_xdir2free,	"<xfs_dir2_free_t>",
				"Dump XFS directory v2 freemap"},
#ifdef XFS_DIR2_TRACE
  {  "xd2itrc",	kdbm_xfs_xdir2itrace,	"<xfs_inode_t>",
				"Dump XFS directory v2 per-inode trace" },
#endif
  {  "xdaargs",	kdbm_xfs_xdaargs,	"<xfs_da_args_t>",
				"Dump XFS dir/attr args structure"},
  {  "xdabuf",	kdbm_xfs_xdabuf,	"<xfs_dabuf_t>",
				"Dump XFS dir/attr buf structure"},
  {  "xdanode",	kdbm_xfs_xdanode,	"<xfs_da_intnode_t>",
				"Dump XFS dir/attr node block"},
  {  "xdastat",	kdbm_xfs_xdastate,	"<xfs_da_state_t>",
				"Dump XFS dir/attr state_blk struct"},
  {  "xdelay",	kdbm_xfs_delayed_blocks,	"<xfs_mount_t>",
				"Dump delayed block totals"},
  {  "xdirlf",	kdbm_xfs_xdirleaf,	"<xfs_dir_leafblock_t>",
				"Dump XFS directory leaf block"},
  {  "xdirsf",	kdbm_xfs_xdirsf,	"<xfs_dir_shortform_t>",
				"Dump XFS directory shortform"},
  {  "xdir2sf",	kdbm_xfs_xdir2sf,	"<xfs_dir2_sf_t>",
				"Dump XFS directory v2 shortform"},
#ifdef XFS_DIR_TRACE
  {  "xdirtrc",	kdbm_xfs_xdirtrace,	"<count>",
				"Dump XFS directory getdents() trace" },
#endif
  {  "xdiskdq",	kdbm_xfs_xqm_diskdq,	"<xfs_disk_dquot_t>",
				"Dump XFS ondisk dquot (quota) struct"},
  {  "xdqatt",	kdbm_xfs_xqm_dqattached_inos,	"<xfs_mount_t>",
				 "All incore inodes with dquots"},
  {  "xdqinfo",	kdbm_xfs_xqm_tpdqinfo,	"<xfs_trans_t>",
				"Dump dqinfo structure of a trans"},
#ifdef XFS_DQUOT_TRACE
  {  "xdqtrace",kdbm_xfs_xqm_dqtrace,	"<xfs_dquot_t>",
				"Dump trace of a given dquot" },
#endif
  {  "xdquot",	kdbm_xfs_xqm_dquot,	"<xfs_dquot_t>",
				"Dump XFS dquot (quota) structure"},
  {  "xexlist",	kdbm_xfs_xexlist,	"<xfs_inode_t>",
				"Dump XFS bmap extents in inode"},
  {  "xflist",	kdbm_xfs_xflist,	"<xfs_bmap_free_t>",
				"Dump XFS to-be-freed extent records"},
  {  "xhelp",	kdbm_xfs_xhelp,		"",
				"Print idbg-xfs help"},
  {  "xicall",	kdbm_xfs_xiclogall,	"<xlog_in_core_t>",
				"Dump All XFS in-core logs"},
  {  "xiclog",	kdbm_xfs_xiclog,	"<xlog_in_core_t>",
				"Dump XFS in-core log"},
#ifdef XFS_LOG_TRACE
  {  "xictrc",	kdbm_xfs_xiclogtrace,	"<xlog_in_core_t>",
				"Dump XFS in-core log trace" },
#endif
  {  "xihash",	kdbm_xfs_xihash,	"<xfs_mount_t>",
				"Dump XFS inode hash statistics"},
#ifdef XFS_ILOCK_TRACE
  {  "xilocktrc",kdbm_xfs_xilock_trace,	"<xfs_inode_t>",
				"Dump XFS ilock trace" },
  {  "xailcktrc",kdbm_xfs_xailock_trace,"<count>",
				"Dump XFS global ilock trace" },
#endif
  {  "xinodes",	kdbm_xfs_xinodes,	"<xfs_mount_t>",
				"Dump XFS inodes per mount"},
  {  "xquiesce",kdbm_xfs_xinodes_quiesce, "<xfs_mount_t>",
				"Dump non-quiesced XFS inodes per mount"},
#ifdef XFS_LOG_TRACE
  {  "xl_grtr",	kdbm_xfs_xlog_granttrace,	"<xlog_t>",
				"Dump XFS log grant trace" },
#endif
  {  "xl_rcit",	kdbm_xfs_xlog_ritem,	"<xlog_recover_item_t>",
				"Dump XFS recovery item"},
  {  "xl_rctr",	kdbm_xfs_xlog_rtrans,	"<xlog_recover_t>",
				"Dump XFS recovery transaction"},
  {  "xl_rctr2",kdbm_xfs_xlog_rtrans_entire,	"<xlog_recover_t>",
				"Dump entire recovery transaction"},
  {  "xl_tic",	kdbm_xfs_xlog_tic,	"<xlog_ticket_t>",
				"Dump XFS log ticket"},
  {  "xlog",	kdbm_xfs_xlog,	"<xlog_t>",
				"Dump XFS log"},
  {  "xlogcb",	kdbm_xfs_xiclogcb,	"<xlog_in_core_t>",
				"Dump XFS in-core log callbacks"},
  {  "xlogitm",	kdbm_xfs_xlogitem,	"<xfs_log_item_t>",
				"Dump XFS log item structure"},
  {  "xmount",	kdbm_xfs_xmount,	"<xfs_mount_t>",
				"Dump XFS mount structure"},
  {  "xnode",	kdbm_xfs_xnode,		"<xfs_inode_t>",
				"Dump XFS inode"},
  {  "xiocore",	kdbm_xfs_xcore,		"<xfs_iocore_t>",
				"Dump XFS iocore"},
  {  "xperag",	kdbm_xfs_xperag,	"<xfs_mount_t>",
				"Dump XFS per-allocation group data"},
  {  "xqinfo",  kdbm_xfs_xqm_qinfo,	"<xfs_mount_t>",
				"Dump mount->m_quotainfo structure"},
#ifdef	CONFIG_XFS_QUOTA
  {  "xqm",	kdbm_xfs_xqm,		"",
				"Dump XFS quota manager structure"},
  {  "xqmfree",	kdbm_xfs_xqm_freelist,	"",
				"Dump XFS global freelist of dquots"},
  {  "xqmhtab",	kdbm_xfs_xqm_htab,	"",
				"Dump XFS hashtable of dquots"},
#endif	/* CONFIG_XFS_QUOTA */
  {  "xqmplist",kdbm_xfs_xqm_mplist,	"<xfs_mount_t>",
				"Dump XFS all dquots of a f/s"},
#ifdef XFS_RW_TRACE
  {  "xrwtrc",	kdbm_xfs_xrwtrace,	"<xfs_inode_t>",
				"Dump XFS inode read/write trace" },
#endif
  {  "xsb",	kdbm_xfs_xsb,		"<xfs_sb_t> <cnv>",
				"Dump XFS superblock"},
  {  "xtp",	kdbm_xfs_xtp,		"<xfs_trans_t>",
				"Dump XFS transaction structure"},
  {  "xtrres",	kdbm_xfs_xtrans_res,	"<xfs_mount_t>",
				"Dump XFS reservation values"},
  {  NULL,		NULL,	NULL }
};

static struct xif xfsbuf_funcs[] = {
  {  "bp",	kdbm_bp,	"<vaddr>",	"Display xfs_buf_t" },
  {  "bpflags",	kdbm_bp_flags,	"<flags>",	"Display xfs_buf flags" },
  {  "xiomap",	kdbm_iomap,	"<xfs_iomap_t *>",	"Display IOmap" },
  {  "i2vnode",	kdbm_i2vnode,	"<inode *>",	"Display Vnode" },
  {  "bpdelay",	kdbm_bpdelay,	"0|1",		"Display delwri buffers" },
#ifdef XFS_BUF_TRACE
  {  "bptrace",	kdbm_bptrace,	"<vaddr>|<count>",	"xfs_buf_t trace" },
  {  "bpoffset",kdbm_bptrace_offset, "<daddr> [<mask>]","xfs_buf_t trace" },
#endif
  {  NULL,		NULL,	NULL }
};

static int
xfsidbg_init(void)
{
	struct xif	*p;

	for (p = xfsidbg_funcs; p->name; p++)
		kdb_register(p->name, p->func, p->args, p->help, 0);
	for (p = xfsbuf_funcs; p->name; p++)
		kdb_register(p->name, p->func, p->args, p->help, 0);
	return 0;
}

static void
xfsidbg_exit(void)
{
	struct xif	*p;

	for (p = xfsidbg_funcs; p->name; p++)
		kdb_unregister(p->name);
	for (p = xfsbuf_funcs; p->name; p++)
		kdb_unregister(p->name);
}

/*
 * Argument to xfs_alloc routines, for allocation type.
 */
static char *xfs_alloctype[] = {
	"any_ag", "first_ag", "start_ag", "this_ag",
	"start_bno", "near_bno", "this_bno"
};

/*
 * Static functions.
 */

#ifdef XFS_ALLOC_TRACE
/*
 * Print xfs alloc trace buffer entry.
 */
static int
xfs_alloc_trace_entry(ktrace_entry_t *ktep)
{
	static char *modagf_flags[] = {
		"magicnum",
		"versionnum",
		"seqno",
		"length",
		"roots",
		"levels",
		"flfirst",
		"fllast",
		"flcount",
		"freeblks",
		"longest",
		NULL
	};

	if (((__psint_t)ktep->val[0] & 0xffff) == 0)
		return 0;
	switch ((long)ktep->val[0] & 0xffffL) {
	case XFS_ALLOC_KTRACE_ALLOC:
		kdb_printf("alloc %s[%s %ld] mp 0x%p\n",
			(char *)ktep->val[1],
			ktep->val[2] ? (char *)ktep->val[2] : "",
			(long)ktep->val[0] >> 16,
			(xfs_mount_t *)ktep->val[3]);
		kdb_printf(
	"agno %ld agbno %ld minlen %ld maxlen %ld mod %ld prod %ld minleft %ld\n",
			(long)ktep->val[4],
			(long)ktep->val[5],
			(long)ktep->val[6],
			(long)ktep->val[7],
			(long)ktep->val[8],
			(long)ktep->val[9],
			(long)ktep->val[10]);
		kdb_printf("total %ld alignment %ld len %ld type %s otype %s\n",
			(long)ktep->val[11],
			(long)ktep->val[12],
			(long)ktep->val[13],
			xfs_alloctype[((__psint_t)ktep->val[14]) >> 16],
			xfs_alloctype[((__psint_t)ktep->val[14]) & 0xffff]);
		kdb_printf("wasdel %d wasfromfl %d isfl %d userdata %d\n",
			((__psint_t)ktep->val[15] & (1 << 3)) != 0,
			((__psint_t)ktep->val[15] & (1 << 2)) != 0,
			((__psint_t)ktep->val[15] & (1 << 1)) != 0,
			((__psint_t)ktep->val[15] & (1 << 0)) != 0);
		break;
	case XFS_ALLOC_KTRACE_FREE:
		kdb_printf("free %s[%s %ld] mp 0x%p\n",
			(char *)ktep->val[1],
			ktep->val[2] ? (char *)ktep->val[2] : "",
			(long)ktep->val[0] >> 16,
			(xfs_mount_t *)ktep->val[3]);
		kdb_printf("agno %ld agbno %ld len %ld isfl %d\n",
			(long)ktep->val[4],
			(long)ktep->val[5],
			(long)ktep->val[6],
			(__psint_t)ktep->val[7] != 0);
		break;
	case XFS_ALLOC_KTRACE_MODAGF:
		kdb_printf("modagf %s[%s %ld] mp 0x%p\n",
			(char *)ktep->val[1],
			ktep->val[2] ? (char *)ktep->val[2] : "",
			(long)ktep->val[0] >> 16,
			(xfs_mount_t *)ktep->val[3]);
		printflags((__psint_t)ktep->val[4], modagf_flags, "modified");
		kdb_printf("seqno %lu length %lu roots b %lu c %lu\n",
			(unsigned long)ktep->val[5],
			(unsigned long)ktep->val[6],
			(unsigned long)ktep->val[7],
			(unsigned long)ktep->val[8]);
		kdb_printf("levels b %lu c %lu flfirst %lu fllast %lu flcount %lu\n",
			(unsigned long)ktep->val[9],
			(unsigned long)ktep->val[10],
			(unsigned long)ktep->val[11],
			(unsigned long)ktep->val[12],
			(unsigned long)ktep->val[13]);
		kdb_printf("freeblks %lu longest %lu\n",
			(unsigned long)ktep->val[14],
			(unsigned long)ktep->val[15]);
		break;

	case XFS_ALLOC_KTRACE_UNBUSY:
		kdb_printf("unbusy %s [%s %ld] mp 0x%p\n",
			(char *)ktep->val[1],
			ktep->val[2] ? (char *)ktep->val[2] : "",
			(long)ktep->val[0] >> 16,
			(xfs_mount_t *)ktep->val[3]);
		kdb_printf("      agno %lu slot %lu tp 0x%p\n",
			(unsigned long)ktep->val[4],
			(unsigned long)ktep->val[7],
			(xfs_trans_t *)ktep->val[8]);
		break;
	case XFS_ALLOC_KTRACE_BUSY:
		kdb_printf("busy %s [%s %ld] mp 0x%p\n",
			(char *)ktep->val[1],
			ktep->val[2] ? (char *)ktep->val[2] : "",
			(long)ktep->val[0] >> 16,
			(xfs_mount_t *)ktep->val[3]);
		kdb_printf("      agno %lu agbno %lu len %lu slot %lu tp 0x%p\n",
			(unsigned long)ktep->val[4],
			(unsigned long)ktep->val[5],
			(unsigned long)ktep->val[6],
			(unsigned long)ktep->val[7],
			(xfs_trans_t *)ktep->val[8]);
		break;
	case XFS_ALLOC_KTRACE_BUSYSEARCH:
		kdb_printf("busy-search %s [%s %ld] mp 0x%p\n",
			(char *)ktep->val[1],
			ktep->val[2] ? (char *)ktep->val[2] : "",
			(long)ktep->val[0] >> 16,
			(xfs_mount_t *)ktep->val[3]);
		kdb_printf("      agno %ld agbno %ld len %ld slot %ld tp 0x%p\n",
			(unsigned long)ktep->val[4],
			(unsigned long)ktep->val[5],
			(unsigned long)ktep->val[6],
			(unsigned long)ktep->val[7],
			(xfs_trans_t *)ktep->val[8]);
		break;
	default:
		kdb_printf("unknown alloc trace record\n");
		break;
	}
	return 1;
}
#endif /* XFS_ALLOC_TRACE */

#ifdef XFS_ATTR_TRACE
/*
 * Print an attribute trace buffer entry.
 */
static int
xfs_attr_trace_entry(ktrace_entry_t *ktep)
{
	static char *attr_arg_flags[] = {
		"DONTFOLLOW",	/* 0x0001 */
		"ROOT",		/* 0x0002 */
		"TRUSTED",	/* 0x0004 */
		"SECURE",	/* 0x0008 */
		"CREATE",	/* 0x0010 */
		"REPLACE",	/* 0x0020 */
		"?",		/* 0x0040 */
		"?",		/* 0x0080 */
		"SYSTEM",	/* 0x0100 */
		"?",		/* 0x0200 */
		"?",		/* 0x0400 */
		"?",		/* 0x0800 */
		"KERNOTIME",	/* 0x1000 */
		"KERNOVAL",	/* 0x2000 */
		"KERNAMELS",	/* 0x4000 */
		"KERNFULLS",	/* 0x8000 */
		NULL
	};

	if (!ktep->val[0])
		return 0;

	qprintf("-- %s: cursor h/b/o 0x%lx/0x%lx/%lu, dupcnt %lu, dp 0x%p\n",
		 (char *)ktep->val[1],
		 (unsigned long)ktep->val[3],
		 (unsigned long)ktep->val[4],
		 (unsigned long)ktep->val[5],
		 (unsigned long)ktep->val[11],
		 (xfs_inode_t *)ktep->val[2]);
	qprintf("   alist 0x%p, size %lu, count %lu, firstu %lu, Llen %lu",
		 (attrlist_t *)ktep->val[6],
		 (unsigned long)ktep->val[7],
		 (unsigned long)ktep->val[8],
		 (unsigned long)ktep->val[9],
		 (unsigned long)ktep->val[10]);
	printflags((__psunsigned_t)(ktep->val[12]), attr_arg_flags, ", flags");
	qprintf("\n");

	switch ((__psint_t)ktep->val[0]) {
	case XFS_ATTR_KTRACE_L_C:
		break;
	case XFS_ATTR_KTRACE_L_CN:
		qprintf("   node: count %lu, 1st hash 0x%lx, last hash 0x%lx\n",
			 (unsigned long)ktep->val[13],
			 (unsigned long)ktep->val[14],
			 (unsigned long)ktep->val[15]);
		break;
	case XFS_ATTR_KTRACE_L_CB:
		qprintf("   btree: hash 0x%lx, blkno 0x%lx\n",
			 (unsigned long)ktep->val[13],
			 (unsigned long)ktep->val[14]);
		break;
	case XFS_ATTR_KTRACE_L_CL:
		qprintf("   leaf: count %ld, 1st hash 0x%lx, last hash 0x%lx\n",
			 (unsigned long)ktep->val[13],
			 (unsigned long)ktep->val[14],
			 (unsigned long)ktep->val[15]);
		break;
	default:
		qprintf("   unknown attr trace record format\n");
		break;
	}
	return 1;
}
#endif /* XFS_ATTR_TRACE */

#ifdef XFS_BMAP_TRACE
/*
 * Print xfs bmap extent trace buffer entry.
 */
static int
xfs_bmap_trace_entry(ktrace_entry_t *ktep)
{
	xfs_dfsbno_t	    b;
	xfs_dfilblks_t	  c;
	xfs_inode_t	     *ip;
	xfs_ino_t	       ino;
	xfs_dfiloff_t	   o;
	int		     flag;
	int		     opcode;
	static char	     *ops[] = { "del", "ins", "pre", "post" };
	xfs_bmbt_rec_32_t       r;
	int		     whichfork;

	opcode = ((__psint_t)ktep->val[0]) & 0xffff;
	if (opcode == 0)
		return 0;
	whichfork = ((__psint_t)ktep->val[0]) >> 16;
	ip = (xfs_inode_t *)ktep->val[3];
	ino = ((xfs_ino_t)(unsigned long)ktep->val[6] << 32) |
		((xfs_ino_t)(unsigned long)ktep->val[7]);
	qprintf("%s %s:%s ip %p ino %s %cf\n",
		ops[opcode - 1], (char *)ktep->val[1],
		(char *)ktep->val[2], ip, xfs_fmtino(ino, ip->i_mount),
		"da"[whichfork]);
	r.l0 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[8];
	r.l1 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[9];
	r.l2 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[10];
	r.l3 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[11];
	xfs_convert_extent(&r, &o, &b, &c, &flag);
	qprintf(" idx %ld offset %lld block %s",
		(long)ktep->val[4], o,
		xfs_fmtfsblock((xfs_fsblock_t)b, ip->i_mount));
	qprintf(" count %lld flag %d\n", c, flag);
	if ((__psint_t)ktep->val[5] != 2)
		return 1;
	r.l0 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[12];
	r.l1 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[13];
	r.l2 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[14];
	r.l3 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[15];
	xfs_convert_extent(&r, &o, &b, &c, &flag);
	qprintf(" offset %lld block %s", o,
		xfs_fmtfsblock((xfs_fsblock_t)b, ip->i_mount));
	qprintf(" count %lld flag %d\n", c, flag);
	return 1;
}

/*
 * Print xfs bmap btree trace buffer entry.
 */
static int
xfs_bmbt_trace_entry(
	ktrace_entry_t	  *ktep)
{
	int			line;
	xfs_bmbt_rec_32_t	r;
	xfs_bmbt_irec_t		s;
	int			type;
	int			whichfork;

	type = (__psint_t)ktep->val[0] & 0xff;
	if (type == 0)
		return 0;
	whichfork = ((__psint_t)ktep->val[0] >> 8) & 0xff;
	line = ((__psint_t)ktep->val[0] >> 16) & 0xffff;
	qprintf("%s[%s@%d] ip 0x%p %cf cur 0x%p\n",
		(char *)ktep->val[1],
		(char *)ktep->val[2],
		line,
		(xfs_inode_t *)ktep->val[3],
		"da"[whichfork],
		(xfs_btree_cur_t *)ktep->val[4]);
	switch (type) {
	case XFS_BMBT_KTRACE_ARGBI:
		qprintf(" buf 0x%p i %ld\n",
			(xfs_buf_t *)ktep->val[5],
			(long)ktep->val[6]);
		break;
	case XFS_BMBT_KTRACE_ARGBII:
		qprintf(" buf 0x%p i0 %ld i1 %ld\n",
			(xfs_buf_t *)ktep->val[5],
			(long)ktep->val[6],
			(long)ktep->val[7]);
		break;
	case XFS_BMBT_KTRACE_ARGFFFI:
		qprintf(" o 0x%x%08x b 0x%x%08x i 0x%x%08x j %ld\n",
			(unsigned int)(long)ktep->val[5],
			(unsigned int)(long)ktep->val[6],
			(unsigned int)(long)ktep->val[7],
			(unsigned int)(long)ktep->val[8],
			(unsigned int)(long)ktep->val[9],
			(unsigned int)(long)ktep->val[10],
			(long)ktep->val[11]);
		break;
	case XFS_BMBT_KTRACE_ARGI:
		qprintf(" i 0x%lx\n",
			(long)ktep->val[5]);
		break;
	case XFS_BMBT_KTRACE_ARGIFK:
		qprintf(" i 0x%lx f 0x%x%08x o 0x%x%08x\n",
			(long)ktep->val[5],
			(unsigned int)(long)ktep->val[6],
			(unsigned int)(long)ktep->val[7],
			(unsigned int)(long)ktep->val[8],
			(unsigned int)(long)ktep->val[9]);
		break;
	case XFS_BMBT_KTRACE_ARGIFR:
		qprintf(" i 0x%lx f 0x%x%08x ",
			(long)ktep->val[5],
			(unsigned int)(long)ktep->val[6],
			(unsigned int)(long)ktep->val[7]);
		s.br_startoff = (xfs_fileoff_t)
			(((xfs_dfiloff_t)(unsigned long)ktep->val[8] << 32) |
				(xfs_dfiloff_t)(unsigned long)ktep->val[9]);
		s.br_startblock = (xfs_fsblock_t)
			(((xfs_dfsbno_t)(unsigned long)ktep->val[10] << 32) |
				(xfs_dfsbno_t)(unsigned long)ktep->val[11]);
		s.br_blockcount = (xfs_filblks_t)
			(((xfs_dfilblks_t)(unsigned long)ktep->val[12] << 32) |
				(xfs_dfilblks_t)(unsigned long)ktep->val[13]);
		xfsidbg_xbirec(&s);
		break;
	case XFS_BMBT_KTRACE_ARGIK:
		qprintf(" i 0x%lx o 0x%x%08x\n",
			(long)ktep->val[5],
			(unsigned int)(long)ktep->val[6],
			(unsigned int)(long)ktep->val[7]);
		break;
	case XFS_BMBT_KTRACE_CUR:
		qprintf(" nlevels %ld flags %ld allocated %ld ",
			((long)ktep->val[5] >> 24) & 0xff,
			((long)ktep->val[5] >> 16) & 0xff,
			(long)ktep->val[5] & 0xffff);
		r.l0 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[6];
		r.l1 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[7];
		r.l2 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[8];
		r.l3 = (xfs_bmbt_rec_base_t)(unsigned long)ktep->val[9];
		xfsidbg_xbrec((xfs_bmbt_rec_64_t *)&r);
		qprintf(" bufs 0x%p 0x%p 0x%p 0x%p ",
			(xfs_buf_t *)ktep->val[10],
			(xfs_buf_t *)ktep->val[11],
			(xfs_buf_t *)ktep->val[12],
			(xfs_buf_t *)ktep->val[13]);
		qprintf("ptrs %ld %ld %ld %ld\n",
			(long)ktep->val[14] >> 16,
			(long)ktep->val[14] & 0xffff,
			(long)ktep->val[15] >> 16,
			(long)ktep->val[15] & 0xffff);
		break;
	default:
		qprintf("unknown bmbt trace record\n");
		break;
	}
	return 1;
}
#endif

/*
 * Print an xfs in-inode bmap btree root.
 */
static void
xfs_broot(xfs_inode_t *ip, xfs_ifork_t *f)
{
	xfs_bmbt_block_t	*broot;
	int			format;
	int			i;
	xfs_bmbt_key_t		*kp;
	xfs_bmbt_ptr_t		*pp;

	format = f == &ip->i_df ? ip->i_d.di_format : ip->i_d.di_aformat;
	if ((f->if_flags & XFS_IFBROOT) == 0 ||
	    format != XFS_DINODE_FMT_BTREE) {
		kdb_printf("inode 0x%p not btree format\n", ip);
		return;
	}
	broot = f->if_broot;
	kdb_printf("block @0x%p magic %x level %d numrecs %d\n",
		broot,
		be32_to_cpu(broot->bb_magic),
		be16_to_cpu(broot->bb_level),
		be16_to_cpu(broot->bb_numrecs));
	kp = XFS_BMAP_BROOT_KEY_ADDR(broot, 1, f->if_broot_bytes);
	pp = XFS_BMAP_BROOT_PTR_ADDR(broot, 1, f->if_broot_bytes);
	for (i = 1; i <= be16_to_cpu(broot->bb_numrecs); i++)
		kdb_printf("\t%d: startoff %Ld ptr %Lx %s\n",
			i, INT_GET(kp[i - 1].br_startoff, ARCH_CONVERT), INT_GET(pp[i - 1], ARCH_CONVERT),
			xfs_fmtfsblock(INT_GET(pp[i - 1], ARCH_CONVERT), ip->i_mount));
}

/*
 * Print allocation btree block.
 */
static void
xfs_btalloc(xfs_alloc_block_t *bt, int bsz)
{
	int i;

	kdb_printf("magic 0x%x level %d numrecs %d leftsib 0x%x rightsib 0x%x\n",
		be32_to_cpu(bt->bb_magic),
		be16_to_cpu(bt->bb_level),
		be16_to_cpu(bt->bb_numrecs),
		be32_to_cpu(bt->bb_leftsib),
		be32_to_cpu(bt->bb_rightsib));
	if (!bt->bb_level) {
		for (i = 1; i <= be16_to_cpu(bt->bb_numrecs); i++) {
			xfs_alloc_rec_t *r;

			r = XFS_BTREE_REC_ADDR(bsz, xfs_alloc, bt, i, 0);
			kdb_printf("rec %d startblock 0x%x blockcount %d\n",
				i,
				be32_to_cpu(r->ar_startblock),
				be32_to_cpu(r->ar_blockcount));
		}
	} else {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_alloc, 0);
		for (i = 1; i <= be16_to_cpu(bt->bb_numrecs); i++) {
			xfs_alloc_key_t *k;
			xfs_alloc_ptr_t *p;

			k = XFS_BTREE_KEY_ADDR(bsz, xfs_alloc, bt, i, mxr);
			p = XFS_BTREE_PTR_ADDR(bsz, xfs_alloc, bt, i, mxr);
			kdb_printf("key %d startblock 0x%x blockcount %d ptr 0x%x\n",
				i,
				be32_to_cpu(k->ar_startblock),
				be32_to_cpu(k->ar_blockcount),
				be32_to_cpu(*p));
		}
	}
}

/*
 * Print a bmap btree block.
 */
static void
xfs_btbmap(xfs_bmbt_block_t *bt, int bsz)
{
	int i;

	kdb_printf("magic 0x%x level %d numrecs %d leftsib %Lx rightsib %Lx\n",
		be32_to_cpu(bt->bb_magic),
		be16_to_cpu(bt->bb_level),
		be16_to_cpu(bt->bb_numrecs),
		be64_to_cpu(bt->bb_leftsib),
		be64_to_cpu(bt->bb_rightsib));
	if (!bt->bb_level) {
		for (i = 1; i <= be16_to_cpu(bt->bb_numrecs); i++) {
			xfs_bmbt_rec_t *r;
			xfs_bmbt_irec_t	irec;

			r = (xfs_bmbt_rec_t *)XFS_BTREE_REC_ADDR(bsz,
				xfs_bmbt, bt, i, 0);

			xfs_bmbt_disk_get_all((xfs_bmbt_rec_t *)r, &irec);
			kdb_printf("rec %d startoff %Ld startblock %Lx blockcount %Ld flag %d\n",
				i, irec.br_startoff,
				(__uint64_t)irec.br_startblock,
				irec.br_blockcount, irec.br_state);
		}
	} else {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_bmbt, 0);
		for (i = 1; i <= be16_to_cpu(bt->bb_numrecs); i++) {
			xfs_bmbt_key_t *k;
			xfs_bmbt_ptr_t *p;

			k = XFS_BTREE_KEY_ADDR(bsz, xfs_bmbt, bt, i, mxr);
			p = XFS_BTREE_PTR_ADDR(bsz, xfs_bmbt, bt, i, mxr);
			kdb_printf("key %d startoff %Ld ",
				i, INT_GET(k->br_startoff, ARCH_CONVERT));
			kdb_printf("ptr %Lx\n", INT_GET(*p, ARCH_CONVERT));
		}
	}
}

/*
 * Print an inode btree block.
 */
static void
xfs_btino(xfs_inobt_block_t *bt, int bsz)
{
	int i;

	kdb_printf("magic 0x%x level %d numrecs %d leftsib 0x%x rightsib 0x%x\n",
		be32_to_cpu(bt->bb_magic),
		be16_to_cpu(bt->bb_level),
		be16_to_cpu(bt->bb_numrecs),
		be32_to_cpu(bt->bb_leftsib),
		be32_to_cpu(bt->bb_rightsib));
	if (!bt->bb_level) {
		for (i = 1; i <= be16_to_cpu(bt->bb_numrecs); i++) {
			xfs_inobt_rec_t *r;

			r = XFS_BTREE_REC_ADDR(bsz, xfs_inobt, bt, i, 0);
			kdb_printf("rec %d startino 0x%x freecount %d, free %Lx\n",
				i, INT_GET(r->ir_startino, ARCH_CONVERT),
				INT_GET(r->ir_freecount, ARCH_CONVERT),
				INT_GET(r->ir_free, ARCH_CONVERT));
		}
	} else {
		int mxr;

		mxr = XFS_BTREE_BLOCK_MAXRECS(bsz, xfs_inobt, 0);
		for (i = 1; i <= be16_to_cpu(bt->bb_numrecs); i++) {
			xfs_inobt_key_t *k;
			xfs_inobt_ptr_t *p;

			k = XFS_BTREE_KEY_ADDR(bsz, xfs_inobt, bt, i, mxr);
			p = XFS_BTREE_PTR_ADDR(bsz, xfs_inobt, bt, i, mxr);
			kdb_printf("key %d startino 0x%x ptr 0x%x\n",
				i, INT_GET(k->ir_startino, ARCH_CONVERT),
				be32_to_cpu(*p));
		}
	}
}

/*
 * Print a buf log item.
 */
static void
xfs_buf_item_print(xfs_buf_log_item_t *blip, int summary)
{
	static char *bli_flags[] = {
		"hold",		/* 0x1 */
		"dirty",	/* 0x2 */
		"stale",	/* 0x4 */
		"logged",	/* 0x8 */
		"ialloc",	/* 0x10 */
		"inode_stale",  /* 0x20 */
		NULL
		};
	static char *blf_flags[] = {
		"inode",	/* 0x1 */
		"cancel",	/* 0x2 */
		NULL
		};

	if (summary) {
		kdb_printf("buf 0x%p blkno 0x%Lx ", blip->bli_buf,
			     blip->bli_format.blf_blkno);
		printflags(blip->bli_flags, bli_flags, "flags:");
		kdb_printf("\n   ");
		xfsidbg_xbuf_real(blip->bli_buf, 1);
		return;
	}
	kdb_printf("buf 0x%p recur %d refcount %d flags:",
		blip->bli_buf, blip->bli_recur,
		atomic_read(&blip->bli_refcount));
	printflags(blip->bli_flags, bli_flags, NULL);
	kdb_printf("\n");
	kdb_printf("size %d blkno 0x%Lx len 0x%x map size %d map 0x%p\n",
		blip->bli_format.blf_size, blip->bli_format.blf_blkno,
		(uint) blip->bli_format.blf_len, blip->bli_format.blf_map_size,
		&(blip->bli_format.blf_data_map[0]));
	kdb_printf("blf flags: ");
	printflags((uint)blip->bli_format.blf_flags, blf_flags, NULL);
#ifdef XFS_TRANS_DEBUG
	kdb_printf("orig 0x%x logged 0x%x",
		blip->bli_orig, blip->bli_logged);
#endif
	kdb_printf("\n");
}

#ifdef XFS_BMAP_TRACE
/*
 * Convert an external extent descriptor to internal form.
 */
static void
xfs_convert_extent(xfs_bmbt_rec_32_t *rp, xfs_dfiloff_t *op, xfs_dfsbno_t *sp,
		   xfs_dfilblks_t *cp, int *fp)
{
	xfs_dfiloff_t o;
	xfs_dfsbno_t s;
	xfs_dfilblks_t c;
	int flag;

	flag = (((xfs_dfiloff_t)rp->l0) >> 31) & 1;
	o = ((((xfs_dfiloff_t)rp->l0) & 0x7fffffff) << 23) |
	    (((xfs_dfiloff_t)rp->l1) >> 9);
	s = (((xfs_dfsbno_t)(rp->l1 & 0x000001ff)) << 43) |
	    (((xfs_dfsbno_t)rp->l2) << 11) |
	    (((xfs_dfsbno_t)rp->l3) >> 21);
	c = (xfs_dfilblks_t)(rp->l3 & 0x001fffff);
	*op = o;
	*sp = s;
	*cp = c;
	*fp = flag;
}
#endif

#ifdef XFS_RW_TRACE
/*
 * Print itrunc entry trace.
 */
static void
xfs_ctrunc_trace_entry(ktrace_entry_t	*ktep)
{
	qprintf("ip 0x%p cpu %ld\n",
		(xfs_inode_t *)(unsigned long)ktep->val[1], (long)ktep->val[2]);
}
#endif

/*
 * Print an xfs_da_state_path structure.
 */
static void
xfs_dastate_path(xfs_da_state_path_t *p)
{
	int i;

	kdb_printf("active %d\n", p->active);
	for (i = 0; i < XFS_DA_NODE_MAXDEPTH; i++) {
		kdb_printf(" blk %d bp 0x%p blkno 0x%x",
			i, p->blk[i].bp, p->blk[i].blkno);
		kdb_printf(" index %d hashval 0x%x ",
			p->blk[i].index, (uint_t)p->blk[i].hashval);
		switch(p->blk[i].magic) {
		case XFS_DA_NODE_MAGIC:		kdb_printf("NODE\n");	break;
		case XFS_DIR_LEAF_MAGIC:	kdb_printf("DIR\n");	break;
		case XFS_ATTR_LEAF_MAGIC:	kdb_printf("ATTR\n");	break;
		case XFS_DIR2_LEAFN_MAGIC:	kdb_printf("DIR2\n");	break;
		default:			kdb_printf("type ?\n");	break;
		}
	}
}

#ifdef XFS_DIR_TRACE
/*
 * Print a xfs directory trace buffer entry.
 */
static int
xfs_dir_trace_entry(ktrace_entry_t *ktep)
{
	xfs_mount_t *mp;
	__uint32_t hash;
	xfs_off_t cookie;

	if (!ktep->val[0] || !ktep->val[1])
		return 0;

	mp = (xfs_mount_t *)ktep->val[3];
	cookie = (__psunsigned_t)ktep->val[4];
	cookie <<= 32;
	cookie |= (__psunsigned_t)ktep->val[5];
	qprintf("%s -- dp=0x%p b/e/h=%ld/%ld/0x%08lx resid=0x%lx ",
		    (char *)ktep->val[1],
		    (xfs_inode_t *)ktep->val[2],
		    (long)XFS_DA_COOKIE_BNO(mp, cookie),
		    (long)XFS_DA_COOKIE_ENTRY(mp, cookie),
		    (unsigned long)XFS_DA_COOKIE_HASH(mp, cookie),
		    (long)ktep->val[6]);

	switch ((__psint_t)ktep->val[0]) {
	case XFS_DIR_KTRACE_G_DU:
		break;
	case XFS_DIR_KTRACE_G_DUB:
		qprintf("bno=%ld", (long)ktep->val[7]);
		break;
	case XFS_DIR_KTRACE_G_DUN:
		qprintf("forw=%ld, cnt=%ld, 0x%08lx - 0x%08lx",
			      (long)ktep->val[7],
			      (long)ktep->val[8],
			      (unsigned long)ktep->val[9],
			      (unsigned long)ktep->val[10]);
		break;
	case XFS_DIR_KTRACE_G_DUL:
		qprintf("forw=%ld, cnt=%ld, 0x%08lx - 0x%08lx",
			      (long)ktep->val[7],
			      (long)ktep->val[8],
			      (unsigned long)ktep->val[9],
			      (unsigned long)ktep->val[10]);
		break;
	case XFS_DIR_KTRACE_G_DUE:
		qprintf("entry hashval 0x%08lx", (unsigned long)ktep->val[7]);
		break;
	case XFS_DIR_KTRACE_G_DUC:
		cookie = (__psunsigned_t)ktep->val[7];
		cookie <<= 32;
		cookie |= (__psunsigned_t)ktep->val[8];
		hash = XFS_DA_COOKIE_HASH(mp, cookie);
		qprintf("b/e/h=%ld/%ld/0x%08x",
		    (long)XFS_DA_COOKIE_BNO(mp, cookie),
		    (long)XFS_DA_COOKIE_ENTRY(mp, cookie),
		    hash);
		break;
	default:
		qprintf("unknown dir trace record format");
		break;
	}
	return 1;
}
#endif

#ifdef XFS_DIR2_TRACE
/*
 * Print a xfs v2 directory trace buffer entry.
 */
static int
xfs_dir2_trace_entry(ktrace_entry_t *ktep)
{
	char	    *cp;
	int	     i;
	int	     len;

	if (!ktep->val[0])
		return 0;
	cp = (char *)&ktep->val[10];
	qprintf("%s: '", (char *)ktep->val[1]);
	len = min((__psint_t)ktep->val[9], (__psint_t)sizeof(ktep->val[10])*6);
	for (i = 0; i < len; i++)
		qprintf("%c", cp[i]);
	qprintf("'(%ld)", (long)ktep->val[9]);
	if ((__psunsigned_t)ktep->val[0] != XFS_DIR2_KTRACE_ARGS_BIBII)
		qprintf(" hashval 0x%llx inumber %lld dp 0x%p tp 0x%p check %d",
			(__uint64_t)(unsigned long)ktep->val[2],
			(__int64_t)(unsigned long)ktep->val[3],
			ktep->val[4], ktep->val[5],
			(int)(__psint_t)ktep->val[6]);
	switch ((__psunsigned_t)ktep->val[0]) {
	case XFS_DIR2_KTRACE_ARGS:
		break;
	case XFS_DIR2_KTRACE_ARGS_B:
		qprintf(" bp 0x%p", ktep->val[7]);
		break;
	case XFS_DIR2_KTRACE_ARGS_BB:
		qprintf(" lbp 0x%p dbp 0x%p", ktep->val[7], ktep->val[8]);
		break;
	case XFS_DIR2_KTRACE_ARGS_BIBII:
		qprintf(" dp 0x%p tp 0x%p srcbp 0x%p srci %d dstbp 0x%p dsti %d count %d",
			ktep->val[2], ktep->val[3], ktep->val[4],
			(int)(__psint_t)ktep->val[5], ktep->val[6],
			(int)(__psint_t)ktep->val[7],
			(int)(__psint_t)ktep->val[8]);
		break;
	case XFS_DIR2_KTRACE_ARGS_DB:
		qprintf(" db 0x%x bp 0x%p",
			(xfs_dir2_db_t)(unsigned long)ktep->val[7],
			ktep->val[8]);
		break;
	case XFS_DIR2_KTRACE_ARGS_I:
		qprintf(" i 0x%lx", (unsigned long)ktep->val[7]);
		break;
	case XFS_DIR2_KTRACE_ARGS_S:
		qprintf(" s 0x%x", (int)(__psint_t)ktep->val[7]);
		break;
	case XFS_DIR2_KTRACE_ARGS_SB:
		qprintf(" s 0x%x bp 0x%p", (int)(__psint_t)ktep->val[7],
			ktep->val[8]);
		break;
	default:
		qprintf("unknown dirv2 trace record format");
		break;
	}
	return 1;
}
#endif

/*
 * Print an efd log item.
 */
static void
xfs_efd_item_print(xfs_efd_log_item_t *efdp, int summary)
{
	int		i;
	xfs_extent_t	*ep;

	if (summary) {
		kdb_printf("Extent Free Done: ID 0x%Lx nextents %d (at 0x%p)\n",
				efdp->efd_format.efd_efi_id,
				efdp->efd_format.efd_nextents, efdp);
		return;
	}
	kdb_printf("size %d nextents %d next extent %d efip 0x%p\n",
		efdp->efd_format.efd_size, efdp->efd_format.efd_nextents,
		efdp->efd_next_extent, efdp->efd_efip);
	kdb_printf("efi_id 0x%Lx\n", efdp->efd_format.efd_efi_id);
	kdb_printf("efd extents:\n");
	ep = &(efdp->efd_format.efd_extents[0]);
	for (i = 0; i < efdp->efd_next_extent; i++, ep++) {
		kdb_printf("    block %Lx len %d\n",
			ep->ext_start, ep->ext_len);
	}
}

/*
 * Print an efi log item.
 */
static void
xfs_efi_item_print(xfs_efi_log_item_t *efip, int summary)
{
	int		i;
	xfs_extent_t	*ep;
	static char *efi_flags[] = {
		"recovered",	/* 0x1 */
		"committed",	/* 0x2 */
		"cancelled",	/* 0x4 */
		NULL,
		};

	if (summary) {
		kdb_printf("Extent Free Intention: ID 0x%Lx nextents %d (at 0x%p)\n",
				efip->efi_format.efi_id,
				efip->efi_format.efi_nextents, efip);
		return;
	}
	kdb_printf("size %d nextents %d next extent %d\n",
		efip->efi_format.efi_size, efip->efi_format.efi_nextents,
		efip->efi_next_extent);
	kdb_printf("id %Lx", efip->efi_format.efi_id);
	printflags(efip->efi_flags, efi_flags, "flags :");
	kdb_printf("\n");
	kdb_printf("efi extents:\n");
	ep = &(efip->efi_format.efi_extents[0]);
	for (i = 0; i < efip->efi_next_extent; i++, ep++) {
		kdb_printf("    block %Lx len %d\n",
			ep->ext_start, ep->ext_len);
	}
}

/*
 * Format inode "format" into a static buffer & return it.
 */
static char *
xfs_fmtformat(xfs_dinode_fmt_t f)
{
	static char *t[] = {
		"dev",
		"local",
		"extents",
		"btree",
		"uuid"
	};

	return t[f];
}

/*
 * Format fsblock number into a static buffer & return it.
 */
char *
xfs_fmtfsblock(xfs_fsblock_t bno, xfs_mount_t *mp)
{
	static char rval[50];

	if (bno == NULLFSBLOCK)
		sprintf(rval, "NULLFSBLOCK");
	else if (ISNULLSTARTBLOCK(bno))
		sprintf(rval, "NULLSTARTBLOCK(%Ld)", STARTBLOCKVAL(bno));
	else if (mp)
		sprintf(rval, "%Ld[%x:%x]", (xfs_dfsbno_t)bno,
			XFS_FSB_TO_AGNO(mp, bno), XFS_FSB_TO_AGBNO(mp, bno));
	else
		sprintf(rval, "%Ld", (xfs_dfsbno_t)bno);
	return rval;
}

/*
 * Format inode number into a static buffer & return it.
 */
static char *
xfs_fmtino(xfs_ino_t ino, xfs_mount_t *mp)
{
	static char rval[50];

	if (mp)
		sprintf(rval, "%llu[%x:%x:%x]",
			(unsigned long long) ino,
			XFS_INO_TO_AGNO(mp, ino),
			XFS_INO_TO_AGBNO(mp, ino),
			XFS_INO_TO_OFFSET(mp, ino));
	else
		sprintf(rval, "%llu", (unsigned long long) ino);
	return rval;
}

/*
 * Format an lsn for printing into a static buffer & return it.
 */
static char *
xfs_fmtlsn(xfs_lsn_t *lsnp)
{
	uint		*wordp;
	uint		*word2p;
	static char	buf[20];

	wordp = (uint *)lsnp;
	word2p = wordp++;
	sprintf(buf, "[%u:%u]", *wordp, *word2p);

	return buf;
}

/*
 * Format file mode into a static buffer & return it.
 */
static char *
xfs_fmtmode(int m)
{
	static char rval[16];

	sprintf(rval, "%c%c%c%c%c%c%c%c%c%c%c%c%c",
		"?fc?dxb?r?l?S?m?"[(m & S_IFMT) >> 12],
		m & S_ISUID ? 'u' : '-',
		m & S_ISGID ? 'g' : '-',
		m & S_ISVTX ? 'v' : '-',
		m & S_IRUSR ? 'r' : '-',
		m & S_IWUSR ? 'w' : '-',
		m & S_IXUSR ? 'x' : '-',
		m & S_IRGRP ? 'r' : '-',
		m & S_IWGRP ? 'w' : '-',
		m & S_IXGRP ? 'x' : '-',
		m & S_IROTH ? 'r' : '-',
		m & S_IWOTH ? 'w' : '-',
		m & S_IXOTH ? 'x' : '-');
	return rval;
}

/*
 * Format a size into a static buffer & return it.
 */
static char *
xfs_fmtsize(size_t i)
{
	static char rval[20];

	/* size_t is 32 bits in 32-bit kernel, 64 bits in 64-bit kernel */
	sprintf(rval, "0x%lx", (unsigned long) i);
	return rval;
}

/*
 * Format a uuid into a static buffer & return it.
 */
static char *
xfs_fmtuuid(uuid_t *uu)
{
	static char rval[40];
	char	*o	  = rval;
	char	*i	  = (unsigned char*)uu;
	int	 b;

	for (b=0;b<16;b++) {
	    o+=sprintf(o, "%02x", *i++);
	    if (b==3||b==5||b==7||b==9) *o++='-';
	}
	*o='\0';

	return rval;
}

/*
 * Print an inode log item.
 */
static void
xfs_inode_item_print(xfs_inode_log_item_t *ilip, int summary)
{
	static char *ili_flags[] = {
		"hold",		/* 0x1 */
		"iolock excl",	/* 0x2 */
		"iolock shrd",	/* 0x4 */
		NULL
		};
	static char *ilf_fields[] = {
		"core",		/* 0x001 */
		"ddata",	/* 0x002 */
		"dexts",	/* 0x004 */
		"dbroot",	/* 0x008 */
		"dev",		/* 0x010 */
		"uuid",		/* 0x020 */
		"adata",	/* 0x040 */
		"aext",		/* 0x080 */
		"abroot",	/* 0x100 */
		NULL
		};

	if (summary) {
		kdb_printf("inode 0x%p logged %d ",
			ilip->ili_inode, ilip->ili_logged);
		printflags(ilip->ili_flags, ili_flags, "flags:");
		printflags(ilip->ili_format.ilf_fields, ilf_fields, "format:");
		printflags(ilip->ili_last_fields, ilf_fields, "lastfield:");
		kdb_printf("\n");
		return;
	}
	kdb_printf("inode 0x%p ino 0x%llu pushbuf %d logged %d flags: ",
		ilip->ili_inode, (unsigned long long) ilip->ili_format.ilf_ino,
		ilip->ili_pushbuf_flag, ilip->ili_logged);
	printflags(ilip->ili_flags, ili_flags, NULL);
	kdb_printf("\n");
	kdb_printf("ilock recur %d iolock recur %d ext buf 0x%p\n",
		ilip->ili_ilock_recur, ilip->ili_iolock_recur,
		ilip->ili_extents_buf);
#ifdef XFS_TRANS_DEBUG
	kdb_printf("root bytes %d root orig 0x%x\n",
		ilip->ili_root_size, ilip->ili_orig_root);
#endif
	kdb_printf("size %d ", ilip->ili_format.ilf_size);
	printflags(ilip->ili_format.ilf_fields, ilf_fields, "fields:");
	printflags(ilip->ili_last_fields, ilf_fields, " last fields: ");
	kdb_printf("\n");
	kdb_printf(" flush lsn %s last lsn %s\n",
		xfs_fmtlsn(&(ilip->ili_flush_lsn)),
		xfs_fmtlsn(&(ilip->ili_last_lsn)));
	kdb_printf("dsize %d, asize %d, rdev 0x%x\n",
		ilip->ili_format.ilf_dsize,
		ilip->ili_format.ilf_asize,
		ilip->ili_format.ilf_u.ilfu_rdev);
	kdb_printf("blkno 0x%Lx len 0x%x boffset 0x%x\n",
		ilip->ili_format.ilf_blkno,
		ilip->ili_format.ilf_len,
		ilip->ili_format.ilf_boffset);
}

/*
 * Print a dquot log item.
 */
/* ARGSUSED */
static void
xfs_dquot_item_print(xfs_dq_logitem_t *lip, int summary)
{
	kdb_printf("dquot 0x%p\n",
		lip->qli_dquot);

}

/*
 * Print a quotaoff log item.
 */
/* ARGSUSED */
static void
xfs_qoff_item_print(xfs_qoff_logitem_t *lip, int summary)
{
	kdb_printf("start qoff item 0x%p flags 0x%x\n",
		lip->qql_start_lip, lip->qql_format.qf_flags);

}

/*
 * Print buffer full of inodes.
 */
static void
xfs_inodebuf(xfs_buf_t *bp)
{
	xfs_dinode_t *di;
	xfs_dinode_core_t dic;
	int n, i;

	n = XFS_BUF_COUNT(bp) >> 8;
	for (i = 0; i < n; i++) {
		di = (xfs_dinode_t *)xfs_buf_offset(bp,
					i * 256);

		xfs_xlate_dinode_core((xfs_caddr_t)&di->di_core, &dic, 1);
		xfs_prdinode_core(&dic);
		kdb_printf("next_unlinked 0x%x u@0x%p\n",
			   INT_GET(di->di_next_unlinked, ARCH_CONVERT),
			   &di->di_u);
	}
}

#ifdef XFS_RW_TRACE
/*
 * Print iomap entry trace.
 */
static void
xfs_iomap_enter_trace_entry(ktrace_entry_t *ktep)
{
	qprintf("ip 0x%p size 0x%x%x offset 0x%x%x count 0x%x\n",
		ktep->val[1],
		(unsigned int)(long)ktep->val[2],
		(unsigned int)(long)ktep->val[3],
		(unsigned int)(long)ktep->val[4],
		(unsigned int)(long)ktep->val[5],
		(unsigned int)(long)ktep->val[6]);
	qprintf("io new size 0x%x%x pid=%d\n",
		(unsigned int)(long)ktep->val[7],
		(unsigned int)(long)ktep->val[8],
		(unsigned int)(long)ktep->val[9]);
}

/*
 * Print iomap map trace.
 */
static void
xfs_iomap_map_trace_entry(ktrace_entry_t *ktep)
{
	static char *bmapi_flags[] = {
		"read",		/* BMAPI_READ */
		"write",	/* BMAPI_WRITE */
		"allocate",	/* BMAPI_ALLOCATE */
		"unwritten",	/* BMAPI_UNWRITTEN */
		"ignstate",	/* BMAPI_IGNSTATE */
		"direct",	/* BMAPI_DIRECT */
		"mmap",		/* BMAPI_MMAP */
		"sync",		/* BMAPI_SYNC */
		"trylock",	/* BMAPI_TRYLOCK */
		"device",	/* BMAPI_DEVICE */
		NULL
	};

	qprintf("ip 0x%p size 0x%x%x offset 0x%x%x count 0x%x\n",
		ktep->val[1],
		(unsigned int)(long)ktep->val[2],
		(unsigned int)(long)ktep->val[3],
		(unsigned int)(long)ktep->val[4],
		(unsigned int)(long)ktep->val[5],
		(unsigned int)(long)ktep->val[6]);
	printflags((__psint_t)ktep->val[7], bmapi_flags, "bmapi flags");
	qprintf("iomap off 0x%x%x delta 0x%x bsize 0x%x bno 0x%x\n",
		(unsigned int)(long)ktep->val[8],
		(unsigned int)(long)ktep->val[9],
		(unsigned int)(long)ktep->val[10],
		(unsigned int)(long)ktep->val[11],
		(unsigned int)(long)ktep->val[12]);
	qprintf("imap off 0x%x count 0x%x block 0x%x\n",
		(unsigned int)(long)ktep->val[13],
		(unsigned int)(long)ktep->val[14],
		(unsigned int)(long)ktep->val[15]);
}

/*
 * Print itrunc entry trace.
 */
static void
xfs_itrunc_trace_entry(ktrace_entry_t   *ktep)
{
	qprintf("ip 0x%p size 0x%x%x flag %ld new size 0x%x%x\n",
		ktep->val[1],
		(unsigned int)(long)ktep->val[2],
		(unsigned int)(long)ktep->val[3],
		(long)ktep->val[4],
		(unsigned int)(long)ktep->val[5],
		(unsigned int)(long)ktep->val[6]);
	qprintf("toss start 0x%x%x toss finish 0x%x%x cpu id %ld pid %d\n",
		(unsigned int)(long)ktep->val[7],
		(unsigned int)(long)ktep->val[8],
		(unsigned int)(long)ktep->val[9],
		(unsigned int)(long)ktep->val[10],
		(long)ktep->val[11],
		(unsigned int)(long)ktep->val[12]);
}

/*
 * Print bunmap entry trace.
 */
static void
xfs_bunmap_trace_entry(ktrace_entry_t   *ktep)
{
	static char *bunmapi_flags[] = {
		"write",	/* 0x01 */
		"delay",	/* 0x02 */
		"entire",       /* 0x04 */
		"metadata",     /* 0x08 */
		"exact",	/* 0x10 */
		"attrfork",     /* 0x20 */
		"async",	/* 0x40 */
		"rsvblocks",    /* 0x80 */
		NULL
	};

	qprintf("ip 0x%p size 0x%x%x bno 0x%x%x len 0x%x cpu id %ld\n",
		ktep->val[1],
		(unsigned int)(long)ktep->val[2],
		(unsigned int)(long)ktep->val[3],
		(unsigned int)(long)ktep->val[4],
		(unsigned int)(long)ktep->val[5],
		(unsigned int)(long)ktep->val[6],
		(long)ktep->val[8]);
	qprintf("ra 0x%p pid %d ", ktep->val[9], (int)(long)ktep->val[10]);
	printflags((__psint_t)ktep->val[7], bunmapi_flags, "flags");
}

/*
 * Print inval_cached_pages entry trace.
 */
static void
xfs_inval_cached_trace_entry(ktrace_entry_t     *ktep)
{
	qprintf("ip 0x%p offset 0x%x%x len 0x%x%x first 0x%x%x last 0x%x%x pid %d\n",
		ktep->val[1],
		(unsigned int)(long)ktep->val[2],
		(unsigned int)(long)ktep->val[3],
		(unsigned int)(long)ktep->val[4],
		(unsigned int)(long)ktep->val[5],
		(unsigned int)(long)ktep->val[6],
		(unsigned int)(long)ktep->val[7],
		(unsigned int)(long)ktep->val[8],
		(unsigned int)(long)ktep->val[9],
		(unsigned int)(long)ktep->val[10]);
}
#endif


/*
 * Print disk inode core.
 */
static void
xfs_prdinode_core(xfs_dinode_core_t *dip)
{
	static char *diflags[] = {
		"realtime",		/* XFS_DIFLAG_REALTIME */
		"prealloc",		/* XFS_DIFLAG_PREALLOC */
		"newrtbm",		/* XFS_DIFLAG_NEWRTBM */
		"immutable",		/* XFS_DIFLAG_IMMUTABLE */
		"append",		/* XFS_DIFLAG_APPEND */
		"sync",			/* XFS_DIFLAG_SYNC */
		"noatime",		/* XFS_DIFLAG_NOATIME */
		"nodump",		/* XFS_DIFLAG_NODUMP */
		"rtinherit",		/* XFS_DIFLAG_RTINHERIT */
		"projinherit",		/* XFS_DIFLAG_PROJINHERIT */
		"nosymlinks",		/* XFS_DIFLAG_NOSYMLINKS */
		"extsize",		/* XFS_DIFLAG_EXTSIZE */
		"extszinherit",		/* XFS_DIFLAG_EXTSZINHERIT */
		NULL
	};

	kdb_printf("magic 0x%x mode 0%o (%s) version 0x%x format 0x%x (%s)\n",
		dip->di_magic, dip->di_mode,
		xfs_fmtmode(dip->di_mode),
		dip->di_version, dip->di_format,
		xfs_fmtformat((xfs_dinode_fmt_t)dip->di_format));
	kdb_printf("nlink %d uid %d gid %d projid %d flushiter %u\n",
		dip->di_nlink,
		dip->di_uid,
		dip->di_gid,
		(uint)dip->di_projid,
		(uint)dip->di_flushiter);
	kdb_printf("atime %u:%u mtime %ud:%u ctime %u:%u\n",
		dip->di_atime.t_sec, dip->di_atime.t_nsec,
		dip->di_mtime.t_sec, dip->di_mtime.t_nsec,
		dip->di_ctime.t_sec, dip->di_ctime.t_nsec);
	kdb_printf("size %Ld ", dip->di_size);
	kdb_printf("nblocks %Ld extsize 0x%x nextents 0x%x anextents 0x%x\n",
		dip->di_nblocks, dip->di_extsize, dip->di_nextents,
		dip->di_anextents);
	kdb_printf("forkoff %d aformat 0x%x (%s) dmevmask 0x%x dmstate 0x%x ",
		dip->di_forkoff, dip->di_aformat,
		xfs_fmtformat((xfs_dinode_fmt_t)dip->di_aformat),
		dip->di_dmevmask, dip->di_dmstate);
	printflags(dip->di_flags, diflags, "flags");
	kdb_printf("gen 0x%x\n", dip->di_gen);
}

#ifdef XFS_RW_TRACE
/*
 * Print read/write entry trace.
 */
static void
xfs_rw_enter_trace_entry(ktrace_entry_t *ktep)
{
	qprintf("ip 0x%p size 0x%x%x ptr 0x%p size %lu\n",
		ktep->val[1],
		(unsigned int)(long)ktep->val[2],
		(unsigned int)(long)ktep->val[3],
		ktep->val[4],
		(unsigned long)ktep->val[5]);
	qprintf("io offset 0x%x%x ioflags 0x%x new size 0x%x%x pid %d\n",
		(unsigned int)(long)ktep->val[6],
		(unsigned int)(long)ktep->val[7],
		(unsigned int)(long)ktep->val[8],
		(unsigned int)(long)ktep->val[9],
		(unsigned int)(long)ktep->val[10],
		(unsigned int)(long)ktep->val[11]);
}

/*
 * Print page write/release trace.
 */
static void
xfs_page_trace_entry(ktrace_entry_t *ktep)
{
	qprintf("ip 0x%p inode 0x%p page 0x%p\n",
		ktep->val[1], ktep->val[2], ktep->val[3]);
	qprintf("mask 0x%x di_size 0x%x%x isize 0x%x%x offset 0x%x%x\n",
		(unsigned int)(long)ktep->val[4],
		(unsigned int)(long)ktep->val[5],
		(unsigned int)(long)ktep->val[6],
		(unsigned int)(long)ktep->val[7],
		(unsigned int)(long)ktep->val[8],
		(unsigned int)(long)ktep->val[9],
		(unsigned int)(long)ktep->val[10]);
	qprintf("delalloc %d unmapped %d unwritten %d pid %d\n",
		(unsigned int)(long)ktep->val[11],
		(unsigned int)(long)ktep->val[12],
		(unsigned int)(long)ktep->val[13],
		(unsigned int)(long)ktep->val[14]);
}

/*
 * Print read/write trace entry.
 */
static int
xfs_rw_trace_entry(ktrace_entry_t *ktep)
{
	switch ( (long)ktep->val[0] ) {
	case XFS_READ_ENTER:
		qprintf("READ ENTER:\n");
		xfs_rw_enter_trace_entry(ktep);
		break;
	case XFS_WRITE_ENTER:
		qprintf("WRITE ENTER:\n");
		xfs_rw_enter_trace_entry(ktep);
		break;
	case XFS_SENDFILE_ENTER:
		qprintf("SENDFILE ENTER:\n");
		xfs_rw_enter_trace_entry(ktep);
		break;
	case XFS_IOMAP_READ_ENTER:
		qprintf("IOMAP READ ENTER:\n");
		xfs_iomap_enter_trace_entry(ktep);
		break;
	case XFS_IOMAP_WRITE_ENTER:
		qprintf("IOMAP WRITE ENTER:\n");
		xfs_iomap_enter_trace_entry(ktep);
		break;
	case XFS_IOMAP_WRITE_NOSPACE:
		qprintf("IOMAP WRITE NOSPACE:\n");
		xfs_iomap_enter_trace_entry(ktep);
		break;
	case XFS_IOMAP_READ_MAP:
		qprintf("IOMAP READ MAP:\n");
		xfs_iomap_map_trace_entry(ktep);
		break;
	case XFS_IOMAP_WRITE_MAP:
		qprintf("IOMAP WRITE MAP:\n");
		xfs_iomap_map_trace_entry(ktep);
		break;
	case XFS_ITRUNC_START:
		qprintf("ITRUNC START:\n");
		xfs_itrunc_trace_entry(ktep);
		break;
	case XFS_ITRUNC_FINISH1:
		qprintf("ITRUNC FINISH1:\n");
		xfs_itrunc_trace_entry(ktep);
		break;
	case XFS_ITRUNC_FINISH2:
		qprintf("ITRUNC FINISH2:\n");
		xfs_itrunc_trace_entry(ktep);
		break;
	case XFS_CTRUNC1:
		qprintf("CTRUNC1:\n");
		xfs_ctrunc_trace_entry(ktep);
		break;
	case XFS_CTRUNC2:
		qprintf("CTRUNC2:\n");
		xfs_ctrunc_trace_entry(ktep);
		break;
	case XFS_CTRUNC3:
		qprintf("CTRUNC3:\n");
		xfs_ctrunc_trace_entry(ktep);
		break;
	case XFS_CTRUNC4:
		qprintf("CTRUNC4:\n");
		xfs_ctrunc_trace_entry(ktep);
		break;
	case XFS_CTRUNC5:
		qprintf("CTRUNC5:\n");
		xfs_ctrunc_trace_entry(ktep);
		break;
	case XFS_CTRUNC6:
		qprintf("CTRUNC6:\n");
		xfs_ctrunc_trace_entry(ktep);
		break;
	case XFS_BUNMAP:
		qprintf("BUNMAP:\n");
		xfs_bunmap_trace_entry(ktep);
		break;
	case XFS_INVAL_CACHED:
		qprintf("INVAL CACHED:\n");
		xfs_inval_cached_trace_entry(ktep);
		break;
	case XFS_DIORD_ENTER:
		qprintf("DIORD ENTER:\n");
		xfs_rw_enter_trace_entry(ktep);
		break;
	case XFS_DIOWR_ENTER:
		qprintf("DIOWR ENTER:\n");
		xfs_rw_enter_trace_entry(ktep);
		break;
	case XFS_WRITEPAGE_ENTER:
		qprintf("PAGE WRITE:\n");
		xfs_page_trace_entry(ktep);
		break;
	case XFS_RELEASEPAGE_ENTER:
		qprintf("PAGE RELEASE:\n");
		xfs_page_trace_entry(ktep);
		break;
	case XFS_INVALIDPAGE_ENTER:
		qprintf("PAGE INVALIDATE:\n");
		xfs_page_trace_entry(ktep);
		break;
	case XFS_IOMAP_ALLOC_ENTER:
		qprintf("ALLOC ENTER:\n");
		xfs_iomap_enter_trace_entry(ktep);
		break;
	case XFS_IOMAP_ALLOC_MAP:
		qprintf("ALLOC MAP:\n");
		xfs_iomap_map_trace_entry(ktep);
		break;
	case XFS_IOMAP_UNWRITTEN:
		qprintf("UNWRITTEN:\n");
		xfs_iomap_enter_trace_entry(ktep);
		break;

	default:
		qprintf("UNKNOWN RW TRACE\n");
		return 0;
	}

	return 1;
}
#endif

/*
 * Print xfs extent records for a fork.
 */
static void
xfs_xexlist_fork(xfs_inode_t *ip, int whichfork)
{
	int nextents, i;
	xfs_ifork_t *ifp;
	xfs_bmbt_irec_t irec;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (ifp->if_flags & XFS_IFEXTENTS) {
		nextents = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
		kdb_printf("inode 0x%p %cf extents 0x%p nextents 0x%x\n",
			ip, "da"[whichfork], xfs_iext_get_ext(ifp, 0),
			nextents);
		for (i = 0; i < nextents; i++) {
			xfs_bmbt_get_all(xfs_iext_get_ext(ifp, i), &irec);
			kdb_printf(
		"%d: startoff %Ld startblock %s blockcount %Ld flag %d\n",
			i, irec.br_startoff,
			xfs_fmtfsblock(irec.br_startblock, ip->i_mount),
			irec.br_blockcount, irec.br_state);
		}
	}
}

static void
xfs_xnode_fork(char *name, xfs_ifork_t *f)
{
	static char *tab_flags[] = {
		"inline",	/* XFS_IFINLINE */
		"extents",	/* XFS_IFEXTENTS */
		"broot",	/* XFS_IFBROOT */
		NULL
	};
	int *p;

	kdb_printf("%s fork", name);
	if (f == NULL) {
		kdb_printf(" empty\n");
		return;
	} else
		kdb_printf("\n");
	kdb_printf(" bytes %s ", xfs_fmtsize(f->if_bytes));
	kdb_printf("real_bytes %s lastex 0x%x u1:%s 0x%p\n",
		xfs_fmtsize(f->if_real_bytes), f->if_lastex,
		f->if_flags & XFS_IFINLINE ? "data" : "extents",
		f->if_flags & XFS_IFINLINE ?
			f->if_u1.if_data :
			(char *)f->if_u1.if_extents);
	kdb_printf(" broot 0x%p broot_bytes %s ext_max %d ",
		f->if_broot, xfs_fmtsize(f->if_broot_bytes), f->if_ext_max);
	printflags(f->if_flags, tab_flags, "flags");
	kdb_printf("\n");
	kdb_printf(" u2");
	for (p = (int *)&f->if_u2;
	     p < (int *)((char *)&f->if_u2 + XFS_INLINE_DATA);
	     p++)
		kdb_printf(" 0x%x", *p);
	kdb_printf("\n");
}

/*
 * Command-level xfs-idbg functions.
 */

/*
 * Print xfs allocation group freespace header.
 */
static void
xfsidbg_xagf(xfs_agf_t *agf)
{
	kdb_printf("magicnum 0x%x versionnum 0x%x seqno 0x%x length 0x%x\n",
		be32_to_cpu(agf->agf_magicnum),
		be32_to_cpu(agf->agf_versionnum),
		be32_to_cpu(agf->agf_seqno),
		be32_to_cpu(agf->agf_length));
	kdb_printf("roots b 0x%x c 0x%x levels b %d c %d\n",
		be32_to_cpu(agf->agf_roots[XFS_BTNUM_BNO]),
		be32_to_cpu(agf->agf_roots[XFS_BTNUM_CNT]),
		be32_to_cpu(agf->agf_levels[XFS_BTNUM_BNO]),
		be32_to_cpu(agf->agf_levels[XFS_BTNUM_CNT]));
	kdb_printf("flfirst %d fllast %d flcount %d freeblks %d longest %d\n",
		be32_to_cpu(agf->agf_flfirst),
		be32_to_cpu(agf->agf_fllast),
		be32_to_cpu(agf->agf_flcount),
		be32_to_cpu(agf->agf_freeblks),
		be32_to_cpu(agf->agf_longest));
}

/*
 * Print xfs allocation group inode header.
 */
static void
xfsidbg_xagi(xfs_agi_t *agi)
{
	int	i;
	int	j;

	kdb_printf("magicnum 0x%x versionnum 0x%x seqno 0x%x length 0x%x\n",
		be32_to_cpu(agi->agi_magicnum),
		be32_to_cpu(agi->agi_versionnum),
		be32_to_cpu(agi->agi_seqno),
		be32_to_cpu(agi->agi_length));
	kdb_printf("count 0x%x root 0x%x level 0x%x\n",
		be32_to_cpu(agi->agi_count),
		be32_to_cpu(agi->agi_root),
		be32_to_cpu(agi->agi_level));
	kdb_printf("freecount 0x%x newino 0x%x dirino 0x%x\n",
		be32_to_cpu(agi->agi_freecount),
		be32_to_cpu(agi->agi_newino),
		be32_to_cpu(agi->agi_dirino));

	kdb_printf("unlinked buckets\n");
	for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++) {
		for (j = 0; j < 4; j++, i++) {
			kdb_printf("0x%08x ",
				be32_to_cpu(agi->agi_unlinked[i]));
		}
		kdb_printf("\n");
	}
}

#ifdef XFS_ALLOC_TRACE
/*
 * Print out the last "count" entries in the allocation trace buffer.
 */
static void
xfsidbg_xalatrace(int count)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	int	     nentries;
	int	     skip_entries;

	if (xfs_alloc_trace_buf == NULL) {
		qprintf("The xfs alloc trace buffer is not initialized\n");
		return;
	}
	nentries = ktrace_nentries(xfs_alloc_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(xfs_alloc_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(xfs_alloc_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		if (xfs_alloc_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(xfs_alloc_trace_buf, &kts);
	}
}

/*
 * Print out all the entries in the alloc trace buf corresponding
 * to the given block number.
 */
static void
xfsidbg_xalbtrace(xfs_agblock_t bno)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;

	if (xfs_alloc_trace_buf == NULL) {
		qprintf("The xfs alloc trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(xfs_alloc_trace_buf, &kts);
	while (ktep != NULL) {
		switch ((__psint_t)ktep->val[0]) {
		case XFS_ALLOC_KTRACE_ALLOC:
		case XFS_ALLOC_KTRACE_FREE:
			if (bno >= (xfs_agblock_t)((__psint_t)ktep->val[5]) &&
			    bno < (xfs_agblock_t)((__psint_t)ktep->val[5]) +
				  (xfs_extlen_t)((__psint_t)ktep->val[13])) {
				(void)xfs_alloc_trace_entry(ktep);
				qprintf("\n");
			}
			break;
		}
		ktep = ktrace_next(xfs_alloc_trace_buf, &kts);
	}
}

/*
 * Print out all the entries in the alloc trace buf corresponding
 * to the given allocation group.
 */
static void
xfsidbg_xalgtrace(xfs_agnumber_t agno)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;

	if (xfs_alloc_trace_buf == NULL) {
		qprintf("The xfs alloc trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(xfs_alloc_trace_buf, &kts);
	while (ktep != NULL) {
		if (  (__psint_t)ktep->val[0] &&
		      ((xfs_agnumber_t)((__psint_t)ktep->val[4])) == agno ) {
			(void)xfs_alloc_trace_entry(ktep);
			qprintf("\n");
		}
		ktep = ktrace_next(xfs_alloc_trace_buf, &kts);
	}
}
#endif

/*
 * Print an allocation argument structure for XFS.
 */
static void
xfsidbg_xalloc(xfs_alloc_arg_t *args)
{
	kdb_printf("tp 0x%p mp 0x%p agbp 0x%p pag 0x%p fsbno %s\n",
		args->tp, args->mp, args->agbp, args->pag,
		xfs_fmtfsblock(args->fsbno, args->mp));
	kdb_printf("agno 0x%x agbno 0x%x minlen 0x%x maxlen 0x%x mod 0x%x\n",
		args->agno, args->agbno, args->minlen, args->maxlen, args->mod);
	kdb_printf("prod 0x%x minleft 0x%x total 0x%x alignment 0x%x\n",
		args->prod, args->minleft, args->total, args->alignment);
	kdb_printf("minalignslop 0x%x len 0x%x type %s otype %s wasdel %d\n",
		args->minalignslop, args->len, xfs_alloctype[args->type],
		xfs_alloctype[args->otype], args->wasdel);
	kdb_printf("wasfromfl %d isfl %d userdata %d\n",
		args->wasfromfl, args->isfl, args->userdata);
}

#ifdef XFS_ALLOC_TRACE
/*
 * Print out all the entries in the alloc trace buf corresponding
 * to the given mount point.
 */
static void
xfsidbg_xalmtrace(xfs_mount_t *mp)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;

	if (xfs_alloc_trace_buf == NULL) {
		kdb_printf("The xfs alloc trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(xfs_alloc_trace_buf, &kts);
	while (ktep != NULL) {
		if ((__psint_t)ktep->val[0] &&
		    (xfs_mount_t *)ktep->val[3] == mp) {
			(void)xfs_alloc_trace_entry(ktep);
			kdb_printf("\n");
		}
		ktep = ktrace_next(xfs_alloc_trace_buf, &kts);
	}
}

/*
 * Print out all the entries in the alloc trace buf corresponding
 * to the given entry type.
 */
static void
xfsidbg_xalttrace(int tag)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;

	if (xfs_alloc_trace_buf == NULL) {
		qprintf("The xfs alloc trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(xfs_alloc_trace_buf, &kts);
	while (ktep != NULL) {
		if ((__psint_t)ktep->val[0] &&
		    ((long)ktep->val[0] & 0xffffL) == (long)tag) {
			(void)xfs_alloc_trace_entry(ktep);
			qprintf("\n");
		}
		ktep = ktrace_next(xfs_alloc_trace_buf, &kts);
	}
}
#endif

static int xargument = 0;

/*
 * Set xtra argument, used by xchksum.
 */
static void
xfsidbg_xarg(int xarg)
{
	if (xarg == -1)
		qprintf("xargument: %d\n", xargument);
	else
		xargument = xarg;
}       /* xfsidbg_xarg */

/*
 * Print an attr_list() context structure.
 */
static void
xfsidbg_xattrcontext(xfs_attr_list_context_t *context)
{
	static char *attr_arg_flags[] = {
		"DONTFOLLOW",	/* 0x0001 */
		"?",		/* 0x0002 */
		"?",		/* 0x0004 */
		"?",		/* 0x0008 */
		"CREATE",	/* 0x0010 */
		"?",		/* 0x0020 */
		"?",		/* 0x0040 */
		"?",		/* 0x0080 */
		"?",		/* 0x0100 */
		"?",		/* 0x0200 */
		"?",		/* 0x0400 */
		"?",		/* 0x0800 */
		"KERNOTIME",	/* 0x1000 */
		NULL
	};

	kdb_printf("dp 0x%p, dupcnt %d, resynch %d",
		    context->dp, context->dupcnt, context->resynch);
	printflags((__psunsigned_t)context->flags, attr_arg_flags, ", flags");
	kdb_printf("\ncursor h/b/o 0x%x/0x%x/%d -- p/p/i 0x%x/0x%x/0x%x\n",
			  context->cursor->hashval, context->cursor->blkno,
			  context->cursor->offset, context->cursor->pad1,
			  context->cursor->pad2, context->cursor->initted);
	kdb_printf("alist 0x%p, bufsize 0x%x, count %d, firstu 0x%x\n",
		       context->alist, context->bufsize, context->count,
		       context->firstu);
}

/*
 * Print attribute leaf block.
 */
static void
xfsidbg_xattrleaf(xfs_attr_leafblock_t *leaf)
{
	xfs_attr_leaf_hdr_t *h;
	xfs_da_blkinfo_t *i;
	xfs_attr_leaf_map_t *m;
	xfs_attr_leaf_entry_t *e;
	xfs_attr_leaf_name_local_t *l;
	xfs_attr_leaf_name_remote_t *r;
	int j, k;

	h = &leaf->hdr;
	i = &h->info;
	kdb_printf("hdr info forw 0x%x back 0x%x magic 0x%x\n",
		i->forw, i->back, i->magic);
	kdb_printf("hdr count %d usedbytes %d firstused %d holes %d\n",
		INT_GET(h->count, ARCH_CONVERT),
		INT_GET(h->usedbytes, ARCH_CONVERT),
		INT_GET(h->firstused, ARCH_CONVERT), h->holes);
	for (j = 0, m = h->freemap; j < XFS_ATTR_LEAF_MAPSIZE; j++, m++) {
		kdb_printf("hdr freemap %d base %d size %d\n",
			j, INT_GET(m->base, ARCH_CONVERT),
			INT_GET(m->size, ARCH_CONVERT));
	}
	for (j = 0, e = leaf->entries; j < INT_GET(h->count, ARCH_CONVERT); j++, e++) {
		kdb_printf("[%2d] hash 0x%x nameidx %d flags 0x%x",
			j, INT_GET(e->hashval, ARCH_CONVERT),
			INT_GET(e->nameidx, ARCH_CONVERT), e->flags);
		if (e->flags & XFS_ATTR_LOCAL)
			kdb_printf("LOCAL ");
		if (e->flags & XFS_ATTR_ROOT)
			kdb_printf("ROOT ");
		if (e->flags & XFS_ATTR_SECURE)
			kdb_printf("SECURE ");
		if (e->flags & XFS_ATTR_INCOMPLETE)
			kdb_printf("INCOMPLETE ");
		k = ~(XFS_ATTR_LOCAL | XFS_ATTR_ROOT |
			XFS_ATTR_SECURE | XFS_ATTR_INCOMPLETE);
		if ((e->flags & k) != 0)
			kdb_printf("0x%x", e->flags & k);
		kdb_printf(">\n     name \"");
		if (e->flags & XFS_ATTR_LOCAL) {
			l = XFS_ATTR_LEAF_NAME_LOCAL(leaf, j);
			for (k = 0; k < l->namelen; k++)
				kdb_printf("%c", l->nameval[k]);
			kdb_printf("\"(%d) value \"", l->namelen);
			for (k = 0; (k < INT_GET(l->valuelen, ARCH_CONVERT)) && (k < 32); k++)
				kdb_printf("%c", l->nameval[l->namelen + k]);
			if (k == 32)
				kdb_printf("...");
			kdb_printf("\"(%d)\n",
				INT_GET(l->valuelen, ARCH_CONVERT));
		} else {
			r = XFS_ATTR_LEAF_NAME_REMOTE(leaf, j);
			for (k = 0; k < r->namelen; k++)
				kdb_printf("%c", r->name[k]);
			kdb_printf("\"(%d) value blk 0x%x len %d\n",
				    r->namelen,
				    INT_GET(r->valueblk, ARCH_CONVERT),
				    INT_GET(r->valuelen, ARCH_CONVERT));
		}
	}
}

/*
 * Print a shortform attribute list.
 */
static void
xfsidbg_xattrsf(xfs_attr_shortform_t *s)
{
	xfs_attr_sf_hdr_t *sfh;
	xfs_attr_sf_entry_t *sfe;
	int i, j;

	sfh = &s->hdr;
	kdb_printf("hdr count %d\n", INT_GET(sfh->count, ARCH_CONVERT));
	for (i = 0, sfe = s->list; i < INT_GET(sfh->count, ARCH_CONVERT); i++) {
		kdb_printf("entry %d namelen %d name \"", i, sfe->namelen);
		for (j = 0; j < sfe->namelen; j++)
			kdb_printf("%c", sfe->nameval[j]);
		kdb_printf("\" valuelen %d value \"", INT_GET(sfe->valuelen, ARCH_CONVERT));
		for (j = 0; (j < INT_GET(sfe->valuelen, ARCH_CONVERT)) && (j < 32); j++)
			kdb_printf("%c", sfe->nameval[sfe->namelen + j]);
		if (j == 32)
			kdb_printf("...");
		kdb_printf("\"\n");
		sfe = XFS_ATTR_SF_NEXTENTRY(sfe);
	}
}

#ifdef XFS_ATTR_TRACE
/*
 * Print out the last "count" entries in the attribute trace buffer.
 */
static void
xfsidbg_xattrtrace(int count)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	int	     nentries;
	int	     skip_entries;

	if (xfs_attr_trace_buf == NULL) {
		qprintf("The xfs attribute trace buffer is not initialized\n");
		return;
	}
	nentries = ktrace_nentries(xfs_attr_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(xfs_attr_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(xfs_attr_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		xfs_attr_trace_entry(ktep);
		ktep = ktrace_next(xfs_attr_trace_buf, &kts);
	}
}
#endif

/*
 * Print xfs bmap internal record
 */
static void
xfsidbg_xbirec(xfs_bmbt_irec_t *r)
{
	kdb_printf(
	"startoff %Ld startblock %Lx blockcount %Ld state %Ld\n",
		(__uint64_t)r->br_startoff,
		(__uint64_t)r->br_startblock,
		(__uint64_t)r->br_blockcount,
		(__uint64_t)r->br_state);
}

#ifdef XFS_BLI_TRACE
/*
 * Print out the buf log item trace for the given buf log item.
 */
static void
xfsidbg_xblitrace(xfs_buf_log_item_t *bip)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	uint64_t	flags;
	static char *xbli_flags[] = {
		"hold",		/* 0x01 */
		"dirty",	/* 0x02 */
		"stale",	/* 0x04 */
		"logged",	/* 0x08 */
		NULL
		};
	static char *xli_flags[] = {
		"in ail",       /* 0x1 */
		NULL
		};

	if (bip->bli_trace == NULL) {
		qprintf("The bli trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(bip->bli_trace, &kts);
	while (ktep != NULL) {
		qprintf("%s bp 0x%p flags ",
			(char *)ktep->val[0], ktep->val[1]);
		printflags((__psint_t)(ktep->val[2]), xbli_flags, "xbli");
		qprintf("\n");
		qprintf("recur %ld refcount %ld blkno 0x%lx bcount 0x%lx\n",
			(long)ktep->val[3], (long)ktep->val[4],
			(unsigned long)ktep->val[5],
			(unsigned long)ktep->val[6]);
		flags = (((uint64_t)(unsigned long)ktep->val[7] << 32) &
					0xFFFFFFFF00000000ULL) |
			(((uint64_t)(unsigned long)ktep->val[8]) &
					0x00000000FFFFFFFFULL);
		qprintf("bp flags ");
		printflags(flags, bp_flag_vals, NULL);
		qprintf("\n");
		qprintf("fspriv 0x%p fspriv2 0x%p pincount %ld iodone 0x%p\n",
			ktep->val[9], ktep->val[10],
			(long)ktep->val[11], ktep->val[12]);
		qprintf("lockval %ld lid 0x%lx log item flags ",
			(long)ktep->val[13], (unsigned long)ktep->val[14]);
		printflags((__psint_t)(ktep->val[15]), xli_flags, "xli");
		qprintf("\n");

		ktep = ktrace_next(bip->bli_trace, &kts);
	}
}
#endif

/*
 * Print a bmap alloc argument structure for XFS.
 */
static void
xfsidbg_xbmalla(xfs_bmalloca_t *a)
{
	kdb_printf("tp 0x%p ip 0x%p eof %d prevp 0x%p\n",
		a->tp, a->ip, a->eof, a->prevp);
	kdb_printf("gotp 0x%p firstblock %s alen %d total %d\n",
		a->gotp, xfs_fmtfsblock(a->firstblock, a->ip->i_mount),
		a->alen, a->total);
	kdb_printf("off %s wasdel %d userdata %d minlen %d\n",
		xfs_fmtfsblock(a->off, a->ip->i_mount), a->wasdel,
		a->userdata, a->minlen);
	kdb_printf("minleft %d low %d rval %s aeof %d conv %d\n",
		a->minleft, a->low, xfs_fmtfsblock(a->rval, a->ip->i_mount),
		a->aeof, a->conv);
}

#ifdef XFS_BMAP_TRACE
/*
 * Print out the last "count" entries in the bmap btree trace buffer.
 * The "a" is for "all" inodes.
 */
static void
xfsidbg_xbmatrace(int count)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	int	     nentries;
	int	     skip_entries;

	if (xfs_bmbt_trace_buf == NULL) {
		qprintf("The xfs bmap btree trace buffer is not initialized\n");		return;
	}
	nentries = ktrace_nentries(xfs_bmbt_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(xfs_bmbt_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(xfs_bmbt_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		if (xfs_bmbt_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(xfs_bmbt_trace_buf, &kts);
	}
}

/*
 * Print out the bmap btree trace buffer attached to the given inode.
 */
static void
xfsidbg_xbmitrace(xfs_inode_t *ip)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;

	if (ip->i_btrace == NULL) {
		qprintf("The inode trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(ip->i_btrace, &kts);
	while (ktep != NULL) {
		if (xfs_bmbt_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(ip->i_btrace, &kts);
	}
}

/*
 * Print out all the entries in the bmap btree trace buf corresponding
 * to the given inode.  The "s" is for "single" inode.
 */
static void
xfsidbg_xbmstrace(xfs_inode_t *ip)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;

	if (xfs_bmbt_trace_buf == NULL) {
		qprintf("The xfs bmap btree trace buffer is not initialized\n");		return;
	}

	ktep = ktrace_first(xfs_bmbt_trace_buf, &kts);
	while (ktep != NULL) {
		if ((xfs_inode_t *)(ktep->val[2]) == ip) {
			if (xfs_bmbt_trace_entry(ktep))
				qprintf("\n");
		}
		ktep = ktrace_next(xfs_bmbt_trace_buf, &kts);
	}
}
#endif

/*
 * Print xfs bmap record
 */
static void
xfsidbg_xbrec(xfs_bmbt_rec_64_t *r)
{
	xfs_bmbt_irec_t	irec;

	xfs_bmbt_get_all((xfs_bmbt_rec_t *)r, &irec);
	kdb_printf("startoff %Ld startblock %Lx blockcount %Ld flag %d\n",
		irec.br_startoff, (__uint64_t)irec.br_startblock,
		irec.br_blockcount, irec.br_state);
}

/*
 * Print an xfs in-inode bmap btree root (data fork).
 */
static void
xfsidbg_xbroot(xfs_inode_t *ip)
{
	xfs_broot(ip, &ip->i_df);
}

/*
 * Print an xfs in-inode bmap btree root (attribute fork).
 */
static void
xfsidbg_xbroota(xfs_inode_t *ip)
{
	if (ip->i_afp)
		xfs_broot(ip, ip->i_afp);
}

/*
 * Print xfs btree cursor.
 */
static void
xfsidbg_xbtcur(xfs_btree_cur_t *c)
{
	int l;

	kdb_printf("tp 0x%p mp 0x%p\n",
		c->bc_tp,
		c->bc_mp);
	if (c->bc_btnum == XFS_BTNUM_BMAP) {
		kdb_printf("rec.b ");
		xfsidbg_xbirec(&c->bc_rec.b);
	} else if (c->bc_btnum == XFS_BTNUM_INO) {
		kdb_printf("rec.i startino 0x%x freecount 0x%x free %Lx\n",
			c->bc_rec.i.ir_startino, c->bc_rec.i.ir_freecount,
			c->bc_rec.i.ir_free);
	} else {
		kdb_printf("rec.a startblock 0x%x blockcount 0x%x\n",
			c->bc_rec.a.ar_startblock,
			c->bc_rec.a.ar_blockcount);
	}
	kdb_printf("bufs");
	for (l = 0; l < c->bc_nlevels; l++)
		kdb_printf(" 0x%p", c->bc_bufs[l]);
	kdb_printf("\n");
	kdb_printf("ptrs");
	for (l = 0; l < c->bc_nlevels; l++)
		kdb_printf(" 0x%x", c->bc_ptrs[l]);
	kdb_printf("  ra");
	for (l = 0; l < c->bc_nlevels; l++)
		kdb_printf(" %d", c->bc_ra[l]);
	kdb_printf("\n");
	kdb_printf("nlevels %d btnum %s blocklog %d\n",
		c->bc_nlevels,
		c->bc_btnum == XFS_BTNUM_BNO ? "bno" :
		(c->bc_btnum == XFS_BTNUM_CNT ? "cnt" :
		 (c->bc_btnum == XFS_BTNUM_BMAP ? "bmap" : "ino")),
		c->bc_blocklog);
	if (c->bc_btnum == XFS_BTNUM_BMAP) {
		kdb_printf("private forksize 0x%x whichfork %d ip 0x%p flags %d\n",
			c->bc_private.b.forksize,
			c->bc_private.b.whichfork,
			c->bc_private.b.ip,
			c->bc_private.b.flags);
		kdb_printf("private firstblock %s flist 0x%p allocated 0x%x\n",
			xfs_fmtfsblock(c->bc_private.b.firstblock, c->bc_mp),
			c->bc_private.b.flist,
			c->bc_private.b.allocated);
	} else if (c->bc_btnum == XFS_BTNUM_INO) {
		kdb_printf("private agbp 0x%p agno 0x%x\n",
			c->bc_private.i.agbp,
			c->bc_private.i.agno);
	} else {
		kdb_printf("private agbp 0x%p agno 0x%x\n",
			c->bc_private.a.agbp,
			c->bc_private.a.agno);
	}
}

/*
 * Figure out what kind of xfs block the buffer contains,
 * and invoke a print routine.
 */
static void
xfsidbg_xbuf(xfs_buf_t *bp)
{
	xfsidbg_xbuf_real(bp, 0);
}

/*
 * Figure out what kind of xfs block the buffer contains,
 * and invoke a print routine (if asked to).
 */
static void
xfsidbg_xbuf_real(xfs_buf_t *bp, int summary)
{
	void *d;
	xfs_agf_t *agf;
	xfs_agi_t *agi;
	xfs_sb_t *sb;
	xfs_alloc_block_t *bta;
	xfs_bmbt_block_t *btb;
	xfs_inobt_block_t *bti;
	xfs_attr_leafblock_t *aleaf;
	xfs_dir_leafblock_t *dleaf;
	xfs_da_intnode_t *node;
	xfs_dinode_t *di;
	xfs_disk_dquot_t *dqb;
	xfs_dir2_block_t *d2block;
	xfs_dir2_data_t *d2data;
	xfs_dir2_leaf_t *d2leaf;
	xfs_dir2_free_t *d2free;

	d = XFS_BUF_PTR(bp);
	if (be32_to_cpu((agf = d)->agf_magicnum) == XFS_AGF_MAGIC) {
		if (summary) {
			kdb_printf("freespace hdr for AG %d (at 0x%p)\n",
				be32_to_cpu(agf->agf_seqno), agf);
		} else {
			kdb_printf("buf 0x%p agf 0x%p\n", bp, agf);
			xfsidbg_xagf(agf);
		}
	} else if (be32_to_cpu((agi = d)->agi_magicnum) == XFS_AGI_MAGIC) {
		if (summary) {
			kdb_printf("Inode hdr for AG %d (at 0x%p)\n",
			       be32_to_cpu(agi->agi_seqno), agi);
		} else {
			kdb_printf("buf 0x%p agi 0x%p\n", bp, agi);
			xfsidbg_xagi(agi);
		}
	} else if (be32_to_cpu((bta = d)->bb_magic) == XFS_ABTB_MAGIC) {
		if (summary) {
			kdb_printf("Alloc BNO Btree blk, level %d (at 0x%p)\n",
				       be16_to_cpu(bta->bb_level), bta);
		} else {
			kdb_printf("buf 0x%p abtbno 0x%p\n", bp, bta);
			xfs_btalloc(bta, XFS_BUF_COUNT(bp));
		}
	} else if (be32_to_cpu((bta = d)->bb_magic) == XFS_ABTC_MAGIC) {
		if (summary) {
			kdb_printf("Alloc COUNT Btree blk, level %d (at 0x%p)\n",
				       be16_to_cpu(bta->bb_level), bta);
		} else {
			kdb_printf("buf 0x%p abtcnt 0x%p\n", bp, bta);
			xfs_btalloc(bta, XFS_BUF_COUNT(bp));
		}
	} else if (be32_to_cpu((btb = d)->bb_magic) == XFS_BMAP_MAGIC) {
		if (summary) {
			kdb_printf("Bmap Btree blk, level %d (at 0x%p)\n",
				      be16_to_cpu(btb->bb_level), btb);
		} else {
			kdb_printf("buf 0x%p bmapbt 0x%p\n", bp, btb);
			xfs_btbmap(btb, XFS_BUF_COUNT(bp));
		}
	} else if (be32_to_cpu((bti = d)->bb_magic) == XFS_IBT_MAGIC) {
		if (summary) {
			kdb_printf("Inode Btree blk, level %d (at 0x%p)\n",
				       be16_to_cpu(bti->bb_level), bti);
		} else {
			kdb_printf("buf 0x%p inobt 0x%p\n", bp, bti);
			xfs_btino(bti, XFS_BUF_COUNT(bp));
		}
	} else if (INT_GET((aleaf = d)->hdr.info.magic, ARCH_CONVERT) == XFS_ATTR_LEAF_MAGIC) {
		if (summary) {
			kdb_printf("Attr Leaf, 1st hash 0x%x (at 0x%p)\n",
				      INT_GET(aleaf->entries[0].hashval, ARCH_CONVERT), aleaf);
		} else {
			kdb_printf("buf 0x%p attr leaf 0x%p\n", bp, aleaf);
			xfsidbg_xattrleaf(aleaf);
		}
	} else if (INT_GET((dleaf = d)->hdr.info.magic, ARCH_CONVERT) == XFS_DIR_LEAF_MAGIC) {
		if (summary) {
			kdb_printf("Dir Leaf, 1st hash 0x%x (at 0x%p)\n",
				     dleaf->entries[0].hashval, dleaf);
		} else {
			kdb_printf("buf 0x%p dir leaf 0x%p\n", bp, dleaf);
			xfsidbg_xdirleaf(dleaf);
		}
	} else if (INT_GET((node = d)->hdr.info.magic, ARCH_CONVERT) == XFS_DA_NODE_MAGIC) {
		if (summary) {
			kdb_printf("Dir/Attr Node, level %d, 1st hash 0x%x (at 0x%p)\n",
			      node->hdr.level, node->btree[0].hashval, node);
		} else {
			kdb_printf("buf 0x%p dir/attr node 0x%p\n", bp, node);
			xfsidbg_xdanode(node);
		}
	} else if (INT_GET((di = d)->di_core.di_magic, ARCH_CONVERT) == XFS_DINODE_MAGIC) {
		if (summary) {
			kdb_printf("Disk Inode (at 0x%p)\n", di);
		} else {
			kdb_printf("buf 0x%p dinode 0x%p\n", bp, di);
			xfs_inodebuf(bp);
		}
	} else if (INT_GET((sb = d)->sb_magicnum, ARCH_CONVERT) == XFS_SB_MAGIC) {
		if (summary) {
			kdb_printf("Superblock (at 0x%p)\n", sb);
		} else {
			kdb_printf("buf 0x%p sb 0x%p\n", bp, sb);
			/* SB in a buffer - we need to convert */
			xfsidbg_xsb_convert(sb);
		}
	} else if ((dqb = d)->d_magic == cpu_to_be16(XFS_DQUOT_MAGIC)) {
#define XFSIDBG_DQTYPESTR(d)     \
	(((d)->d_flags & XFS_DQ_USER) ? "USR" : \
	(((d)->d_flags & XFS_DQ_GROUP) ? "GRP" : \
	(((d)->d_flags & XFS_DQ_PROJ) ? "PRJ" : "???")))
		kdb_printf("Quota blk starting ID [%d], type %s at 0x%p\n",
			be32_to_cpu(dqb->d_id), XFSIDBG_DQTYPESTR(dqb), dqb);

	} else if (INT_GET((d2block = d)->hdr.magic, ARCH_CONVERT) == XFS_DIR2_BLOCK_MAGIC) {
		if (summary) {
			kdb_printf("Dir2 block (at 0x%p)\n", d2block);
		} else {
			kdb_printf("buf 0x%p dir2 block 0x%p\n", bp, d2block);
			xfs_dir2data((void *)d2block, XFS_BUF_COUNT(bp));
		}
	} else if (INT_GET((d2data = d)->hdr.magic, ARCH_CONVERT) == XFS_DIR2_DATA_MAGIC) {
		if (summary) {
			kdb_printf("Dir2 data (at 0x%p)\n", d2data);
		} else {
			kdb_printf("buf 0x%p dir2 data 0x%p\n", bp, d2data);
			xfs_dir2data((void *)d2data, XFS_BUF_COUNT(bp));
		}
	} else if (INT_GET((d2leaf = d)->hdr.info.magic, ARCH_CONVERT) == XFS_DIR2_LEAF1_MAGIC) {
		if (summary) {
			kdb_printf("Dir2 leaf(1) (at 0x%p)\n", d2leaf);
		} else {
			kdb_printf("buf 0x%p dir2 leaf 0x%p\n", bp, d2leaf);
			xfs_dir2leaf(d2leaf, XFS_BUF_COUNT(bp));
		}
	} else if (INT_GET(d2leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR2_LEAFN_MAGIC) {
		if (summary) {
			kdb_printf("Dir2 leaf(n) (at 0x%p)\n", d2leaf);
		} else {
			kdb_printf("buf 0x%p dir2 leaf 0x%p\n", bp, d2leaf);
			xfs_dir2leaf(d2leaf, XFS_BUF_COUNT(bp));
		}
	} else if (INT_GET((d2free = d)->hdr.magic, ARCH_CONVERT) == XFS_DIR2_FREE_MAGIC) {
		if (summary) {
			kdb_printf("Dir2 free (at 0x%p)\n", d2free);
		} else {
			kdb_printf("buf 0x%p dir2 free 0x%p\n", bp, d2free);
			xfsidbg_xdir2free(d2free);
		}
	} else {
		kdb_printf("buf 0x%p unknown 0x%p\n", bp, d);
	}
}

#ifdef XFS_BMAP_TRACE
/*
 * Print out the last "count" entries in the bmap extent trace buffer.
 * The "a" is for "all" inodes.
 */
static void
xfsidbg_xbxatrace(int count)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	int	     nentries;
	int	     skip_entries;

	if (xfs_bmap_trace_buf == NULL) {
		qprintf("The xfs bmap extent trace buffer is not initialized\n");
		return;
	}
	nentries = ktrace_nentries(xfs_bmap_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(xfs_bmap_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(xfs_bmap_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		if (xfs_bmap_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(xfs_bmap_trace_buf, &kts);
	}
}

/*
 * Print out the bmap extent trace buffer attached to the given inode.
 */
static void
xfsidbg_xbxitrace(xfs_inode_t *ip)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	if (ip->i_xtrace == NULL) {
		qprintf("The inode trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(ip->i_xtrace, &kts);
	while (ktep != NULL) {
		if (xfs_bmap_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(ip->i_xtrace, &kts);
	}
}

/*
 * Print out all the entries in the bmap extent trace buf corresponding
 * to the given inode.  The "s" is for "single" inode.
 */
static void
xfsidbg_xbxstrace(xfs_inode_t *ip)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;

	if (xfs_bmap_trace_buf == NULL) {
		qprintf("The xfs bmap extent trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(xfs_bmap_trace_buf, &kts);
	while (ktep != NULL) {
		if ((xfs_inode_t *)(ktep->val[3]) == ip) {
			if (xfs_bmap_trace_entry(ktep))
				qprintf("\n");
		}
		ktep = ktrace_next(xfs_bmap_trace_buf, &kts);
	}
}
#endif

#ifdef XFS_ILOCK_TRACE
/*
 * Print out the ilock trace buffer attached to the given inode.
 */
static void
xfsidbg_xilock_trace_entry(ktrace_entry_t *ktep)
{
	static char	*xiflags[] = {
		"IOLOCK_EXCL",
		"IOLOCK_SHAR",
		"ILOCK_EXCL",
		"ILOCK_SHAR",
		"IUNLK_NONOT",
		NULL
	};

	if ((__psint_t)ktep->val[0] &&
	     (__psint_t)ktep->val[7] == 0) {
		 printflags((__psint_t)ktep->val[2], xiflags,"Flags ");
		if ((__psint_t)ktep->val[1] == 1)
			 qprintf("LOCK\n");
		else if ((__psint_t)ktep->val[1] == 2)
			 qprintf("LOCK SHARED\n");
		else if ((__psint_t)ktep->val[1] == 3)
			 qprintf("UNLOCK\n");
		qprintf("ip 0x%p %llx %ld\n",
			ktep->val[0],
			(unsigned long long)((xfs_inode_t*)ktep->val[0])->i_ino,
			(long)ktep->val[6]);
		qprintf("raddr 0x%p\n", ktep->val[3]);
		qprintf("  Pid %ld, cpu %ld\n",
			 (long)ktep->val[5],
			 (long)ktep->val[4]);
		qprintf("-----------------------\n");

	} else if ((__psint_t)ktep->val[7] == 1) {
		if ((__psint_t)ktep->val[1] == 1)
			qprintf("FlushLOCK ");
		else if ((__psint_t)ktep->val[1] == 2)
			qprintf("FlushTRYLOCK %ld ",
				(long)ktep->val[2]);
		else if ((__psint_t)ktep->val[1] == 3)
			qprintf("FlushUNLOCK ");
		else if ((__psint_t)ktep->val[1] == 4)
			qprintf("FlushInode 0x%p",
				ktep->val[2]);
		else if ((__psint_t)ktep->val[1] == 5)
			qprintf("FlushInodeInt ");
		else     qprintf("FlushUNKNOWN ");
		qprintf("ip 0x%p ino %llx @ %ld\n",
			ktep->val[0],
			(unsigned long long)((xfs_inode_t*)ktep->val[0])->i_ino,
			(long)ktep->val[6]);
		qprintf("raddr 0x%p\n", ktep->val[3]);
		qprintf("  Pid %ld, cpu %ld\n",
			(long)ktep->val[5],
			(long)ktep->val[4]);
		qprintf("-----------------------\n");
	}
}

static void
xfsidbg_xilock_trace(xfs_inode_t *ip)
{
	static char *xiflags[] = {
		"IOLOCK_EXCL",
		"IOLOCK_SHAR",
		"ILOCK_EXCL",
		"ILOCK_SHAR",
		"IUNLK_NONOT",
		NULL
	};

	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	if (ip->i_lock_trace == NULL) {
		qprintf("The inode ilock trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(ip->i_lock_trace, &kts);
	while (ktep != NULL) {
		 if ((__psint_t)ktep->val[0] &&
		     (__psint_t)ktep->val[7] == 0) {
			 printflags((__psint_t)ktep->val[2], xiflags,"Flags ");
			 if ((__psint_t)ktep->val[1] == 1)
				 qprintf("LOCK\n");
			 else if ((__psint_t)ktep->val[1] == 2)
				 qprintf("LOCK SHARED\n");
			 else if ((__psint_t)ktep->val[1] == 3)
				 qprintf("UNLOCK\n");
			qprintf("ip 0x%p %lld %ld\n",
				ktep->val[0], (unsigned long long)
				((xfs_inode_t*)ktep->val[0])->i_ino,
				(long)ktep->val[6]);
			 qprintf("raddr 0x%p\n", ktep->val[3]);
			 qprintf("  Pid %ld, cpu %ld\n",
				 (long)ktep->val[5],
				 (long)ktep->val[4]);
			 qprintf("-----------------------\n");
		 } else if ((__psint_t)ktep->val[7] == 1) {
			if ((__psint_t)ktep->val[1] == 1)
				qprintf("LOCK ");
			else if ((__psint_t)ktep->val[1] == 2)
				qprintf("TRYLOCK %ld ",
					(long)ktep->val[2]);
			else if ((__psint_t)ktep->val[1] == 3)
				qprintf("UNLOCK ");
			else     qprintf("UNKNOWN ");
			qprintf("ip 0x%p %lld %ld\n",
				ktep->val[0], (unsigned long long)
				((xfs_inode_t*)ktep->val[0])->i_ino,
				(long)ktep->val[6]);
			qprintf("raddr 0x%p\n", ktep->val[3]);
			qprintf("  Pid %ld, cpu %ld\n",
				(long)ktep->val[5],
				(long)ktep->val[4]);
			qprintf("-----------------------\n");
		 }

		 ktep = ktrace_next(ip->i_lock_trace, &kts);
	}
}

/*
 * Print out the last "count" entries in the inode lock trace buffer.
 * The "a" is for "all" entries.
 */
static void
xfsidbg_xailock_trace(int count)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	int	     nentries;
	int	     skip_entries;

	if (xfs_ilock_trace_buf == NULL) {
		qprintf("The xfs inode lock trace buffer is not initialized\n");		return;
	}
	nentries = ktrace_nentries(xfs_ilock_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(xfs_ilock_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(xfs_ilock_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		xfsidbg_xilock_trace_entry(ktep);
		ktep = ktrace_next(xfs_ilock_trace_buf, &kts);
	}
}
#endif

/*
 * Compute & print buffer's checksum.
 */
static void
xfsidbg_xchksum(uint *addr)
{
	uint i, chksum = 0;

	if (((__psint_t)addr) == ((__psint_t)-1)) {
		qprintf("USAGE xchksum <address>\n");
		qprintf("       length is set with xarg\n");
	} else {
		for (i=0; i<xargument; i++) {
			chksum ^= *addr;
			addr++;
		}
		qprintf("chksum (0x%x)  length (%d)\n", chksum, xargument);
	}
}       /* xfsidbg_xchksum */

/*
 * Print an xfs_da_args structure.
 */
static void
xfsidbg_xdaargs(xfs_da_args_t *n)
{
	char *ch;
	int i;

	kdb_printf(" name \"");
	for (i = 0; i < n->namelen; i++) {
		kdb_printf("%c", n->name[i]);
	}
	kdb_printf("\"(%d) value ", n->namelen);
	if (n->value) {
		kdb_printf("\"");
		ch = n->value;
		for (i = 0; (i < n->valuelen) && (i < 32); ch++, i++) {
			switch(*ch) {
			case '\n':	kdb_printf("\n");		break;
			case '\b':	kdb_printf("\b");		break;
			case '\t':	kdb_printf("\t");		break;
			default:	kdb_printf("%c", *ch);	break;
			}
		}
		if (i == 32)
			kdb_printf("...");
		kdb_printf("\"(%d)\n", n->valuelen);
	} else {
		kdb_printf("(NULL)(%d)\n", n->valuelen);
	}
	kdb_printf(" hashval 0x%x whichfork %d flags <",
		  (uint_t)n->hashval, n->whichfork);
	if (n->flags & ATTR_ROOT)
		kdb_printf("ROOT ");
	if (n->flags & ATTR_SECURE)
		kdb_printf("SECURE ");
	if (n->flags & ATTR_CREATE)
		kdb_printf("CREATE ");
	if (n->flags & ATTR_REPLACE)
		kdb_printf("REPLACE ");
	if (n->flags & XFS_ATTR_INCOMPLETE)
		kdb_printf("INCOMPLETE ");
	i = ~(ATTR_ROOT | ATTR_SECURE |
		ATTR_CREATE | ATTR_REPLACE | XFS_ATTR_INCOMPLETE);
	if ((n->flags & i) != 0)
		kdb_printf("0x%x", n->flags & i);
	kdb_printf(">\n");
	kdb_printf(" rename %d justcheck %d addname %d oknoent %d\n",
		  n->rename, n->justcheck, n->addname, n->oknoent);
	kdb_printf(" leaf: blkno %d index %d rmtblkno %d rmtblkcnt %d\n",
		  n->blkno, n->index, n->rmtblkno, n->rmtblkcnt);
	kdb_printf(" leaf2: blkno %d index %d rmtblkno %d rmtblkcnt %d\n",
		  n->blkno2, n->index2, n->rmtblkno2, n->rmtblkcnt2);
	kdb_printf(" inumber %llu dp 0x%p firstblock 0x%p flist 0x%p\n",
		  (unsigned long long) n->inumber,
		  n->dp, n->firstblock, n->flist);
	kdb_printf(" trans 0x%p total %d\n",
		  n->trans, n->total);
}

/*
 * Print a da buffer structure.
 */
static void
xfsidbg_xdabuf(xfs_dabuf_t *dabuf)
{
	int	i;

	kdb_printf("nbuf %d dirty %d bbcount %d data 0x%p bps",
		dabuf->nbuf, dabuf->dirty, dabuf->bbcount, dabuf->data);
	for (i = 0; i < dabuf->nbuf; i++)
		kdb_printf(" %d:0x%p", i, dabuf->bps[i]);
	kdb_printf("\n");
#ifdef XFS_DABUF_DEBUG
	kdb_printf(" ra 0x%x prev 0x%x next 0x%x dev 0x%x blkno 0x%x\n",
		dabuf->ra, dabuf->prev, dabuf->next, dabuf->dev, dabuf->blkno);
#endif
}

/*
 * Print a directory/attribute internal node block.
 */
static void
xfsidbg_xdanode(xfs_da_intnode_t *node)
{
	xfs_da_node_hdr_t *h;
	xfs_da_blkinfo_t *i;
	xfs_da_node_entry_t *e;
	int j;

	h = &node->hdr;
	i = &h->info;
	kdb_printf("hdr info forw 0x%x back 0x%x magic 0x%x\n",
		INT_GET(i->forw, ARCH_CONVERT), INT_GET(i->back, ARCH_CONVERT), INT_GET(i->magic, ARCH_CONVERT));
	kdb_printf("hdr count %d level %d\n",
		INT_GET(h->count, ARCH_CONVERT), INT_GET(h->level, ARCH_CONVERT));
	for (j = 0, e = node->btree; j < INT_GET(h->count, ARCH_CONVERT); j++, e++) {
		kdb_printf("btree %d hashval 0x%x before 0x%x\n",
			j, (uint_t)INT_GET(e->hashval, ARCH_CONVERT), INT_GET(e->before, ARCH_CONVERT));
	}
}

/*
 * Print an xfs_da_state_blk structure.
 */
static void
xfsidbg_xdastate(xfs_da_state_t *s)
{
	xfs_da_state_blk_t *eblk;

	kdb_printf("args 0x%p mp 0x%p blocksize %u node_ents %u inleaf %u\n",
		s->args, s->mp, s->blocksize, s->node_ents, s->inleaf);
	if (s->args)
		xfsidbg_xdaargs(s->args);

	kdb_printf("path:  ");
	xfs_dastate_path(&s->path);

	kdb_printf("altpath:  ");
	xfs_dastate_path(&s->altpath);

	eblk = &s->extrablk;
	kdb_printf("extra: valid %d, after %d\n", s->extravalid, s->extraafter);
	kdb_printf(" bp 0x%p blkno 0x%x ", eblk->bp, eblk->blkno);
	kdb_printf("index %d hashval 0x%x\n", eblk->index, (uint_t)eblk->hashval);
}

/*
 * Print a directory leaf block.
 */
static void
xfsidbg_xdirleaf(xfs_dir_leafblock_t *leaf)
{
	xfs_dir_leaf_hdr_t *h;
	xfs_da_blkinfo_t *i;
	xfs_dir_leaf_map_t *m;
	xfs_dir_leaf_entry_t *e;
	xfs_dir_leaf_name_t *n;
	int j, k;
	xfs_ino_t ino;

	h = &leaf->hdr;
	i = &h->info;
	kdb_printf("hdr info forw 0x%x back 0x%x magic 0x%x\n",
		INT_GET(i->forw, ARCH_CONVERT), INT_GET(i->back, ARCH_CONVERT), INT_GET(i->magic, ARCH_CONVERT));
	kdb_printf("hdr count %d namebytes %d firstused %d holes %d\n",
		INT_GET(h->count, ARCH_CONVERT), INT_GET(h->namebytes, ARCH_CONVERT), INT_GET(h->firstused, ARCH_CONVERT), h->holes);
	for (j = 0, m = h->freemap; j < XFS_DIR_LEAF_MAPSIZE; j++, m++) {
		kdb_printf("hdr freemap %d base %d size %d\n",
			j, INT_GET(m->base, ARCH_CONVERT), INT_GET(m->size, ARCH_CONVERT));
	}
	for (j = 0, e = leaf->entries; j < INT_GET(h->count, ARCH_CONVERT); j++, e++) {
		n = XFS_DIR_LEAF_NAMESTRUCT(leaf, INT_GET(e->nameidx, ARCH_CONVERT));
		XFS_DIR_SF_GET_DIRINO(&n->inumber, &ino);
		kdb_printf("leaf %d hashval 0x%x nameidx %d inumber %llu ",
			j, (uint_t)INT_GET(e->hashval, ARCH_CONVERT),
			INT_GET(e->nameidx, ARCH_CONVERT),
			(unsigned long long)ino);
		kdb_printf("namelen %d name \"", e->namelen);
		for (k = 0; k < e->namelen; k++)
			kdb_printf("%c", n->name[k]);
		kdb_printf("\"\n");
	}
}

/*
 * Print a directory v2 data block, single or multiple.
 */
static void
xfs_dir2data(void *addr, int size)
{
	xfs_dir2_data_t *db;
	xfs_dir2_block_t *bb;
	xfs_dir2_data_hdr_t *h;
	xfs_dir2_data_free_t *m;
	xfs_dir2_data_entry_t *e;
	xfs_dir2_data_unused_t *u;
	xfs_dir2_leaf_entry_t *l=NULL;
	int j, k;
	char *p;
	char *t;
	xfs_dir2_block_tail_t *tail=NULL;

	db = (xfs_dir2_data_t *)addr;
	bb = (xfs_dir2_block_t *)addr;
	h = &db->hdr;
	kdb_printf("hdr magic 0x%x (%s)\nhdr bestfree", INT_GET(h->magic, ARCH_CONVERT),
		INT_GET(h->magic, ARCH_CONVERT) == XFS_DIR2_DATA_MAGIC ? "DATA" :
			(INT_GET(h->magic, ARCH_CONVERT) == XFS_DIR2_BLOCK_MAGIC ? "BLOCK" : ""));
	for (j = 0, m = h->bestfree; j < XFS_DIR2_DATA_FD_COUNT; j++, m++) {
		kdb_printf(" %d: 0x%x@0x%x", j, INT_GET(m->length, ARCH_CONVERT), INT_GET(m->offset, ARCH_CONVERT));
	}
	kdb_printf("\n");
	if (INT_GET(h->magic, ARCH_CONVERT) == XFS_DIR2_DATA_MAGIC)
		t = (char *)db + size;
	else {
		/* XFS_DIR2_BLOCK_TAIL_P */
		tail = (xfs_dir2_block_tail_t *)
		       ((char *)bb + size - sizeof(xfs_dir2_block_tail_t));
		l = XFS_DIR2_BLOCK_LEAF_P(tail);
		t = (char *)l;
	}
	for (p = (char *)(h + 1); p < t; ) {
		u = (xfs_dir2_data_unused_t *)p;
		if (u->freetag == XFS_DIR2_DATA_FREE_TAG) {
			kdb_printf("0x%lx unused freetag 0x%x length 0x%x tag 0x%x\n",
				(unsigned long) (p - (char *)addr),
				INT_GET(u->freetag, ARCH_CONVERT),
				INT_GET(u->length, ARCH_CONVERT),
				INT_GET(*XFS_DIR2_DATA_UNUSED_TAG_P(u), ARCH_CONVERT));
			p += INT_GET(u->length, ARCH_CONVERT);
			continue;
		}
		e = (xfs_dir2_data_entry_t *)p;
		kdb_printf("0x%lx entry inumber %llu namelen %d name \"",
			(unsigned long) (p - (char *)addr),
			(unsigned long long) INT_GET(e->inumber, ARCH_CONVERT),
			e->namelen);
		for (k = 0; k < e->namelen; k++)
			kdb_printf("%c", e->name[k]);
		kdb_printf("\" tag 0x%x\n", INT_GET(*XFS_DIR2_DATA_ENTRY_TAG_P(e), ARCH_CONVERT));
		p += XFS_DIR2_DATA_ENTSIZE(e->namelen);
	}
	if (INT_GET(h->magic, ARCH_CONVERT) == XFS_DIR2_DATA_MAGIC)
		return;
	for (j = 0; j < INT_GET(tail->count, ARCH_CONVERT); j++, l++) {
		kdb_printf("0x%lx leaf %d hashval 0x%x address 0x%x (byte 0x%x)\n",
			(unsigned long) ((char *)l - (char *)addr), j,
			(uint_t)INT_GET(l->hashval, ARCH_CONVERT),
			INT_GET(l->address, ARCH_CONVERT),
			/* XFS_DIR2_DATAPTR_TO_BYTE */
			INT_GET(l->address, ARCH_CONVERT) << XFS_DIR2_DATA_ALIGN_LOG);
	}
	kdb_printf("0x%lx tail count %d\n",
		(unsigned long) ((char *)tail - (char *)addr),
		INT_GET(tail->count, ARCH_CONVERT));
}

static void
xfs_dir2leaf(xfs_dir2_leaf_t *leaf, int size)
{
	xfs_dir2_leaf_hdr_t *h;
	xfs_da_blkinfo_t *i;
	xfs_dir2_leaf_entry_t *e;
	xfs_dir2_data_off_t *b;
	xfs_dir2_leaf_tail_t *t;
	int j;

	h = &leaf->hdr;
	i = &h->info;
	e = leaf->ents;
	kdb_printf("hdr info forw 0x%x back 0x%x magic 0x%x\n",
		INT_GET(i->forw, ARCH_CONVERT), INT_GET(i->back, ARCH_CONVERT), INT_GET(i->magic, ARCH_CONVERT));
	kdb_printf("hdr count %d stale %d\n", INT_GET(h->count, ARCH_CONVERT), INT_GET(h->stale, ARCH_CONVERT));
	for (j = 0; j < INT_GET(h->count, ARCH_CONVERT); j++, e++) {
		kdb_printf("0x%lx ent %d hashval 0x%x address 0x%x (byte 0x%x)\n",
			(unsigned long) ((char *)e - (char *)leaf), j,
			(uint_t)INT_GET(e->hashval, ARCH_CONVERT),
			INT_GET(e->address, ARCH_CONVERT),
			/* XFS_DIR2_DATAPTR_TO_BYTE */
			INT_GET(e->address, ARCH_CONVERT) << XFS_DIR2_DATA_ALIGN_LOG);
	}
	if (INT_GET(i->magic, ARCH_CONVERT) == XFS_DIR2_LEAFN_MAGIC)
		return;
	/* XFS_DIR2_LEAF_TAIL_P */
	t = (xfs_dir2_leaf_tail_t *)((char *)leaf + size - sizeof(*t));
	b = XFS_DIR2_LEAF_BESTS_P(t);
	for (j = 0; j < INT_GET(t->bestcount, ARCH_CONVERT); j++, b++) {
		kdb_printf("0x%lx best %d 0x%x\n",
			(unsigned long) ((char *)b - (char *)leaf), j,
			INT_GET(*b, ARCH_CONVERT));
	}
	kdb_printf("tail bestcount %d\n", INT_GET(t->bestcount, ARCH_CONVERT));
}

/*
 * Print a shortform directory.
 */
static void
xfsidbg_xdirsf(xfs_dir_shortform_t *s)
{
	xfs_dir_sf_hdr_t *sfh;
	xfs_dir_sf_entry_t *sfe;
	xfs_ino_t ino;
	int i, j;

	sfh = &s->hdr;
	XFS_DIR_SF_GET_DIRINO(&sfh->parent, &ino);
	kdb_printf("hdr parent %llu", (unsigned long long)ino);
	kdb_printf(" count %d\n", sfh->count);
	for (i = 0, sfe = s->list; i < sfh->count; i++) {
		XFS_DIR_SF_GET_DIRINO(&sfe->inumber, &ino);
		kdb_printf("entry %d inumber %llu", i, (unsigned long long)ino);
		kdb_printf(" namelen %d name \"", sfe->namelen);
		for (j = 0; j < sfe->namelen; j++)
			kdb_printf("%c", sfe->name[j]);
		kdb_printf("\"\n");
		sfe = XFS_DIR_SF_NEXTENTRY(sfe);
	}
}

/*
 * Print a shortform v2 directory.
 */
static void
xfsidbg_xdir2sf(xfs_dir2_sf_t *s)
{
	xfs_dir2_sf_hdr_t *sfh;
	xfs_dir2_sf_entry_t *sfe;
	xfs_ino_t ino;
	int i, j;

	sfh = &s->hdr;
	ino = XFS_DIR2_SF_GET_INUMBER(s, &sfh->parent);
	kdb_printf("hdr count %d i8count %d parent %llu\n",
		sfh->count, sfh->i8count, (unsigned long long) ino);
	for (i = 0, sfe = XFS_DIR2_SF_FIRSTENTRY(s); i < sfh->count; i++) {
		ino = XFS_DIR2_SF_GET_INUMBER(s, XFS_DIR2_SF_INUMBERP(sfe));
		kdb_printf("entry %d inumber %llu offset 0x%x namelen %d name \"",
			i, (unsigned long long) ino,
			XFS_DIR2_SF_GET_OFFSET(sfe),
			sfe->namelen);
		for (j = 0; j < sfe->namelen; j++)
			kdb_printf("%c", sfe->name[j]);
		kdb_printf("\"\n");
		sfe = XFS_DIR2_SF_NEXTENTRY(s, sfe);
	}
}

/*
 * Print a node-form v2 directory freemap block.
 */
static void
xfsidbg_xdir2free(xfs_dir2_free_t *f)
{
	int	i;

	kdb_printf("hdr magic 0x%x firstdb %d nvalid %d nused %d\n",
		INT_GET(f->hdr.magic, ARCH_CONVERT), INT_GET(f->hdr.firstdb, ARCH_CONVERT), INT_GET(f->hdr.nvalid, ARCH_CONVERT), INT_GET(f->hdr.nused, ARCH_CONVERT));
	for (i = 0; i < INT_GET(f->hdr.nvalid, ARCH_CONVERT); i++) {
		kdb_printf("entry %d db %d count %d\n",
			i, i + INT_GET(f->hdr.firstdb, ARCH_CONVERT), INT_GET(f->bests[i], ARCH_CONVERT));
	}
}

#ifdef XFS_DIR_TRACE
/*
 * Print out the last "count" entries in the directory trace buffer.
 */
static void
xfsidbg_xdirtrace(int count)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	int	     nentries;
	int	     skip_entries;

	if (xfs_dir_trace_buf == NULL) {
		qprintf("The xfs directory trace buffer is not initialized\n");
		return;
	}
	nentries = ktrace_nentries(xfs_dir_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(xfs_dir_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(xfs_dir_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		if (xfs_dir_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(xfs_dir_trace_buf, &kts);
	}
}
#endif

#ifdef XFS_DIR2_TRACE
/*
 * Print out the last "count" entries in the directory v2 trace buffer.
 */
static void
xfsidbg_xdir2atrace(int count)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	int	     nentries;
	int	     skip_entries;

	if (xfs_dir2_trace_buf == NULL) {
		qprintf("The xfs dirv2 trace buffer is not initialized\n");
		return;
	}
	nentries = ktrace_nentries(xfs_dir2_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(xfs_dir2_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(xfs_dir2_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		if (xfs_dir2_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(xfs_dir2_trace_buf, &kts);
	}
}

/*
 * Print out the directory v2 trace buffer attached to the given inode.
 */
static void
xfsidbg_xdir2itrace(xfs_inode_t *ip)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;

	if (ip->i_dir_trace == NULL) {
		qprintf("The inode trace buffer is not initialized\n");
		return;
	}

	ktep = ktrace_first(ip->i_dir_trace, &kts);
	while (ktep != NULL) {
		if (xfs_dir2_trace_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(ip->i_dir_trace, &kts);
	}
}
#endif

/*
 * Print xfs extent records.
 */
static void
xfsidbg_xexlist(xfs_inode_t *ip)
{
	xfs_xexlist_fork(ip, XFS_DATA_FORK);
	if (XFS_IFORK_Q(ip))
		xfs_xexlist_fork(ip, XFS_ATTR_FORK);
}

/*
 * Print an xfs free-extent records.
 */
static void
xfsidbg_xflist(xfs_bmap_free_t *flist)
{
	xfs_bmap_free_item_t	*item;

	kdb_printf("flist@0x%p: first 0x%p count %d low %d\n", flist,
		flist->xbf_first, flist->xbf_count, flist->xbf_low);
	for (item = flist->xbf_first; item; item = item->xbfi_next) {
		kdb_printf("item@0x%p: startblock %Lx blockcount %d", item,
			(xfs_dfsbno_t)item->xbfi_startblock,
			item->xbfi_blockcount);
	}
}

/*
 * Print out the help messages for these functions.
 */
static void
xfsidbg_xhelp(void)
{
	struct xif	*p;

	for (p = xfsidbg_funcs; p->name; p++)
		kdb_printf("%-16s %s %s\n", p->name, p->args, p->help);
}

/*
 * Print out an XFS in-core log structure.
 */
static void
xfsidbg_xiclog(xlog_in_core_t *iclog)
{
	int i;
	static char *ic_flags[] = {
		"ACTIVE",	/* 0x0001 */
		"WANT_SYNC",	/* 0x0002 */
		"SYNCING",	/* 0X0004 */
		"DONE_SYNC",	/* 0X0008 */
		"DO_CALLBACK",	/* 0X0010 */
		"CALLBACK",	/* 0X0020 */
		"DIRTY",	/* 0X0040 */
		"IOERROR",	/* 0X0080 */
		"NOTUSED",	/* 0X8000 */
		NULL
	};

	kdb_printf("xlog_in_core/header at 0x%p/0x%p\n",
		iclog, iclog->hic_data);
	kdb_printf("magicno: %x  cycle: %d  version: %d  lsn: 0x%Lx\n",
		INT_GET(iclog->ic_header.h_magicno, ARCH_CONVERT), INT_GET(iclog->ic_header.h_cycle, ARCH_CONVERT),
		INT_GET(iclog->ic_header.h_version, ARCH_CONVERT), INT_GET(iclog->ic_header.h_lsn, ARCH_CONVERT));
	kdb_printf("tail_lsn: 0x%Lx  len: %d  prev_block: %d  num_ops: %d\n",
		INT_GET(iclog->ic_header.h_tail_lsn, ARCH_CONVERT), INT_GET(iclog->ic_header.h_len, ARCH_CONVERT),
		INT_GET(iclog->ic_header.h_prev_block, ARCH_CONVERT), INT_GET(iclog->ic_header.h_num_logops, ARCH_CONVERT));
	kdb_printf("cycle_data: ");
	for (i=0; i<(iclog->ic_size>>BBSHIFT); i++) {
		kdb_printf("%x  ", INT_GET(iclog->ic_header.h_cycle_data[i], ARCH_CONVERT));
	}
	kdb_printf("\n");
	kdb_printf("size: %d\n", INT_GET(iclog->ic_header.h_size, ARCH_CONVERT));
	kdb_printf("\n");
	kdb_printf("--------------------------------------------------\n");
	kdb_printf("data: 0x%p  &forcesema: 0x%p  next: 0x%p bp: 0x%p\n",
		iclog->ic_datap, &iclog->ic_forcesema, iclog->ic_next,
		iclog->ic_bp);
	kdb_printf("log: 0x%p  callb: 0x%p  callb_tail: 0x%p\n",
		iclog->ic_log, iclog->ic_callback, iclog->ic_callback_tail);
	kdb_printf("size: %d (OFFSET: %d) trace: 0x%p refcnt: %d bwritecnt: %d",
		iclog->ic_size, iclog->ic_offset,
#ifdef XFS_LOG_TRACE
		iclog->ic_trace,
#else
		NULL,
#endif
		iclog->ic_refcnt, iclog->ic_bwritecnt);
	if (iclog->ic_state & XLOG_STATE_ALL)
		printflags(iclog->ic_state, ic_flags, " state:");
	else
		kdb_printf(" state: INVALID 0x%x", iclog->ic_state);
	kdb_printf("\n");
}	/* xfsidbg_xiclog */


/*
 * Print all incore logs.
 */
static void
xfsidbg_xiclogall(xlog_in_core_t *iclog)
{
    xlog_in_core_t *first_iclog = iclog;

    do {
	xfsidbg_xiclog(iclog);
	kdb_printf("=================================================\n");
	iclog = iclog->ic_next;
    } while (iclog != first_iclog);
}	/* xfsidbg_xiclogall */

/*
 * Print out the callback structures attached to an iclog.
 */
static void
xfsidbg_xiclogcb(xlog_in_core_t *iclog)
{
	xfs_log_callback_t	*cb;
	kdb_symtab_t		 symtab;

	for (cb = iclog->ic_callback; cb != NULL; cb = cb->cb_next) {

		if (kdbnearsym((unsigned long)cb->cb_func, &symtab)) {
			unsigned long offval;

			offval = (unsigned long)cb->cb_func - symtab.sym_start;

			if (offval)
				kdb_printf("func = %s+0x%lx",
							symtab.sym_name,
							offval);
			else
				kdb_printf("func = %s", symtab.sym_name);
		} else
			kdb_printf("func = ?? 0x%p", (void *)cb->cb_func);

		kdb_printf(" arg 0x%p next 0x%p\n", cb->cb_arg, cb->cb_next);
	}
}

#ifdef XFS_LOG_TRACE
/*
 * Print trace from incore log.
 */
static void
xfsidbg_xiclogtrace(xlog_in_core_t *iclog)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	ktrace_t	*kt = iclog->ic_trace;

	qprintf("iclog->ic_trace 0x%p\n", kt);
	ktep = ktrace_first(kt, &kts);
	while (ktep != NULL) {
		switch ((__psint_t)ktep->val[0]) {
		    case XLOG_TRACE_GRAB_FLUSH: {
			    qprintf("grabbing semaphore\n");
			    break;
		    }
		    case XLOG_TRACE_REL_FLUSH: {
			    qprintf("releasing semaphore\n");
			    break;
		    }
		    case XLOG_TRACE_SLEEP_FLUSH: {
			    qprintf("sleeping on semaphore\n");
			    break;
		    }
		    case XLOG_TRACE_WAKE_FLUSH: {
			    qprintf("waking up on semaphore\n");
			    break;
		    }
		    default: {
		    }
		}
		ktep = ktrace_next(kt, &kts);
	}
}       /* xfsidbg_xiclogtrace */
#endif

/*
 * Print all of the inodes attached to the given mount structure.
 */
static void
xfsidbg_xinodes(xfs_mount_t *mp)
{
	xfs_inode_t	*ip;

	kdb_printf("xfs_mount at 0x%p\n", mp);
	ip = mp->m_inodes;
	if (ip != NULL) {
		do {
			if (ip->i_mount == NULL) {
				ip = ip->i_mnext;
				continue;
			}
			kdb_printf("\n");
			xfsidbg_xnode(ip);
			ip = ip->i_mnext;
		} while (ip != mp->m_inodes);
	}
	kdb_printf("\nEnd of Inodes\n");
}

static void
xfsidbg_delayed_blocks(xfs_mount_t *mp)
{
	xfs_inode_t	*ip;
	unsigned int	total = 0;
	unsigned int	icount = 0;

	ip = mp->m_inodes;
	if (ip != NULL) {
		do {
			if (ip->i_mount == NULL) {
				ip = ip->i_mnext;
				continue;
			}
			if (ip->i_delayed_blks) {
				total += ip->i_delayed_blks;
				icount++;
			}
			ip = ip->i_mnext;
		} while (ip != mp->m_inodes);
	}
	kdb_printf("delayed blocks total: %d in %d inodes\n", total, icount);
}

static void
xfsidbg_xinodes_quiesce(xfs_mount_t *mp)
{
	xfs_inode_t	*ip;

	kdb_printf("xfs_mount at 0x%p\n", mp);
	ip = mp->m_inodes;
	if (ip != NULL) {
		do {
			if (ip->i_mount == NULL) {
				ip = ip->i_mnext;
				continue;
			}
			if (!(ip->i_flags & XFS_IQUIESCE)) {
				kdb_printf("ip 0x%p not quiesced\n", ip);
			}
			ip = ip->i_mnext;
		} while (ip != mp->m_inodes);
	}
	kdb_printf("\nEnd of Inodes\n");
}

static char *
xfsidbg_get_cstate(int state)
{
	switch(state) {
	case  XLOG_STATE_COVER_IDLE:
		return("idle");
	case  XLOG_STATE_COVER_NEED:
		return("need");
	case  XLOG_STATE_COVER_DONE:
		return("done");
	case  XLOG_STATE_COVER_NEED2:
		return("need2");
	case  XLOG_STATE_COVER_DONE2:
		return("done2");
	default:
		return("unknown");
	}
}

/*
 * Print out an XFS log structure.
 */
static void
xfsidbg_xlog(xlog_t *log)
{
	static char *t_flags[] = {
		"CHKSUM_MISMATCH",	/* 0x01 */
		"ACTIVE_RECOVERY",	/* 0x02 */
		"RECOVERY_NEEDED",	/* 0x04 */
		"IO_ERROR",		/* 0x08 */
		NULL
	};

	kdb_printf("xlog at 0x%p\n", log);
	kdb_printf("&flushsm: 0x%p  flushcnt: %d tic_cnt: %d	 tic_tcnt: %d  \n",
		&log->l_flushsema, log->l_flushcnt,
		log->l_ticket_cnt, log->l_ticket_tcnt);
	kdb_printf("freelist: 0x%p  tail: 0x%p	ICLOG: 0x%p  \n",
		log->l_freelist, log->l_tail, log->l_iclog);
	kdb_printf("&icloglock: 0x%p  tail_lsn: %s  last_sync_lsn: %s \n",
		&log->l_icloglock, xfs_fmtlsn(&log->l_tail_lsn),
		xfs_fmtlsn(&log->l_last_sync_lsn));
	kdb_printf("mp: 0x%p  xbuf: 0x%p  l_covered_state: %s \n",
		log->l_mp, log->l_xbuf,
			xfsidbg_get_cstate(log->l_covered_state));
	kdb_printf("flags: ");
	printflags(log->l_flags, t_flags,"log");
	kdb_printf("  logBBstart: %lld logsize: %d logBBsize: %d\n",
		(long long) log->l_logBBstart,
		log->l_logsize,log->l_logBBsize);
	kdb_printf("curr_cycle: %d  prev_cycle: %d  curr_block: %d  prev_block: %d\n",
	     log->l_curr_cycle, log->l_prev_cycle, log->l_curr_block,
	     log->l_prev_block);
	kdb_printf("iclog_bak: 0x%p  iclog_size: 0x%x (%d)  num iclogs: %d\n",
		log->l_iclog_bak, log->l_iclog_size, log->l_iclog_size,
		log->l_iclog_bufs);
	kdb_printf("l_iclog_hsize %d l_iclog_heads %d\n",
		log->l_iclog_hsize, log->l_iclog_heads);
	kdb_printf("l_sectbb_log %u l_sectbb_mask %u\n",
		log->l_sectbb_log, log->l_sectbb_mask);
	kdb_printf("&grant_lock: 0x%p  resHeadQ: 0x%p  wrHeadQ: 0x%p\n",
		&log->l_grant_lock, log->l_reserve_headq, log->l_write_headq);
	kdb_printf("GResCycle: %d  GResBytes: %d  GWrCycle: %d  GWrBytes: %d\n",
		log->l_grant_reserve_cycle, log->l_grant_reserve_bytes,
		log->l_grant_write_cycle, log->l_grant_write_bytes);
	qprintf("GResBlocks: %d GResRemain: %d  GWrBlocks: %d GWrRemain: %d\n",
		(int)BTOBBT(log->l_grant_reserve_bytes),
		log->l_grant_reserve_bytes % BBSIZE,
		(int)BTOBBT(log->l_grant_write_bytes),
		log->l_grant_write_bytes % BBSIZE);
#ifdef XFS_LOG_TRACE
	qprintf("trace: 0x%p  grant_trace: use xlog value\n", log->l_trace);
#endif
}	/* xfsidbg_xlog */

static void
xfsidbg_print_trans_type(unsigned int t_type)
{
	switch (t_type) {
	case XFS_TRANS_SETATTR_NOT_SIZE: kdb_printf("SETATTR_NOT_SIZE");break;
	case XFS_TRANS_SETATTR_SIZE:	kdb_printf("SETATTR_SIZE");	break;
	case XFS_TRANS_INACTIVE:	kdb_printf("INACTIVE");		break;
	case XFS_TRANS_CREATE:		kdb_printf("CREATE");		break;
	case XFS_TRANS_CREATE_TRUNC:	kdb_printf("CREATE_TRUNC");	break;
	case XFS_TRANS_TRUNCATE_FILE:	kdb_printf("TRUNCATE_FILE");	break;
	case XFS_TRANS_REMOVE:		kdb_printf("REMOVE");		break;
	case XFS_TRANS_LINK:		kdb_printf("LINK");		break;
	case XFS_TRANS_RENAME:		kdb_printf("RENAME");		break;
	case XFS_TRANS_MKDIR:		kdb_printf("MKDIR");		break;
	case XFS_TRANS_RMDIR:		kdb_printf("RMDIR");		break;
	case XFS_TRANS_SYMLINK:		kdb_printf("SYMLINK");		break;
	case XFS_TRANS_SET_DMATTRS:	kdb_printf("SET_DMATTRS");	break;
	case XFS_TRANS_GROWFS:		kdb_printf("GROWFS");		break;
	case XFS_TRANS_STRAT_WRITE:	kdb_printf("STRAT_WRITE");	break;
	case XFS_TRANS_DIOSTRAT:	kdb_printf("DIOSTRAT");		break;
	case XFS_TRANS_WRITE_SYNC:	kdb_printf("WRITE_SYNC");	break;
	case XFS_TRANS_WRITEID:		kdb_printf("WRITEID");		break;
	case XFS_TRANS_ADDAFORK:	kdb_printf("ADDAFORK");		break;
	case XFS_TRANS_ATTRINVAL:	kdb_printf("ATTRINVAL");	break;
	case XFS_TRANS_ATRUNCATE:	kdb_printf("ATRUNCATE");	break;
	case XFS_TRANS_ATTR_SET:	kdb_printf("ATTR_SET");		break;
	case XFS_TRANS_ATTR_RM:		kdb_printf("ATTR_RM");		break;
	case XFS_TRANS_ATTR_FLAG:	kdb_printf("ATTR_FLAG");	break;
	case XFS_TRANS_CLEAR_AGI_BUCKET:kdb_printf("CLEAR_AGI_BUCKET");	break;
	case XFS_TRANS_QM_SBCHANGE:	kdb_printf("QM_SBCHANGE");	break;
	case XFS_TRANS_QM_QUOTAOFF:	kdb_printf("QM_QUOTAOFF");	break;
	case XFS_TRANS_QM_DQALLOC:	kdb_printf("QM_DQALLOC");	break;
	case XFS_TRANS_QM_SETQLIM:	kdb_printf("QM_SETQLIM");	break;
	case XFS_TRANS_QM_DQCLUSTER:	kdb_printf("QM_DQCLUSTER");	break;
	case XFS_TRANS_QM_QINOCREATE:	kdb_printf("QM_QINOCREATE");	break;
	case XFS_TRANS_QM_QUOTAOFF_END:	kdb_printf("QM_QOFF_END");	break;
	case XFS_TRANS_SB_UNIT:		kdb_printf("SB_UNIT");		break;
	case XFS_TRANS_FSYNC_TS:	kdb_printf("FSYNC_TS");		break;
	case XFS_TRANS_GROWFSRT_ALLOC:	kdb_printf("GROWFSRT_ALLOC");	break;
	case XFS_TRANS_GROWFSRT_ZERO:	kdb_printf("GROWFSRT_ZERO");	break;
	case XFS_TRANS_GROWFSRT_FREE:	kdb_printf("GROWFSRT_FREE");	break;
	default:			kdb_printf("unknown(0x%x)", t_type); break;
	}
}

#ifdef XFS_LOG_TRACE
/*
 * Print grant trace for a log.
 */
static void
xfsidbg_xlog_granttrace(xlog_t *log)
{
	ktrace_entry_t  *ktep;
	ktrace_snap_t   kts;
	ktrace_t	*kt;
	int		i = 0;
	unsigned long	cnts,t_ocnt, t_cnt;

	if (((__psint_t)log) == ((__psint_t)-1)) {
		qprintf("Usage: xl_grtr <log>\n");
		return;
	}
	if ((kt = log->l_grant_trace))
		qprintf("log->l_grant_trace 0x%p\n", kt);
	else {
		qprintf("log->l_grant_trace is empty!\n");
		return;
	}
	ktep = ktrace_first(kt, &kts);
	while (ktep != NULL) {
		/* split cnts into two parts: cnt and ocnt */
		cnts = (unsigned long)ktep->val[13];
		t_ocnt = 0xff & cnts;
		t_cnt =  cnts >> 8;

		qprintf("%d: %s [", i++, (char *)ktep->val[11]);
		xfsidbg_print_trans_type((unsigned long)ktep->val[12]);
		qprintf("]\n");
		qprintf("  t_ocnt = %lu, t_cnt = %lu, t_curr_res = %lu, "
			"t_unit_res = %lu\n", 
			t_ocnt, t_cnt, (unsigned long)ktep->val[14],
			(unsigned long)ktep->val[15]);
		qprintf("  tic:0x%p resQ:0x%p wrQ:0x%p ",
			ktep->val[0], ktep->val[1], ktep->val[2]);
		qprintf("  GrResC:%ld GrResB:%ld GrWrC:%ld GrWrB:%ld \n",
			(long)ktep->val[3], (long)ktep->val[4],
			(long)ktep->val[5], (long)ktep->val[6]);
		qprintf("  HeadC:%ld HeadB:%ld TailC:%ld TailB:%ld\n",
			(long)ktep->val[7], (long)ktep->val[8],
			(long)ktep->val[9], (long)ktep->val[10]);
		ktep = ktrace_next(kt, &kts);
	}
}       /* xfsidbg_xlog_granttrace */
#endif

/*
 * Print out an XFS recovery transaction
 */
static void
xfsidbg_xlog_ritem(xlog_recover_item_t *item)
{
	int i = XLOG_MAX_REGIONS_IN_ITEM;

	kdb_printf("(xlog_recover_item 0x%p) ", item);
	kdb_printf("next: 0x%p prev: 0x%p type: %d cnt: %d ttl: %d\n",
		item->ri_next, item->ri_prev, ITEM_TYPE(item), item->ri_cnt,
		item->ri_total);
	for ( ; i > 0; i--) {
		if (!item->ri_buf[XLOG_MAX_REGIONS_IN_ITEM-i].i_addr)
			break;
		kdb_printf("a: 0x%p l: %d ",
			item->ri_buf[XLOG_MAX_REGIONS_IN_ITEM-i].i_addr,
			item->ri_buf[XLOG_MAX_REGIONS_IN_ITEM-i].i_len);
	}
	kdb_printf("\n");
}	/* xfsidbg_xlog_ritem */

/*
 * Print out an XFS recovery transaction
 */
static void
xfsidbg_xlog_rtrans(xlog_recover_t *trans)
{
	xlog_recover_item_t *rip, *first_rip;

	kdb_printf("(xlog_recover 0x%p) ", trans);
	kdb_printf("tid: %x type: %d items: %d ttid: 0x%x  ",
		trans->r_log_tid, trans->r_theader.th_type,
		trans->r_theader.th_num_items, trans->r_theader.th_tid);
	kdb_printf("itemq: 0x%p\n", trans->r_itemq);
	if (trans->r_itemq) {
		rip = first_rip = trans->r_itemq;
		do {
			kdb_printf("(recovery item: 0x%p) ", rip);
			kdb_printf("type: %d cnt: %d total: %d\n",
				ITEM_TYPE(rip), rip->ri_cnt, rip->ri_total);
			rip = rip->ri_next;
		} while (rip != first_rip);
	}
}	/* xfsidbg_xlog_rtrans */

static void
xfsidbg_xlog_buf_logitem(xlog_recover_item_t *item)
{
	xfs_buf_log_format_t	*buf_f;
	int			i, j;
	int			bit;
	int			nbits;
	unsigned int		*data_map;
	unsigned int		map_size;
	int			size;

	buf_f = (xfs_buf_log_format_t *)item->ri_buf[0].i_addr;
	if (buf_f->blf_flags & XFS_BLI_INODE_BUF) {
		kdb_printf("\tINODE BUF <blkno=0x%Lx, len=0x%x>\n",
			buf_f->blf_blkno, buf_f->blf_len);
	} else if (buf_f->blf_flags &
		  (XFS_BLI_UDQUOT_BUF|XFS_BLI_PDQUOT_BUF|XFS_BLI_GDQUOT_BUF)) {
		kdb_printf("\tDQUOT BUF <blkno=0x%Lx, len=0x%x>\n",
			buf_f->blf_blkno, buf_f->blf_len);
	} else {
		data_map = buf_f->blf_data_map;
		map_size = buf_f->blf_map_size;
		kdb_printf("\tREG BUF <blkno=0x%Lx, len=0x%x map 0x%p size %d>\n",
			buf_f->blf_blkno, buf_f->blf_len, data_map, map_size);
		bit = 0;
		i = 0;  /* 0 is the buf format structure */
		while (1) {
			bit = xfs_next_bit(data_map, map_size, bit);
			if (bit == -1)
				break;
			nbits = xfs_contig_bits(data_map, map_size, bit);
			size = ((uint)bit << XFS_BLI_SHIFT)+(nbits<<XFS_BLI_SHIFT);
			kdb_printf("\t\tlogbuf.i_addr 0x%p, size 0x%x\n",
				item->ri_buf[i].i_addr, size);
			kdb_printf("\t\t\t\"");
			for (j=0; j<8 && j<size; j++) {
				kdb_printf("%02x", ((char *)item->ri_buf[i].i_addr)[j]);
			}
			kdb_printf("...\"\n");
			i++;
			bit += nbits;
		}

	}
}

/*
 * Print out an ENTIRE XFS recovery transaction
 */
static void
xfsidbg_xlog_rtrans_entire(xlog_recover_t *trans)
{
	xlog_recover_item_t *item, *first_rip;

	kdb_printf("(Recovering Xact 0x%p) ", trans);
	kdb_printf("tid: %x type: %d nitems: %d ttid: 0x%x  ",
		trans->r_log_tid, trans->r_theader.th_type,
		trans->r_theader.th_num_items, trans->r_theader.th_tid);
	kdb_printf("itemq: 0x%p\n", trans->r_itemq);
	if (trans->r_itemq) {
		item = first_rip = trans->r_itemq;
		do {
			/*
			   kdb_printf("(recovery item: 0x%x) ", item);
			   kdb_printf("type: %d cnt: %d total: %d\n",
				   item->ri_type, item->ri_cnt, item->ri_total);
				   */
			if ((ITEM_TYPE(item) == XFS_LI_BUF) ||
			    (ITEM_TYPE(item) == XFS_LI_6_1_BUF) ||
			    (ITEM_TYPE(item) == XFS_LI_5_3_BUF)) {
				kdb_printf("BUF:");
				xfsidbg_xlog_buf_logitem(item);
			} else if ((ITEM_TYPE(item) == XFS_LI_INODE) ||
				   (ITEM_TYPE(item) == XFS_LI_6_1_INODE) ||
				   (ITEM_TYPE(item) == XFS_LI_5_3_INODE)) {
				kdb_printf("INODE:\n");
			} else if (ITEM_TYPE(item) == XFS_LI_EFI) {
				kdb_printf("EFI:\n");
			} else if (ITEM_TYPE(item) == XFS_LI_EFD) {
				kdb_printf("EFD:\n");
			} else if (ITEM_TYPE(item) == XFS_LI_DQUOT) {
				kdb_printf("DQUOT:\n");
			} else if ((ITEM_TYPE(item) == XFS_LI_QUOTAOFF)) {
				kdb_printf("QUOTAOFF:\n");
			} else {
				kdb_printf("UNKNOWN LOGITEM 0x%x\n", ITEM_TYPE(item));
			}
			item = item->ri_next;
		} while (item != first_rip);
	}
}	/* xfsidbg_xlog_rtrans */

/*
 * Print out an XFS ticket structure.
 */
static void
xfsidbg_xlog_tic(xlog_ticket_t *tic)
{
	static char *t_flags[] = {
		"INIT",		/* 0x1 */
		"PERM_RES",	/* 0x2 */
		"IN_Q",		/* 0x4 */
		NULL
	};

	kdb_printf("xlog_ticket at 0x%p\n", tic);
	kdb_printf("next: 0x%p  prev: 0x%p  tid: 0x%x  \n",
		tic->t_next, tic->t_prev, tic->t_tid);
	kdb_printf("curr_res: %d  unit_res: %d  ocnt: %d  cnt: %d\n",
		tic->t_curr_res, tic->t_unit_res, (int)tic->t_ocnt,
		(int)tic->t_cnt);
	kdb_printf("clientid: %c  \n", tic->t_clientid);
	printflags(tic->t_flags, t_flags,"ticket");
	kdb_printf("\n");
	qprintf("trans type: ");
	xfsidbg_print_trans_type(tic->t_trans_type);
	qprintf("\n");
}	/* xfsidbg_xlog_tic */

/*
 * Print out a single log item.
 */
static void
xfsidbg_xlogitem(xfs_log_item_t *lip)
{
	xfs_log_item_t	*bio_lip;
	static char *lid_type[] = {
		"???",		/* 0 */
		"5-3-buf",	/* 1 */
		"5-3-inode",	/* 2 */
		"efi",		/* 3 */
		"efd",		/* 4 */
		"iunlink",	/* 5 */
		"6-1-inode",	/* 6 */
		"6-1-buf",	/* 7 */
		"inode",	/* 8 */
		"buf",		/* 9 */
		"dquot",	/* 10 */
		NULL
		};
	static char *li_flags[] = {
		"in ail",	/* 0x1 */
		NULL
		};

	kdb_printf("type %s mountp 0x%p flags ",
		lid_type[lip->li_type - XFS_LI_5_3_BUF + 1],
		lip->li_mountp);
	printflags((uint)(lip->li_flags), li_flags,"log");
	kdb_printf("\n");
	kdb_printf("ail forw 0x%p ail back 0x%p lsn %s\ndesc %p ops 0x%p",
		lip->li_ail.ail_forw, lip->li_ail.ail_back,
		xfs_fmtlsn(&(lip->li_lsn)), lip->li_desc, lip->li_ops);
	kdb_printf(" iodonefunc &0x%p\n", lip->li_cb);
	if (lip->li_type == XFS_LI_BUF) {
		bio_lip = lip->li_bio_list;
		if (bio_lip != NULL) {
			kdb_printf("iodone list:\n");
		}
		while (bio_lip != NULL) {
			kdb_printf("item 0x%p func 0x%p\n",
				bio_lip, bio_lip->li_cb);
			bio_lip = bio_lip->li_bio_list;
		}
	}
	switch (lip->li_type) {
	case XFS_LI_BUF:
		xfs_buf_item_print((xfs_buf_log_item_t *)lip, 0);
		break;
	case XFS_LI_INODE:
		xfs_inode_item_print((xfs_inode_log_item_t *)lip, 0);
		break;
	case XFS_LI_EFI:
		xfs_efi_item_print((xfs_efi_log_item_t *)lip, 0);
		break;
	case XFS_LI_EFD:
		xfs_efd_item_print((xfs_efd_log_item_t *)lip, 0);
		break;
	case XFS_LI_DQUOT:
		xfs_dquot_item_print((xfs_dq_logitem_t *)lip, 0);
		break;
	case XFS_LI_QUOTAOFF:
		xfs_qoff_item_print((xfs_qoff_logitem_t *)lip, 0);
		break;

	default:
		kdb_printf("Unknown item type %d\n", lip->li_type);
		break;
	}
}

/*
 * Print out a summary of the AIL hanging off of a mount struct.
 */
static void
xfsidbg_xaildump(xfs_mount_t *mp)
{
	xfs_log_item_t *lip;
	static char *lid_type[] = {
		"???",		/* 0 */
		"5-3-buf",	/* 1 */
		"5-3-inode",	/* 2 */
		"efi",		/* 3 */
		"efd",		/* 4 */
		"iunlink",	/* 5 */
		"6-1-inode",	/* 6 */
		"6-1-buf",	/* 7 */
		"inode",	/* 8 */
		"buf",		/* 9 */
		"dquot",	/* 10 */
		NULL
		};
	static char *li_flags[] = {
		"in ail",	/* 0x1 */
		NULL
		};
	int count;

	if ((mp->m_ail.ail_forw == NULL) ||
	    (mp->m_ail.ail_forw == (xfs_log_item_t *)&mp->m_ail)) {
		kdb_printf("AIL is empty\n");
		return;
	}
	kdb_printf("AIL for mp 0x%p, oldest first\n", mp);
	lip = (xfs_log_item_t*)mp->m_ail.ail_forw;
	for (count = 0; lip; count++) {
		kdb_printf("[%d] type %s ", count,
			      lid_type[lip->li_type - XFS_LI_5_3_BUF + 1]);
		printflags((uint)(lip->li_flags), li_flags, "flags:");
		kdb_printf("  lsn %s\n   ", xfs_fmtlsn(&(lip->li_lsn)));
		switch (lip->li_type) {
		case XFS_LI_BUF:
			xfs_buf_item_print((xfs_buf_log_item_t *)lip, 1);
			break;
		case XFS_LI_INODE:
			xfs_inode_item_print((xfs_inode_log_item_t *)lip, 1);
			break;
		case XFS_LI_EFI:
			xfs_efi_item_print((xfs_efi_log_item_t *)lip, 1);
			break;
		case XFS_LI_EFD:
			xfs_efd_item_print((xfs_efd_log_item_t *)lip, 1);
			break;
		case XFS_LI_DQUOT:
			xfs_dquot_item_print((xfs_dq_logitem_t *)lip, 1);
			break;
		case XFS_LI_QUOTAOFF:
			xfs_qoff_item_print((xfs_qoff_logitem_t *)lip, 1);
			break;
		default:
			kdb_printf("Unknown item type %d\n", lip->li_type);
			break;
		}

		if (lip->li_ail.ail_forw == (xfs_log_item_t*)&mp->m_ail) {
			lip = NULL;
		} else {
			lip = lip->li_ail.ail_forw;
		}
	}
}

/*
 * Print xfs mount structure.
 */
static void
xfsidbg_xmount(xfs_mount_t *mp)
{
	static char *xmount_flags[] = {
		"WSYNC",	/* 0x0001 */
		"INO64",	/* 0x0002 */
		"UNUSED_4",	/* 0x0004 */
		"UNUSED_8",	/* 0x0008 */
		"FSSHUTDOWN",	/* 0x0010 */
		"NOATIME",	/* 0x0020 */
		"RETERR",	/* 0x0040 */
		"NOALIGN",	/* 0x0080 */
		"ATTR2",	/* 0x0100 */
		"UNUSED_200",	/* 0x0200 */
		"NORECOVERY",	/* 0x0400 */
		"SHARED",	/* 0x0800 */
		"DFLT_IOSIZE",	/* 0x1000 */
		"OSYNCISOSYNC",	/* 0x2000 */
		"32BITINODES",	/* 0x4000 */
		"32BITINOOPT",	/* 0x8000 */
		"NOUUID",	/* 0x10000 */
		"BARRIER",	/* 0x20000 */
		"IDELETE",	/* 0x40000 */
		"SWALLOC",	/* 0x80000 */
		"IHASHSIZE",	/* 0x100000 */
		"DIRSYNC",	/* 0x200000 */
		"COMPAT_IOSIZE",/* 0x400000 */
		NULL
	};

	static char *quota_flags[] = {
		"UQ",		/* 0x0001 */
		"UQE",		/* 0x0002 */
		"UQCHKD",	/* 0x0004 */
		"PQ",		/* 0x0008 */
		"GQE",		/* 0x0010 */
		"GQCHKD",	/* 0x0020 */
		"GQ",		/* 0x0040 */
		"UQACTV",	/* 0x0080 */
		"GQACTV",	/* 0x0100 */
		"QMAYBE",	/* 0x0200 */
		NULL
	};

	kdb_printf("xfs_mount at 0x%p\n", mp);
	kdb_printf("vfsp 0x%p tid 0x%x ail_lock 0x%p &ail 0x%p\n",
		XFS_MTOVFS(mp), mp->m_tid, &mp->m_ail_lock, &mp->m_ail);
	kdb_printf("ail_gen 0x%x &sb 0x%p\n",
		mp->m_ail_gen, &mp->m_sb);
	kdb_printf("sb_lock 0x%p sb_bp 0x%p dev 0x%s logdev 0x%s rtdev 0x%s\n",
		&mp->m_sb_lock, mp->m_sb_bp,
		mp->m_ddev_targp ? devtoname(mp->m_ddev_targp->dev) : 0,
		mp->m_logdev_targp ? devtoname(mp->m_logdev_targp->dev) : 0,
		mp->m_rtdev_targp ? devtoname(mp->m_rtdev_targp->dev) : 0);
	kdb_printf("bsize %d agfrotor %d xfs_rotorstep %d agirotor %d\n",
		mp->m_bsize, mp->m_agfrotor, xfs_rotorstep, mp->m_agirotor);
	kdb_printf("ihash 0x%p ihsize %d\n",
		mp->m_ihash, mp->m_ihsize);
	kdb_printf("inodes 0x%p ilock 0x%p ireclaims 0x%x\n",
		mp->m_inodes, &mp->m_ilock, mp->m_ireclaims);
	kdb_printf("readio_log 0x%x readio_blocks 0x%x ",
		mp->m_readio_log, mp->m_readio_blocks);
	kdb_printf("writeio_log 0x%x writeio_blocks 0x%x\n",
		mp->m_writeio_log, mp->m_writeio_blocks);
	kdb_printf("logbufs %d logbsize %d LOG 0x%p\n", mp->m_logbufs,
		mp->m_logbsize, mp->m_log);
	kdb_printf("rsumlevels 0x%x rsumsize 0x%x rbmip 0x%p rsumip 0x%p\n",
		mp->m_rsumlevels, mp->m_rsumsize, mp->m_rbmip, mp->m_rsumip);
	kdb_printf("rootip 0x%p\n", mp->m_rootip);
	kdb_printf("dircook_elog %d blkbit_log %d blkbb_log %d agno_log %d\n",
		mp->m_dircook_elog, mp->m_blkbit_log, mp->m_blkbb_log,
		mp->m_agno_log);
	kdb_printf("agino_log %d nreadaheads %d inode cluster size %d\n",
		mp->m_agino_log, mp->m_nreadaheads,
		mp->m_inode_cluster_size);
	kdb_printf("blockmask 0x%x blockwsize 0x%x blockwmask 0x%x\n",
		mp->m_blockmask, mp->m_blockwsize, mp->m_blockwmask);
	kdb_printf("alloc_mxr[lf,nd] %d %d alloc_mnr[lf,nd] %d %d\n",
		mp->m_alloc_mxr[0], mp->m_alloc_mxr[1],
		mp->m_alloc_mnr[0], mp->m_alloc_mnr[1]);
	kdb_printf("bmap_dmxr[lfnr,ndnr] %d %d bmap_dmnr[lfnr,ndnr] %d %d\n",
		mp->m_bmap_dmxr[0], mp->m_bmap_dmxr[1],
		mp->m_bmap_dmnr[0], mp->m_bmap_dmnr[1]);
	kdb_printf("inobt_mxr[lf,nd] %d %d inobt_mnr[lf,nd] %d %d\n",
		mp->m_inobt_mxr[0], mp->m_inobt_mxr[1],
		mp->m_inobt_mnr[0], mp->m_inobt_mnr[1]);
	kdb_printf("ag_maxlevels %d bm_maxlevels[d,a] %d %d in_maxlevels %d\n",
		mp->m_ag_maxlevels, mp->m_bm_maxlevels[0],
		mp->m_bm_maxlevels[1], mp->m_in_maxlevels);
	kdb_printf("perag 0x%p &peraglock 0x%p &growlock 0x%p\n",
		mp->m_perag, &mp->m_peraglock, &mp->m_growlock);
	printflags(mp->m_flags, xmount_flags,"flags");
	kdb_printf("ialloc_inos %d ialloc_blks %d litino %d\n",
		mp->m_ialloc_inos, mp->m_ialloc_blks, mp->m_litino);
	kdb_printf("dir_node_ents %u attr_node_ents %u\n",
		mp->m_dir_node_ents, mp->m_attr_node_ents);
	kdb_printf("attroffset %d maxicount %Ld inoalign_mask %d\n",
		mp->m_attroffset, mp->m_maxicount, mp->m_inoalign_mask);
	kdb_printf("resblks %Ld resblks_avail %Ld\n", mp->m_resblks,
		mp->m_resblks_avail);
#if XFS_BIG_INUMS
	kdb_printf(" inoadd %llx\n", (unsigned long long) mp->m_inoadd);
#else
	kdb_printf("\n");
#endif
	if (mp->m_quotainfo)
		kdb_printf("quotainfo 0x%p (uqip = 0x%p, gqip = 0x%p)\n",
			mp->m_quotainfo,
			mp->m_quotainfo->qi_uquotaip,
			mp->m_quotainfo->qi_gquotaip);
	else
		kdb_printf("quotainfo NULL\n");
	printflags(mp->m_qflags, quota_flags,"quotaflags");
	kdb_printf("\n");
	kdb_printf("dalign %d swidth %d sinoalign %d attr_magicpct %d dir_magicpct %d\n",
		mp->m_dalign, mp->m_swidth, mp->m_sinoalign,
		mp->m_attr_magicpct, mp->m_dir_magicpct);
	kdb_printf("mk_sharedro %d inode_quiesce %d sectbb_log %d\n",
		mp->m_mk_sharedro, mp->m_inode_quiesce, mp->m_sectbb_log);
	kdb_printf("dirversion %d dirblkfsbs %d &dirops 0x%p\n",
		mp->m_dirversion, mp->m_dirblkfsbs, &mp->m_dirops);
	kdb_printf("dirblksize %d dirdatablk 0x%Lx dirleafblk 0x%Lx dirfreeblk 0x%Lx\n",
		mp->m_dirblksize,
		(xfs_dfiloff_t)mp->m_dirdatablk,
		(xfs_dfiloff_t)mp->m_dirleafblk,
		(xfs_dfiloff_t)mp->m_dirfreeblk);
	kdb_printf("chsize %d chash 0x%p\n",
		mp->m_chsize, mp->m_chash);
	if (mp->m_fsname != NULL)
		kdb_printf("mountpoint \"%s\"\n", mp->m_fsname);
	else
		kdb_printf("No name!!!\n");

}

static void
xfsidbg_xihash(xfs_mount_t *mp)
{
	xfs_ihash_t	*ih;
	int		i;
	int		j;
	int		total;
	int		numzeros;
	xfs_inode_t	*ip;
	int		*hist;
	int		hist_bytes = mp->m_ihsize * sizeof(int);
	int		hist2[21];

	hist = (int *) kmem_alloc(hist_bytes, KM_SLEEP);

	if (hist == NULL) {
		kdb_printf("xfsidbg_xihash: kmalloc(%d) failed!\n",
							hist_bytes);
		return;
	}

	for (i = 0; i < mp->m_ihsize; i++) {
		ih = mp->m_ihash + i;
		j = 0;
		for (ip = ih->ih_next; ip != NULL; ip = ip->i_next)
			j++;
		hist[i] = j;
	}

	numzeros = total = 0;

	for (i = 0; i < 21; i++)
		hist2[i] = 0;

	for (i = 0; i < mp->m_ihsize; i++)  {
		kdb_printf("%d ", hist[i]);
		total += hist[i];
		numzeros += hist[i] == 0 ? 1 : 0;
		if (hist[i] > 20)
			j = 20;
		else
			j = hist[i];

		if (! (j <= 20)) {
			kdb_printf("xfsidbg_xihash: (j > 20)/%d @ line # %d\n",
							j, __LINE__);
			return;
		}

		hist2[j]++;
	}

	kdb_printf("\n");

	kdb_printf("total inodes = %d, average length = %d, adjusted average = %d \n",
		total, total / mp->m_ihsize,
		total / (mp->m_ihsize - numzeros));

	for (i = 0; i < 21; i++)  {
		kdb_printf("%d - %d , ", i, hist2[i]);
	}
	kdb_printf("\n");
	kmem_free(hist, hist_bytes);
}

/*
 * Command to print xfs inodes: kp xnode <addr>
 */
static void
xfsidbg_xnode(xfs_inode_t *ip)
{
	static char *tab_flags[] = {
		"grio",		/* XFS_IGRIO */
		"uiosize",	/* XFS_IUIOSZ */
		"quiesce",	/* XFS_IQUIESCE */
		"reclaim",	/* XFS_IRECLAIM */
		"stale",	/* XFS_ISTALE */
		NULL
	};

	kdb_printf("hash 0x%p next 0x%p prevp 0x%p mount 0x%p\n",
		ip->i_hash,
		ip->i_next,
		ip->i_prevp,
		ip->i_mount);
	kdb_printf("mnext 0x%p mprev 0x%p vnode 0x%p \n",
		ip->i_mnext,
		ip->i_mprev,
		XFS_ITOV_NULL(ip));
	kdb_printf("dev %s ino %s\n",
		devtoname(ip->i_mount->m_dev),
		xfs_fmtino(ip->i_ino, ip->i_mount));
	kdb_printf("blkno 0x%llx len 0x%x boffset 0x%x\n",
		(long long) ip->i_blkno,
		ip->i_len,
		ip->i_boffset);
	kdb_printf("transp 0x%p &itemp 0x%p\n",
		ip->i_transp,
		ip->i_itemp);
	kdb_printf("&lock 0x%p &iolock 0x%p ",
		&ip->i_lock,
		&ip->i_iolock);
	kdb_printf("&flock 0x%p (%d) pincount 0x%x\n",
		&ip->i_flock, valusema(&ip->i_flock),
		xfs_ipincount(ip));
	kdb_printf("udquotp 0x%p gdquotp 0x%p\n",
		ip->i_udquot, ip->i_gdquot);
	kdb_printf("new_size %Ld\n", ip->i_iocore.io_new_size);
	printflags((int)ip->i_flags, tab_flags, "flags");
	kdb_printf("\n");
	kdb_printf("update_core %d update size %d\n",
		(int)(ip->i_update_core), (int) ip->i_update_size);
	kdb_printf("gen 0x%x delayed blks %d",
		ip->i_gen,
		ip->i_delayed_blks);
#ifdef XFS_BMAP_TRACE
	qprintf(" bmap_trace 0x%p\n", ip->i_xtrace);
#endif
#ifdef XFS_BMBT_TRACE
	qprintf(" bmbt trace 0x%p\n", ip->i_btrace);
#endif  
#ifdef XFS_RW_TRACE
	qprintf(" rw trace 0x%p\n", ip->i_rwtrace);
#endif  
#ifdef XFS_ILOCK_TRACE
	qprintf(" ilock trace 0x%p\n", ip->i_lock_trace);
#endif
#ifdef XFS_DIR2_TRACE
	qprintf(" dir trace 0x%p\n", ip->i_dir_trace);
#endif  
	kdb_printf("\n");
	kdb_printf("chash 0x%p cnext 0x%p cprev 0x%p\n",
		ip->i_chash,
		ip->i_cnext,
		ip->i_cprev);
	xfs_xnode_fork("data", &ip->i_df);
	xfs_xnode_fork("attr", ip->i_afp);
	kdb_printf("\n");
	xfs_prdinode_core(&ip->i_d);
}

static void
xfsidbg_xcore(xfs_iocore_t *io)
{
	kdb_printf("io_obj 0x%p io_flags 0x%x io_mount 0x%p\n",
			io->io_obj, io->io_flags, io->io_mount);
	kdb_printf("new_size %Lx\n", io->io_new_size);
}

/*
 * Command to print xfs inode cluster hash table: kp xchash <addr>
 */
static void
xfsidbg_xchash(xfs_mount_t *mp)
{
	int		i;
	xfs_chash_t	*ch;

	kdb_printf("m_chash 0x%p size %d\n",
		mp->m_chash, mp->m_chsize);
	for (i = 0; i < mp->m_chsize; i++) {
		ch = mp->m_chash + i;
		kdb_printf("[%3d] ch 0x%p chashlist 0x%p\n", i, ch, ch->ch_list);
		xfsidbg_xchashlist(ch->ch_list);
	}
}

/*
 * Command to print xfs inode cluster hash list: kp xchashlist <addr>
 */
static void
xfsidbg_xchashlist(xfs_chashlist_t *chl)
{
	xfs_inode_t	*ip;

	while (chl != NULL) {
		kdb_printf("hashlist inode 0x%p blkno %lld buf 0x%p",
		       chl->chl_ip, (long long) chl->chl_blkno, chl->chl_buf);

		kdb_printf("\n");

		/* print inodes on chashlist */
		ip = chl->chl_ip;
		do {
			kdb_printf("0x%p ", ip);
			ip = ip->i_cnext;
		} while (ip != chl->chl_ip);
		kdb_printf("\n");

		chl=chl->chl_next;
	}
}

/*
 * Print xfs per-ag data structures for filesystem.
 */
static void
xfsidbg_xperag(xfs_mount_t *mp)
{
	xfs_agnumber_t	agno;
	xfs_perag_t	*pag;
	int		busy;

	pag = mp->m_perag;
	for (agno = 0; agno < mp->m_sb.sb_agcount; agno++, pag++) {
		kdb_printf("ag %d f_init %d i_init %d\n",
			agno, pag->pagf_init, pag->pagi_init);
		if (pag->pagf_init)
			kdb_printf(
	"    f_levels[b,c] %d,%d f_flcount %d f_freeblks %d f_longest %d\n"
	"    f__metadata %d\n",
				pag->pagf_levels[XFS_BTNUM_BNOi],
				pag->pagf_levels[XFS_BTNUM_CNTi],
				pag->pagf_flcount, pag->pagf_freeblks,
				pag->pagf_longest, pag->pagf_metadata);
		if (pag->pagi_init)
			kdb_printf("    i_freecount %d i_inodeok %d\n",
				pag->pagi_freecount, pag->pagi_inodeok);
		if (pag->pagf_init) {
			for (busy = 0; busy < XFS_PAGB_NUM_SLOTS; busy++) {
				if (pag->pagb_list[busy].busy_length != 0) {
					kdb_printf(
		"	 %04d: start %d length %d tp 0x%p\n",
					    busy,
					    pag->pagb_list[busy].busy_start,
					    pag->pagb_list[busy].busy_length,
					    pag->pagb_list[busy].busy_tp);
				}
			}
		}
	}
}

#ifdef CONFIG_XFS_QUOTA
static void
xfsidbg_xqm(void)
{
	if (xfs_Gqm == NULL) {
		kdb_printf("NULL XQM!!\n");
		return;
	}

	kdb_printf("usrhtab 0x%p grphtab 0x%p  ndqfree 0x%x  hashmask 0x%x\n",
		xfs_Gqm->qm_usr_dqhtable,
		xfs_Gqm->qm_grp_dqhtable,
		xfs_Gqm->qm_dqfreelist.qh_nelems,
		xfs_Gqm->qm_dqhashmask);
	kdb_printf("&freelist 0x%p, totaldquots 0x%x nrefs 0x%x\n",
		&xfs_Gqm->qm_dqfreelist,
		atomic_read(&xfs_Gqm->qm_totaldquots),
		xfs_Gqm->qm_nrefs);
}
#endif

static void
xfsidbg_xqm_diskdq(xfs_disk_dquot_t *d)
{
	kdb_printf("magic 0x%x\tversion 0x%x\tID 0x%x (%d)\t\n",
		be16_to_cpu(d->d_magic), d->d_version,
		be32_to_cpu(d->d_id), be32_to_cpu(d->d_id));
	kdb_printf("bhard 0x%llx\tbsoft 0x%llx\tihard 0x%llx\tisoft 0x%llx\n",
		be64_to_cpu(d->d_blk_hardlimit),
		be64_to_cpu(d->d_blk_softlimit),
		be64_to_cpu(d->d_ino_hardlimit),
		be64_to_cpu(d->d_ino_softlimit));
	kdb_printf("bcount 0x%llx icount 0x%llx\n",
		be64_to_cpu(d->d_bcount),
		be64_to_cpu(d->d_icount));
	kdb_printf("btimer 0x%x itimer 0x%x \n",
		be32_to_cpu(d->d_btimer),
		be32_to_cpu(d->d_itimer));
}

static char *xdq_flags[] = {
	"USER",		/* XFS_DQ_USER */
	"PROJ",		/* XFS_DQ_PROJ */
	"GROUP",	/* XFS_DQ_GROUP */
	"FLKD",		/* XFS_DQ_FLOCKED */
	"DIRTY",	/* XFS_DQ_DIRTY */
	"WANT",		/* XFS_DQ_WANT */
	"INACT",	/* XFS_DQ_INACTIVE */
	"MARKER",	/* XFS_DQ_MARKER */
	NULL
};

static void
xfsidbg_xqm_dquot(xfs_dquot_t *dqp)
{
	kdb_printf("mount 0x%p hash 0x%p gdquotp 0x%p HL_next 0x%p HL_prevp 0x%p\n",
		dqp->q_mount,
		dqp->q_hash,
		dqp->q_gdquot,
		dqp->HL_NEXT,
		dqp->HL_PREVP);
	kdb_printf("MPL_next 0x%p MPL_prevp 0x%p FL_next 0x%p FL_prev 0x%p\n",
		dqp->MPL_NEXT,
		dqp->MPL_PREVP,
		dqp->dq_flnext,
		dqp->dq_flprev);

	kdb_printf("nrefs 0x%x blkno 0x%llx boffset 0x%x ", dqp->q_nrefs,
		(unsigned long long)dqp->q_blkno, dqp->q_bufoffset);
	printflags(dqp->dq_flags, xdq_flags, "flags:");
	kdb_printf("res_bcount %llu res_icount %llu res_rtbcount %llu\n",
		(unsigned long long)dqp->q_res_bcount,
		(unsigned long long)dqp->q_res_icount,
		(unsigned long long)dqp->q_res_rtbcount);
	kdb_printf("qlock 0x%p  flock 0x%p (%s) pincount 0x%x\n",
		&dqp->q_qlock,
		&dqp->q_flock,
		(valusema(&dqp->q_flock) <= 0) ? "LCK" : "UNLKD",
		dqp->q_pincount);
#ifdef XFS_DQUOT_TRACE
	qprintf("dqtrace 0x%p\n", dqp->q_trace);
#endif
	kdb_printf("disk-dquot 0x%p\n", &dqp->q_core);
	xfsidbg_xqm_diskdq(&dqp->q_core);

}


#define XQMIDBG_LIST_PRINT(l, NXT) \
{ \
	  xfs_dquot_t	*dqp;\
	  int i = 0; \
	  kdb_printf("[#%d dquots]\n", (int) (l)->qh_nelems); \
	  for (dqp = (l)->qh_next; dqp != NULL; dqp = dqp->NXT) {\
	   kdb_printf( \
	      "\t%d. [0x%p] \"%d (%s)\"\t blks = %d, inos = %d refs = %d\n", \
			 ++i, dqp, (int) be32_to_cpu(dqp->q_core.d_id), \
			 DQFLAGTO_TYPESTR(dqp),      \
			 (int) be64_to_cpu(dqp->q_core.d_bcount), \
			 (int) be64_to_cpu(dqp->q_core.d_icount), \
			 (int) dqp->q_nrefs); }\
	  kdb_printf("\n"); \
}

static void
xfsidbg_xqm_dqattached_inos(xfs_mount_t	*mp)
{
	xfs_inode_t	*ip;
	int		n = 0;

	ip = mp->m_inodes;
	do {
		if (ip->i_mount == NULL) {
			ip = ip->i_mnext;
			continue;
		}
		if (ip->i_udquot || ip->i_gdquot) {
			n++;
			kdb_printf("inode = 0x%p, ino %d: udq 0x%p, gdq 0x%p\n",
				ip, (int)ip->i_ino, ip->i_udquot, ip->i_gdquot);
		}
		ip = ip->i_mnext;
	} while (ip != mp->m_inodes);
	kdb_printf("\nNumber of inodes with dquots attached: %d\n", n);
}

#ifdef CONFIG_XFS_QUOTA
static void
xfsidbg_xqm_freelist_print(xfs_frlist_t *qlist, char *title)
{
	xfs_dquot_t *dq;
	int i = 0;
	kdb_printf("%s (#%d)\n", title, (int) qlist->qh_nelems);
	FOREACH_DQUOT_IN_FREELIST(dq, qlist) {
		kdb_printf("\t%d.\t\"%d (%s:0x%p)\"\t bcnt = %d, icnt = %d "
		       "refs = %d\n",
		       ++i, (int) be32_to_cpu(dq->q_core.d_id),
		       DQFLAGTO_TYPESTR(dq), dq,
		       (int) be64_to_cpu(dq->q_core.d_bcount),
		       (int) be64_to_cpu(dq->q_core.d_icount),
		       (int) dq->q_nrefs);
	}
}

static void
xfsidbg_xqm_freelist(void)
{
	if (xfs_Gqm) {
		xfsidbg_xqm_freelist_print(&(xfs_Gqm->qm_dqfreelist), "Freelist");
	} else
		kdb_printf("NULL XQM!!\n");
}

static void
xfsidbg_xqm_htab(void)
{
	int		i;
	xfs_dqhash_t	*h;

	if (xfs_Gqm == NULL) {
		kdb_printf("NULL XQM!!\n");
		return;
	}
	for (i = 0; i <= xfs_Gqm->qm_dqhashmask; i++) {
		h = &xfs_Gqm->qm_usr_dqhtable[i];
		if (h->qh_next) {
			kdb_printf("USR %d: ", i);
			XQMIDBG_LIST_PRINT(h, HL_NEXT);
		}
	}
	for (i = 0; i <= xfs_Gqm->qm_dqhashmask; i++) {
		h = &xfs_Gqm->qm_grp_dqhtable[i];
		if (h->qh_next) {
			kdb_printf("GRP/PRJ %d: ", i);
			XQMIDBG_LIST_PRINT(h, HL_NEXT);
		}
	}
}
#endif

#ifdef XFS_DQUOT_TRACE
static int
xfsidbg_xqm_pr_dqentry(ktrace_entry_t *ktep)
{
	if ((__psint_t)ktep->val[0] == 0)
		return 0;
	switch ((__psint_t)ktep->val[0]) {
	      case DQUOT_KTRACE_ENTRY:
		qprintf("[%ld] %s\t",
			(long)ktep->val[12], /* pid */
			(char *)ktep->val[1]);
		printflags((__psint_t)ktep->val[3], xdq_flags,"flgs ");
		qprintf("\nnrefs = %u, "
			"flags = 0x%x, "
			"id = %d, "
			"res_bc = 0x%x\n"
			"bcnt = 0x%x [0x%x | 0x%x], "
			"icnt = 0x%x [0x%x | 0x%x]\n"
			"@ %ld\n",
			(unsigned int)(long)ktep->val[2], /* nrefs */
			(unsigned int)(long)ktep->val[3], /* flags */
			(unsigned int)(long)ktep->val[11], /* ID */
			(unsigned int)(long)ktep->val[4], /* res_bc */
			(unsigned int)(long)ktep->val[5], /* bcnt */
			(unsigned int)(long)ktep->val[8], /* bsoft */
			(unsigned int)(long)ktep->val[7], /* bhard */
			(unsigned int)(long)ktep->val[6], /* icnt */
			(unsigned int)(long)ktep->val[10], /* isoft */
			(unsigned int)(long)ktep->val[9], /* ihard */
			(long) ktep->val[13] /* time */
			);
		break;

	      default:
		qprintf("unknown dqtrace record\n");
		break;
	}
	return (1);
}

void
xfsidbg_xqm_dqtrace(xfs_dquot_t *dqp)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;

	if (dqp->q_trace == NULL) {
		qprintf("The xfs dquot trace buffer is not initialized\n");
		return;
	}
	qprintf("xdqtrace dquot 0x%p\n", dqp);

	ktep = ktrace_first(dqp->q_trace, &kts);
	while (ktep != NULL) {
		if (xfsidbg_xqm_pr_dqentry(ktep))
			qprintf("---------------------------------\n");
		ktep = ktrace_next(dqp->q_trace, &kts);
	}
}
#endif

static void
xfsidbg_xqm_mplist(xfs_mount_t *mp)
{
	if (mp->m_quotainfo == NULL) {
		kdb_printf("NULL quotainfo\n");
		return;
	}

	XQMIDBG_LIST_PRINT(&(mp->m_quotainfo->qi_dqlist), MPL_NEXT);

}


static void
xfsidbg_xqm_qinfo(xfs_mount_t *mp)
{
	if (mp == NULL || mp->m_quotainfo == NULL) {
		kdb_printf("NULL quotainfo\n");
		return;
	}

	kdb_printf("uqip 0x%p, gqip 0x%p, &pinlock 0x%p &dqlist 0x%p\n",
		mp->m_quotainfo->qi_uquotaip,
		mp->m_quotainfo->qi_gquotaip,
		&mp->m_quotainfo->qi_pinlock,
		&mp->m_quotainfo->qi_dqlist);

	kdb_printf("btmlimit 0x%x, itmlimit 0x%x, RTbtmlim 0x%x\n",
		(int)mp->m_quotainfo->qi_btimelimit,
		(int)mp->m_quotainfo->qi_itimelimit,
		(int)mp->m_quotainfo->qi_rtbtimelimit);

	kdb_printf("bwarnlim 0x%x, iwarnlim 0x%x, RTbwarnlim 0x%x\n",
		(int)mp->m_quotainfo->qi_bwarnlimit,
		(int)mp->m_quotainfo->qi_iwarnlimit,
		(int)mp->m_quotainfo->qi_rtbwarnlimit);

	kdb_printf("nreclaims %d, &qofflock 0x%p, chunklen 0x%x, dqperchunk 0x%x\n",
		(int)mp->m_quotainfo->qi_dqreclaims,
		&mp->m_quotainfo->qi_quotaofflock,
		(int)mp->m_quotainfo->qi_dqchunklen,
		(int)mp->m_quotainfo->qi_dqperchunk);
}

static void
xfsidbg_xqm_tpdqinfo(xfs_trans_t *tp)
{
	xfs_dqtrx_t	*qa, *q;
	int		i,j;

	kdb_printf("dqinfo 0x%p\n", tp->t_dqinfo);
	if (! tp->t_dqinfo)
		return;
	kdb_printf("USR: \n");
	qa = tp->t_dqinfo->dqa_usrdquots;
	for (j = 0; j < 2; j++) {
		for (i = 0; i < XFS_QM_TRANS_MAXDQS; i++) {
			if (qa[i].qt_dquot == NULL)
				break;
			q = &qa[i];
			kdb_printf(
  "\"%d\"[0x%p]: bres %d, bres-used %d, bdelta %d, del-delta %d, icnt-delta %d\n",
				(int) be32_to_cpu(q->qt_dquot->q_core.d_id),
				q->qt_dquot,
				(int) q->qt_blk_res,
				(int) q->qt_blk_res_used,
				(int) q->qt_bcount_delta,
				(int) q->qt_delbcnt_delta,
				(int) q->qt_icount_delta);
		}
		if (j == 0) {
			qa = tp->t_dqinfo->dqa_grpdquots;
			kdb_printf("GRP/PRJ: \n");
		}
	}
}

/*
 * Print xfs superblock.
 */
static void
xfsidbg_xsb(xfs_sb_t *sbp)
{
	kdb_printf("magicnum 0x%x blocksize 0x%x dblocks %Ld rblocks %Ld\n",
		sbp->sb_magicnum, sbp->sb_blocksize,
		sbp->sb_dblocks, sbp->sb_rblocks);
	kdb_printf("rextents %Ld uuid %s logstart %s\n",
		sbp->sb_rextents, xfs_fmtuuid(&sbp->sb_uuid),
		xfs_fmtfsblock(sbp->sb_logstart, NULL));
	kdb_printf("rootino %s ", xfs_fmtino(sbp->sb_rootino, NULL));
	kdb_printf("rbmino %s ", xfs_fmtino(sbp->sb_rbmino, NULL));
	kdb_printf("rsumino %s\n", xfs_fmtino(sbp->sb_rsumino, NULL));
	kdb_printf("rextsize 0x%x agblocks 0x%x agcount 0x%x rbmblocks 0x%x\n",
		sbp->sb_rextsize, sbp->sb_agblocks, sbp->sb_agcount,
		sbp->sb_rbmblocks);
	kdb_printf("logblocks 0x%x versionnum 0x%x sectsize 0x%x inodesize 0x%x\n",
		sbp->sb_logblocks, sbp->sb_versionnum, sbp->sb_sectsize,
		sbp->sb_inodesize);
	kdb_printf("inopblock 0x%x blocklog 0x%x sectlog 0x%x inodelog 0x%x\n",
		sbp->sb_inopblock, sbp->sb_blocklog, sbp->sb_sectlog,
		sbp->sb_inodelog);
	kdb_printf("inopblog %d agblklog %d rextslog %d inprogress %d imax_pct %d\n",
		sbp->sb_inopblog, sbp->sb_agblklog, sbp->sb_rextslog,
		sbp->sb_inprogress, sbp->sb_imax_pct);
	kdb_printf("icount %Lx ifree %Lx fdblocks %Lx frextents %Lx\n",
		sbp->sb_icount, sbp->sb_ifree,
		sbp->sb_fdblocks, sbp->sb_frextents);
	kdb_printf("uquotino %s ", xfs_fmtino(sbp->sb_uquotino, NULL));
	kdb_printf("gquotino %s ", xfs_fmtino(sbp->sb_gquotino, NULL));
	kdb_printf("qflags 0x%x flags 0x%x shared_vn %d inoaligmt %d\n",
		sbp->sb_qflags, sbp->sb_flags, sbp->sb_shared_vn,
		sbp->sb_inoalignmt);
	kdb_printf("unit %d width %d dirblklog %d\n",
		sbp->sb_unit, sbp->sb_width, sbp->sb_dirblklog);
	kdb_printf("log sunit %d\n", sbp->sb_logsunit);
}

static void
xfsidbg_xsb_convert(xfs_sb_t *sbp)
{
	xfs_sb_t sb;

	xfs_xlatesb(sbp, &sb, 1, XFS_SB_ALL_BITS);

	kdb_printf("<converted>\n");
	xfsidbg_xsb(&sb);
}

/*
 * Print out an XFS transaction structure.  Print summaries for
 * each of the items.
 */
static void
xfsidbg_xtp(xfs_trans_t *tp)
{
	xfs_log_item_chunk_t	*licp;
	xfs_log_item_desc_t	*lidp;
	xfs_log_busy_chunk_t	*lbcp;
	int			i;
	int			chunk;
	static char *xtp_flags[] = {
		"dirty",	/* 0x1 */
		"sb_dirty",	/* 0x2 */
		"perm_log_res",	/* 0x4 */
		"sync",		/* 0x08 */
		"dq_dirty",     /* 0x10 */
		NULL
		};
	static char *lid_flags[] = {
		"dirty",	/* 0x1 */
		"pinned",	/* 0x2 */
		"sync unlock",	/* 0x4 */
		"buf stale",	/* 0x8 */
		NULL
		};

	kdb_printf("tp 0x%p type ", tp);
	xfsidbg_print_trans_type(tp->t_type);
	kdb_printf(" mount 0x%p\n", tp->t_mountp);
	kdb_printf("flags ");
	printflags(tp->t_flags, xtp_flags,"xtp");
	kdb_printf("\n");
	kdb_printf("callback 0x%p forw 0x%p back 0x%p\n",
		&tp->t_logcb, tp->t_forw, tp->t_back);
	kdb_printf("log res %d block res %d block res used %d\n",
		tp->t_log_res, tp->t_blk_res, tp->t_blk_res_used);
	kdb_printf("rt res %d rt res used %d\n", tp->t_rtx_res,
		tp->t_rtx_res_used);
	kdb_printf("ticket 0x%lx lsn %s commit_lsn %s\n",
		(unsigned long) tp->t_ticket,
		xfs_fmtlsn(&tp->t_lsn),
		xfs_fmtlsn(&tp->t_commit_lsn));
	kdb_printf("callback 0x%p callarg 0x%p\n",
		tp->t_callback, tp->t_callarg);
	kdb_printf("icount delta %ld ifree delta %ld\n",
		tp->t_icount_delta, tp->t_ifree_delta);
	kdb_printf("blocks delta %ld res blocks delta %ld\n",
		tp->t_fdblocks_delta, tp->t_res_fdblocks_delta);
	kdb_printf("rt delta %ld res rt delta %ld\n",
		tp->t_frextents_delta, tp->t_res_frextents_delta);
	kdb_printf("ag freeblks delta %ld ag flist delta %ld ag btree delta %ld\n",
		tp->t_ag_freeblks_delta, tp->t_ag_flist_delta,
		tp->t_ag_btree_delta);
	kdb_printf("dblocks delta %ld agcount delta %ld imaxpct delta %ld\n",
		tp->t_dblocks_delta, tp->t_agcount_delta, tp->t_imaxpct_delta);
	kdb_printf("rextsize delta %ld rbmblocks delta %ld\n",
		tp->t_rextsize_delta, tp->t_rbmblocks_delta);
	kdb_printf("rblocks delta %ld rextents delta %ld rextslog delta %ld\n",
		tp->t_rblocks_delta, tp->t_rextents_delta,
		tp->t_rextslog_delta);
	kdb_printf("dqinfo 0x%p\n", tp->t_dqinfo);
	kdb_printf("log items:\n");
	licp = &tp->t_items;
	chunk = 0;
	while (licp != NULL) {
		if (XFS_LIC_ARE_ALL_FREE(licp)) {
			licp = licp->lic_next;
			chunk++;
			continue;
		}
		for (i = 0; i < licp->lic_unused; i++) {
			if (XFS_LIC_ISFREE(licp, i)) {
				continue;
			}

			lidp = XFS_LIC_SLOT(licp, i);
			kdb_printf("\n");
			kdb_printf("chunk %d index %d item 0x%p size %d\n",
				chunk, i, lidp->lid_item, lidp->lid_size);
			kdb_printf("flags ");
			printflags(lidp->lid_flags, lid_flags,"lic");
			kdb_printf("\n");
			xfsidbg_xlogitem(lidp->lid_item);
		}
		chunk++;
		licp = licp->lic_next;
	}

	kdb_printf("log busy free %d, list:\n", tp->t_busy_free);
	lbcp = &tp->t_busy;
	chunk = 0;
	while (lbcp != NULL) {
		kdb_printf("Chunk %d at 0x%p next 0x%p free 0x%08x unused %d\n",
			chunk, lbcp, lbcp->lbc_next, lbcp->lbc_free,
			lbcp->lbc_unused);
		for (i = 0; i < XFS_LBC_NUM_SLOTS; i++) {
			kdb_printf("  %02d: ag %d idx %d\n",
				i,
				lbcp->lbc_busy[i].lbc_ag,
				lbcp->lbc_busy[i].lbc_idx);
		}
		lbcp = lbcp->lbc_next;
	}
}

static void
xfsidbg_xtrans_res(
	xfs_mount_t	*mp)
{
	xfs_trans_reservations_t	*xtrp;

	xtrp = &mp->m_reservations;
	kdb_printf("write: %d\ttruncate: %d\trename: %d\n",
		xtrp->tr_write, xtrp->tr_itruncate, xtrp->tr_rename);
	kdb_printf("link: %d\tremove: %d\tsymlink: %d\n",
		xtrp->tr_link, xtrp->tr_remove, xtrp->tr_symlink);
	kdb_printf("create: %d\tmkdir: %d\tifree: %d\n",
		xtrp->tr_create, xtrp->tr_mkdir, xtrp->tr_ifree);
	kdb_printf("ichange: %d\tgrowdata: %d\tswrite: %d\n",
		xtrp->tr_ichange, xtrp->tr_growdata, xtrp->tr_swrite);
	kdb_printf("addafork: %d\twriteid: %d\tattrinval: %d\n",
		xtrp->tr_addafork, xtrp->tr_writeid, xtrp->tr_attrinval);
	kdb_printf("attrset: %d\tattrrm: %d\tclearagi: %d\n",
		xtrp->tr_attrset, xtrp->tr_attrrm, xtrp->tr_clearagi);
	kdb_printf("growrtalloc: %d\tgrowrtzero: %d\tgrowrtfree: %d\n",
		xtrp->tr_growrtalloc, xtrp->tr_growrtzero, xtrp->tr_growrtfree);
}
