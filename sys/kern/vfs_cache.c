/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1989, 1993, 1995
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Poul-Henning Kamp of the FreeBSD Project.
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

#include "opt_ddb.h"
#include "opt_ktrace.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/capsicum.h>
#include <sys/counter.h>
#include <sys/filedesc.h>
#include <sys/fnv_hash.h>
#include <sys/inotify.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/fcntl.h>
#include <sys/jail.h>
#include <sys/mount.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/seqc.h>
#include <sys/sdt.h>
#include <sys/smr.h>
#include <sys/smp.h>
#include <sys/syscallsubr.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/vnode.h>
#include <ck_queue.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#ifdef INVARIANTS
#include <machine/_inttypes.h>
#endif

#include <security/audit/audit.h>
#include <security/mac/mac_framework.h>

#ifdef DDB
#include <ddb/ddb.h>
#endif

#include <vm/uma.h>

/*
 * High level overview of name caching in the VFS layer.
 *
 * Originally caching was implemented as part of UFS, later extracted to allow
 * use by other filesystems. A decision was made to make it optional and
 * completely detached from the rest of the kernel, which comes with limitations
 * outlined near the end of this comment block.
 *
 * This fundamental choice needs to be revisited. In the meantime, the current
 * state is described below. Significance of all notable routines is explained
 * in comments placed above their implementation. Scattered thoroughout the
 * file are TODO comments indicating shortcomings which can be fixed without
 * reworking everything (most of the fixes will likely be reusable). Various
 * details are omitted from this explanation to not clutter the overview, they
 * have to be checked by reading the code and associated commentary.
 *
 * Keep in mind that it's individual path components which are cached, not full
 * paths. That is, for a fully cached path "foo/bar/baz" there are 3 entries,
 * one for each name.
 *
 * I. Data organization
 *
 * Entries are described by "struct namecache" objects and stored in a hash
 * table. See cache_get_hash for more information.
 *
 * "struct vnode" contains pointers to source entries (names which can be found
 * when traversing through said vnode), destination entries (names of that
 * vnode (see "Limitations" for a breakdown on the subject) and a pointer to
 * the parent vnode.
 *
 * The (directory vnode; name) tuple reliably determines the target entry if
 * it exists.
 *
 * Since there are no small locks at this time (all are 32 bytes in size on
 * LP64), the code works around the problem by introducing lock arrays to
 * protect hash buckets and vnode lists.
 *
 * II. Filesystem integration
 *
 * Filesystems participating in name caching do the following:
 * - set vop_lookup routine to vfs_cache_lookup
 * - set vop_cachedlookup to whatever can perform the lookup if the above fails
 * - if they support lockless lookup (see below), vop_fplookup_vexec and
 *   vop_fplookup_symlink are set along with the MNTK_FPLOOKUP flag on the
 *   mount point
 * - call cache_purge or cache_vop_* routines to eliminate stale entries as
 *   applicable
 * - call cache_enter to add entries depending on the MAKEENTRY flag
 *
 * With the above in mind, there are 2 entry points when doing lookups:
 * - ... -> namei -> cache_fplookup -- this is the default
 * - ... -> VOP_LOOKUP -> vfs_cache_lookup -- normally only called by namei
 *   should the above fail
 *
 * Example code flow how an entry is added:
 * ... -> namei -> cache_fplookup -> cache_fplookup_noentry -> VOP_LOOKUP ->
 * vfs_cache_lookup -> VOP_CACHEDLOOKUP -> ufs_lookup_ino -> cache_enter
 *
 * III. Performance considerations
 *
 * For lockless case forward lookup avoids any writes to shared areas apart
 * from the terminal path component. In other words non-modifying lookups of
 * different files don't suffer any scalability problems in the namecache.
 * Looking up the same file is limited by VFS and goes beyond the scope of this
 * file.
 *
 * At least on amd64 the single-threaded bottleneck for long paths is hashing
 * (see cache_get_hash). There are cases where the code issues acquire fence
 * multiple times, they can be combined on architectures which suffer from it.
 *
 * For locked case each encountered vnode has to be referenced and locked in
 * order to be handed out to the caller (normally that's namei). This
 * introduces significant hit single-threaded and serialization multi-threaded.
 *
 * Reverse lookup (e.g., "getcwd") fully scales provided it is fully cached --
 * avoids any writes to shared areas to any components.
 *
 * Unrelated insertions are partially serialized on updating the global entry
 * counter and possibly serialized on colliding bucket or vnode locks.
 *
 * IV. Observability
 *
 * Note not everything has an explicit dtrace probe nor it should have, thus
 * some of the one-liners below depend on implementation details.
 *
 * Examples:
 *
 * # Check what lookups failed to be handled in a lockless manner. Column 1 is
 * # line number, column 2 is status code (see cache_fpl_status)
 * dtrace -n 'vfs:fplookup:lookup:done { @[arg1, arg2] = count(); }'
 *
 * # Lengths of names added by binary name
 * dtrace -n 'fbt::cache_enter_time:entry { @[execname] = quantize(args[2]->cn_namelen); }'
 *
 * # Same as above but only those which exceed 64 characters
 * dtrace -n 'fbt::cache_enter_time:entry /args[2]->cn_namelen > 64/ { @[execname] = quantize(args[2]->cn_namelen); }'
 *
 * # Who is performing lookups with spurious slashes (e.g., "foo//bar") and what
 * # path is it
 * dtrace -n 'fbt::cache_fplookup_skip_slashes:entry { @[execname, stringof(args[0]->cnp->cn_pnbuf)] = count(); }'
 *
 * V. Limitations and implementation defects
 *
 * - since it is possible there is no entry for an open file, tools like
 *   "procstat" may fail to resolve fd -> vnode -> path to anything
 * - even if a filesystem adds an entry, it may get purged (e.g., due to memory
 *   shortage) in which case the above problem applies
 * - hardlinks are not tracked, thus if a vnode is reachable in more than one
 *   way, resolving a name may return a different path than the one used to
 *   open it (even if said path is still valid)
 * - by default entries are not added for newly created files
 * - adding an entry may need to evict negative entry first, which happens in 2
 *   distinct places (evicting on lookup, adding in a later VOP) making it
 *   impossible to simply reuse it
 * - there is a simple scheme to evict negative entries as the cache is approaching
 *   its capacity, but it is very unclear if doing so is a good idea to begin with
 * - vnodes are subject to being recycled even if target inode is left in memory,
 *   which loses the name cache entries when it perhaps should not. in case of tmpfs
 *   names get duplicated -- kept by filesystem itself and namecache separately
 * - struct namecache has a fixed size and comes in 2 variants, often wasting
 *   space.  now hard to replace with malloc due to dependence on SMR, which
 *   requires UMA zones to opt in
 * - lack of better integration with the kernel also turns nullfs into a layered
 *   filesystem instead of something which can take advantage of caching
 *
 * Appendix A: where is the time lost, expanding on paragraph III
 *
 * While some care went into optimizing lookups, there is still plenty of
 * performance left on the table, most notably from single-threaded standpoint.
 * Below is a woefully incomplete list of changes which can help.  Ideas are
 * mostly sketched out, no claim is made all kinks or prerequisites are laid
 * out.
 *
 * Note there is performance lost all over VFS.
 *
 * === SMR-only lookup
 *
 * For commonly used ops like stat(2), when the terminal vnode *is* cached,
 * lockless lookup could refrain from refing/locking the found vnode and
 * instead return while within the SMR section. Then a call to, say,
 * vop_stat_smr could do the work (or fail with EAGAIN), finally the result
 * would be validated with seqc not changing. This would be faster
 * single-threaded as it dodges atomics and would provide full scalability for
 * multicore uses. This would *not* work for open(2) or other calls which need
 * the vnode to hang around for the long haul, but would work for aforementioned
 * stat(2) but also access(2), readlink(2), realpathat(2) and probably more.
 *
 * === hotpatching for sdt probes
 *
 * They result in *tons* of branches all over with rather regrettable codegen
 * at times. Removing sdt probes altogether gives over 2% boost in lookup rate.
 * Reworking the code to patch itself at runtime with asm goto would solve it.
 * asm goto is fully supported by gcc and clang.
 *
 * === copyinstr
 *
 * On all architectures it operates one byte at a time, while it could be
 * word-sized instead thanks to the Mycroft trick.
 *
 * API itself is rather pessimal for path lookup, accepting arbitrary sizes and
 * *optionally* filling in the length parameter.
 *
 * Instead a new routine (copyinpath?) could be introduced, demanding a buffer
 * size which is a multiply of the word (and never zero), with the length
 * always returned. On top of it the routine could be allowed to transform the
 * buffer in arbitrary ways, most notably writing past the found length (not to
 * be confused with writing past buffer size) -- this would allow word-sized
 * movs while checking for '\0' later.
 *
 * === detour through namei
 *
 * Currently one suffers being called from namei, which then has to check if
 * things worked out locklessly. Instead the lockless lookup could be the
 * actual entry point which calls what is currently namei as a fallback.
 *
 * === avoidable branches in cache_can_fplookup
 *
 * The cache_fast_lookup_enabled flag check could be hotpatchable (in fact if
 * this is off, none of fplookup code should execute).
 *
 * Both audit and capsicum branches can be combined into one, but it requires
 * paying off a lot of tech debt first.
 *
 * ni_startdir could be indicated with a flag in cn_flags, eliminating the
 * branch.
 *
 * === mount stacks
 *
 * Crossing a mount requires checking if perhaps something is mounted on top.
 * Instead, an additional entry could be added to struct mount with a pointer
 * to the final mount on the stack. This would be recalculated on each
 * mount/unmount.
 *
 * === root vnodes
 *
 * It could become part of the API contract to *always* have a rootvnode set in
 * mnt_rootvnode. Such vnodes are annotated with VV_ROOT and vnlru would have
 * to be modified to always skip them.
 *
 * === inactive on v_usecount reaching 0
 *
 * VOP_NEED_INACTIVE should not exist. Filesystems would indicate need for such
 * processing with a bit in usecount.
 *
 * === v_holdcnt
 *
 * Hold count should probably get eliminated, but one can argue it is a useful
 * feature. Even if so, handling of v_usecount could be decoupled from it --
 * vnlru et al would consider the vnode not-freeable if has either hold or
 * usecount on it.
 *
 * This would eliminate 2 atomics.
 */

static SYSCTL_NODE(_vfs, OID_AUTO, cache, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Name cache");

SDT_PROVIDER_DECLARE(vfs);
SDT_PROBE_DEFINE3(vfs, namecache, enter, done, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE3(vfs, namecache, enter, duplicate, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, enter_negative, done, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE2(vfs, namecache, fullpath_smr, hit, "struct vnode *",
    "const char *");
SDT_PROBE_DEFINE4(vfs, namecache, fullpath_smr, miss, "struct vnode *",
    "struct namecache *", "int", "int");
SDT_PROBE_DEFINE1(vfs, namecache, fullpath, entry, "struct vnode *");
SDT_PROBE_DEFINE3(vfs, namecache, fullpath, hit, "struct vnode *",
    "char *", "struct vnode *");
SDT_PROBE_DEFINE1(vfs, namecache, fullpath, miss, "struct vnode *");
SDT_PROBE_DEFINE3(vfs, namecache, fullpath, return, "int",
    "struct vnode *", "char *");
SDT_PROBE_DEFINE3(vfs, namecache, lookup, hit, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, lookup, hit__negative,
    "struct vnode *", "char *");
SDT_PROBE_DEFINE2(vfs, namecache, lookup, miss, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE2(vfs, namecache, removecnp, hit, "struct vnode *",
    "struct componentname *");
SDT_PROBE_DEFINE2(vfs, namecache, removecnp, miss, "struct vnode *",
    "struct componentname *");
SDT_PROBE_DEFINE3(vfs, namecache, purge, done, "struct vnode *", "size_t", "size_t");
SDT_PROBE_DEFINE1(vfs, namecache, purge, batch, "int");
SDT_PROBE_DEFINE1(vfs, namecache, purge_negative, done, "struct vnode *");
SDT_PROBE_DEFINE1(vfs, namecache, purgevfs, done, "struct mount *");
SDT_PROBE_DEFINE3(vfs, namecache, zap, done, "struct vnode *", "char *",
    "struct vnode *");
SDT_PROBE_DEFINE2(vfs, namecache, zap_negative, done, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE2(vfs, namecache, evict_negative, done, "struct vnode *",
    "char *");
SDT_PROBE_DEFINE1(vfs, namecache, symlink, alloc__fail, "size_t");

SDT_PROBE_DEFINE3(vfs, fplookup, lookup, done, "struct nameidata *", "int",
    "enum cache_fpl_status");
SDT_PROBE_DECLARE(vfs, namei, lookup, entry);
SDT_PROBE_DECLARE(vfs, namei, lookup, return);

static char __read_frequently cache_fast_lookup_enabled = true;

/*
 * This structure describes the elements in the cache of recent
 * names looked up by namei.
 */
struct negstate {
	u_char neg_flag;
	u_char neg_hit;
};
_Static_assert(sizeof(struct negstate) <= sizeof(struct vnode *),
    "the state must fit in a union with a pointer without growing it");

struct	namecache {
	LIST_ENTRY(namecache) nc_src;	/* source vnode list */
	TAILQ_ENTRY(namecache) nc_dst;	/* destination vnode list */
	CK_SLIST_ENTRY(namecache) nc_hash;/* hash chain */
	struct	vnode *nc_dvp;		/* vnode of parent of name */
	union {
		struct	vnode *nu_vp;	/* vnode the name refers to */
		struct	negstate nu_neg;/* negative entry state */
	} n_un;
	u_char	nc_flag;		/* flag bits */
	u_char	nc_nlen;		/* length of name */
	char	nc_name[];		/* segment name + nul */
};

/*
 * struct namecache_ts repeats struct namecache layout up to the
 * nc_nlen member.
 * struct namecache_ts is used in place of struct namecache when time(s) need
 * to be stored.  The nc_dotdottime field is used when a cache entry is mapping
 * both a non-dotdot directory name plus dotdot for the directory's
 * parent.
 *
 * See below for alignment requirement.
 */
struct	namecache_ts {
	struct	timespec nc_time;	/* timespec provided by fs */
	struct	timespec nc_dotdottime;	/* dotdot timespec provided by fs */
	int	nc_ticks;		/* ticks value when entry was added */
	int	nc_pad;
	struct namecache nc_nc;
};

TAILQ_HEAD(cache_freebatch, namecache);

/*
 * At least mips n32 performs 64-bit accesses to timespec as found
 * in namecache_ts and requires them to be aligned. Since others
 * may be in the same spot suffer a little bit and enforce the
 * alignment for everyone. Note this is a nop for 64-bit platforms.
 */
#define CACHE_ZONE_ALIGNMENT	UMA_ALIGNOF(time_t)

/*
 * TODO: the initial value of CACHE_PATH_CUTOFF was inherited from the
 * 4.4 BSD codebase. Later on struct namecache was tweaked to become
 * smaller and the value was bumped to retain the total size, but it
 * was never re-evaluated for suitability. A simple test counting
 * lengths during package building shows that the value of 45 covers
 * about 86% of all added entries, reaching 99% at 65.
 *
 * Regardless of the above, use of dedicated zones instead of malloc may be
 * inducing additional waste. This may be hard to address as said zones are
 * tied to VFS SMR. Even if retaining them, the current split should be
 * re-evaluated.
 */
#ifdef __LP64__
#define	CACHE_PATH_CUTOFF	45
#define	CACHE_LARGE_PAD		6
#else
#define	CACHE_PATH_CUTOFF	41
#define	CACHE_LARGE_PAD		2
#endif

#define CACHE_ZONE_SMALL_SIZE		(offsetof(struct namecache, nc_name) + CACHE_PATH_CUTOFF + 1)
#define CACHE_ZONE_SMALL_TS_SIZE	(offsetof(struct namecache_ts, nc_nc) + CACHE_ZONE_SMALL_SIZE)
#define CACHE_ZONE_LARGE_SIZE		(offsetof(struct namecache, nc_name) + NAME_MAX + 1 + CACHE_LARGE_PAD)
#define CACHE_ZONE_LARGE_TS_SIZE	(offsetof(struct namecache_ts, nc_nc) + CACHE_ZONE_LARGE_SIZE)

_Static_assert((CACHE_ZONE_SMALL_SIZE % (CACHE_ZONE_ALIGNMENT + 1)) == 0, "bad zone size");
_Static_assert((CACHE_ZONE_SMALL_TS_SIZE % (CACHE_ZONE_ALIGNMENT + 1)) == 0, "bad zone size");
_Static_assert((CACHE_ZONE_LARGE_SIZE % (CACHE_ZONE_ALIGNMENT + 1)) == 0, "bad zone size");
_Static_assert((CACHE_ZONE_LARGE_TS_SIZE % (CACHE_ZONE_ALIGNMENT + 1)) == 0, "bad zone size");

#define	nc_vp		n_un.nu_vp
#define	nc_neg		n_un.nu_neg

/*
 * Flags in namecache.nc_flag
 */
#define NCF_WHITE	0x01
#define NCF_ISDOTDOT	0x02
#define	NCF_TS		0x04
#define	NCF_DTS		0x08
#define	NCF_DVDROP	0x10
#define	NCF_NEGATIVE	0x20
#define	NCF_INVALID	0x40
#define	NCF_WIP		0x80

/*
 * Flags in negstate.neg_flag
 */
#define NEG_HOT		0x01

static bool	cache_neg_evict_cond(u_long lnumcache);

/*
 * Mark an entry as invalid.
 *
 * This is called before it starts getting deconstructed.
 */
static void
cache_ncp_invalidate(struct namecache *ncp)
{

	KASSERT((ncp->nc_flag & NCF_INVALID) == 0,
	    ("%s: entry %p already invalid", __func__, ncp));
	atomic_store_char(&ncp->nc_flag, ncp->nc_flag | NCF_INVALID);
	atomic_thread_fence_rel();
}

/*
 * Does this entry match the given directory and name?
 */
static bool
cache_ncp_match(struct namecache *ncp, struct vnode *dvp,
    struct componentname *cnp)
{
	return (ncp->nc_dvp == dvp &&
	    ncp->nc_nlen == cnp->cn_namelen &&
	    bcmp(ncp->nc_name, cnp->cn_nameptr, cnp->cn_namelen) == 0);
}

/*
 * Check whether the entry can be safely used.
 *
 * All places which elide locks are supposed to call this after they are
 * done with reading from an entry.
 */
#define cache_ncp_canuse(ncp)	({					\
	struct namecache *_ncp = (ncp);					\
	u_char _nc_flag;						\
									\
	atomic_thread_fence_acq();					\
	_nc_flag = atomic_load_char(&_ncp->nc_flag);			\
	__predict_true((_nc_flag & (NCF_INVALID | NCF_WIP)) == 0);	\
})

/*
 * Like the above but also checks NCF_WHITE.
 */
#define cache_fpl_neg_ncp_canuse(ncp)	({				\
	struct namecache *_ncp = (ncp);					\
	u_char _nc_flag;						\
									\
	atomic_thread_fence_acq();					\
	_nc_flag = atomic_load_char(&_ncp->nc_flag);			\
	__predict_true((_nc_flag & (NCF_INVALID | NCF_WIP | NCF_WHITE)) == 0);	\
})

VFS_SMR_DECLARE;

static SYSCTL_NODE(_vfs_cache, OID_AUTO, param, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Name cache parameters");

static u_int __read_mostly	ncsize; /* the size as computed on creation or resizing */
SYSCTL_UINT(_vfs_cache_param, OID_AUTO, size, CTLFLAG_RD, &ncsize, 0,
    "Total namecache capacity");

u_int ncsizefactor = 2;
SYSCTL_UINT(_vfs_cache_param, OID_AUTO, sizefactor, CTLFLAG_RW, &ncsizefactor, 0,
    "Size factor for namecache");

static u_long __read_mostly	ncnegfactor = 5; /* ratio of negative entries */
SYSCTL_ULONG(_vfs_cache_param, OID_AUTO, negfactor, CTLFLAG_RW, &ncnegfactor, 0,
    "Ratio of negative namecache entries");

/*
 * Negative entry % of namecache capacity above which automatic eviction is allowed.
 *
 * Check cache_neg_evict_cond for details.
 */
static u_int ncnegminpct = 3;

static u_int __read_mostly     neg_min; /* the above recomputed against ncsize */
SYSCTL_UINT(_vfs_cache_param, OID_AUTO, negmin, CTLFLAG_RD, &neg_min, 0,
    "Negative entry count above which automatic eviction is allowed");

/*
 * Structures associated with name caching.
 */
#define NCHHASH(hash) \
	(&nchashtbl[(hash) & nchash])
static __read_mostly CK_SLIST_HEAD(nchashhead, namecache) *nchashtbl;/* Hash Table */
static u_long __read_mostly	nchash;			/* size of hash table */
SYSCTL_ULONG(_debug, OID_AUTO, nchash, CTLFLAG_RD, &nchash, 0,
    "Size of namecache hash table");
static u_long __exclusive_cache_line	numneg;	/* number of negative entries allocated */
static u_long __exclusive_cache_line	numcache;/* number of cache entries allocated */

struct nchstats	nchstats;		/* cache effectiveness statistics */

static u_int __exclusive_cache_line neg_cycle;

#define ncneghash	3
#define	numneglists	(ncneghash + 1)

struct neglist {
	struct mtx		nl_evict_lock;
	struct mtx		nl_lock __aligned(CACHE_LINE_SIZE);
	TAILQ_HEAD(, namecache) nl_list;
	TAILQ_HEAD(, namecache) nl_hotlist;
	u_long			nl_hotnum;
} __aligned(CACHE_LINE_SIZE);

static struct neglist neglists[numneglists];

static inline struct neglist *
NCP2NEGLIST(struct namecache *ncp)
{

	return (&neglists[(((uintptr_t)(ncp) >> 8) & ncneghash)]);
}

static inline struct negstate *
NCP2NEGSTATE(struct namecache *ncp)
{

	MPASS(atomic_load_char(&ncp->nc_flag) & NCF_NEGATIVE);
	return (&ncp->nc_neg);
}

#define	numbucketlocks (ncbuckethash + 1)
static u_int __read_mostly  ncbuckethash;
static struct mtx_padalign __read_mostly  *bucketlocks;
#define	HASH2BUCKETLOCK(hash) \
	((struct mtx *)(&bucketlocks[((hash) & ncbuckethash)]))

#define	numvnodelocks (ncvnodehash + 1)
static u_int __read_mostly  ncvnodehash;
static struct mtx __read_mostly *vnodelocks;
static inline struct mtx *
VP2VNODELOCK(struct vnode *vp)
{

	return (&vnodelocks[(((uintptr_t)(vp) >> 8) & ncvnodehash)]);
}

/*
 * Search the hash table for a namecache entry.  Either the corresponding bucket
 * must be locked, or the caller must be in an SMR read section.
 */
static struct namecache *
cache_ncp_find(struct vnode *dvp, struct componentname *cnp, uint32_t hash)
{
	struct namecache *ncp;

	KASSERT(mtx_owned(HASH2BUCKETLOCK(hash)) || VFS_SMR_ENTERED(),
	    ("%s: hash %u not locked", __func__, hash));
	CK_SLIST_FOREACH(ncp, NCHHASH(hash), nc_hash) {
		if (cache_ncp_match(ncp, dvp, cnp))
			break;
	}
	return (ncp);
}

static void
cache_out_ts(struct namecache *ncp, struct timespec *tsp, int *ticksp)
{
	struct namecache_ts *ncp_ts;

	KASSERT((ncp->nc_flag & NCF_TS) != 0 ||
	    (tsp == NULL && ticksp == NULL),
	    ("No NCF_TS"));

	if (tsp == NULL)
		return;

	ncp_ts = __containerof(ncp, struct namecache_ts, nc_nc);
	*tsp = ncp_ts->nc_time;
	*ticksp = ncp_ts->nc_ticks;
}

#ifdef DEBUG_CACHE
static int __read_mostly	doingcache = 1;	/* 1 => enable the cache */
SYSCTL_INT(_debug, OID_AUTO, vfscache, CTLFLAG_RW, &doingcache, 0,
    "VFS namecache enabled");
#endif

/* Export size information to userland */
SYSCTL_SIZEOF_STRUCT(namecache);

/*
 * The new name cache statistics
 */
static SYSCTL_NODE(_vfs_cache, OID_AUTO, stats, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Name cache statistics");

#define STATNODE_ULONG(name, varname, descr)					\
	SYSCTL_ULONG(_vfs_cache_stats, OID_AUTO, name, CTLFLAG_RD, &varname, 0, descr);
#define STATNODE_COUNTER(name, varname, descr)					\
	static COUNTER_U64_DEFINE_EARLY(varname);				\
	SYSCTL_COUNTER_U64(_vfs_cache_stats, OID_AUTO, name, CTLFLAG_RD, &varname, \
	    descr);
STATNODE_ULONG(neg, numneg, "Number of negative cache entries");
STATNODE_ULONG(count, numcache, "Number of cache entries");
STATNODE_COUNTER(heldvnodes, numcachehv, "Number of namecache entries with vnodes held");
STATNODE_COUNTER(drops, numdrops, "Number of dropped entries due to reaching the limit");
STATNODE_COUNTER(miss, nummiss, "Number of cache misses");
STATNODE_COUNTER(misszap, nummisszap, "Number of cache misses we do not want to cache");
STATNODE_COUNTER(poszaps, numposzaps,
    "Number of cache hits (positive) we do not want to cache");
STATNODE_COUNTER(poshits, numposhits, "Number of cache hits (positive)");
STATNODE_COUNTER(negzaps, numnegzaps,
    "Number of cache hits (negative) we do not want to cache");
STATNODE_COUNTER(neghits, numneghits, "Number of cache hits (negative)");
/* These count for vn_getcwd(), too. */
STATNODE_COUNTER(fullpathcalls, numfullpathcalls, "Number of fullpath search calls");
STATNODE_COUNTER(fullpathfail2, numfullpathfail2,
    "Number of fullpath search errors (VOP_VPTOCNP failures)");
STATNODE_COUNTER(fullpathfail4, numfullpathfail4, "Number of fullpath search errors (ENOMEM)");
STATNODE_COUNTER(fullpathfound, numfullpathfound, "Number of successful fullpath calls");
STATNODE_COUNTER(symlinktoobig, symlinktoobig, "Number of times symlink did not fit the cache");

/*
 * Debug or developer statistics.
 */
static SYSCTL_NODE(_vfs_cache, OID_AUTO, debug, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Name cache debugging");
#define DEBUGNODE_ULONG(name, varname, descr)					\
	SYSCTL_ULONG(_vfs_cache_debug, OID_AUTO, name, CTLFLAG_RD, &varname, 0, descr);
static u_long zap_bucket_relock_success;
DEBUGNODE_ULONG(zap_bucket_relock_success, zap_bucket_relock_success,
    "Number of successful removals after relocking");
static u_long zap_bucket_fail;
DEBUGNODE_ULONG(zap_bucket_fail, zap_bucket_fail, "");
static u_long zap_bucket_fail2;
DEBUGNODE_ULONG(zap_bucket_fail2, zap_bucket_fail2, "");
static u_long cache_lock_vnodes_cel_3_failures;
DEBUGNODE_ULONG(vnodes_cel_3_failures, cache_lock_vnodes_cel_3_failures,
    "Number of times 3-way vnode locking failed");

static void cache_zap_locked(struct namecache *ncp);
static int vn_fullpath_any_smr(struct vnode *vp, struct vnode *rdir, char *buf,
    char **retbuf, size_t *buflen, size_t addend);
static int vn_fullpath_any(struct vnode *vp, struct vnode *rdir, char *buf,
    char **retbuf, size_t *buflen);
static int vn_fullpath_dir(struct vnode *vp, struct vnode *rdir, char *buf,
    char **retbuf, size_t *len, size_t addend);

static MALLOC_DEFINE(M_VFSCACHE, "vfscache", "VFS name cache entries");

static inline void
cache_assert_vlp_locked(struct mtx *vlp)
{

	if (vlp != NULL)
		mtx_assert(vlp, MA_OWNED);
}

static inline void
cache_assert_vnode_locked(struct vnode *vp)
{
	struct mtx *vlp;

	vlp = VP2VNODELOCK(vp);
	cache_assert_vlp_locked(vlp);
}

/*
 * Directory vnodes with entries are held for two reasons:
 * 1. make them less of a target for reclamation in vnlru
 * 2. suffer smaller performance penalty in locked lookup as requeieing is avoided
 *
 * It will be feasible to stop doing it altogether if all filesystems start
 * supporting lockless lookup.
 */
static void
cache_hold_vnode(struct vnode *vp)
{

	cache_assert_vnode_locked(vp);
	VNPASS(LIST_EMPTY(&vp->v_cache_src), vp);
	vhold(vp);
	counter_u64_add(numcachehv, 1);
}

static void
cache_drop_vnode(struct vnode *vp)
{

	/*
	 * Called after all locks are dropped, meaning we can't assert
	 * on the state of v_cache_src.
	 */
	vdrop(vp);
	counter_u64_add(numcachehv, -1);
}

/*
 * UMA zones.
 */
static uma_zone_t __read_mostly cache_zone_small;
static uma_zone_t __read_mostly cache_zone_small_ts;
static uma_zone_t __read_mostly cache_zone_large;
static uma_zone_t __read_mostly cache_zone_large_ts;

char *
cache_symlink_alloc(size_t size, int flags)
{

	if (size < CACHE_ZONE_SMALL_SIZE) {
		return (uma_zalloc_smr(cache_zone_small, flags));
	}
	if (size < CACHE_ZONE_LARGE_SIZE) {
		return (uma_zalloc_smr(cache_zone_large, flags));
	}
	counter_u64_add(symlinktoobig, 1);
	SDT_PROBE1(vfs, namecache, symlink, alloc__fail, size);
	return (NULL);
}

void
cache_symlink_free(char *string, size_t size)
{

	MPASS(string != NULL);
	KASSERT(size < CACHE_ZONE_LARGE_SIZE,
	    ("%s: size %zu too big", __func__, size));

	if (size < CACHE_ZONE_SMALL_SIZE) {
		uma_zfree_smr(cache_zone_small, string);
		return;
	}
	if (size < CACHE_ZONE_LARGE_SIZE) {
		uma_zfree_smr(cache_zone_large, string);
		return;
	}
	__assert_unreachable();
}

static struct namecache *
cache_alloc_uma(int len, bool ts)
{
	struct namecache_ts *ncp_ts;
	struct namecache *ncp;

	if (__predict_false(ts)) {
		if (len <= CACHE_PATH_CUTOFF)
			ncp_ts = uma_zalloc_smr(cache_zone_small_ts, M_WAITOK);
		else
			ncp_ts = uma_zalloc_smr(cache_zone_large_ts, M_WAITOK);
		ncp = &ncp_ts->nc_nc;
	} else {
		if (len <= CACHE_PATH_CUTOFF)
			ncp = uma_zalloc_smr(cache_zone_small, M_WAITOK);
		else
			ncp = uma_zalloc_smr(cache_zone_large, M_WAITOK);
	}
	return (ncp);
}

static void
cache_free_uma(struct namecache *ncp)
{
	struct namecache_ts *ncp_ts;

	if (__predict_false(ncp->nc_flag & NCF_TS)) {
		ncp_ts = __containerof(ncp, struct namecache_ts, nc_nc);
		if (ncp->nc_nlen <= CACHE_PATH_CUTOFF)
			uma_zfree_smr(cache_zone_small_ts, ncp_ts);
		else
			uma_zfree_smr(cache_zone_large_ts, ncp_ts);
	} else {
		if (ncp->nc_nlen <= CACHE_PATH_CUTOFF)
			uma_zfree_smr(cache_zone_small, ncp);
		else
			uma_zfree_smr(cache_zone_large, ncp);
	}
}

static struct namecache *
cache_alloc(int len, bool ts)
{
	u_long lnumcache;

	/*
	 * Avoid blowout in namecache entries.
	 *
	 * Bugs:
	 * 1. filesystems may end up trying to add an already existing entry
	 * (for example this can happen after a cache miss during concurrent
	 * lookup), in which case we will call cache_neg_evict despite not
	 * adding anything.
	 * 2. the routine may fail to free anything and no provisions are made
	 * to make it try harder (see the inside for failure modes)
	 * 3. it only ever looks at negative entries.
	 */
	lnumcache = atomic_fetchadd_long(&numcache, 1) + 1;
	if (cache_neg_evict_cond(lnumcache)) {
		lnumcache = atomic_load_long(&numcache);
	}
	if (__predict_false(lnumcache >= ncsize)) {
		atomic_subtract_long(&numcache, 1);
		counter_u64_add(numdrops, 1);
		return (NULL);
	}
	return (cache_alloc_uma(len, ts));
}

static void
cache_free(struct namecache *ncp)
{

	MPASS(ncp != NULL);
	if ((ncp->nc_flag & NCF_DVDROP) != 0) {
		cache_drop_vnode(ncp->nc_dvp);
	}
	cache_free_uma(ncp);
	atomic_subtract_long(&numcache, 1);
}

static void
cache_free_batch(struct cache_freebatch *batch)
{
	struct namecache *ncp, *nnp;
	int i;

	i = 0;
	if (TAILQ_EMPTY(batch))
		goto out;
	TAILQ_FOREACH_SAFE(ncp, batch, nc_dst, nnp) {
		if ((ncp->nc_flag & NCF_DVDROP) != 0) {
			cache_drop_vnode(ncp->nc_dvp);
		}
		cache_free_uma(ncp);
		i++;
	}
	atomic_subtract_long(&numcache, i);
out:
	SDT_PROBE1(vfs, namecache, purge, batch, i);
}

/*
 * Hashing.
 *
 * The code was made to use FNV in 2001 and this choice needs to be revisited.
 *
 * Short summary of the difficulty:
 * The longest name which can be inserted is NAME_MAX characters in length (or
 * 255 at the time of writing this comment), while majority of names used in
 * practice are significantly shorter (mostly below 10). More importantly
 * majority of lookups performed find names are even shorter than that.
 *
 * This poses a problem where hashes which do better than FNV past word size
 * (or so) tend to come with additional overhead when finalizing the result,
 * making them noticeably slower for the most commonly used range.
 *
 * Consider a path like: /usr/obj/usr/src/sys/amd64/GENERIC/vnode_if.c
 *
 * When looking it up the most time consuming part by a large margin (at least
 * on amd64) is hashing.  Replacing FNV with something which pessimizes short
 * input would make the slowest part stand out even more.
 */

/*
 * TODO: With the value stored we can do better than computing the hash based
 * on the address.
 */
static void
cache_prehash(struct vnode *vp)
{

	vp->v_nchash = fnv_32_buf(&vp, sizeof(vp), FNV1_32_INIT);
}

static uint32_t
cache_get_hash(char *name, u_char len, struct vnode *dvp)
{

	return (fnv_32_buf(name, len, dvp->v_nchash));
}

static uint32_t
cache_get_hash_iter_start(struct vnode *dvp)
{

	return (dvp->v_nchash);
}

static uint32_t
cache_get_hash_iter(char c, uint32_t hash)
{

	return (fnv_32_buf(&c, 1, hash));
}

static uint32_t
cache_get_hash_iter_finish(uint32_t hash)
{

	return (hash);
}

static inline struct nchashhead *
NCP2BUCKET(struct namecache *ncp)
{
	uint32_t hash;

	hash = cache_get_hash(ncp->nc_name, ncp->nc_nlen, ncp->nc_dvp);
	return (NCHHASH(hash));
}

static inline struct mtx *
NCP2BUCKETLOCK(struct namecache *ncp)
{
	uint32_t hash;

	hash = cache_get_hash(ncp->nc_name, ncp->nc_nlen, ncp->nc_dvp);
	return (HASH2BUCKETLOCK(hash));
}

#ifdef INVARIANTS
static void
cache_assert_bucket_locked(struct namecache *ncp)
{
	struct mtx *blp;

	blp = NCP2BUCKETLOCK(ncp);
	mtx_assert(blp, MA_OWNED);
}

static void
cache_assert_bucket_unlocked(struct namecache *ncp)
{
	struct mtx *blp;

	blp = NCP2BUCKETLOCK(ncp);
	mtx_assert(blp, MA_NOTOWNED);
}
#else
#define cache_assert_bucket_locked(x) do { } while (0)
#define cache_assert_bucket_unlocked(x) do { } while (0)
#endif

#define cache_sort_vnodes(x, y)	_cache_sort_vnodes((void **)(x), (void **)(y))
static void
_cache_sort_vnodes(void **p1, void **p2)
{
	void *tmp;

	MPASS(*p1 != NULL || *p2 != NULL);

	if (*p1 > *p2) {
		tmp = *p2;
		*p2 = *p1;
		*p1 = tmp;
	}
}

static void
cache_lock_all_buckets(void)
{
	u_int i;

	for (i = 0; i < numbucketlocks; i++)
		mtx_lock(&bucketlocks[i]);
}

static void
cache_unlock_all_buckets(void)
{
	u_int i;

	for (i = 0; i < numbucketlocks; i++)
		mtx_unlock(&bucketlocks[i]);
}

static void
cache_lock_all_vnodes(void)
{
	u_int i;

	for (i = 0; i < numvnodelocks; i++)
		mtx_lock(&vnodelocks[i]);
}

static void
cache_unlock_all_vnodes(void)
{
	u_int i;

	for (i = 0; i < numvnodelocks; i++)
		mtx_unlock(&vnodelocks[i]);
}

static int
cache_trylock_vnodes(struct mtx *vlp1, struct mtx *vlp2)
{

	cache_sort_vnodes(&vlp1, &vlp2);

	if (vlp1 != NULL) {
		if (!mtx_trylock(vlp1))
			return (EAGAIN);
	}
	if (!mtx_trylock(vlp2)) {
		if (vlp1 != NULL)
			mtx_unlock(vlp1);
		return (EAGAIN);
	}

	return (0);
}

static void
cache_lock_vnodes(struct mtx *vlp1, struct mtx *vlp2)
{

	MPASS(vlp1 != NULL || vlp2 != NULL);
	MPASS(vlp1 <= vlp2);

	if (vlp1 != NULL)
		mtx_lock(vlp1);
	if (vlp2 != NULL)
		mtx_lock(vlp2);
}

static void
cache_unlock_vnodes(struct mtx *vlp1, struct mtx *vlp2)
{

	MPASS(vlp1 != NULL || vlp2 != NULL);

	if (vlp1 != NULL)
		mtx_unlock(vlp1);
	if (vlp2 != NULL)
		mtx_unlock(vlp2);
}

static int
sysctl_nchstats(SYSCTL_HANDLER_ARGS)
{
	struct nchstats snap;

	if (req->oldptr == NULL)
		return (SYSCTL_OUT(req, 0, sizeof(snap)));

	snap = nchstats;
	snap.ncs_goodhits = counter_u64_fetch(numposhits);
	snap.ncs_neghits = counter_u64_fetch(numneghits);
	snap.ncs_badhits = counter_u64_fetch(numposzaps) +
	    counter_u64_fetch(numnegzaps);
	snap.ncs_miss = counter_u64_fetch(nummisszap) +
	    counter_u64_fetch(nummiss);

	return (SYSCTL_OUT(req, &snap, sizeof(snap)));
}
SYSCTL_PROC(_vfs_cache, OID_AUTO, nchstats, CTLTYPE_OPAQUE | CTLFLAG_RD |
    CTLFLAG_MPSAFE, 0, 0, sysctl_nchstats, "LU",
    "VFS cache effectiveness statistics");

static int
sysctl_hitpct(SYSCTL_HANDLER_ARGS)
{
	long poshits, neghits, miss, total;
	long pct;

	poshits = counter_u64_fetch(numposhits);
	neghits = counter_u64_fetch(numneghits);
	miss = counter_u64_fetch(nummiss);
	total = poshits + neghits + miss;

	pct = 0;
	if (total != 0)
		pct = ((poshits + neghits) * 100) / total;
	return (sysctl_handle_int(oidp, 0, pct, req));
}
SYSCTL_PROC(_vfs_cache_stats, OID_AUTO, hitpct,
    CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RD, NULL, 0, sysctl_hitpct,
    "I", "Percentage of hits");

static void
cache_recalc_neg_min(void)
{

	neg_min = (ncsize * ncnegminpct) / 100;
}

static int
sysctl_negminpct(SYSCTL_HANDLER_ARGS)
{
	u_int val;
	int error;

	val = ncnegminpct;
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);

	if (val == ncnegminpct)
		return (0);
	if (val < 0 || val > 99)
		return (EINVAL);
	ncnegminpct = val;
	cache_recalc_neg_min();
	return (0);
}

SYSCTL_PROC(_vfs_cache_param, OID_AUTO, negminpct,
    CTLTYPE_INT | CTLFLAG_MPSAFE | CTLFLAG_RW, NULL, 0, sysctl_negminpct,
    "I", "Negative entry \% of namecache capacity above which automatic eviction is allowed");

#ifdef DEBUG_CACHE
/*
 * Grab an atomic snapshot of the name cache hash chain lengths
 */
static SYSCTL_NODE(_debug, OID_AUTO, hashstat,
    CTLFLAG_RW | CTLFLAG_MPSAFE, NULL,
    "hash table stats");

static int
sysctl_debug_hashstat_rawnchash(SYSCTL_HANDLER_ARGS)
{
	struct nchashhead *ncpp;
	struct namecache *ncp;
	int i, error, n_nchash, *cntbuf;

retry:
	n_nchash = nchash + 1;	/* nchash is max index, not count */
	if (req->oldptr == NULL)
		return SYSCTL_OUT(req, 0, n_nchash * sizeof(int));
	cntbuf = malloc(n_nchash * sizeof(int), M_TEMP, M_ZERO | M_WAITOK);
	cache_lock_all_buckets();
	if (n_nchash != nchash + 1) {
		cache_unlock_all_buckets();
		free(cntbuf, M_TEMP);
		goto retry;
	}
	/* Scan hash tables counting entries */
	for (ncpp = nchashtbl, i = 0; i < n_nchash; ncpp++, i++)
		CK_SLIST_FOREACH(ncp, ncpp, nc_hash)
			cntbuf[i]++;
	cache_unlock_all_buckets();
	for (error = 0, i = 0; i < n_nchash; i++)
		if ((error = SYSCTL_OUT(req, &cntbuf[i], sizeof(int))) != 0)
			break;
	free(cntbuf, M_TEMP);
	return (error);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, rawnchash, CTLTYPE_INT|CTLFLAG_RD|
    CTLFLAG_MPSAFE, 0, 0, sysctl_debug_hashstat_rawnchash, "S,int",
    "nchash chain lengths");

static int
sysctl_debug_hashstat_nchash(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct nchashhead *ncpp;
	struct namecache *ncp;
	int n_nchash;
	int count, maxlength, used, pct;

	if (!req->oldptr)
		return SYSCTL_OUT(req, 0, 4 * sizeof(int));

	cache_lock_all_buckets();
	n_nchash = nchash + 1;	/* nchash is max index, not count */
	used = 0;
	maxlength = 0;

	/* Scan hash tables for applicable entries */
	for (ncpp = nchashtbl; n_nchash > 0; n_nchash--, ncpp++) {
		count = 0;
		CK_SLIST_FOREACH(ncp, ncpp, nc_hash) {
			count++;
		}
		if (count)
			used++;
		if (maxlength < count)
			maxlength = count;
	}
	n_nchash = nchash + 1;
	cache_unlock_all_buckets();
	pct = (used * 100) / (n_nchash / 100);
	error = SYSCTL_OUT(req, &n_nchash, sizeof(n_nchash));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &used, sizeof(used));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &maxlength, sizeof(maxlength));
	if (error)
		return (error);
	error = SYSCTL_OUT(req, &pct, sizeof(pct));
	if (error)
		return (error);
	return (0);
}
SYSCTL_PROC(_debug_hashstat, OID_AUTO, nchash, CTLTYPE_INT|CTLFLAG_RD|
    CTLFLAG_MPSAFE, 0, 0, sysctl_debug_hashstat_nchash, "I",
    "nchash statistics (number of total/used buckets, maximum chain length, usage percentage)");
#endif

/*
 * Negative entries management
 *
 * Various workloads create plenty of negative entries and barely use them
 * afterwards. Moreover malicious users can keep performing bogus lookups
 * adding even more entries. For example "make tinderbox" as of writing this
 * comment ends up with 2.6M namecache entries in total, 1.2M of which are
 * negative.
 *
 * As such, a rather aggressive eviction method is needed. The currently
 * employed method is a placeholder.
 *
 * Entries are split over numneglists separate lists, each of which is further
 * split into hot and cold entries. Entries get promoted after getting a hit.
 * Eviction happens on addition of new entry.
 */
static SYSCTL_NODE(_vfs_cache, OID_AUTO, neg, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "Name cache negative entry statistics");

SYSCTL_ULONG(_vfs_cache_neg, OID_AUTO, count, CTLFLAG_RD, &numneg, 0,
    "Number of negative cache entries");

static COUNTER_U64_DEFINE_EARLY(neg_created);
SYSCTL_COUNTER_U64(_vfs_cache_neg, OID_AUTO, created, CTLFLAG_RD, &neg_created,
    "Number of created negative entries");

static COUNTER_U64_DEFINE_EARLY(neg_evicted);
SYSCTL_COUNTER_U64(_vfs_cache_neg, OID_AUTO, evicted, CTLFLAG_RD, &neg_evicted,
    "Number of evicted negative entries");

static COUNTER_U64_DEFINE_EARLY(neg_evict_skipped_empty);
SYSCTL_COUNTER_U64(_vfs_cache_neg, OID_AUTO, evict_skipped_empty, CTLFLAG_RD,
    &neg_evict_skipped_empty,
    "Number of times evicting failed due to lack of entries");

static COUNTER_U64_DEFINE_EARLY(neg_evict_skipped_missed);
SYSCTL_COUNTER_U64(_vfs_cache_neg, OID_AUTO, evict_skipped_missed, CTLFLAG_RD,
    &neg_evict_skipped_missed,
    "Number of times evicting failed due to target entry disappearing");

static COUNTER_U64_DEFINE_EARLY(neg_evict_skipped_contended);
SYSCTL_COUNTER_U64(_vfs_cache_neg, OID_AUTO, evict_skipped_contended, CTLFLAG_RD,
    &neg_evict_skipped_contended,
    "Number of times evicting failed due to contention");

SYSCTL_COUNTER_U64(_vfs_cache_neg, OID_AUTO, hits, CTLFLAG_RD, &numneghits,
    "Number of cache hits (negative)");

static int
sysctl_neg_hot(SYSCTL_HANDLER_ARGS)
{
	int i, out;

	out = 0;
	for (i = 0; i < numneglists; i++)
		out += neglists[i].nl_hotnum;

	return (SYSCTL_OUT(req, &out, sizeof(out)));
}
SYSCTL_PROC(_vfs_cache_neg, OID_AUTO, hot, CTLTYPE_INT | CTLFLAG_RD |
    CTLFLAG_MPSAFE, 0, 0, sysctl_neg_hot, "I",
    "Number of hot negative entries");

static void
cache_neg_init(struct namecache *ncp)
{
	struct negstate *ns;

	ncp->nc_flag |= NCF_NEGATIVE;
	ns = NCP2NEGSTATE(ncp);
	ns->neg_flag = 0;
	ns->neg_hit = 0;
	counter_u64_add(neg_created, 1);
}

#define CACHE_NEG_PROMOTION_THRESH 2

static bool
cache_neg_hit_prep(struct namecache *ncp)
{
	struct negstate *ns;
	u_char n;

	ns = NCP2NEGSTATE(ncp);
	n = atomic_load_char(&ns->neg_hit);
	for (;;) {
		if (n >= CACHE_NEG_PROMOTION_THRESH)
			return (false);
		if (atomic_fcmpset_8(&ns->neg_hit, &n, n + 1))
			break;
	}
	return (n + 1 == CACHE_NEG_PROMOTION_THRESH);
}

/*
 * Nothing to do here but it is provided for completeness as some
 * cache_neg_hit_prep callers may end up returning without even
 * trying to promote.
 */
#define cache_neg_hit_abort(ncp)	do { } while (0)

static void
cache_neg_hit_finish(struct namecache *ncp)
{

	SDT_PROBE2(vfs, namecache, lookup, hit__negative, ncp->nc_dvp, ncp->nc_name);
	counter_u64_add(numneghits, 1);
}

/*
 * Move a negative entry to the hot list.
 */
static void
cache_neg_promote_locked(struct namecache *ncp)
{
	struct neglist *nl;
	struct negstate *ns;

	ns = NCP2NEGSTATE(ncp);
	nl = NCP2NEGLIST(ncp);
	mtx_assert(&nl->nl_lock, MA_OWNED);
	if ((ns->neg_flag & NEG_HOT) == 0) {
		TAILQ_REMOVE(&nl->nl_list, ncp, nc_dst);
		TAILQ_INSERT_TAIL(&nl->nl_hotlist, ncp, nc_dst);
		nl->nl_hotnum++;
		ns->neg_flag |= NEG_HOT;
	}
}

/*
 * Move a hot negative entry to the cold list.
 */
static void
cache_neg_demote_locked(struct namecache *ncp)
{
	struct neglist *nl;
	struct negstate *ns;

	ns = NCP2NEGSTATE(ncp);
	nl = NCP2NEGLIST(ncp);
	mtx_assert(&nl->nl_lock, MA_OWNED);
	MPASS(ns->neg_flag & NEG_HOT);
	TAILQ_REMOVE(&nl->nl_hotlist, ncp, nc_dst);
	TAILQ_INSERT_TAIL(&nl->nl_list, ncp, nc_dst);
	nl->nl_hotnum--;
	ns->neg_flag &= ~NEG_HOT;
	atomic_store_char(&ns->neg_hit, 0);
}

/*
 * Move a negative entry to the hot list if it matches the lookup.
 *
 * We have to take locks, but they may be contended and in the worst
 * case we may need to go off CPU. We don't want to spin within the
 * smr section and we can't block with it. Exiting the section means
 * the found entry could have been evicted. We are going to look it
 * up again.
 */
static bool
cache_neg_promote_cond(struct vnode *dvp, struct componentname *cnp,
    struct namecache *oncp, uint32_t hash)
{
	struct namecache *ncp;
	struct neglist *nl;
	u_char nc_flag;

	nl = NCP2NEGLIST(oncp);

	mtx_lock(&nl->nl_lock);
	/*
	 * For hash iteration.
	 */
	vfs_smr_enter();

	/*
	 * Avoid all surprises by only succeeding if we got the same entry and
	 * bailing completely otherwise.
	 * XXX There are no provisions to keep the vnode around, meaning we may
	 * end up promoting a negative entry for a *new* vnode and returning
	 * ENOENT on its account. This is the error we want to return anyway
	 * and promotion is harmless.
	 *
	 * In particular at this point there can be a new ncp which matches the
	 * search but hashes to a different neglist.
	 */
	CK_SLIST_FOREACH(ncp, (NCHHASH(hash)), nc_hash) {
		if (ncp == oncp)
			break;
	}

	/*
	 * No match to begin with.
	 */
	if (__predict_false(ncp == NULL)) {
		goto out_abort;
	}

	/*
	 * The newly found entry may be something different...
	 */
	if (!cache_ncp_match(ncp, dvp, cnp)) {
		goto out_abort;
	}

	/*
	 * ... and not even negative.
	 */
	nc_flag = atomic_load_char(&ncp->nc_flag);
	if ((nc_flag & NCF_NEGATIVE) == 0) {
		goto out_abort;
	}

	if (!cache_ncp_canuse(ncp)) {
		goto out_abort;
	}

	cache_neg_promote_locked(ncp);
	cache_neg_hit_finish(ncp);
	vfs_smr_exit();
	mtx_unlock(&nl->nl_lock);
	return (true);
out_abort:
	vfs_smr_exit();
	mtx_unlock(&nl->nl_lock);
	return (false);
}

static void
cache_neg_promote(struct namecache *ncp)
{
	struct neglist *nl;

	nl = NCP2NEGLIST(ncp);
	mtx_lock(&nl->nl_lock);
	cache_neg_promote_locked(ncp);
	mtx_unlock(&nl->nl_lock);
}

static void
cache_neg_insert(struct namecache *ncp)
{
	struct neglist *nl;

	MPASS(ncp->nc_flag & NCF_NEGATIVE);
	cache_assert_bucket_locked(ncp);
	nl = NCP2NEGLIST(ncp);
	mtx_lock(&nl->nl_lock);
	TAILQ_INSERT_TAIL(&nl->nl_list, ncp, nc_dst);
	mtx_unlock(&nl->nl_lock);
	atomic_add_long(&numneg, 1);
}

static void
cache_neg_remove(struct namecache *ncp)
{
	struct neglist *nl;
	struct negstate *ns;

	cache_assert_bucket_locked(ncp);
	nl = NCP2NEGLIST(ncp);
	ns = NCP2NEGSTATE(ncp);
	mtx_lock(&nl->nl_lock);
	if ((ns->neg_flag & NEG_HOT) != 0) {
		TAILQ_REMOVE(&nl->nl_hotlist, ncp, nc_dst);
		nl->nl_hotnum--;
	} else {
		TAILQ_REMOVE(&nl->nl_list, ncp, nc_dst);
	}
	mtx_unlock(&nl->nl_lock);
	atomic_subtract_long(&numneg, 1);
}

static struct neglist *
cache_neg_evict_select_list(void)
{
	struct neglist *nl;
	u_int c;

	c = atomic_fetchadd_int(&neg_cycle, 1) + 1;
	nl = &neglists[c % numneglists];
	if (!mtx_trylock(&nl->nl_evict_lock)) {
		counter_u64_add(neg_evict_skipped_contended, 1);
		return (NULL);
	}
	return (nl);
}

static struct namecache *
cache_neg_evict_select_entry(struct neglist *nl)
{
	struct namecache *ncp, *lncp;
	struct negstate *ns, *lns;
	int i;

	mtx_assert(&nl->nl_evict_lock, MA_OWNED);
	mtx_assert(&nl->nl_lock, MA_OWNED);
	ncp = TAILQ_FIRST(&nl->nl_list);
	if (ncp == NULL)
		return (NULL);
	lncp = ncp;
	lns = NCP2NEGSTATE(lncp);
	for (i = 1; i < 4; i++) {
		ncp = TAILQ_NEXT(ncp, nc_dst);
		if (ncp == NULL)
			break;
		ns = NCP2NEGSTATE(ncp);
		if (ns->neg_hit < lns->neg_hit) {
			lncp = ncp;
			lns = ns;
		}
	}
	return (lncp);
}

static bool
cache_neg_evict(void)
{
	struct namecache *ncp, *ncp2;
	struct neglist *nl;
	struct vnode *dvp;
	struct mtx *dvlp;
	struct mtx *blp;
	uint32_t hash;
	u_char nlen;
	bool evicted;

	nl = cache_neg_evict_select_list();
	if (nl == NULL) {
		return (false);
	}

	mtx_lock(&nl->nl_lock);
	ncp = TAILQ_FIRST(&nl->nl_hotlist);
	if (ncp != NULL) {
		cache_neg_demote_locked(ncp);
	}
	ncp = cache_neg_evict_select_entry(nl);
	if (ncp == NULL) {
		counter_u64_add(neg_evict_skipped_empty, 1);
		mtx_unlock(&nl->nl_lock);
		mtx_unlock(&nl->nl_evict_lock);
		return (false);
	}
	nlen = ncp->nc_nlen;
	dvp = ncp->nc_dvp;
	hash = cache_get_hash(ncp->nc_name, nlen, dvp);
	dvlp = VP2VNODELOCK(dvp);
	blp = HASH2BUCKETLOCK(hash);
	mtx_unlock(&nl->nl_lock);
	mtx_unlock(&nl->nl_evict_lock);
	mtx_lock(dvlp);
	mtx_lock(blp);
	/*
	 * Note that since all locks were dropped above, the entry may be
	 * gone or reallocated to be something else.
	 */
	CK_SLIST_FOREACH(ncp2, (NCHHASH(hash)), nc_hash) {
		if (ncp2 == ncp && ncp2->nc_dvp == dvp &&
		    ncp2->nc_nlen == nlen && (ncp2->nc_flag & NCF_NEGATIVE) != 0)
			break;
	}
	if (ncp2 == NULL) {
		counter_u64_add(neg_evict_skipped_missed, 1);
		ncp = NULL;
		evicted = false;
	} else {
		MPASS(dvlp == VP2VNODELOCK(ncp->nc_dvp));
		MPASS(blp == NCP2BUCKETLOCK(ncp));
		SDT_PROBE2(vfs, namecache, evict_negative, done, ncp->nc_dvp,
		    ncp->nc_name);
		cache_zap_locked(ncp);
		counter_u64_add(neg_evicted, 1);
		evicted = true;
	}
	mtx_unlock(blp);
	mtx_unlock(dvlp);
	if (ncp != NULL)
		cache_free(ncp);
	return (evicted);
}

/*
 * Maybe evict a negative entry to create more room.
 *
 * The ncnegfactor parameter limits what fraction of the total count
 * can comprise of negative entries. However, if the cache is just
 * warming up this leads to excessive evictions.  As such, ncnegminpct
 * (recomputed to neg_min) dictates whether the above should be
 * applied.
 *
 * Try evicting if the cache is close to full capacity regardless of
 * other considerations.
 */
static bool
cache_neg_evict_cond(u_long lnumcache)
{
	u_long lnumneg;

	if (ncsize - 1000 < lnumcache)
		goto out_evict;
	lnumneg = atomic_load_long(&numneg);
	if (lnumneg < neg_min)
		return (false);
	if (lnumneg * ncnegfactor < lnumcache)
		return (false);
out_evict:
	return (cache_neg_evict());
}

/*
 * cache_zap_locked():
 *
 *   Removes a namecache entry from cache, whether it contains an actual
 *   pointer to a vnode or if it is just a negative cache entry.
 */
static void
cache_zap_locked(struct namecache *ncp)
{
	struct nchashhead *ncpp;
	struct vnode *dvp, *vp;

	dvp = ncp->nc_dvp;
	vp = ncp->nc_vp;

	if (!(ncp->nc_flag & NCF_NEGATIVE))
		cache_assert_vnode_locked(vp);
	cache_assert_vnode_locked(dvp);
	cache_assert_bucket_locked(ncp);

	cache_ncp_invalidate(ncp);

	ncpp = NCP2BUCKET(ncp);
	CK_SLIST_REMOVE(ncpp, ncp, namecache, nc_hash);
	if (!(ncp->nc_flag & NCF_NEGATIVE)) {
		SDT_PROBE3(vfs, namecache, zap, done, dvp, ncp->nc_name, vp);
		TAILQ_REMOVE(&vp->v_cache_dst, ncp, nc_dst);
		if (ncp == vp->v_cache_dd) {
			atomic_store_ptr(&vp->v_cache_dd, NULL);
		}
	} else {
		SDT_PROBE2(vfs, namecache, zap_negative, done, dvp, ncp->nc_name);
		cache_neg_remove(ncp);
	}
	if (ncp->nc_flag & NCF_ISDOTDOT) {
		if (ncp == dvp->v_cache_dd) {
			atomic_store_ptr(&dvp->v_cache_dd, NULL);
		}
	} else {
		LIST_REMOVE(ncp, nc_src);
		if (LIST_EMPTY(&dvp->v_cache_src)) {
			ncp->nc_flag |= NCF_DVDROP;
		}
	}
}

static void
cache_zap_negative_locked_vnode_kl(struct namecache *ncp, struct vnode *vp)
{
	struct mtx *blp;

	MPASS(ncp->nc_dvp == vp);
	MPASS(ncp->nc_flag & NCF_NEGATIVE);
	cache_assert_vnode_locked(vp);

	blp = NCP2BUCKETLOCK(ncp);
	mtx_lock(blp);
	cache_zap_locked(ncp);
	mtx_unlock(blp);
}

static bool
cache_zap_locked_vnode_kl2(struct namecache *ncp, struct vnode *vp,
    struct mtx **vlpp)
{
	struct mtx *pvlp, *vlp1, *vlp2, *to_unlock;
	struct mtx *blp;

	MPASS(vp == ncp->nc_dvp || vp == ncp->nc_vp);
	cache_assert_vnode_locked(vp);

	if (ncp->nc_flag & NCF_NEGATIVE) {
		if (*vlpp != NULL) {
			mtx_unlock(*vlpp);
			*vlpp = NULL;
		}
		cache_zap_negative_locked_vnode_kl(ncp, vp);
		return (true);
	}

	pvlp = VP2VNODELOCK(vp);
	blp = NCP2BUCKETLOCK(ncp);
	vlp1 = VP2VNODELOCK(ncp->nc_dvp);
	vlp2 = VP2VNODELOCK(ncp->nc_vp);

	if (*vlpp == vlp1 || *vlpp == vlp2) {
		to_unlock = *vlpp;
		*vlpp = NULL;
	} else {
		if (*vlpp != NULL) {
			mtx_unlock(*vlpp);
			*vlpp = NULL;
		}
		cache_sort_vnodes(&vlp1, &vlp2);
		if (vlp1 == pvlp) {
			mtx_lock(vlp2);
			to_unlock = vlp2;
		} else {
			if (!mtx_trylock(vlp1))
				goto out_relock;
			to_unlock = vlp1;
		}
	}
	mtx_lock(blp);
	cache_zap_locked(ncp);
	mtx_unlock(blp);
	if (to_unlock != NULL)
		mtx_unlock(to_unlock);
	return (true);

out_relock:
	mtx_unlock(vlp2);
	mtx_lock(vlp1);
	mtx_lock(vlp2);
	MPASS(*vlpp == NULL);
	*vlpp = vlp1;
	return (false);
}

/*
 * If trylocking failed we can get here. We know enough to take all needed locks
 * in the right order and re-lookup the entry.
 */
static int
cache_zap_unlocked_bucket(struct namecache *ncp, struct componentname *cnp,
    struct vnode *dvp, struct mtx *dvlp, struct mtx *vlp, uint32_t hash,
    struct mtx *blp)
{
	struct namecache *rncp;
	struct mtx *rvlp;

	cache_assert_bucket_unlocked(ncp);

	cache_sort_vnodes(&dvlp, &vlp);
	cache_lock_vnodes(dvlp, vlp);
	mtx_lock(blp);
	CK_SLIST_FOREACH(rncp, (NCHHASH(hash)), nc_hash) {
		if (rncp == ncp && cache_ncp_match(rncp, dvp, cnp))
			break;
	}
	if (rncp == NULL)
		goto out_mismatch;

	if (!(ncp->nc_flag & NCF_NEGATIVE))
		rvlp = VP2VNODELOCK(rncp->nc_vp);
	else
		rvlp = NULL;
	if (rvlp != vlp)
		goto out_mismatch;

	cache_zap_locked(rncp);
	mtx_unlock(blp);
	cache_unlock_vnodes(dvlp, vlp);
	atomic_add_long(&zap_bucket_relock_success, 1);
	return (0);

out_mismatch:
	mtx_unlock(blp);
	cache_unlock_vnodes(dvlp, vlp);
	return (EAGAIN);
}

static int __noinline
cache_zap_locked_bucket(struct namecache *ncp, struct componentname *cnp,
    uint32_t hash, struct mtx *blp)
{
	struct mtx *dvlp, *vlp;
	struct vnode *dvp;

	cache_assert_bucket_locked(ncp);

	dvlp = VP2VNODELOCK(ncp->nc_dvp);
	vlp = NULL;
	if (!(ncp->nc_flag & NCF_NEGATIVE))
		vlp = VP2VNODELOCK(ncp->nc_vp);
	if (cache_trylock_vnodes(dvlp, vlp) == 0) {
		cache_zap_locked(ncp);
		mtx_unlock(blp);
		cache_unlock_vnodes(dvlp, vlp);
		return (0);
	}

	dvp = ncp->nc_dvp;
	mtx_unlock(blp);
	return (cache_zap_unlocked_bucket(ncp, cnp, dvp, dvlp, vlp, hash, blp));
}

static __noinline int
cache_remove_cnp(struct vnode *dvp, struct componentname *cnp)
{
	struct namecache *ncp;
	struct mtx *blp;
	struct mtx *dvlp, *dvlp2;
	uint32_t hash;
	int error;

	if (cnp->cn_namelen == 2 &&
	    cnp->cn_nameptr[0] == '.' && cnp->cn_nameptr[1] == '.') {
		dvlp = VP2VNODELOCK(dvp);
		dvlp2 = NULL;
		mtx_lock(dvlp);
retry_dotdot:
		ncp = dvp->v_cache_dd;
		if (ncp == NULL) {
			mtx_unlock(dvlp);
			if (dvlp2 != NULL)
				mtx_unlock(dvlp2);
			SDT_PROBE2(vfs, namecache, removecnp, miss, dvp, cnp);
			return (0);
		}
		if ((ncp->nc_flag & NCF_ISDOTDOT) != 0) {
			if (!cache_zap_locked_vnode_kl2(ncp, dvp, &dvlp2))
				goto retry_dotdot;
			MPASS(dvp->v_cache_dd == NULL);
			mtx_unlock(dvlp);
			if (dvlp2 != NULL)
				mtx_unlock(dvlp2);
			cache_free(ncp);
		} else {
			atomic_store_ptr(&dvp->v_cache_dd, NULL);
			mtx_unlock(dvlp);
			if (dvlp2 != NULL)
				mtx_unlock(dvlp2);
		}
		SDT_PROBE2(vfs, namecache, removecnp, hit, dvp, cnp);
		return (1);
	}

	/*
	 * XXX note that access here is completely unlocked with no provisions
	 * to keep the hash allocated. If one is sufficiently unlucky a
	 * parallel cache resize can reallocate the hash, unmap backing pages
	 * and cause the empty check below to fault.
	 *
	 * Fixing this has epsilon priority, but can be done with no overhead
	 * for this codepath with sufficient effort.
	 */
	hash = cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp);
	blp = HASH2BUCKETLOCK(hash);
retry:
	if (CK_SLIST_EMPTY(NCHHASH(hash)))
		goto out_no_entry;

	mtx_lock(blp);
	ncp = cache_ncp_find(dvp, cnp, hash);
	if (ncp == NULL) {
		mtx_unlock(blp);
		goto out_no_entry;
	}

	error = cache_zap_locked_bucket(ncp, cnp, hash, blp);
	if (__predict_false(error != 0)) {
		atomic_add_long(&zap_bucket_fail, 1);
		goto retry;
	}
	counter_u64_add(numposzaps, 1);
	SDT_PROBE2(vfs, namecache, removecnp, hit, dvp, cnp);
	cache_free(ncp);
	return (1);
out_no_entry:
	counter_u64_add(nummisszap, 1);
	SDT_PROBE2(vfs, namecache, removecnp, miss, dvp, cnp);
	return (0);
}

static int __noinline
cache_lookup_dot(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct timespec *tsp, int *ticksp)
{
	int ltype;

	*vpp = dvp;
	SDT_PROBE3(vfs, namecache, lookup, hit, dvp, ".", *vpp);
	if (tsp != NULL)
		timespecclear(tsp);
	if (ticksp != NULL)
		*ticksp = ticks;
	vrefact(*vpp);
	/*
	 * When we lookup "." we still can be asked to lock it
	 * differently...
	 */
	ltype = cnp->cn_lkflags & LK_TYPE_MASK;
	if (ltype != VOP_ISLOCKED(*vpp)) {
		if (ltype == LK_EXCLUSIVE) {
			vn_lock(*vpp, LK_UPGRADE | LK_RETRY);
			if (VN_IS_DOOMED((*vpp))) {
				/* forced unmount */
				vrele(*vpp);
				*vpp = NULL;
				return (ENOENT);
			}
		} else
			vn_lock(*vpp, LK_DOWNGRADE | LK_RETRY);
	}
	return (-1);
}

static int __noinline
cache_lookup_dotdot(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct timespec *tsp, int *ticksp)
{
	struct namecache_ts *ncp_ts;
	struct namecache *ncp;
	struct mtx *dvlp;
	enum vgetstate vs;
	int error, ltype;
	bool whiteout;

	MPASS((cnp->cn_flags & ISDOTDOT) != 0);

	if ((cnp->cn_flags & MAKEENTRY) == 0) {
		cache_remove_cnp(dvp, cnp);
		return (0);
	}

retry:
	dvlp = VP2VNODELOCK(dvp);
	mtx_lock(dvlp);
	ncp = dvp->v_cache_dd;
	if (ncp == NULL) {
		SDT_PROBE2(vfs, namecache, lookup, miss, dvp, "..");
		mtx_unlock(dvlp);
		return (0);
	}
	if ((ncp->nc_flag & NCF_ISDOTDOT) != 0) {
		if (ncp->nc_flag & NCF_NEGATIVE)
			*vpp = NULL;
		else
			*vpp = ncp->nc_vp;
	} else
		*vpp = ncp->nc_dvp;
	if (*vpp == NULL)
		goto negative_success;
	SDT_PROBE3(vfs, namecache, lookup, hit, dvp, "..", *vpp);
	cache_out_ts(ncp, tsp, ticksp);
	if ((ncp->nc_flag & (NCF_ISDOTDOT | NCF_DTS)) ==
	    NCF_DTS && tsp != NULL) {
		ncp_ts = __containerof(ncp, struct namecache_ts, nc_nc);
		*tsp = ncp_ts->nc_dotdottime;
	}

	MPASS(dvp != *vpp);
	ltype = VOP_ISLOCKED(dvp);
	VOP_UNLOCK(dvp);
	vs = vget_prep(*vpp);
	mtx_unlock(dvlp);
	error = vget_finish(*vpp, cnp->cn_lkflags, vs);
	vn_lock(dvp, ltype | LK_RETRY);
	if (VN_IS_DOOMED(dvp)) {
		if (error == 0)
			vput(*vpp);
		*vpp = NULL;
		return (ENOENT);
	}
	if (error) {
		*vpp = NULL;
		goto retry;
	}
	return (-1);
negative_success:
	if (__predict_false(cnp->cn_nameiop == CREATE)) {
		if (cnp->cn_flags & ISLASTCN) {
			counter_u64_add(numnegzaps, 1);
			cache_zap_negative_locked_vnode_kl(ncp, dvp);
			mtx_unlock(dvlp);
			cache_free(ncp);
			return (0);
		}
	}

	whiteout = (ncp->nc_flag & NCF_WHITE);
	cache_out_ts(ncp, tsp, ticksp);
	if (cache_neg_hit_prep(ncp))
		cache_neg_promote(ncp);
	else
		cache_neg_hit_finish(ncp);
	mtx_unlock(dvlp);
	if (whiteout)
		cnp->cn_flags |= ISWHITEOUT;
	return (ENOENT);
}

/**
 * Lookup a name in the name cache
 *
 * # Arguments
 *
 * - dvp:	Parent directory in which to search.
 * - vpp:	Return argument.  Will contain desired vnode on cache hit.
 * - cnp:	Parameters of the name search.  The most interesting bits of
 *   		the cn_flags field have the following meanings:
 *   	- MAKEENTRY:	If clear, free an entry from the cache rather than look
 *   			it up.
 *   	- ISDOTDOT:	Must be set if and only if cn_nameptr == ".."
 * - tsp:	Return storage for cache timestamp.  On a successful (positive
 *   		or negative) lookup, tsp will be filled with any timespec that
 *   		was stored when this cache entry was created.  However, it will
 *   		be clear for "." entries.
 * - ticks:	Return storage for alternate cache timestamp.  On a successful
 *   		(positive or negative) lookup, it will contain the ticks value
 *   		that was current when the cache entry was created, unless cnp
 *   		was ".".
 *
 * Either both tsp and ticks have to be provided or neither of them.
 *
 * # Returns
 *
 * - -1:	A positive cache hit.  vpp will contain the desired vnode.
 * - ENOENT:	A negative cache hit, or dvp was recycled out from under us due
 *		to a forced unmount.  vpp will not be modified.  If the entry
 *		is a whiteout, then the ISWHITEOUT flag will be set in
 *		cnp->cn_flags.
 * - 0:		A cache miss.  vpp will not be modified.
 *
 * # Locking
 *
 * On a cache hit, vpp will be returned locked and ref'd.  If we're looking up
 * .., dvp is unlocked.  If we're looking up . an extra ref is taken, but the
 * lock is not recursively acquired.
 */
static int __noinline
cache_lookup_fallback(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct timespec *tsp, int *ticksp)
{
	struct namecache *ncp;
	struct mtx *blp;
	uint32_t hash;
	enum vgetstate vs;
	int error;
	bool whiteout;

	MPASS((cnp->cn_flags & ISDOTDOT) == 0);
	MPASS((cnp->cn_flags & (MAKEENTRY | NC_KEEPPOSENTRY)) != 0);

retry:
	hash = cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp);
	blp = HASH2BUCKETLOCK(hash);
	mtx_lock(blp);

	ncp = cache_ncp_find(dvp, cnp, hash);
	if (__predict_false(ncp == NULL)) {
		mtx_unlock(blp);
		SDT_PROBE2(vfs, namecache, lookup, miss, dvp, cnp->cn_nameptr);
		counter_u64_add(nummiss, 1);
		return (0);
	}

	if (ncp->nc_flag & NCF_NEGATIVE)
		goto negative_success;

	counter_u64_add(numposhits, 1);
	*vpp = ncp->nc_vp;
	SDT_PROBE3(vfs, namecache, lookup, hit, dvp, ncp->nc_name, *vpp);
	cache_out_ts(ncp, tsp, ticksp);
	MPASS(dvp != *vpp);
	vs = vget_prep(*vpp);
	mtx_unlock(blp);
	error = vget_finish(*vpp, cnp->cn_lkflags, vs);
	if (error) {
		*vpp = NULL;
		goto retry;
	}
	return (-1);
negative_success:
	/*
	 * We don't get here with regular lookup apart from corner cases.
	 */
	if (__predict_true(cnp->cn_nameiop == CREATE)) {
		if (cnp->cn_flags & ISLASTCN) {
			counter_u64_add(numnegzaps, 1);
			error = cache_zap_locked_bucket(ncp, cnp, hash, blp);
			if (__predict_false(error != 0)) {
				atomic_add_long(&zap_bucket_fail2, 1);
				goto retry;
			}
			cache_free(ncp);
			return (0);
		}
	}

	whiteout = (ncp->nc_flag & NCF_WHITE);
	cache_out_ts(ncp, tsp, ticksp);
	if (cache_neg_hit_prep(ncp))
		cache_neg_promote(ncp);
	else
		cache_neg_hit_finish(ncp);
	mtx_unlock(blp);
	if (whiteout)
		cnp->cn_flags |= ISWHITEOUT;
	return (ENOENT);
}

int
cache_lookup(struct vnode *dvp, struct vnode **vpp, struct componentname *cnp,
    struct timespec *tsp, int *ticksp)
{
	struct namecache *ncp;
	uint32_t hash;
	enum vgetstate vs;
	int error;
	bool whiteout, neg_promote;
	u_short nc_flag;

	MPASS((tsp == NULL && ticksp == NULL) || (tsp != NULL && ticksp != NULL));

#ifdef DEBUG_CACHE
	if (__predict_false(!doingcache)) {
		cnp->cn_flags &= ~MAKEENTRY;
		return (0);
	}
#endif

	if (__predict_false(cnp->cn_nameptr[0] == '.')) {
		if (cnp->cn_namelen == 1)
			return (cache_lookup_dot(dvp, vpp, cnp, tsp, ticksp));
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.')
			return (cache_lookup_dotdot(dvp, vpp, cnp, tsp, ticksp));
	}

	MPASS((cnp->cn_flags & ISDOTDOT) == 0);

	if ((cnp->cn_flags & (MAKEENTRY | NC_KEEPPOSENTRY)) == 0) {
		cache_remove_cnp(dvp, cnp);
		return (0);
	}

	hash = cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp);
	vfs_smr_enter();

	ncp = cache_ncp_find(dvp, cnp, hash);
	if (__predict_false(ncp == NULL)) {
		vfs_smr_exit();
		SDT_PROBE2(vfs, namecache, lookup, miss, dvp, cnp->cn_nameptr);
		counter_u64_add(nummiss, 1);
		return (0);
	}

	nc_flag = atomic_load_char(&ncp->nc_flag);
	if (nc_flag & NCF_NEGATIVE)
		goto negative_success;

	counter_u64_add(numposhits, 1);
	*vpp = ncp->nc_vp;
	SDT_PROBE3(vfs, namecache, lookup, hit, dvp, ncp->nc_name, *vpp);
	cache_out_ts(ncp, tsp, ticksp);
	MPASS(dvp != *vpp);
	if (!cache_ncp_canuse(ncp)) {
		vfs_smr_exit();
		*vpp = NULL;
		goto out_fallback;
	}
	vs = vget_prep_smr(*vpp);
	vfs_smr_exit();
	if (__predict_false(vs == VGET_NONE)) {
		*vpp = NULL;
		goto out_fallback;
	}
	error = vget_finish(*vpp, cnp->cn_lkflags, vs);
	if (error) {
		*vpp = NULL;
		goto out_fallback;
	}
	return (-1);
negative_success:
	if (cnp->cn_nameiop == CREATE) {
		if (cnp->cn_flags & ISLASTCN) {
			vfs_smr_exit();
			goto out_fallback;
		}
	}

	cache_out_ts(ncp, tsp, ticksp);
	whiteout = (atomic_load_char(&ncp->nc_flag) & NCF_WHITE);
	neg_promote = cache_neg_hit_prep(ncp);
	if (!cache_ncp_canuse(ncp)) {
		cache_neg_hit_abort(ncp);
		vfs_smr_exit();
		goto out_fallback;
	}
	if (neg_promote) {
		vfs_smr_exit();
		if (!cache_neg_promote_cond(dvp, cnp, ncp, hash))
			goto out_fallback;
	} else {
		cache_neg_hit_finish(ncp);
		vfs_smr_exit();
	}
	if (whiteout)
		cnp->cn_flags |= ISWHITEOUT;
	return (ENOENT);
out_fallback:
	return (cache_lookup_fallback(dvp, vpp, cnp, tsp, ticksp));
}

struct celockstate {
	struct mtx *vlp[3];
	struct mtx *blp[2];
};
CTASSERT((nitems(((struct celockstate *)0)->vlp) == 3));
CTASSERT((nitems(((struct celockstate *)0)->blp) == 2));

static inline void
cache_celockstate_init(struct celockstate *cel)
{

	bzero(cel, sizeof(*cel));
}

static void
cache_lock_vnodes_cel(struct celockstate *cel, struct vnode *vp,
    struct vnode *dvp)
{
	struct mtx *vlp1, *vlp2;

	MPASS(cel->vlp[0] == NULL);
	MPASS(cel->vlp[1] == NULL);
	MPASS(cel->vlp[2] == NULL);

	MPASS(vp != NULL || dvp != NULL);

	vlp1 = VP2VNODELOCK(vp);
	vlp2 = VP2VNODELOCK(dvp);
	cache_sort_vnodes(&vlp1, &vlp2);

	if (vlp1 != NULL) {
		mtx_lock(vlp1);
		cel->vlp[0] = vlp1;
	}
	mtx_lock(vlp2);
	cel->vlp[1] = vlp2;
}

static void
cache_unlock_vnodes_cel(struct celockstate *cel)
{

	MPASS(cel->vlp[0] != NULL || cel->vlp[1] != NULL);

	if (cel->vlp[0] != NULL)
		mtx_unlock(cel->vlp[0]);
	if (cel->vlp[1] != NULL)
		mtx_unlock(cel->vlp[1]);
	if (cel->vlp[2] != NULL)
		mtx_unlock(cel->vlp[2]);
}

static bool
cache_lock_vnodes_cel_3(struct celockstate *cel, struct vnode *vp)
{
	struct mtx *vlp;
	bool ret;

	cache_assert_vlp_locked(cel->vlp[0]);
	cache_assert_vlp_locked(cel->vlp[1]);
	MPASS(cel->vlp[2] == NULL);

	MPASS(vp != NULL);
	vlp = VP2VNODELOCK(vp);

	ret = true;
	if (vlp >= cel->vlp[1]) {
		mtx_lock(vlp);
	} else {
		if (mtx_trylock(vlp))
			goto out;
		cache_unlock_vnodes_cel(cel);
		atomic_add_long(&cache_lock_vnodes_cel_3_failures, 1);
		if (vlp < cel->vlp[0]) {
			mtx_lock(vlp);
			mtx_lock(cel->vlp[0]);
			mtx_lock(cel->vlp[1]);
		} else {
			if (cel->vlp[0] != NULL)
				mtx_lock(cel->vlp[0]);
			mtx_lock(vlp);
			mtx_lock(cel->vlp[1]);
		}
		ret = false;
	}
out:
	cel->vlp[2] = vlp;
	return (ret);
}

static void
cache_lock_buckets_cel(struct celockstate *cel, struct mtx *blp1,
    struct mtx *blp2)
{

	MPASS(cel->blp[0] == NULL);
	MPASS(cel->blp[1] == NULL);

	cache_sort_vnodes(&blp1, &blp2);

	if (blp1 != NULL) {
		mtx_lock(blp1);
		cel->blp[0] = blp1;
	}
	mtx_lock(blp2);
	cel->blp[1] = blp2;
}

static void
cache_unlock_buckets_cel(struct celockstate *cel)
{

	if (cel->blp[0] != NULL)
		mtx_unlock(cel->blp[0]);
	mtx_unlock(cel->blp[1]);
}

/*
 * Lock part of the cache affected by the insertion.
 *
 * This means vnodelocks for dvp, vp and the relevant bucketlock.
 * However, insertion can result in removal of an old entry. In this
 * case we have an additional vnode and bucketlock pair to lock.
 *
 * That is, in the worst case we have to lock 3 vnodes and 2 bucketlocks, while
 * preserving the locking order (smaller address first).
 */
static void
cache_enter_lock(struct celockstate *cel, struct vnode *dvp, struct vnode *vp,
    uint32_t hash)
{
	struct namecache *ncp;
	struct mtx *blps[2];
	u_char nc_flag;

	blps[0] = HASH2BUCKETLOCK(hash);
	for (;;) {
		blps[1] = NULL;
		cache_lock_vnodes_cel(cel, dvp, vp);
		if (vp == NULL || vp->v_type != VDIR)
			break;
		ncp = atomic_load_consume_ptr(&vp->v_cache_dd);
		if (ncp == NULL)
			break;
		nc_flag = atomic_load_char(&ncp->nc_flag);
		if ((nc_flag & NCF_ISDOTDOT) == 0)
			break;
		MPASS(ncp->nc_dvp == vp);
		blps[1] = NCP2BUCKETLOCK(ncp);
		if ((nc_flag & NCF_NEGATIVE) != 0)
			break;
		if (cache_lock_vnodes_cel_3(cel, ncp->nc_vp))
			break;
		/*
		 * All vnodes got re-locked. Re-validate the state and if
		 * nothing changed we are done. Otherwise restart.
		 */
		if (ncp == vp->v_cache_dd &&
		    (ncp->nc_flag & NCF_ISDOTDOT) != 0 &&
		    blps[1] == NCP2BUCKETLOCK(ncp) &&
		    VP2VNODELOCK(ncp->nc_vp) == cel->vlp[2])
			break;
		cache_unlock_vnodes_cel(cel);
		cel->vlp[0] = NULL;
		cel->vlp[1] = NULL;
		cel->vlp[2] = NULL;
	}
	cache_lock_buckets_cel(cel, blps[0], blps[1]);
}

static void
cache_enter_lock_dd(struct celockstate *cel, struct vnode *dvp, struct vnode *vp,
    uint32_t hash)
{
	struct namecache *ncp;
	struct mtx *blps[2];
	u_char nc_flag;

	blps[0] = HASH2BUCKETLOCK(hash);
	for (;;) {
		blps[1] = NULL;
		cache_lock_vnodes_cel(cel, dvp, vp);
		ncp = atomic_load_consume_ptr(&dvp->v_cache_dd);
		if (ncp == NULL)
			break;
		nc_flag = atomic_load_char(&ncp->nc_flag);
		if ((nc_flag & NCF_ISDOTDOT) == 0)
			break;
		MPASS(ncp->nc_dvp == dvp);
		blps[1] = NCP2BUCKETLOCK(ncp);
		if ((nc_flag & NCF_NEGATIVE) != 0)
			break;
		if (cache_lock_vnodes_cel_3(cel, ncp->nc_vp))
			break;
		if (ncp == dvp->v_cache_dd &&
		    (ncp->nc_flag & NCF_ISDOTDOT) != 0 &&
		    blps[1] == NCP2BUCKETLOCK(ncp) &&
		    VP2VNODELOCK(ncp->nc_vp) == cel->vlp[2])
			break;
		cache_unlock_vnodes_cel(cel);
		cel->vlp[0] = NULL;
		cel->vlp[1] = NULL;
		cel->vlp[2] = NULL;
	}
	cache_lock_buckets_cel(cel, blps[0], blps[1]);
}

static void
cache_enter_unlock(struct celockstate *cel)
{

	cache_unlock_buckets_cel(cel);
	cache_unlock_vnodes_cel(cel);
}

static void __noinline
cache_enter_dotdot_prep(struct vnode *dvp, struct vnode *vp,
    struct componentname *cnp)
{
	struct celockstate cel;
	struct namecache *ncp;
	uint32_t hash;
	int len;

	if (atomic_load_ptr(&dvp->v_cache_dd) == NULL)
		return;
	len = cnp->cn_namelen;
	cache_celockstate_init(&cel);
	hash = cache_get_hash(cnp->cn_nameptr, len, dvp);
	cache_enter_lock_dd(&cel, dvp, vp, hash);
	ncp = dvp->v_cache_dd;
	if (ncp != NULL && (ncp->nc_flag & NCF_ISDOTDOT)) {
		KASSERT(ncp->nc_dvp == dvp, ("wrong isdotdot parent"));
		cache_zap_locked(ncp);
	} else {
		ncp = NULL;
	}
	atomic_store_ptr(&dvp->v_cache_dd, NULL);
	cache_enter_unlock(&cel);
	if (ncp != NULL)
		cache_free(ncp);
}

/*
 * Add an entry to the cache.
 */
void
cache_enter_time(struct vnode *dvp, struct vnode *vp, struct componentname *cnp,
    struct timespec *tsp, struct timespec *dtsp)
{
	struct celockstate cel;
	struct namecache *ncp, *n2, *ndd;
	struct namecache_ts *ncp_ts;
	uint32_t hash;
	int flag;
	int len;

	KASSERT(cnp->cn_namelen <= NAME_MAX,
	    ("%s: passed len %ld exceeds NAME_MAX (%d)", __func__, cnp->cn_namelen,
	    NAME_MAX));
	VNPASS(!VN_IS_DOOMED(dvp), dvp);
	VNPASS(dvp->v_type != VNON, dvp);
	if (vp != NULL) {
		VNPASS(!VN_IS_DOOMED(vp), vp);
		VNPASS(vp->v_type != VNON, vp);
	}
	if (cnp->cn_namelen == 1 && cnp->cn_nameptr[0] == '.') {
		KASSERT(dvp == vp,
		    ("%s: different vnodes for dot entry (%p; %p)\n", __func__,
		    dvp, vp));
	} else {
		KASSERT(dvp != vp,
		    ("%s: same vnode for non-dot entry [%s] (%p)\n", __func__,
		    cnp->cn_nameptr, dvp));
	}

#ifdef DEBUG_CACHE
	if (__predict_false(!doingcache))
		return;
#endif

	flag = 0;
	if (__predict_false(cnp->cn_nameptr[0] == '.')) {
		if (cnp->cn_namelen == 1)
			return;
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			cache_enter_dotdot_prep(dvp, vp, cnp);
			flag = NCF_ISDOTDOT;
		}
	}

	ncp = cache_alloc(cnp->cn_namelen, tsp != NULL);
	if (ncp == NULL)
		return;

	cache_celockstate_init(&cel);
	ndd = NULL;
	ncp_ts = NULL;

	/*
	 * Calculate the hash key and setup as much of the new
	 * namecache entry as possible before acquiring the lock.
	 */
	ncp->nc_flag = flag | NCF_WIP;
	ncp->nc_vp = vp;
	if (vp == NULL)
		cache_neg_init(ncp);
	ncp->nc_dvp = dvp;
	if (tsp != NULL) {
		ncp_ts = __containerof(ncp, struct namecache_ts, nc_nc);
		ncp_ts->nc_time = *tsp;
		ncp_ts->nc_ticks = ticks;
		ncp_ts->nc_nc.nc_flag |= NCF_TS;
		if (dtsp != NULL) {
			ncp_ts->nc_dotdottime = *dtsp;
			ncp_ts->nc_nc.nc_flag |= NCF_DTS;
		}
	}
	len = ncp->nc_nlen = cnp->cn_namelen;
	hash = cache_get_hash(cnp->cn_nameptr, len, dvp);
	memcpy(ncp->nc_name, cnp->cn_nameptr, len);
	ncp->nc_name[len] = '\0';
	cache_enter_lock(&cel, dvp, vp, hash);

	/*
	 * See if this vnode or negative entry is already in the cache
	 * with this name.  This can happen with concurrent lookups of
	 * the same path name.
	 */
	n2 = cache_ncp_find(dvp, cnp, hash);
	if (n2 != NULL) {
		MPASS(cache_ncp_canuse(n2));
		if ((n2->nc_flag & NCF_NEGATIVE) != 0)
			KASSERT(vp == NULL,
			    ("%s: found entry pointing to a different vnode "
			    "(%p != %p); name [%s]",
			    __func__, NULL, vp, cnp->cn_nameptr));
		else
			KASSERT(n2->nc_vp == vp,
			    ("%s: found entry pointing to a different vnode "
			    "(%p != %p); name [%s]",
			    __func__, n2->nc_vp, vp, cnp->cn_nameptr));
		/*
		 * Entries are supposed to be immutable unless in the
		 * process of getting destroyed. Accommodating for
		 * changing timestamps is possible but not worth it.
		 * This should be harmless in terms of correctness, in
		 * the worst case resulting in an earlier expiration.
		 * Alternatively, the found entry can be replaced
		 * altogether.
		 */
		MPASS((n2->nc_flag & (NCF_TS | NCF_DTS)) ==
		    (ncp->nc_flag & (NCF_TS | NCF_DTS)));
#if 0
		if (tsp != NULL) {
			KASSERT((n2->nc_flag & NCF_TS) != 0,
			    ("no NCF_TS"));
			n2_ts = __containerof(n2, struct namecache_ts, nc_nc);
			n2_ts->nc_time = ncp_ts->nc_time;
			n2_ts->nc_ticks = ncp_ts->nc_ticks;
			if (dtsp != NULL) {
				n2_ts->nc_dotdottime = ncp_ts->nc_dotdottime;
				n2_ts->nc_nc.nc_flag |= NCF_DTS;
			}
		}
#endif
		SDT_PROBE3(vfs, namecache, enter, duplicate, dvp, ncp->nc_name,
		    vp);
		goto out_unlock_free;
	}

	if (flag == NCF_ISDOTDOT) {
		/*
		 * See if we are trying to add .. entry, but some other lookup
		 * has populated v_cache_dd pointer already.
		 */
		if (dvp->v_cache_dd != NULL)
			goto out_unlock_free;
		KASSERT(vp == NULL || vp->v_type == VDIR,
		    ("wrong vnode type %p", vp));
		atomic_thread_fence_rel();
		atomic_store_ptr(&dvp->v_cache_dd, ncp);
	} else if (vp != NULL) {
		/*
		 * Take the slow path in INOTIFY().  This flag will be lazily
		 * cleared by cache_vop_inotify() once all directories referring
		 * to vp are unwatched.
		 */
		if (__predict_false((vn_irflag_read(dvp) & VIRF_INOTIFY) != 0))
			vn_irflag_set_cond(vp, VIRF_INOTIFY_PARENT);

		/*
		 * For this case, the cache entry maps both the
		 * directory name in it and the name ".." for the
		 * directory's parent.
		 */
		if ((ndd = vp->v_cache_dd) != NULL) {
			if ((ndd->nc_flag & NCF_ISDOTDOT) != 0)
				cache_zap_locked(ndd);
			else
				ndd = NULL;
		}
		atomic_thread_fence_rel();
		atomic_store_ptr(&vp->v_cache_dd, ncp);
	}

	if (flag != NCF_ISDOTDOT) {
		if (LIST_EMPTY(&dvp->v_cache_src)) {
			cache_hold_vnode(dvp);
		}
		LIST_INSERT_HEAD(&dvp->v_cache_src, ncp, nc_src);
	}

	/*
	 * If the entry is "negative", we place it into the
	 * "negative" cache queue, otherwise, we place it into the
	 * destination vnode's cache entries queue.
	 */
	if (vp != NULL) {
		TAILQ_INSERT_HEAD(&vp->v_cache_dst, ncp, nc_dst);
		SDT_PROBE3(vfs, namecache, enter, done, dvp, ncp->nc_name,
		    vp);
	} else {
		if (cnp->cn_flags & ISWHITEOUT)
			atomic_store_char(&ncp->nc_flag, ncp->nc_flag | NCF_WHITE);
		cache_neg_insert(ncp);
		SDT_PROBE2(vfs, namecache, enter_negative, done, dvp,
		    ncp->nc_name);
	}

	/*
	 * Insert the new namecache entry into the appropriate chain
	 * within the cache entries table.
	 */
	CK_SLIST_INSERT_HEAD(NCHHASH(hash), ncp, nc_hash);

	atomic_thread_fence_rel();
	/*
	 * Mark the entry as fully constructed.
	 * It is immutable past this point until its removal.
	 */
	atomic_store_char(&ncp->nc_flag, ncp->nc_flag & ~NCF_WIP);

	cache_enter_unlock(&cel);
	if (ndd != NULL)
		cache_free(ndd);
	return;
out_unlock_free:
	cache_enter_unlock(&cel);
	cache_free(ncp);
	return;
}

/*
 * A variant of the above accepting flags.
 *
 * - VFS_CACHE_DROPOLD -- if a conflicting entry is found, drop it.
 *
 * TODO: this routine is a hack. It blindly removes the old entry, even if it
 * happens to match and it is doing it in an inefficient manner. It was added
 * to accommodate NFS which runs into a case where the target for a given name
 * may change from under it. Note this does nothing to solve the following
 * race: 2 callers of cache_enter_time_flags pass a different target vnode for
 * the same [dvp, cnp]. It may be argued that code doing this is broken.
 */
void
cache_enter_time_flags(struct vnode *dvp, struct vnode *vp, struct componentname *cnp,
    struct timespec *tsp, struct timespec *dtsp, int flags)
{

	MPASS((flags & ~(VFS_CACHE_DROPOLD)) == 0);

	if (flags & VFS_CACHE_DROPOLD)
		cache_remove_cnp(dvp, cnp);
	cache_enter_time(dvp, vp, cnp, tsp, dtsp);
}

static u_long
cache_roundup_2(u_long val)
{
	u_long res;

	for (res = 1; res <= val; res <<= 1)
		continue;

	return (res);
}

static struct nchashhead *
nchinittbl(u_long elements, u_long *hashmask)
{
	struct nchashhead *hashtbl;
	u_long hashsize, i;

	hashsize = cache_roundup_2(elements) / 2;

	hashtbl = malloc(hashsize * sizeof(*hashtbl), M_VFSCACHE, M_WAITOK);
	for (i = 0; i < hashsize; i++)
		CK_SLIST_INIT(&hashtbl[i]);
	*hashmask = hashsize - 1;
	return (hashtbl);
}

static void
ncfreetbl(struct nchashhead *hashtbl)
{

	free(hashtbl, M_VFSCACHE);
}

/*
 * Name cache initialization, from vfs_init() when we are booting
 */
static void
nchinit(void *dummy __unused)
{
	u_int i;

	cache_zone_small = uma_zcreate("S VFS Cache", CACHE_ZONE_SMALL_SIZE,
	    NULL, NULL, NULL, NULL, CACHE_ZONE_ALIGNMENT, UMA_ZONE_ZINIT);
	cache_zone_small_ts = uma_zcreate("STS VFS Cache", CACHE_ZONE_SMALL_TS_SIZE,
	    NULL, NULL, NULL, NULL, CACHE_ZONE_ALIGNMENT, UMA_ZONE_ZINIT);
	cache_zone_large = uma_zcreate("L VFS Cache", CACHE_ZONE_LARGE_SIZE,
	    NULL, NULL, NULL, NULL, CACHE_ZONE_ALIGNMENT, UMA_ZONE_ZINIT);
	cache_zone_large_ts = uma_zcreate("LTS VFS Cache", CACHE_ZONE_LARGE_TS_SIZE,
	    NULL, NULL, NULL, NULL, CACHE_ZONE_ALIGNMENT, UMA_ZONE_ZINIT);

	VFS_SMR_ZONE_SET(cache_zone_small);
	VFS_SMR_ZONE_SET(cache_zone_small_ts);
	VFS_SMR_ZONE_SET(cache_zone_large);
	VFS_SMR_ZONE_SET(cache_zone_large_ts);

	ncsize = desiredvnodes * ncsizefactor;
	cache_recalc_neg_min();
	nchashtbl = nchinittbl(ncsize, &nchash);
	ncbuckethash = cache_roundup_2(mp_ncpus * mp_ncpus) - 1;
	if (ncbuckethash < 7) /* arbitrarily chosen to avoid having one lock */
		ncbuckethash = 7;
	if (ncbuckethash > nchash)
		ncbuckethash = nchash;
	bucketlocks = malloc(sizeof(*bucketlocks) * numbucketlocks, M_VFSCACHE,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < numbucketlocks; i++)
		mtx_init(&bucketlocks[i], "ncbuc", NULL, MTX_DUPOK | MTX_RECURSE);
	ncvnodehash = ncbuckethash;
	vnodelocks = malloc(sizeof(*vnodelocks) * numvnodelocks, M_VFSCACHE,
	    M_WAITOK | M_ZERO);
	for (i = 0; i < numvnodelocks; i++)
		mtx_init(&vnodelocks[i], "ncvn", NULL, MTX_DUPOK | MTX_RECURSE);

	for (i = 0; i < numneglists; i++) {
		mtx_init(&neglists[i].nl_evict_lock, "ncnege", NULL, MTX_DEF);
		mtx_init(&neglists[i].nl_lock, "ncnegl", NULL, MTX_DEF);
		TAILQ_INIT(&neglists[i].nl_list);
		TAILQ_INIT(&neglists[i].nl_hotlist);
	}
}
SYSINIT(vfs, SI_SUB_VFS, SI_ORDER_SECOND, nchinit, NULL);

void
cache_vnode_init(struct vnode *vp)
{

	LIST_INIT(&vp->v_cache_src);
	TAILQ_INIT(&vp->v_cache_dst);
	vp->v_cache_dd = NULL;
	cache_prehash(vp);
}

/*
 * Induce transient cache misses for lockless operation in cache_lookup() by
 * using a temporary hash table.
 *
 * This will force a fs lookup.
 *
 * Synchronisation is done in 2 steps, calling vfs_smr_synchronize each time
 * to observe all CPUs not performing the lookup.
 */
static void
cache_changesize_set_temp(struct nchashhead *temptbl, u_long temphash)
{

	MPASS(temphash < nchash);
	/*
	 * Change the size. The new size is smaller and can safely be used
	 * against the existing table. All lookups which now hash wrong will
	 * result in a cache miss, which all callers are supposed to know how
	 * to handle.
	 */
	atomic_store_long(&nchash, temphash);
	atomic_thread_fence_rel();
	vfs_smr_synchronize();
	/*
	 * At this point everyone sees the updated hash value, but they still
	 * see the old table.
	 */
	atomic_store_ptr(&nchashtbl, temptbl);
	atomic_thread_fence_rel();
	vfs_smr_synchronize();
	/*
	 * At this point everyone sees the updated table pointer and size pair.
	 */
}

/*
 * Set the new hash table.
 *
 * Similarly to cache_changesize_set_temp(), this has to synchronize against
 * lockless operation in cache_lookup().
 */
static void
cache_changesize_set_new(struct nchashhead *new_tbl, u_long new_hash)
{

	MPASS(nchash < new_hash);
	/*
	 * Change the pointer first. This wont result in out of bounds access
	 * since the temporary table is guaranteed to be smaller.
	 */
	atomic_store_ptr(&nchashtbl, new_tbl);
	atomic_thread_fence_rel();
	vfs_smr_synchronize();
	/*
	 * At this point everyone sees the updated pointer value, but they
	 * still see the old size.
	 */
	atomic_store_long(&nchash, new_hash);
	atomic_thread_fence_rel();
	vfs_smr_synchronize();
	/*
	 * At this point everyone sees the updated table pointer and size pair.
	 */
}

void
cache_changesize(u_long newmaxvnodes)
{
	struct nchashhead *new_nchashtbl, *old_nchashtbl, *temptbl;
	u_long new_nchash, old_nchash, temphash;
	struct namecache *ncp;
	uint32_t hash;
	u_long newncsize;
	u_long i;

	newncsize = newmaxvnodes * ncsizefactor;
	newmaxvnodes = cache_roundup_2(newmaxvnodes * 2);
	if (newmaxvnodes < numbucketlocks)
		newmaxvnodes = numbucketlocks;

	new_nchashtbl = nchinittbl(newmaxvnodes, &new_nchash);
	/* If same hash table size, nothing to do */
	if (nchash == new_nchash) {
		ncfreetbl(new_nchashtbl);
		return;
	}

	temptbl = nchinittbl(1, &temphash);

	/*
	 * Move everything from the old hash table to the new table.
	 * None of the namecache entries in the table can be removed
	 * because to do so, they have to be removed from the hash table.
	 */
	cache_lock_all_vnodes();
	cache_lock_all_buckets();
	old_nchashtbl = nchashtbl;
	old_nchash = nchash;
	cache_changesize_set_temp(temptbl, temphash);
	for (i = 0; i <= old_nchash; i++) {
		while ((ncp = CK_SLIST_FIRST(&old_nchashtbl[i])) != NULL) {
			hash = cache_get_hash(ncp->nc_name, ncp->nc_nlen,
			    ncp->nc_dvp);
			CK_SLIST_REMOVE(&old_nchashtbl[i], ncp, namecache, nc_hash);
			CK_SLIST_INSERT_HEAD(&new_nchashtbl[hash & new_nchash], ncp, nc_hash);
		}
	}
	ncsize = newncsize;
	cache_recalc_neg_min();
	cache_changesize_set_new(new_nchashtbl, new_nchash);
	cache_unlock_all_buckets();
	cache_unlock_all_vnodes();
	ncfreetbl(old_nchashtbl);
	ncfreetbl(temptbl);
}

/*
 * Remove all entries from and to a particular vnode.
 */
static void
cache_purge_impl(struct vnode *vp)
{
	struct cache_freebatch batch;
	struct namecache *ncp;
	struct mtx *vlp, *vlp2;

	TAILQ_INIT(&batch);
	vlp = VP2VNODELOCK(vp);
	vlp2 = NULL;
	mtx_lock(vlp);
retry:
	while (!LIST_EMPTY(&vp->v_cache_src)) {
		ncp = LIST_FIRST(&vp->v_cache_src);
		if (!cache_zap_locked_vnode_kl2(ncp, vp, &vlp2))
			goto retry;
		TAILQ_INSERT_TAIL(&batch, ncp, nc_dst);
	}
	while (!TAILQ_EMPTY(&vp->v_cache_dst)) {
		ncp = TAILQ_FIRST(&vp->v_cache_dst);
		if (!cache_zap_locked_vnode_kl2(ncp, vp, &vlp2))
			goto retry;
		TAILQ_INSERT_TAIL(&batch, ncp, nc_dst);
	}
	ncp = vp->v_cache_dd;
	if (ncp != NULL) {
		KASSERT(ncp->nc_flag & NCF_ISDOTDOT,
		   ("lost dotdot link"));
		if (!cache_zap_locked_vnode_kl2(ncp, vp, &vlp2))
			goto retry;
		TAILQ_INSERT_TAIL(&batch, ncp, nc_dst);
	}
	KASSERT(vp->v_cache_dd == NULL, ("incomplete purge"));
	mtx_unlock(vlp);
	if (vlp2 != NULL)
		mtx_unlock(vlp2);
	cache_free_batch(&batch);
}

/*
 * Opportunistic check to see if there is anything to do.
 */
static bool
cache_has_entries(struct vnode *vp)
{

	if (LIST_EMPTY(&vp->v_cache_src) && TAILQ_EMPTY(&vp->v_cache_dst) &&
	    atomic_load_ptr(&vp->v_cache_dd) == NULL)
		return (false);
	return (true);
}

void
cache_purge(struct vnode *vp)
{

	SDT_PROBE1(vfs, namecache, purge, done, vp);
	if (!cache_has_entries(vp))
		return;
	cache_purge_impl(vp);
}

/*
 * Only to be used by vgone.
 */
void
cache_purge_vgone(struct vnode *vp)
{
	struct mtx *vlp;

	VNPASS(VN_IS_DOOMED(vp), vp);
	if (cache_has_entries(vp)) {
		cache_purge_impl(vp);
		return;
	}

	/*
	 * Serialize against a potential thread doing cache_purge.
	 */
	vlp = VP2VNODELOCK(vp);
	mtx_wait_unlocked(vlp);
	if (cache_has_entries(vp)) {
		cache_purge_impl(vp);
		return;
	}
	return;
}

/*
 * Remove all negative entries for a particular directory vnode.
 */
void
cache_purge_negative(struct vnode *vp)
{
	struct cache_freebatch batch;
	struct namecache *ncp, *nnp;
	struct mtx *vlp;

	SDT_PROBE1(vfs, namecache, purge_negative, done, vp);
	if (LIST_EMPTY(&vp->v_cache_src))
		return;
	TAILQ_INIT(&batch);
	vlp = VP2VNODELOCK(vp);
	mtx_lock(vlp);
	LIST_FOREACH_SAFE(ncp, &vp->v_cache_src, nc_src, nnp) {
		if (!(ncp->nc_flag & NCF_NEGATIVE))
			continue;
		cache_zap_negative_locked_vnode_kl(ncp, vp);
		TAILQ_INSERT_TAIL(&batch, ncp, nc_dst);
	}
	mtx_unlock(vlp);
	cache_free_batch(&batch);
}

/*
 * Entry points for modifying VOP operations.
 */
void
cache_vop_rename(struct vnode *fdvp, struct vnode *fvp, struct vnode *tdvp,
    struct vnode *tvp, struct componentname *fcnp, struct componentname *tcnp)
{

	ASSERT_VOP_IN_SEQC(fdvp);
	ASSERT_VOP_IN_SEQC(fvp);
	ASSERT_VOP_IN_SEQC(tdvp);
	if (tvp != NULL)
		ASSERT_VOP_IN_SEQC(tvp);

	cache_purge(fvp);
	if (tvp != NULL) {
		cache_purge(tvp);
		KASSERT(!cache_remove_cnp(tdvp, tcnp),
		    ("%s: lingering negative entry", __func__));
	} else {
		cache_remove_cnp(tdvp, tcnp);
	}

	/*
	 * TODO
	 *
	 * Historically renaming was always purging all revelang entries,
	 * but that's quite wasteful. In particular turns out that in many cases
	 * the target file is immediately accessed after rename, inducing a cache
	 * miss.
	 *
	 * Recode this to reduce relocking and reuse the existing entry (if any)
	 * instead of just removing it above and allocating a new one here.
	 */
	cache_enter(tdvp, fvp, tcnp);
}

void
cache_vop_rmdir(struct vnode *dvp, struct vnode *vp)
{

	ASSERT_VOP_IN_SEQC(dvp);
	ASSERT_VOP_IN_SEQC(vp);
	cache_purge(vp);
}

#ifdef INVARIANTS
/*
 * Validate that if an entry exists it matches.
 */
void
cache_validate(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	struct namecache *ncp;
	struct mtx *blp;
	uint32_t hash;

	hash = cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp);
	if (CK_SLIST_EMPTY(NCHHASH(hash)))
		return;
	blp = HASH2BUCKETLOCK(hash);
	mtx_lock(blp);
	ncp = cache_ncp_find(dvp, cnp, hash);
	if (ncp != NULL && ncp->nc_vp != vp) {
		panic("%s: mismatch (%p != %p); ncp %p [%s] dvp %p\n",
		    __func__, vp, ncp->nc_vp, ncp, ncp->nc_name, ncp->nc_dvp);
	}
	mtx_unlock(blp);
}

void
cache_assert_no_entries(struct vnode *vp)
{

	VNPASS(TAILQ_EMPTY(&vp->v_cache_dst), vp);
	VNPASS(LIST_EMPTY(&vp->v_cache_src), vp);
	VNPASS(vp->v_cache_dd == NULL, vp);
}
#endif

/*
 * Flush all entries referencing a particular filesystem.
 */
void
cache_purgevfs(struct mount *mp)
{
	struct vnode *vp, *mvp;
	size_t visited __sdt_used, purged __sdt_used;

	visited = purged = 0;
	/*
	 * Somewhat wasteful iteration over all vnodes. Would be better to
	 * support filtering and avoid the interlock to begin with.
	 */
	MNT_VNODE_FOREACH_ALL(vp, mp, mvp) {
		visited++;
		if (!cache_has_entries(vp)) {
			VI_UNLOCK(vp);
			continue;
		}
		vholdl(vp);
		VI_UNLOCK(vp);
		cache_purge(vp);
		purged++;
		vdrop(vp);
	}

	SDT_PROBE3(vfs, namecache, purgevfs, done, mp, visited, purged);
}

/*
 * Perform canonical checks and cache lookup and pass on to filesystem
 * through the vop_cachedlookup only if needed.
 */

int
vfs_cache_lookup(struct vop_lookup_args *ap)
{
	struct vnode *dvp;
	int error;
	struct vnode **vpp = ap->a_vpp;
	struct componentname *cnp = ap->a_cnp;
	int flags = cnp->cn_flags;

	*vpp = NULL;
	dvp = ap->a_dvp;

	if (dvp->v_type != VDIR)
		return (ENOTDIR);

	if ((flags & ISLASTCN) && (dvp->v_mount->mnt_flag & MNT_RDONLY) &&
	    (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	error = vn_dir_check_exec(dvp, cnp);
	if (error != 0)
		return (error);

	error = cache_lookup(dvp, vpp, cnp, NULL, NULL);
	if (error == 0)
		return (VOP_CACHEDLOOKUP(dvp, vpp, cnp));
	if (error == -1)
		return (0);
	return (error);
}

/* Implementation of the getcwd syscall. */
int
sys___getcwd(struct thread *td, struct __getcwd_args *uap)
{
	char *buf, *retbuf;
	size_t buflen;
	int error;

	buflen = uap->buflen;
	if (__predict_false(buflen < 2))
		return (EINVAL);
	if (buflen > MAXPATHLEN)
		buflen = MAXPATHLEN;

	buf = uma_zalloc(namei_zone, M_WAITOK);
	error = vn_getcwd(buf, &retbuf, &buflen);
	if (error == 0)
		error = copyout(retbuf, uap->buf, buflen);
	uma_zfree(namei_zone, buf);
	return (error);
}

int
vn_getcwd(char *buf, char **retbuf, size_t *buflen)
{
	struct pwd *pwd;
	int error;

	vfs_smr_enter();
	pwd = pwd_get_smr();
	error = vn_fullpath_any_smr(pwd->pwd_cdir, pwd->pwd_rdir, buf, retbuf,
	    buflen, 0);
	VFS_SMR_ASSERT_NOT_ENTERED();
	if (error < 0) {
		pwd = pwd_hold(curthread);
		error = vn_fullpath_any(pwd->pwd_cdir, pwd->pwd_rdir, buf,
		    retbuf, buflen);
		pwd_drop(pwd);
	}

#ifdef KTRACE
	if (KTRPOINT(curthread, KTR_NAMEI) && error == 0)
		ktrnamei(*retbuf);
#endif
	return (error);
}

/*
 * Canonicalize a path by walking it forward and back.
 *
 * BUGS:
 * - Nothing guarantees the integrity of the entire chain. Consider the case
 *   where the path "foo/bar/baz/qux" is passed, but "bar" is moved out of
 *   "foo" into "quux" during the backwards walk. The result will be
 *   "quux/bar/baz/qux", which could not have been obtained by an incremental
 *   walk in userspace. Moreover, the path we return is inaccessible if the
 *   calling thread lacks permission to traverse "quux".
 */
static int
kern___realpathat(struct thread *td, int fd, const char *path, char *buf,
    size_t size, int flags, enum uio_seg pathseg)
{
	struct nameidata nd;
	char *retbuf, *freebuf;
	int error;

	if (flags != 0)
		return (EINVAL);
	NDINIT_ATRIGHTS(&nd, LOOKUP, FOLLOW | WANTPARENT | AUDITVNODE1,
	    pathseg, path, fd, &cap_fstat_rights);
	if ((error = namei(&nd)) != 0)
		return (error);

	if (nd.ni_vp->v_type == VREG && nd.ni_dvp->v_type != VDIR &&
	    (nd.ni_vp->v_vflag & VV_ROOT) != 0) {
		struct vnode *covered_vp;

		/*
		 * This happens if vp is a file mount. The call to
		 * vn_fullpath_hardlink can panic if path resolution can't be
		 * handled without the directory.
		 *
		 * To resolve this, we find the vnode which was mounted on -
		 * this should have a unique global path since we disallow
		 * mounting on linked files.
		 */
		error = vn_lock(nd.ni_vp, LK_SHARED);
		if (error != 0)
			goto out;
		covered_vp = nd.ni_vp->v_mount->mnt_vnodecovered;
		vref(covered_vp);
		VOP_UNLOCK(nd.ni_vp);
		error = vn_fullpath(covered_vp, &retbuf, &freebuf);
		vrele(covered_vp);
	} else {
		error = vn_fullpath_hardlink(nd.ni_vp, nd.ni_dvp,
		    nd.ni_cnd.cn_nameptr, nd.ni_cnd.cn_namelen, &retbuf,
		    &freebuf, &size);
	}
	if (error == 0) {
		size_t len;

		len = strlen(retbuf) + 1;
		if (size < len)
			error = ENAMETOOLONG;
		else if (pathseg == UIO_USERSPACE)
			error = copyout(retbuf, buf, len);
		else
			memcpy(buf, retbuf, len);
		free(freebuf, M_TEMP);
	}
out:
	vrele(nd.ni_vp);
	vrele(nd.ni_dvp);
	NDFREE_PNBUF(&nd);
	return (error);
}

int
sys___realpathat(struct thread *td, struct __realpathat_args *uap)
{

	return (kern___realpathat(td, uap->fd, uap->path, uap->buf, uap->size,
	    uap->flags, UIO_USERSPACE));
}

static int
vn_fullpath_up_to_pwd_vnode(struct vnode *vp,
    struct vnode *(*const get_pwd_vnode)(const struct pwd *),
    char **retbuf, char **freebuf)
{
	struct pwd *pwd;
	char *buf;
	size_t buflen;
	int error;

	if (__predict_false(vp == NULL))
		return (EINVAL);

	buflen = MAXPATHLEN;
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	vfs_smr_enter();
	pwd = pwd_get_smr();
	error = vn_fullpath_any_smr(vp, get_pwd_vnode(pwd), buf, retbuf,
	    &buflen, 0);
	VFS_SMR_ASSERT_NOT_ENTERED();
	if (error < 0) {
		pwd = pwd_hold(curthread);
		error = vn_fullpath_any(vp, get_pwd_vnode(pwd), buf, retbuf,
		    &buflen);
		pwd_drop(pwd);
	}
	if (error == 0)
		*freebuf = buf;
	else
		free(buf, M_TEMP);
	return (error);
}

static inline struct vnode *
get_rdir(const struct pwd *pwd)
{
	return (pwd->pwd_rdir);
}

/*
 * Produce a filesystem path that starts from the current chroot directory and
 * corresponds to the passed vnode, using the name cache (if available).
 */
int
vn_fullpath(struct vnode *vp, char **retbuf, char **freebuf)
{
	return (vn_fullpath_up_to_pwd_vnode(vp, get_rdir, retbuf, freebuf));
}

static inline struct vnode *
get_jdir(const struct pwd *pwd)
{
	return (pwd->pwd_jdir);
}

/*
 * Produce a filesystem path that starts from the current jail's root directory
 * and corresponds to the passed vnode, using the name cache (if available).
 *
 * This function allows to ignore chroots done inside a jail (or the host),
 * allowing path checks to remain unaffected by privileged or unprivileged
 * chroot calls.
 */
int
vn_fullpath_jail(struct vnode *vp, char **retbuf, char **freebuf)
{
	return (vn_fullpath_up_to_pwd_vnode(vp, get_jdir, retbuf, freebuf));
}

/*
 * This function is similar to vn_fullpath, but it attempts to lookup the
 * pathname relative to the global root mount point.  This is required for the
 * auditing sub-system, as audited pathnames must be absolute, relative to the
 * global root mount point.
 */
int
vn_fullpath_global(struct vnode *vp, char **retbuf, char **freebuf)
{
	char *buf;
	size_t buflen;
	int error;

	if (__predict_false(vp == NULL))
		return (EINVAL);
	buflen = MAXPATHLEN;
	buf = malloc(buflen, M_TEMP, M_WAITOK);
	vfs_smr_enter();
	error = vn_fullpath_any_smr(vp, rootvnode, buf, retbuf, &buflen, 0);
	VFS_SMR_ASSERT_NOT_ENTERED();
	if (error < 0) {
		error = vn_fullpath_any(vp, rootvnode, buf, retbuf, &buflen);
	}
	if (error == 0)
		*freebuf = buf;
	else
		free(buf, M_TEMP);
	return (error);
}

static struct namecache *
vn_dd_from_dst(struct vnode *vp)
{
	struct namecache *ncp;

	cache_assert_vnode_locked(vp);
	TAILQ_FOREACH(ncp, &vp->v_cache_dst, nc_dst) {
		if ((ncp->nc_flag & NCF_ISDOTDOT) == 0)
			return (ncp);
	}
	return (NULL);
}

int
vn_vptocnp(struct vnode **vp, char *buf, size_t *buflen)
{
	struct vnode *dvp;
	struct namecache *ncp;
	struct mtx *vlp;
	int error;

	vlp = VP2VNODELOCK(*vp);
	mtx_lock(vlp);
	ncp = (*vp)->v_cache_dd;
	if (ncp != NULL && (ncp->nc_flag & NCF_ISDOTDOT) == 0) {
		KASSERT(ncp == vn_dd_from_dst(*vp),
		    ("%s: mismatch for dd entry (%p != %p)", __func__,
		    ncp, vn_dd_from_dst(*vp)));
	} else {
		ncp = vn_dd_from_dst(*vp);
	}
	if (ncp != NULL) {
		if (*buflen < ncp->nc_nlen) {
			mtx_unlock(vlp);
			vrele(*vp);
			counter_u64_add(numfullpathfail4, 1);
			error = ENOMEM;
			SDT_PROBE3(vfs, namecache, fullpath, return, error,
			    vp, NULL);
			return (error);
		}
		*buflen -= ncp->nc_nlen;
		memcpy(buf + *buflen, ncp->nc_name, ncp->nc_nlen);
		SDT_PROBE3(vfs, namecache, fullpath, hit, ncp->nc_dvp,
		    ncp->nc_name, vp);
		dvp = *vp;
		*vp = ncp->nc_dvp;
		vref(*vp);
		mtx_unlock(vlp);
		vrele(dvp);
		return (0);
	}
	SDT_PROBE1(vfs, namecache, fullpath, miss, vp);

	mtx_unlock(vlp);
	vn_lock(*vp, LK_SHARED | LK_RETRY);
	error = VOP_VPTOCNP(*vp, &dvp, buf, buflen);
	vput(*vp);
	if (error) {
		counter_u64_add(numfullpathfail2, 1);
		SDT_PROBE3(vfs, namecache, fullpath, return,  error, vp, NULL);
		return (error);
	}

	*vp = dvp;
	if (VN_IS_DOOMED(dvp)) {
		/* forced unmount */
		vrele(dvp);
		error = ENOENT;
		SDT_PROBE3(vfs, namecache, fullpath, return, error, vp, NULL);
		return (error);
	}
	/*
	 * *vp has its use count incremented still.
	 */

	return (0);
}

/*
 * Resolve a directory to a pathname.
 *
 * The name of the directory can always be found in the namecache or fetched
 * from the filesystem. There is also guaranteed to be only one parent, meaning
 * we can just follow vnodes up until we find the root.
 *
 * The vnode must be referenced.
 */
static int
vn_fullpath_dir(struct vnode *vp, struct vnode *rdir, char *buf, char **retbuf,
    size_t *len, size_t addend)
{
#ifdef KDTRACE_HOOKS
	struct vnode *startvp = vp;
#endif
	struct vnode *vp1;
	size_t buflen;
	int error;
	bool slash_prefixed;

	VNPASS(vp->v_type == VDIR || VN_IS_DOOMED(vp), vp);
	VNPASS(vp->v_usecount > 0, vp);

	buflen = *len;

	slash_prefixed = true;
	if (addend == 0) {
		MPASS(*len >= 2);
		buflen--;
		buf[buflen] = '\0';
		slash_prefixed = false;
	}

	error = 0;

	SDT_PROBE1(vfs, namecache, fullpath, entry, vp);
	counter_u64_add(numfullpathcalls, 1);
	while (vp != rdir && vp != rootvnode) {
		/*
		 * The vp vnode must be already fully constructed,
		 * since it is either found in namecache or obtained
		 * from VOP_VPTOCNP().  We may test for VV_ROOT safely
		 * without obtaining the vnode lock.
		 */
		if ((vp->v_vflag & VV_ROOT) != 0) {
			vn_lock(vp, LK_RETRY | LK_SHARED);

			/*
			 * With the vnode locked, check for races with
			 * unmount, forced or not.  Note that we
			 * already verified that vp is not equal to
			 * the root vnode, which means that
			 * mnt_vnodecovered can be NULL only for the
			 * case of unmount.
			 */
			if (VN_IS_DOOMED(vp) ||
			    (vp1 = vp->v_mount->mnt_vnodecovered) == NULL ||
			    vp1->v_mountedhere != vp->v_mount) {
				vput(vp);
				error = ENOENT;
				SDT_PROBE3(vfs, namecache, fullpath, return,
				    error, vp, NULL);
				break;
			}

			vref(vp1);
			vput(vp);
			vp = vp1;
			continue;
		}
		VNPASS(vp->v_type == VDIR || VN_IS_DOOMED(vp), vp);
		error = vn_vptocnp(&vp, buf, &buflen);
		if (error)
			break;
		if (buflen == 0) {
			vrele(vp);
			error = ENOMEM;
			SDT_PROBE3(vfs, namecache, fullpath, return, error,
			    startvp, NULL);
			break;
		}
		buf[--buflen] = '/';
		slash_prefixed = true;
	}
	if (error)
		return (error);
	if (!slash_prefixed) {
		if (buflen == 0) {
			vrele(vp);
			counter_u64_add(numfullpathfail4, 1);
			SDT_PROBE3(vfs, namecache, fullpath, return, ENOMEM,
			    startvp, NULL);
			return (ENOMEM);
		}
		buf[--buflen] = '/';
	}
	counter_u64_add(numfullpathfound, 1);
	vrele(vp);

	*retbuf = buf + buflen;
	SDT_PROBE3(vfs, namecache, fullpath, return, 0, startvp, *retbuf);
	*len -= buflen;
	*len += addend;
	return (0);
}

/*
 * Resolve an arbitrary vnode to a pathname.
 *
 * Note 2 caveats:
 * - hardlinks are not tracked, thus if the vnode is not a directory this can
 *   resolve to a different path than the one used to find it
 * - namecache is not mandatory, meaning names are not guaranteed to be added
 *   (in which case resolving fails)
 */
static void __inline
cache_rev_failed_impl(int *reason, int line)
{

	*reason = line;
}
#define cache_rev_failed(var)	cache_rev_failed_impl((var), __LINE__)

static int
vn_fullpath_any_smr(struct vnode *vp, struct vnode *rdir, char *buf,
    char **retbuf, size_t *buflen, size_t addend)
{
#ifdef KDTRACE_HOOKS
	struct vnode *startvp = vp;
#endif
	struct vnode *tvp;
	struct mount *mp;
	struct namecache *ncp;
	size_t orig_buflen;
	int reason;
	int error;
#ifdef KDTRACE_HOOKS
	int i;
#endif
	seqc_t vp_seqc, tvp_seqc;
	u_char nc_flag;

	VFS_SMR_ASSERT_ENTERED();

	if (!atomic_load_char(&cache_fast_lookup_enabled)) {
		vfs_smr_exit();
		return (-1);
	}

	orig_buflen = *buflen;

	if (addend == 0) {
		MPASS(*buflen >= 2);
		*buflen -= 1;
		buf[*buflen] = '\0';
	}

	if (vp == rdir || vp == rootvnode) {
		if (addend == 0) {
			*buflen -= 1;
			buf[*buflen] = '/';
		}
		goto out_ok;
	}

#ifdef KDTRACE_HOOKS
	i = 0;
#endif
	error = -1;
	ncp = NULL; /* for sdt probe down below */
	vp_seqc = vn_seqc_read_any(vp);
	if (seqc_in_modify(vp_seqc)) {
		cache_rev_failed(&reason);
		goto out_abort;
	}

	for (;;) {
#ifdef KDTRACE_HOOKS
		i++;
#endif
		if ((vp->v_vflag & VV_ROOT) != 0) {
			mp = atomic_load_ptr(&vp->v_mount);
			if (mp == NULL) {
				cache_rev_failed(&reason);
				goto out_abort;
			}
			tvp = atomic_load_ptr(&mp->mnt_vnodecovered);
			tvp_seqc = vn_seqc_read_any(tvp);
			if (seqc_in_modify(tvp_seqc)) {
				cache_rev_failed(&reason);
				goto out_abort;
			}
			if (!vn_seqc_consistent(vp, vp_seqc)) {
				cache_rev_failed(&reason);
				goto out_abort;
			}
			vp = tvp;
			vp_seqc = tvp_seqc;
			continue;
		}
		ncp = atomic_load_consume_ptr(&vp->v_cache_dd);
		if (ncp == NULL) {
			cache_rev_failed(&reason);
			goto out_abort;
		}
		nc_flag = atomic_load_char(&ncp->nc_flag);
		if ((nc_flag & NCF_ISDOTDOT) != 0) {
			cache_rev_failed(&reason);
			goto out_abort;
		}
		if (ncp->nc_nlen >= *buflen) {
			cache_rev_failed(&reason);
			error = ENOMEM;
			goto out_abort;
		}
		*buflen -= ncp->nc_nlen;
		memcpy(buf + *buflen, ncp->nc_name, ncp->nc_nlen);
		*buflen -= 1;
		buf[*buflen] = '/';
		tvp = ncp->nc_dvp;
		tvp_seqc = vn_seqc_read_any(tvp);
		if (seqc_in_modify(tvp_seqc)) {
			cache_rev_failed(&reason);
			goto out_abort;
		}
		if (!vn_seqc_consistent(vp, vp_seqc)) {
			cache_rev_failed(&reason);
			goto out_abort;
		}
		/*
		 * Acquire fence provided by vn_seqc_read_any above.
		 */
		if (__predict_false(atomic_load_ptr(&vp->v_cache_dd) != ncp)) {
			cache_rev_failed(&reason);
			goto out_abort;
		}
		if (!cache_ncp_canuse(ncp)) {
			cache_rev_failed(&reason);
			goto out_abort;
		}
		vp = tvp;
		vp_seqc = tvp_seqc;
		if (vp == rdir || vp == rootvnode)
			break;
	}
out_ok:
	vfs_smr_exit();
	*retbuf = buf + *buflen;
	*buflen = orig_buflen - *buflen + addend;
	SDT_PROBE2(vfs, namecache, fullpath_smr, hit, startvp, *retbuf);
	return (0);

out_abort:
	*buflen = orig_buflen;
	SDT_PROBE4(vfs, namecache, fullpath_smr, miss, startvp, ncp, reason, i);
	vfs_smr_exit();
	return (error);
}

static int
vn_fullpath_any(struct vnode *vp, struct vnode *rdir, char *buf, char **retbuf,
    size_t *buflen)
{
	size_t orig_buflen, addend;
	int error;

	if (*buflen < 2)
		return (EINVAL);

	orig_buflen = *buflen;

	vref(vp);
	addend = 0;
	if (vp->v_type != VDIR) {
		*buflen -= 1;
		buf[*buflen] = '\0';
		error = vn_vptocnp(&vp, buf, buflen);
		if (error)
			return (error);
		if (*buflen == 0) {
			vrele(vp);
			return (ENOMEM);
		}
		*buflen -= 1;
		buf[*buflen] = '/';
		addend = orig_buflen - *buflen;
	}

	return (vn_fullpath_dir(vp, rdir, buf, retbuf, buflen, addend));
}

/*
 * Resolve an arbitrary vnode to a pathname (taking care of hardlinks).
 *
 * Since the namecache does not track hardlinks, the caller is expected to
 * first look up the target vnode with WANTPARENT flag passed to namei to get
 * dvp and vp.
 *
 * Then we have 2 cases:
 * - if the found vnode is a directory, the path can be constructed just by
 *   following names up the chain
 * - otherwise we populate the buffer with the saved name and start resolving
 *   from the parent
 */
int
vn_fullpath_hardlink(struct vnode *vp, struct vnode *dvp,
    const char *hrdl_name, size_t hrdl_name_length,
    char **retbuf, char **freebuf, size_t *buflen)
{
	char *buf, *tmpbuf;
	struct pwd *pwd;
	size_t addend;
	int error;
	__enum_uint8(vtype) type;

	if (*buflen < 2)
		return (EINVAL);
	if (*buflen > MAXPATHLEN)
		*buflen = MAXPATHLEN;

	buf = malloc(*buflen, M_TEMP, M_WAITOK);

	addend = 0;

	/*
	 * Check for VBAD to work around the vp_crossmp bug in lookup().
	 *
	 * For example consider tmpfs on /tmp and realpath /tmp. ni_vp will be
	 * set to mount point's root vnode while ni_dvp will be vp_crossmp.
	 * If the type is VDIR (like in this very case) we can skip looking
	 * at ni_dvp in the first place. However, since vnodes get passed here
	 * unlocked the target may transition to doomed state (type == VBAD)
	 * before we get to evaluate the condition. If this happens, we will
	 * populate part of the buffer and descend to vn_fullpath_dir with
	 * vp == vp_crossmp. Prevent the problem by checking for VBAD.
	 */
	type = atomic_load_8(&vp->v_type);
	if (type == VBAD) {
		error = ENOENT;
		goto out_bad;
	}
	if (type != VDIR) {
		addend = hrdl_name_length + 2;
		if (*buflen < addend) {
			error = ENOMEM;
			goto out_bad;
		}
		*buflen -= addend;
		tmpbuf = buf + *buflen;
		tmpbuf[0] = '/';
		memcpy(&tmpbuf[1], hrdl_name, hrdl_name_length);
		tmpbuf[addend - 1] = '\0';
		vp = dvp;
	}

	vfs_smr_enter();
	pwd = pwd_get_smr();
	error = vn_fullpath_any_smr(vp, pwd->pwd_rdir, buf, retbuf, buflen,
	    addend);
	VFS_SMR_ASSERT_NOT_ENTERED();
	if (error < 0) {
		pwd = pwd_hold(curthread);
		vref(vp);
		error = vn_fullpath_dir(vp, pwd->pwd_rdir, buf, retbuf, buflen,
		    addend);
		pwd_drop(pwd);
	}
	if (error != 0)
		goto out_bad;

	*freebuf = buf;

	return (0);
out_bad:
	free(buf, M_TEMP);
	return (error);
}

struct vnode *
vn_dir_dd_ino(struct vnode *vp)
{
	struct namecache *ncp;
	struct vnode *ddvp;
	struct mtx *vlp;
	enum vgetstate vs;

	ASSERT_VOP_LOCKED(vp, "vn_dir_dd_ino");
	vlp = VP2VNODELOCK(vp);
	mtx_lock(vlp);
	TAILQ_FOREACH(ncp, &(vp->v_cache_dst), nc_dst) {
		if ((ncp->nc_flag & NCF_ISDOTDOT) != 0)
			continue;
		ddvp = ncp->nc_dvp;
		vs = vget_prep(ddvp);
		mtx_unlock(vlp);
		if (vget_finish(ddvp, LK_SHARED | LK_NOWAIT, vs))
			return (NULL);
		return (ddvp);
	}
	mtx_unlock(vlp);
	return (NULL);
}

int
vn_commname(struct vnode *vp, char *buf, u_int buflen)
{
	struct namecache *ncp;
	struct mtx *vlp;
	int l;

	vlp = VP2VNODELOCK(vp);
	mtx_lock(vlp);
	TAILQ_FOREACH(ncp, &vp->v_cache_dst, nc_dst)
		if ((ncp->nc_flag & NCF_ISDOTDOT) == 0)
			break;
	if (ncp == NULL) {
		mtx_unlock(vlp);
		return (ENOENT);
	}
	l = min(ncp->nc_nlen, buflen - 1);
	memcpy(buf, ncp->nc_name, l);
	mtx_unlock(vlp);
	buf[l] = '\0';
	return (0);
}

/*
 * This function updates path string to vnode's full global path
 * and checks the size of the new path string against the pathlen argument.
 *
 * Requires a locked, referenced vnode.
 * Vnode is re-locked on success or ENODEV, otherwise unlocked.
 *
 * If vp is a directory, the call to vn_fullpath_global() always succeeds
 * because it falls back to the ".." lookup if the namecache lookup fails.
 */
int
vn_path_to_global_path(struct thread *td, struct vnode *vp, char *path,
    u_int pathlen)
{
	struct nameidata nd;
	struct vnode *vp1;
	char *rpath, *fbuf;
	int error;

	ASSERT_VOP_ELOCKED(vp, __func__);

	/* Construct global filesystem path from vp. */
	VOP_UNLOCK(vp);
	error = vn_fullpath_global(vp, &rpath, &fbuf);

	if (error != 0) {
		vrele(vp);
		return (error);
	}

	if (strlen(rpath) >= pathlen) {
		vrele(vp);
		error = ENAMETOOLONG;
		goto out;
	}

	/*
	 * Re-lookup the vnode by path to detect a possible rename.
	 * As a side effect, the vnode is relocked.
	 * If vnode was renamed, return ENOENT.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1, UIO_SYSSPACE, path);
	error = namei(&nd);
	if (error != 0) {
		vrele(vp);
		goto out;
	}
	NDFREE_PNBUF(&nd);
	vp1 = nd.ni_vp;
	vrele(vp);
	if (vp1 == vp)
		strcpy(path, rpath);
	else {
		vput(vp1);
		error = ENOENT;
	}

out:
	free(fbuf, M_TEMP);
	return (error);
}

/*
 * This is similar to vn_path_to_global_path but allows for regular
 * files which may not be present in the cache.
 *
 * Requires a locked, referenced vnode.
 * Vnode is re-locked on success or ENODEV, otherwise unlocked.
 */
int
vn_path_to_global_path_hardlink(struct thread *td, struct vnode *vp,
    struct vnode *dvp, char *path, u_int pathlen, const char *leaf_name,
    size_t leaf_length)
{
	struct nameidata nd;
	struct vnode *vp1;
	char *rpath, *fbuf;
	size_t len;
	int error;

	ASSERT_VOP_ELOCKED(vp, __func__);

	/*
	 * Construct global filesystem path from dvp, vp and leaf
	 * name.
	 */
	VOP_UNLOCK(vp);
	len = pathlen;
	error = vn_fullpath_hardlink(vp, dvp, leaf_name, leaf_length,
	    &rpath, &fbuf, &len);

	if (error != 0) {
		vrele(vp);
		return (error);
	}

	if (strlen(rpath) >= pathlen) {
		vrele(vp);
		error = ENAMETOOLONG;
		goto out;
	}

	/*
	 * Re-lookup the vnode by path to detect a possible rename.
	 * As a side effect, the vnode is relocked.
	 * If vnode was renamed, return ENOENT.
	 */
	NDINIT(&nd, LOOKUP, FOLLOW | LOCKLEAF | AUDITVNODE1, UIO_SYSSPACE, path);
	error = namei(&nd);
	if (error != 0) {
		vrele(vp);
		goto out;
	}
	NDFREE_PNBUF(&nd);
	vp1 = nd.ni_vp;
	vrele(vp);
	if (vp1 == vp)
		strcpy(path, rpath);
	else {
		vput(vp1);
		error = ENOENT;
	}

out:
	free(fbuf, M_TEMP);
	return (error);
}

void
cache_vop_inotify(struct vnode *vp, int event, uint32_t cookie)
{
	struct mtx *vlp;
	struct namecache *ncp;
	int isdir;
	bool logged, self;

	isdir = vp->v_type == VDIR ? IN_ISDIR : 0;
	self = (vn_irflag_read(vp) & VIRF_INOTIFY) != 0 &&
	    (vp->v_type != VDIR || (event & ~_IN_DIR_EVENTS) != 0);

	if (self) {
		int selfevent;

		if (event == _IN_ATTRIB_LINKCOUNT)
			selfevent = IN_ATTRIB;
		else
			selfevent = event;
		inotify_log(vp, NULL, 0, selfevent | isdir, cookie);
	}
	if ((event & IN_ALL_EVENTS) == 0)
		return;

	logged = false;
	vlp = VP2VNODELOCK(vp);
	mtx_lock(vlp);
	TAILQ_FOREACH(ncp, &vp->v_cache_dst, nc_dst) {
		if ((ncp->nc_flag & NCF_ISDOTDOT) != 0)
			continue;
		if ((vn_irflag_read(ncp->nc_dvp) & VIRF_INOTIFY) != 0) {
			/*
			 * XXX-MJ if the vnode has two links in the same
			 * dir, we'll log the same event twice.
			 */
			inotify_log(ncp->nc_dvp, ncp->nc_name, ncp->nc_nlen,
			    event | isdir, cookie);
			logged = true;
		}
	}
	if (!logged && (vn_irflag_read(vp) & VIRF_INOTIFY_PARENT) != 0) {
		/*
		 * We didn't find a watched directory that contains this vnode,
		 * so stop calling VOP_INOTIFY for operations on the vnode.
		 */
		vn_irflag_unset(vp, VIRF_INOTIFY_PARENT);
	}
	mtx_unlock(vlp);
}

#ifdef DDB
static void
db_print_vpath(struct vnode *vp)
{

	while (vp != NULL) {
		db_printf("%p: ", vp);
		if (vp == rootvnode) {
			db_printf("/");
			vp = NULL;
		} else {
			if (vp->v_vflag & VV_ROOT) {
				db_printf("<mount point>");
				vp = vp->v_mount->mnt_vnodecovered;
			} else {
				struct namecache *ncp;
				char *ncn;
				int i;

				ncp = TAILQ_FIRST(&vp->v_cache_dst);
				if (ncp != NULL) {
					ncn = ncp->nc_name;
					for (i = 0; i < ncp->nc_nlen; i++)
						db_printf("%c", *ncn++);
					vp = ncp->nc_dvp;
				} else {
					vp = NULL;
				}
			}
		}
		db_printf("\n");
	}

	return;
}

DB_SHOW_COMMAND(vpath, db_show_vpath)
{
	struct vnode *vp;

	if (!have_addr) {
		db_printf("usage: show vpath <struct vnode *>\n");
		return;
	}

	vp = (struct vnode *)addr;
	db_print_vpath(vp);
}

#endif

static int cache_fast_lookup = 1;

#define CACHE_FPL_FAILED	-2020

static int
cache_vop_bad_vexec(struct vop_fplookup_vexec_args *v)
{
	vn_printf(v->a_vp, "no proper vop_fplookup_vexec\n");
	panic("no proper vop_fplookup_vexec");
}

static int
cache_vop_bad_symlink(struct vop_fplookup_symlink_args *v)
{
	vn_printf(v->a_vp, "no proper vop_fplookup_symlink\n");
	panic("no proper vop_fplookup_symlink");
}

void
cache_vop_vector_register(struct vop_vector *v)
{
	size_t ops;

	ops = 0;
	if (v->vop_fplookup_vexec != NULL) {
		ops++;
	}
	if (v->vop_fplookup_symlink != NULL) {
		ops++;
	}

	if (ops == 2) {
		return;
	}

	if (ops == 0) {
		v->vop_fplookup_vexec = cache_vop_bad_vexec;
		v->vop_fplookup_symlink = cache_vop_bad_symlink;
		return;
	}

	printf("%s: invalid vop vector %p -- either all or none fplookup vops "
	    "need to be provided",  __func__, v);
	if (v->vop_fplookup_vexec == NULL) {
		printf("%s: missing vop_fplookup_vexec\n", __func__);
	}
	if (v->vop_fplookup_symlink == NULL) {
		printf("%s: missing vop_fplookup_symlink\n", __func__);
	}
	panic("bad vop vector %p", v);
}

#ifdef INVARIANTS
void
cache_validate_vop_vector(struct mount *mp, struct vop_vector *vops)
{
	if (mp == NULL)
		return;

	if ((mp->mnt_kern_flag & MNTK_FPLOOKUP) == 0)
		return;

	if (vops->vop_fplookup_vexec == NULL ||
	    vops->vop_fplookup_vexec == cache_vop_bad_vexec)
		panic("bad vop_fplookup_vexec on vector %p for filesystem %s",
		    vops, mp->mnt_vfc->vfc_name);

	if (vops->vop_fplookup_symlink == NULL ||
	    vops->vop_fplookup_symlink == cache_vop_bad_symlink)
		panic("bad vop_fplookup_symlink on vector %p for filesystem %s",
		    vops, mp->mnt_vfc->vfc_name);
}
#endif

void
cache_fast_lookup_enabled_recalc(void)
{
	int lookup_flag;
	int mac_on;

#ifdef MAC
	mac_on = mac_vnode_check_lookup_enabled();
	mac_on |= mac_vnode_check_readlink_enabled();
#else
	mac_on = 0;
#endif

	lookup_flag = atomic_load_int(&cache_fast_lookup);
	if (lookup_flag && !mac_on) {
		atomic_store_char(&cache_fast_lookup_enabled, true);
	} else {
		atomic_store_char(&cache_fast_lookup_enabled, false);
	}
}

static int
syscal_vfs_cache_fast_lookup(SYSCTL_HANDLER_ARGS)
{
	int error, old;

	old = atomic_load_int(&cache_fast_lookup);
	error = sysctl_handle_int(oidp, arg1, arg2, req);
	if (error == 0 && req->newptr && old != atomic_load_int(&cache_fast_lookup))
		cache_fast_lookup_enabled_recalc();
	return (error);
}
SYSCTL_PROC(_vfs_cache_param, OID_AUTO, fast_lookup, CTLTYPE_INT|CTLFLAG_RW|CTLFLAG_MPSAFE,
    &cache_fast_lookup, 0, syscal_vfs_cache_fast_lookup, "IU", "");

/*
 * Components of nameidata (or objects it can point to) which may
 * need restoring in case fast path lookup fails.
 */
struct nameidata_outer {
	size_t ni_pathlen;
	uint64_t cn_flags;
};

struct nameidata_saved {
#ifdef INVARIANTS
	char *cn_nameptr;
	size_t ni_pathlen;
#endif
};

#ifdef INVARIANTS
struct cache_fpl_debug {
	size_t ni_pathlen;
};
#endif

struct cache_fpl {
	struct nameidata *ndp;
	struct componentname *cnp;
	char *nulchar;
	struct vnode *dvp;
	struct vnode *tvp;
	seqc_t dvp_seqc;
	seqc_t tvp_seqc;
	uint32_t hash;
	struct nameidata_saved snd;
	struct nameidata_outer snd_outer;
	int line;
	enum cache_fpl_status status:8;
	bool in_smr;
	bool fsearch;
	struct pwd **pwd;
#ifdef INVARIANTS
	struct cache_fpl_debug debug;
#endif
};

static bool cache_fplookup_mp_supported(struct mount *mp);
static bool cache_fplookup_is_mp(struct cache_fpl *fpl);
static int cache_fplookup_cross_mount(struct cache_fpl *fpl);
static int cache_fplookup_partial_setup(struct cache_fpl *fpl);
static int cache_fplookup_skip_slashes(struct cache_fpl *fpl);
static int cache_fplookup_trailingslash(struct cache_fpl *fpl);
static void cache_fpl_pathlen_dec(struct cache_fpl *fpl);
static void cache_fpl_pathlen_inc(struct cache_fpl *fpl);
static void cache_fpl_pathlen_add(struct cache_fpl *fpl, size_t n);
static void cache_fpl_pathlen_sub(struct cache_fpl *fpl, size_t n);

static void
cache_fpl_cleanup_cnp(struct componentname *cnp)
{

	uma_zfree(namei_zone, cnp->cn_pnbuf);
	cnp->cn_pnbuf = NULL;
	cnp->cn_nameptr = NULL;
}

static struct vnode *
cache_fpl_handle_root(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	MPASS(*(cnp->cn_nameptr) == '/');
	cnp->cn_nameptr++;
	cache_fpl_pathlen_dec(fpl);

	if (__predict_false(*(cnp->cn_nameptr) == '/')) {
		do {
			cnp->cn_nameptr++;
			cache_fpl_pathlen_dec(fpl);
		} while (*(cnp->cn_nameptr) == '/');
	}

	return (ndp->ni_rootdir);
}

static void
cache_fpl_checkpoint_outer(struct cache_fpl *fpl)
{

	fpl->snd_outer.ni_pathlen = fpl->ndp->ni_pathlen;
	fpl->snd_outer.cn_flags = fpl->ndp->ni_cnd.cn_flags;
}

static void
cache_fpl_checkpoint(struct cache_fpl *fpl)
{

#ifdef INVARIANTS
	fpl->snd.cn_nameptr = fpl->ndp->ni_cnd.cn_nameptr;
	fpl->snd.ni_pathlen = fpl->debug.ni_pathlen;
#endif
}

static void
cache_fpl_restore_partial(struct cache_fpl *fpl)
{

	fpl->ndp->ni_cnd.cn_flags = fpl->snd_outer.cn_flags;
#ifdef INVARIANTS
	fpl->debug.ni_pathlen = fpl->snd.ni_pathlen;
#endif
}

static void
cache_fpl_restore_abort(struct cache_fpl *fpl)
{

	cache_fpl_restore_partial(fpl);
	/*
	 * It is 0 on entry by API contract.
	 */
	fpl->ndp->ni_resflags = 0;
	fpl->ndp->ni_cnd.cn_nameptr = fpl->ndp->ni_cnd.cn_pnbuf;
	fpl->ndp->ni_pathlen = fpl->snd_outer.ni_pathlen;
}

#ifdef INVARIANTS
#define cache_fpl_smr_assert_entered(fpl) ({			\
	struct cache_fpl *_fpl = (fpl);				\
	MPASS(_fpl->in_smr == true);				\
	VFS_SMR_ASSERT_ENTERED();				\
})
#define cache_fpl_smr_assert_not_entered(fpl) ({		\
	struct cache_fpl *_fpl = (fpl);				\
	MPASS(_fpl->in_smr == false);				\
	VFS_SMR_ASSERT_NOT_ENTERED();				\
})
static void
cache_fpl_assert_status(struct cache_fpl *fpl)
{

	switch (fpl->status) {
	case CACHE_FPL_STATUS_UNSET:
		__assert_unreachable();
		break;
	case CACHE_FPL_STATUS_DESTROYED:
	case CACHE_FPL_STATUS_ABORTED:
	case CACHE_FPL_STATUS_PARTIAL:
	case CACHE_FPL_STATUS_HANDLED:
		break;
	}
}
#else
#define cache_fpl_smr_assert_entered(fpl) do { } while (0)
#define cache_fpl_smr_assert_not_entered(fpl) do { } while (0)
#define cache_fpl_assert_status(fpl) do { } while (0)
#endif

#define cache_fpl_smr_enter_initial(fpl) ({			\
	struct cache_fpl *_fpl = (fpl);				\
	vfs_smr_enter();					\
	_fpl->in_smr = true;					\
})

#define cache_fpl_smr_enter(fpl) ({				\
	struct cache_fpl *_fpl = (fpl);				\
	MPASS(_fpl->in_smr == false);				\
	vfs_smr_enter();					\
	_fpl->in_smr = true;					\
})

#define cache_fpl_smr_exit(fpl) ({				\
	struct cache_fpl *_fpl = (fpl);				\
	MPASS(_fpl->in_smr == true);				\
	vfs_smr_exit();						\
	_fpl->in_smr = false;					\
})

static int
cache_fpl_aborted_early_impl(struct cache_fpl *fpl, int line)
{

	if (fpl->status != CACHE_FPL_STATUS_UNSET) {
		KASSERT(fpl->status == CACHE_FPL_STATUS_PARTIAL,
		    ("%s: converting to abort from %d at %d, set at %d\n",
		    __func__, fpl->status, line, fpl->line));
	}
	cache_fpl_smr_assert_not_entered(fpl);
	fpl->status = CACHE_FPL_STATUS_ABORTED;
	fpl->line = line;
	return (CACHE_FPL_FAILED);
}

#define cache_fpl_aborted_early(x)	cache_fpl_aborted_early_impl((x), __LINE__)

static int __noinline
cache_fpl_aborted_impl(struct cache_fpl *fpl, int line)
{
	struct nameidata *ndp;
	struct componentname *cnp;

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	if (fpl->status != CACHE_FPL_STATUS_UNSET) {
		KASSERT(fpl->status == CACHE_FPL_STATUS_PARTIAL,
		    ("%s: converting to abort from %d at %d, set at %d\n",
		    __func__, fpl->status, line, fpl->line));
	}
	fpl->status = CACHE_FPL_STATUS_ABORTED;
	fpl->line = line;
	if (fpl->in_smr)
		cache_fpl_smr_exit(fpl);
	cache_fpl_restore_abort(fpl);
	/*
	 * Resolving symlinks overwrites data passed by the caller.
	 * Let namei know.
	 */
	if (ndp->ni_loopcnt > 0) {
		fpl->status = CACHE_FPL_STATUS_DESTROYED;
		cache_fpl_cleanup_cnp(cnp);
	}
	return (CACHE_FPL_FAILED);
}

#define cache_fpl_aborted(x)	cache_fpl_aborted_impl((x), __LINE__)

static int __noinline
cache_fpl_partial_impl(struct cache_fpl *fpl, int line)
{

	KASSERT(fpl->status == CACHE_FPL_STATUS_UNSET,
	    ("%s: setting to partial at %d, but already set to %d at %d\n",
	    __func__, line, fpl->status, fpl->line));
	cache_fpl_smr_assert_entered(fpl);
	fpl->status = CACHE_FPL_STATUS_PARTIAL;
	fpl->line = line;
	return (cache_fplookup_partial_setup(fpl));
}

#define cache_fpl_partial(x)	cache_fpl_partial_impl((x), __LINE__)

static int
cache_fpl_handled_impl(struct cache_fpl *fpl, int line)
{

	KASSERT(fpl->status == CACHE_FPL_STATUS_UNSET,
	    ("%s: setting to handled at %d, but already set to %d at %d\n",
	    __func__, line, fpl->status, fpl->line));
	cache_fpl_smr_assert_not_entered(fpl);
	fpl->status = CACHE_FPL_STATUS_HANDLED;
	fpl->line = line;
	return (0);
}

#define cache_fpl_handled(x)	cache_fpl_handled_impl((x), __LINE__)

static int
cache_fpl_handled_error_impl(struct cache_fpl *fpl, int error, int line)
{

	KASSERT(fpl->status == CACHE_FPL_STATUS_UNSET,
	    ("%s: setting to handled at %d, but already set to %d at %d\n",
	    __func__, line, fpl->status, fpl->line));
	MPASS(error != 0);
	MPASS(error != CACHE_FPL_FAILED);
	cache_fpl_smr_assert_not_entered(fpl);
	fpl->status = CACHE_FPL_STATUS_HANDLED;
	fpl->line = line;
	fpl->dvp = NULL;
	fpl->tvp = NULL;
	return (error);
}

#define cache_fpl_handled_error(x, e)	cache_fpl_handled_error_impl((x), (e), __LINE__)

static bool
cache_fpl_terminated(struct cache_fpl *fpl)
{

	return (fpl->status != CACHE_FPL_STATUS_UNSET);
}

#define CACHE_FPL_SUPPORTED_CN_FLAGS \
	(NC_NOMAKEENTRY | NC_KEEPPOSENTRY | LOCKLEAF | LOCKPARENT | WANTPARENT | \
	 FAILIFEXISTS | FOLLOW | EMPTYPATH | LOCKSHARED | ISRESTARTED | WILLBEDIR | \
	 ISOPEN | NOMACCHECK | AUDITVNODE1 | AUDITVNODE2 | NOCAPCHECK | OPENREAD | \
	 OPENWRITE | WANTIOCTLCAPS | NAMEILOOKUP)

#define CACHE_FPL_INTERNAL_CN_FLAGS \
	(ISDOTDOT | MAKEENTRY | ISLASTCN)

_Static_assert((CACHE_FPL_SUPPORTED_CN_FLAGS & CACHE_FPL_INTERNAL_CN_FLAGS) == 0,
    "supported and internal flags overlap");

static bool
cache_fpl_islastcn(struct nameidata *ndp)
{

	return (*ndp->ni_next == 0);
}

static bool
cache_fpl_istrailingslash(struct cache_fpl *fpl)
{

	MPASS(fpl->nulchar > fpl->cnp->cn_pnbuf);
	return (*(fpl->nulchar - 1) == '/');
}

static bool
cache_fpl_isdotdot(struct componentname *cnp)
{

	if (cnp->cn_namelen == 2 &&
	    cnp->cn_nameptr[1] == '.' && cnp->cn_nameptr[0] == '.')
		return (true);
	return (false);
}

static bool
cache_can_fplookup(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	struct thread *td;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	td = curthread;

	if (!atomic_load_char(&cache_fast_lookup_enabled)) {
		cache_fpl_aborted_early(fpl);
		return (false);
	}
	if ((cnp->cn_flags & ~CACHE_FPL_SUPPORTED_CN_FLAGS) != 0) {
		cache_fpl_aborted_early(fpl);
		return (false);
	}
	if (IN_CAPABILITY_MODE(td) || CAP_TRACING(td)) {
		cache_fpl_aborted_early(fpl);
		return (false);
	}
	if (AUDITING_TD(td)) {
		cache_fpl_aborted_early(fpl);
		return (false);
	}
	if (ndp->ni_startdir != NULL) {
		cache_fpl_aborted_early(fpl);
		return (false);
	}
	return (true);
}

static int __noinline
cache_fplookup_dirfd(struct cache_fpl *fpl, struct vnode **vpp)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	int error, flags;

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	error = fgetvp_lookup_smr(ndp, vpp, &flags);
	if (__predict_false(error != 0)) {
		return (cache_fpl_aborted(fpl));
	}
	if (__predict_false((flags & O_RESOLVE_BENEATH) != 0)) {
		_Static_assert((CACHE_FPL_SUPPORTED_CN_FLAGS & RBENEATH) == 0,
		    "RBENEATH supported by fplookup");
		cache_fpl_smr_exit(fpl);
		cache_fpl_aborted(fpl);
		return (EOPNOTSUPP);
	}
	fpl->fsearch = (flags & FSEARCH) != 0;
	if ((*vpp)->v_type != VDIR) {
		if (!((cnp->cn_flags & EMPTYPATH) != 0 && cnp->cn_pnbuf[0] == '\0')) {
			cache_fpl_smr_exit(fpl);
			return (cache_fpl_handled_error(fpl, ENOTDIR));
		}
	}
	return (0);
}

static int __noinline
cache_fplookup_negative_promote(struct cache_fpl *fpl, struct namecache *oncp,
    uint32_t hash)
{
	struct componentname *cnp;
	struct vnode *dvp;

	cnp = fpl->cnp;
	dvp = fpl->dvp;

	cache_fpl_smr_exit(fpl);
	if (cache_neg_promote_cond(dvp, cnp, oncp, hash))
		return (cache_fpl_handled_error(fpl, ENOENT));
	else
		return (cache_fpl_aborted(fpl));
}

/*
 * The target vnode is not supported, prepare for the slow path to take over.
 */
static int __noinline
cache_fplookup_partial_setup(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	enum vgetstate dvs;
	struct vnode *dvp;
	struct pwd *pwd;
	seqc_t dvp_seqc;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	pwd = *(fpl->pwd);
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;

	if (!pwd_hold_smr(pwd)) {
		return (cache_fpl_aborted(fpl));
	}

	/*
	 * Note that seqc is checked before the vnode is locked, so by
	 * the time regular lookup gets to it it may have moved.
	 *
	 * Ultimately this does not affect correctness, any lookup errors
	 * are userspace racing with itself. It is guaranteed that any
	 * path which ultimately gets found could also have been found
	 * by regular lookup going all the way in absence of concurrent
	 * modifications.
	 */
	dvs = vget_prep_smr(dvp);
	cache_fpl_smr_exit(fpl);
	if (__predict_false(dvs == VGET_NONE)) {
		pwd_drop(pwd);
		return (cache_fpl_aborted(fpl));
	}

	vget_finish_ref(dvp, dvs);
	if (!vn_seqc_consistent(dvp, dvp_seqc)) {
		vrele(dvp);
		pwd_drop(pwd);
		return (cache_fpl_aborted(fpl));
	}

	cache_fpl_restore_partial(fpl);
#ifdef INVARIANTS
	if (cnp->cn_nameptr != fpl->snd.cn_nameptr) {
		panic("%s: cn_nameptr mismatch (%p != %p) full [%s]\n", __func__,
		    cnp->cn_nameptr, fpl->snd.cn_nameptr, cnp->cn_pnbuf);
	}
#endif

	ndp->ni_startdir = dvp;
	cnp->cn_flags |= MAKEENTRY;
	if (cache_fpl_islastcn(ndp))
		cnp->cn_flags |= ISLASTCN;
	if (cache_fpl_isdotdot(cnp))
		cnp->cn_flags |= ISDOTDOT;

	/*
	 * Skip potential extra slashes parsing did not take care of.
	 * cache_fplookup_skip_slashes explains the mechanism.
	 */
	if (__predict_false(*(cnp->cn_nameptr) == '/')) {
		do {
			cnp->cn_nameptr++;
			cache_fpl_pathlen_dec(fpl);
		} while (*(cnp->cn_nameptr) == '/');
	}

	ndp->ni_pathlen = fpl->nulchar - cnp->cn_nameptr + 1;
#ifdef INVARIANTS
	if (ndp->ni_pathlen != fpl->debug.ni_pathlen) {
		panic("%s: mismatch (%zu != %zu) nulchar %p nameptr %p [%s] ; full string [%s]\n",
		    __func__, ndp->ni_pathlen, fpl->debug.ni_pathlen, fpl->nulchar,
		    cnp->cn_nameptr, cnp->cn_nameptr, cnp->cn_pnbuf);
	}
#endif
	return (0);
}

static int
cache_fplookup_final_child(struct cache_fpl *fpl, enum vgetstate tvs)
{
	struct componentname *cnp;
	struct vnode *tvp;
	seqc_t tvp_seqc;
	int error, lkflags;

	cnp = fpl->cnp;
	tvp = fpl->tvp;
	tvp_seqc = fpl->tvp_seqc;

	if ((cnp->cn_flags & LOCKLEAF) != 0) {
		lkflags = LK_SHARED;
		if ((cnp->cn_flags & LOCKSHARED) == 0)
			lkflags = LK_EXCLUSIVE;
		error = vget_finish(tvp, lkflags, tvs);
		if (__predict_false(error != 0)) {
			return (cache_fpl_aborted(fpl));
		}
	} else {
		vget_finish_ref(tvp, tvs);
	}

	if (!vn_seqc_consistent(tvp, tvp_seqc)) {
		if ((cnp->cn_flags & LOCKLEAF) != 0)
			vput(tvp);
		else
			vrele(tvp);
		return (cache_fpl_aborted(fpl));
	}

	return (cache_fpl_handled(fpl));
}

/*
 * They want to possibly modify the state of the namecache.
 */
static int __noinline
cache_fplookup_final_modifying(struct cache_fpl *fpl)
{
	struct nameidata *ndp __diagused;
	struct componentname *cnp;
	enum vgetstate dvs;
	struct vnode *dvp, *tvp;
	struct mount *mp;
	seqc_t dvp_seqc;
	int error;
	bool docache;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;

	MPASS(*(cnp->cn_nameptr) != '/');
	MPASS(cache_fpl_islastcn(ndp));
	if ((cnp->cn_flags & LOCKPARENT) == 0)
		MPASS((cnp->cn_flags & WANTPARENT) != 0);
	MPASS((cnp->cn_flags & TRAILINGSLASH) == 0);
	MPASS(cnp->cn_nameiop == CREATE || cnp->cn_nameiop == DELETE ||
	    cnp->cn_nameiop == RENAME);
	MPASS((cnp->cn_flags & MAKEENTRY) == 0);
	MPASS((cnp->cn_flags & ISDOTDOT) == 0);

	docache = (cnp->cn_flags & NOCACHE) ^ NOCACHE;
	if (cnp->cn_nameiop == DELETE || cnp->cn_nameiop == RENAME)
		docache = false;

	/*
	 * Regular lookup nulifies the slash, which we don't do here.
	 * Don't take chances with filesystem routines seeing it for
	 * the last entry.
	 */
	if (cache_fpl_istrailingslash(fpl)) {
		return (cache_fpl_partial(fpl));
	}

	mp = atomic_load_ptr(&dvp->v_mount);
	if (__predict_false(mp == NULL)) {
		return (cache_fpl_aborted(fpl));
	}

	if (__predict_false(mp->mnt_flag & MNT_RDONLY)) {
		cache_fpl_smr_exit(fpl);
		/*
		 * Original code keeps not checking for CREATE which
		 * might be a bug. For now let the old lookup decide.
		 */
		if (cnp->cn_nameiop == CREATE) {
			return (cache_fpl_aborted(fpl));
		}
		return (cache_fpl_handled_error(fpl, EROFS));
	}

	if (fpl->tvp != NULL && (cnp->cn_flags & FAILIFEXISTS) != 0) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, EEXIST));
	}

	/*
	 * Secure access to dvp; check cache_fplookup_partial_setup for
	 * reasoning.
	 *
	 * XXX At least UFS requires its lookup routine to be called for
	 * the last path component, which leads to some level of complication
	 * and inefficiency:
	 * - the target routine always locks the target vnode, but our caller
	 *   may not need it locked
	 * - some of the VOP machinery asserts that the parent is locked, which
	 *   once more may be not required
	 *
	 * TODO: add a flag for filesystems which don't need this.
	 */
	dvs = vget_prep_smr(dvp);
	cache_fpl_smr_exit(fpl);
	if (__predict_false(dvs == VGET_NONE)) {
		return (cache_fpl_aborted(fpl));
	}

	vget_finish_ref(dvp, dvs);
	if (!vn_seqc_consistent(dvp, dvp_seqc)) {
		vrele(dvp);
		return (cache_fpl_aborted(fpl));
	}

	error = vn_lock(dvp, LK_EXCLUSIVE);
	if (__predict_false(error != 0)) {
		vrele(dvp);
		return (cache_fpl_aborted(fpl));
	}

	tvp = NULL;
	cnp->cn_flags |= ISLASTCN;
	if (docache)
		cnp->cn_flags |= MAKEENTRY;
	if (cache_fpl_isdotdot(cnp))
		cnp->cn_flags |= ISDOTDOT;
	cnp->cn_lkflags = LK_EXCLUSIVE;
	error = VOP_LOOKUP(dvp, &tvp, cnp);
	switch (error) {
	case EJUSTRETURN:
	case 0:
		break;
	case ENOTDIR:
	case ENOENT:
		vput(dvp);
		return (cache_fpl_handled_error(fpl, error));
	default:
		vput(dvp);
		return (cache_fpl_aborted(fpl));
	}

	fpl->tvp = tvp;

	if (tvp == NULL) {
		MPASS(error == EJUSTRETURN);
		if ((cnp->cn_flags & LOCKPARENT) == 0) {
			VOP_UNLOCK(dvp);
		}
		return (cache_fpl_handled(fpl));
	}

	/*
	 * There are very hairy corner cases concerning various flag combinations
	 * and locking state. In particular here we only hold one lock instead of
	 * two.
	 *
	 * Skip the complexity as it is of no significance for normal workloads.
	 */
	if (__predict_false(tvp == dvp)) {
		vput(dvp);
		vrele(tvp);
		return (cache_fpl_aborted(fpl));
	}

	/*
	 * If they want the symlink itself we are fine, but if they want to
	 * follow it regular lookup has to be engaged.
	 */
	if (tvp->v_type == VLNK) {
		if ((cnp->cn_flags & FOLLOW) != 0) {
			vput(dvp);
			vput(tvp);
			return (cache_fpl_aborted(fpl));
		}
	}

	/*
	 * Since we expect this to be the terminal vnode it should almost never
	 * be a mount point.
	 */
	if (__predict_false(cache_fplookup_is_mp(fpl))) {
		vput(dvp);
		vput(tvp);
		return (cache_fpl_aborted(fpl));
	}

	if ((cnp->cn_flags & FAILIFEXISTS) != 0) {
		vput(dvp);
		vput(tvp);
		return (cache_fpl_handled_error(fpl, EEXIST));
	}

	if ((cnp->cn_flags & LOCKLEAF) == 0) {
		VOP_UNLOCK(tvp);
	}

	if ((cnp->cn_flags & LOCKPARENT) == 0) {
		VOP_UNLOCK(dvp);
	}

	return (cache_fpl_handled(fpl));
}

static int __noinline
cache_fplookup_modifying(struct cache_fpl *fpl)
{
	struct nameidata *ndp;

	ndp = fpl->ndp;

	if (!cache_fpl_islastcn(ndp)) {
		return (cache_fpl_partial(fpl));
	}
	return (cache_fplookup_final_modifying(fpl));
}

static int __noinline
cache_fplookup_final_withparent(struct cache_fpl *fpl)
{
	struct componentname *cnp;
	enum vgetstate dvs, tvs;
	struct vnode *dvp, *tvp;
	seqc_t dvp_seqc;
	int error;

	cnp = fpl->cnp;
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;
	tvp = fpl->tvp;

	MPASS((cnp->cn_flags & (LOCKPARENT|WANTPARENT)) != 0);

	/*
	 * This is less efficient than it can be for simplicity.
	 */
	dvs = vget_prep_smr(dvp);
	if (__predict_false(dvs == VGET_NONE)) {
		return (cache_fpl_aborted(fpl));
	}
	tvs = vget_prep_smr(tvp);
	if (__predict_false(tvs == VGET_NONE)) {
		cache_fpl_smr_exit(fpl);
		vget_abort(dvp, dvs);
		return (cache_fpl_aborted(fpl));
	}

	cache_fpl_smr_exit(fpl);

	if ((cnp->cn_flags & LOCKPARENT) != 0) {
		error = vget_finish(dvp, LK_EXCLUSIVE, dvs);
		if (__predict_false(error != 0)) {
			vget_abort(tvp, tvs);
			return (cache_fpl_aborted(fpl));
		}
	} else {
		vget_finish_ref(dvp, dvs);
	}

	if (!vn_seqc_consistent(dvp, dvp_seqc)) {
		vget_abort(tvp, tvs);
		if ((cnp->cn_flags & LOCKPARENT) != 0)
			vput(dvp);
		else
			vrele(dvp);
		return (cache_fpl_aborted(fpl));
	}

	error = cache_fplookup_final_child(fpl, tvs);
	if (__predict_false(error != 0)) {
		MPASS(fpl->status == CACHE_FPL_STATUS_ABORTED ||
		    fpl->status == CACHE_FPL_STATUS_DESTROYED);
		if ((cnp->cn_flags & LOCKPARENT) != 0)
			vput(dvp);
		else
			vrele(dvp);
		return (error);
	}

	MPASS(fpl->status == CACHE_FPL_STATUS_HANDLED);
	return (0);
}

static int
cache_fplookup_final(struct cache_fpl *fpl)
{
	struct componentname *cnp;
	enum vgetstate tvs;
	struct vnode *dvp, *tvp;
	seqc_t dvp_seqc;

	cnp = fpl->cnp;
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;
	tvp = fpl->tvp;

	MPASS(*(cnp->cn_nameptr) != '/');

	if (cnp->cn_nameiop != LOOKUP) {
		return (cache_fplookup_final_modifying(fpl));
	}

	if ((cnp->cn_flags & (LOCKPARENT|WANTPARENT)) != 0)
		return (cache_fplookup_final_withparent(fpl));

	tvs = vget_prep_smr(tvp);
	if (__predict_false(tvs == VGET_NONE)) {
		return (cache_fpl_partial(fpl));
	}

	if (!vn_seqc_consistent(dvp, dvp_seqc)) {
		cache_fpl_smr_exit(fpl);
		vget_abort(tvp, tvs);
		return (cache_fpl_aborted(fpl));
	}

	cache_fpl_smr_exit(fpl);
	return (cache_fplookup_final_child(fpl, tvs));
}

/*
 * Comment from locked lookup:
 * Check for degenerate name (e.g. / or "") which is a way of talking about a
 * directory, e.g. like "/." or ".".
 */
static int __noinline
cache_fplookup_degenerate(struct cache_fpl *fpl)
{
	struct componentname *cnp;
	struct vnode *dvp;
	enum vgetstate dvs;
	int error, lkflags;
#ifdef INVARIANTS
	char *cp;
#endif

	fpl->tvp = fpl->dvp;
	fpl->tvp_seqc = fpl->dvp_seqc;

	cnp = fpl->cnp;
	dvp = fpl->dvp;

#ifdef INVARIANTS
	for (cp = cnp->cn_pnbuf; *cp != '\0'; cp++) {
		KASSERT(*cp == '/',
		    ("%s: encountered non-slash; string [%s]\n", __func__,
		    cnp->cn_pnbuf));
	}
#endif

	if (__predict_false(cnp->cn_nameiop != LOOKUP)) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, EISDIR));
	}

	if ((cnp->cn_flags & (LOCKPARENT|WANTPARENT)) != 0) {
		return (cache_fplookup_final_withparent(fpl));
	}

	dvs = vget_prep_smr(dvp);
	cache_fpl_smr_exit(fpl);
	if (__predict_false(dvs == VGET_NONE)) {
		return (cache_fpl_aborted(fpl));
	}

	if ((cnp->cn_flags & LOCKLEAF) != 0) {
		lkflags = LK_SHARED;
		if ((cnp->cn_flags & LOCKSHARED) == 0)
			lkflags = LK_EXCLUSIVE;
		error = vget_finish(dvp, lkflags, dvs);
		if (__predict_false(error != 0)) {
			return (cache_fpl_aborted(fpl));
		}
	} else {
		vget_finish_ref(dvp, dvs);
	}
	return (cache_fpl_handled(fpl));
}

static int __noinline
cache_fplookup_emptypath(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	enum vgetstate tvs;
	struct vnode *tvp;
	int error, lkflags;

	fpl->tvp = fpl->dvp;
	fpl->tvp_seqc = fpl->dvp_seqc;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	tvp = fpl->tvp;

	MPASS(*cnp->cn_pnbuf == '\0');

	if (__predict_false((cnp->cn_flags & EMPTYPATH) == 0)) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, ENOENT));
	}

	MPASS((cnp->cn_flags & (LOCKPARENT | WANTPARENT)) == 0);

	tvs = vget_prep_smr(tvp);
	cache_fpl_smr_exit(fpl);
	if (__predict_false(tvs == VGET_NONE)) {
		return (cache_fpl_aborted(fpl));
	}

	if ((cnp->cn_flags & LOCKLEAF) != 0) {
		lkflags = LK_SHARED;
		if ((cnp->cn_flags & LOCKSHARED) == 0)
			lkflags = LK_EXCLUSIVE;
		error = vget_finish(tvp, lkflags, tvs);
		if (__predict_false(error != 0)) {
			return (cache_fpl_aborted(fpl));
		}
	} else {
		vget_finish_ref(tvp, tvs);
	}

	ndp->ni_resflags |= NIRES_EMPTYPATH;
	return (cache_fpl_handled(fpl));
}

static int __noinline
cache_fplookup_noentry(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	enum vgetstate dvs;
	struct vnode *dvp, *tvp;
	seqc_t dvp_seqc;
	int error;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;

	MPASS((cnp->cn_flags & MAKEENTRY) == 0);
	MPASS((cnp->cn_flags & ISDOTDOT) == 0);
	if (cnp->cn_nameiop == LOOKUP)
		MPASS((cnp->cn_flags & NOCACHE) == 0);
	MPASS(!cache_fpl_isdotdot(cnp));

	/*
	 * Hack: delayed name len checking.
	 */
	if (__predict_false(cnp->cn_namelen > NAME_MAX)) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, ENAMETOOLONG));
	}

	if (cnp->cn_nameptr[0] == '/') {
		return (cache_fplookup_skip_slashes(fpl));
	}

	if (cnp->cn_pnbuf[0] == '\0') {
		return (cache_fplookup_emptypath(fpl));
	}

	if (cnp->cn_nameptr[0] == '\0') {
		if (fpl->tvp == NULL) {
			return (cache_fplookup_degenerate(fpl));
		}
		return (cache_fplookup_trailingslash(fpl));
	}

	if (cnp->cn_nameiop != LOOKUP) {
		fpl->tvp = NULL;
		return (cache_fplookup_modifying(fpl));
	}

	/*
	 * Only try to fill in the component if it is the last one,
	 * otherwise not only there may be several to handle but the
	 * walk may be complicated.
	 */
	if (!cache_fpl_islastcn(ndp)) {
		return (cache_fpl_partial(fpl));
	}

	/*
	 * Regular lookup nulifies the slash, which we don't do here.
	 * Don't take chances with filesystem routines seeing it for
	 * the last entry.
	 */
	if (cache_fpl_istrailingslash(fpl)) {
		return (cache_fpl_partial(fpl));
	}

	/*
	 * Secure access to dvp; check cache_fplookup_partial_setup for
	 * reasoning.
	 */
	dvs = vget_prep_smr(dvp);
	cache_fpl_smr_exit(fpl);
	if (__predict_false(dvs == VGET_NONE)) {
		return (cache_fpl_aborted(fpl));
	}

	vget_finish_ref(dvp, dvs);
	if (!vn_seqc_consistent(dvp, dvp_seqc)) {
		vrele(dvp);
		return (cache_fpl_aborted(fpl));
	}

	error = vn_lock(dvp, LK_SHARED);
	if (__predict_false(error != 0)) {
		vrele(dvp);
		return (cache_fpl_aborted(fpl));
	}

	tvp = NULL;
	/*
	 * TODO: provide variants which don't require locking either vnode.
	 */
	cnp->cn_flags |= ISLASTCN | MAKEENTRY;
	cnp->cn_lkflags = LK_SHARED;
	if ((cnp->cn_flags & LOCKSHARED) == 0) {
		cnp->cn_lkflags = LK_EXCLUSIVE;
	}
	error = VOP_LOOKUP(dvp, &tvp, cnp);
	switch (error) {
	case EJUSTRETURN:
	case 0:
		break;
	case ENOTDIR:
	case ENOENT:
		vput(dvp);
		return (cache_fpl_handled_error(fpl, error));
	default:
		vput(dvp);
		return (cache_fpl_aborted(fpl));
	}

	fpl->tvp = tvp;

	if (tvp == NULL) {
		MPASS(error == EJUSTRETURN);
		if ((cnp->cn_flags & (WANTPARENT | LOCKPARENT)) == 0) {
			vput(dvp);
		} else if ((cnp->cn_flags & LOCKPARENT) == 0) {
			VOP_UNLOCK(dvp);
		}
		return (cache_fpl_handled(fpl));
	}

	if (tvp->v_type == VLNK) {
		if ((cnp->cn_flags & FOLLOW) != 0) {
			vput(dvp);
			vput(tvp);
			return (cache_fpl_aborted(fpl));
		}
	}

	if (__predict_false(cache_fplookup_is_mp(fpl))) {
		vput(dvp);
		vput(tvp);
		return (cache_fpl_aborted(fpl));
	}

	if ((cnp->cn_flags & LOCKLEAF) == 0) {
		VOP_UNLOCK(tvp);
	}

	if ((cnp->cn_flags & (WANTPARENT | LOCKPARENT)) == 0) {
		vput(dvp);
	} else if ((cnp->cn_flags & LOCKPARENT) == 0) {
		VOP_UNLOCK(dvp);
	}
	return (cache_fpl_handled(fpl));
}

static int __noinline
cache_fplookup_dot(struct cache_fpl *fpl)
{
	int error;

	MPASS(!seqc_in_modify(fpl->dvp_seqc));

	if (__predict_false(fpl->dvp->v_type != VDIR)) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, ENOTDIR));
	}

	/*
	 * Just re-assign the value. seqc will be checked later for the first
	 * non-dot path component in line and/or before deciding to return the
	 * vnode.
	 */
	fpl->tvp = fpl->dvp;
	fpl->tvp_seqc = fpl->dvp_seqc;

	SDT_PROBE3(vfs, namecache, lookup, hit, fpl->dvp, ".", fpl->dvp);

	error = 0;
	if (cache_fplookup_is_mp(fpl)) {
		error = cache_fplookup_cross_mount(fpl);
	}
	return (error);
}

static int __noinline
cache_fplookup_dotdot(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct namecache *ncp;
	struct vnode *dvp;
	u_char nc_flag;

	ndp = fpl->ndp;
	dvp = fpl->dvp;

	MPASS(cache_fpl_isdotdot(fpl->cnp));

	/*
	 * XXX this is racy the same way regular lookup is
	 */
	if (vfs_lookup_isroot(ndp, dvp)) {
		fpl->tvp = dvp;
		fpl->tvp_seqc = vn_seqc_read_any(dvp);
		if (seqc_in_modify(fpl->tvp_seqc)) {
			return (cache_fpl_aborted(fpl));
		}
		return (0);
	}

	if ((dvp->v_vflag & VV_ROOT) != 0) {
		/*
		 * TODO
		 * The opposite of climb mount is needed here.
		 */
		return (cache_fpl_partial(fpl));
	}

	if (__predict_false(dvp->v_type != VDIR)) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, ENOTDIR));
	}

	ncp = atomic_load_consume_ptr(&dvp->v_cache_dd);
	if (ncp == NULL) {
		return (cache_fpl_aborted(fpl));
	}

	nc_flag = atomic_load_char(&ncp->nc_flag);
	if ((nc_flag & NCF_ISDOTDOT) != 0) {
		if ((nc_flag & NCF_NEGATIVE) != 0)
			return (cache_fpl_aborted(fpl));
		fpl->tvp = ncp->nc_vp;
	} else {
		fpl->tvp = ncp->nc_dvp;
	}

	fpl->tvp_seqc = vn_seqc_read_any(fpl->tvp);
	if (seqc_in_modify(fpl->tvp_seqc)) {
		return (cache_fpl_partial(fpl));
	}

	/*
	 * Acquire fence provided by vn_seqc_read_any above.
	 */
	if (__predict_false(atomic_load_ptr(&dvp->v_cache_dd) != ncp)) {
		return (cache_fpl_aborted(fpl));
	}

	if (!cache_ncp_canuse(ncp)) {
		return (cache_fpl_aborted(fpl));
	}

	return (0);
}

static int __noinline
cache_fplookup_neg(struct cache_fpl *fpl, struct namecache *ncp, uint32_t hash)
{
	u_char nc_flag __diagused;
	bool neg_promote;

#ifdef INVARIANTS
	nc_flag = atomic_load_char(&ncp->nc_flag);
	MPASS((nc_flag & NCF_NEGATIVE) != 0);
#endif
	/*
	 * If they want to create an entry we need to replace this one.
	 */
	if (__predict_false(fpl->cnp->cn_nameiop != LOOKUP)) {
		fpl->tvp = NULL;
		return (cache_fplookup_modifying(fpl));
	}
	neg_promote = cache_neg_hit_prep(ncp);
	if (!cache_fpl_neg_ncp_canuse(ncp)) {
		cache_neg_hit_abort(ncp);
		return (cache_fpl_partial(fpl));
	}
	if (neg_promote) {
		return (cache_fplookup_negative_promote(fpl, ncp, hash));
	}
	cache_neg_hit_finish(ncp);
	cache_fpl_smr_exit(fpl);
	return (cache_fpl_handled_error(fpl, ENOENT));
}

/*
 * Resolve a symlink. Called by filesystem-specific routines.
 *
 * Code flow is:
 * ... -> cache_fplookup_symlink -> VOP_FPLOOKUP_SYMLINK -> cache_symlink_resolve
 */
int
cache_symlink_resolve(struct cache_fpl *fpl, const char *string, size_t len)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	size_t adjust;

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	if (__predict_false(len == 0)) {
		return (ENOENT);
	}

	if (__predict_false(len > MAXPATHLEN - 2)) {
		if (cache_fpl_istrailingslash(fpl)) {
			return (EAGAIN);
		}
	}

	ndp->ni_pathlen = fpl->nulchar - cnp->cn_nameptr - cnp->cn_namelen + 1;
#ifdef INVARIANTS
	if (ndp->ni_pathlen != fpl->debug.ni_pathlen) {
		panic("%s: mismatch (%zu != %zu) nulchar %p nameptr %p [%s] ; full string [%s]\n",
		    __func__, ndp->ni_pathlen, fpl->debug.ni_pathlen, fpl->nulchar,
		    cnp->cn_nameptr, cnp->cn_nameptr, cnp->cn_pnbuf);
	}
#endif

	if (__predict_false(len + ndp->ni_pathlen > MAXPATHLEN)) {
		return (ENAMETOOLONG);
	}

	if (__predict_false(ndp->ni_loopcnt++ >= MAXSYMLINKS)) {
		return (ELOOP);
	}

	adjust = len;
	if (ndp->ni_pathlen > 1) {
		bcopy(ndp->ni_next, cnp->cn_pnbuf + len, ndp->ni_pathlen);
	} else {
		if (cache_fpl_istrailingslash(fpl)) {
			adjust = len + 1;
			cnp->cn_pnbuf[len] = '/';
			cnp->cn_pnbuf[len + 1] = '\0';
		} else {
			cnp->cn_pnbuf[len] = '\0';
		}
	}
	bcopy(string, cnp->cn_pnbuf, len);

	ndp->ni_pathlen += adjust;
	cache_fpl_pathlen_add(fpl, adjust);
	cnp->cn_nameptr = cnp->cn_pnbuf;
	fpl->nulchar = &cnp->cn_nameptr[ndp->ni_pathlen - 1];
	fpl->tvp = NULL;
	return (0);
}

static int __noinline
cache_fplookup_symlink(struct cache_fpl *fpl)
{
	struct mount *mp;
	struct nameidata *ndp;
	struct componentname *cnp;
	struct vnode *dvp, *tvp;
	struct pwd *pwd;
	int error;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	dvp = fpl->dvp;
	tvp = fpl->tvp;
	pwd = *(fpl->pwd);

	if (cache_fpl_islastcn(ndp)) {
		if ((cnp->cn_flags & FOLLOW) == 0) {
			return (cache_fplookup_final(fpl));
		}
	}

	mp = atomic_load_ptr(&dvp->v_mount);
	if (__predict_false(mp == NULL)) {
		return (cache_fpl_aborted(fpl));
	}

	/*
	 * Note this check races against setting the flag just like regular
	 * lookup.
	 */
	if (__predict_false((mp->mnt_flag & MNT_NOSYMFOLLOW) != 0)) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, EACCES));
	}

	error = VOP_FPLOOKUP_SYMLINK(tvp, fpl);
	if (__predict_false(error != 0)) {
		switch (error) {
		case EAGAIN:
			return (cache_fpl_partial(fpl));
		case ENOENT:
		case ENAMETOOLONG:
		case ELOOP:
			cache_fpl_smr_exit(fpl);
			return (cache_fpl_handled_error(fpl, error));
		default:
			return (cache_fpl_aborted(fpl));
		}
	}

	if (*(cnp->cn_nameptr) == '/') {
		fpl->dvp = cache_fpl_handle_root(fpl);
		fpl->dvp_seqc = vn_seqc_read_any(fpl->dvp);
		if (seqc_in_modify(fpl->dvp_seqc)) {
			return (cache_fpl_aborted(fpl));
		}
		/*
		 * The main loop assumes that ->dvp points to a vnode belonging
		 * to a filesystem which can do lockless lookup, but the absolute
		 * symlink can be wandering off to one which does not.
		 */
		mp = atomic_load_ptr(&fpl->dvp->v_mount);
		if (__predict_false(mp == NULL)) {
			return (cache_fpl_aborted(fpl));
		}
		if (!cache_fplookup_mp_supported(mp)) {
			cache_fpl_checkpoint(fpl);
			return (cache_fpl_partial(fpl));
		}
		if (__predict_false(pwd->pwd_adir != pwd->pwd_rdir)) {
			return (cache_fpl_aborted(fpl));
		}
	}
	return (0);
}

static int
cache_fplookup_next(struct cache_fpl *fpl)
{
	struct componentname *cnp;
	struct namecache *ncp;
	struct vnode *dvp, *tvp;
	u_char nc_flag;
	uint32_t hash;
	int error;

	cnp = fpl->cnp;
	dvp = fpl->dvp;
	hash = fpl->hash;

	if (__predict_false(cnp->cn_nameptr[0] == '.')) {
		if (cnp->cn_namelen == 1) {
			return (cache_fplookup_dot(fpl));
		}
		if (cnp->cn_namelen == 2 && cnp->cn_nameptr[1] == '.') {
			return (cache_fplookup_dotdot(fpl));
		}
	}

	MPASS(!cache_fpl_isdotdot(cnp));

	ncp = cache_ncp_find(dvp, cnp, hash);
	if (__predict_false(ncp == NULL)) {
		return (cache_fplookup_noentry(fpl));
	}

	tvp = atomic_load_ptr(&ncp->nc_vp);
	nc_flag = atomic_load_char(&ncp->nc_flag);
	if ((nc_flag & NCF_NEGATIVE) != 0) {
		return (cache_fplookup_neg(fpl, ncp, hash));
	}

	if (!cache_ncp_canuse(ncp)) {
		return (cache_fpl_partial(fpl));
	}

	fpl->tvp = tvp;
	fpl->tvp_seqc = vn_seqc_read_any(tvp);
	if (seqc_in_modify(fpl->tvp_seqc)) {
		return (cache_fpl_partial(fpl));
	}

	counter_u64_add(numposhits, 1);
	SDT_PROBE3(vfs, namecache, lookup, hit, dvp, ncp->nc_name, tvp);

	error = 0;
	if (cache_fplookup_is_mp(fpl)) {
		error = cache_fplookup_cross_mount(fpl);
	}
	return (error);
}

static bool
cache_fplookup_mp_supported(struct mount *mp)
{

	MPASS(mp != NULL);
	if ((mp->mnt_kern_flag & MNTK_FPLOOKUP) == 0)
		return (false);
	return (true);
}

/*
 * Walk up the mount stack (if any).
 *
 * Correctness is provided in the following ways:
 * - all vnodes are protected from freeing with SMR
 * - struct mount objects are type stable making them always safe to access
 * - stability of the particular mount is provided by busying it
 * - relationship between the vnode which is mounted on and the mount is
 *   verified with the vnode sequence counter after busying
 * - association between root vnode of the mount and the mount is protected
 *   by busy
 *
 * From that point on we can read the sequence counter of the root vnode
 * and get the next mount on the stack (if any) using the same protection.
 *
 * By the end of successful walk we are guaranteed the reached state was
 * indeed present at least at some point which matches the regular lookup.
 */
static int __noinline
cache_fplookup_climb_mount(struct cache_fpl *fpl)
{
	struct mount *mp, *prev_mp;
	struct mount_pcpu *mpcpu, *prev_mpcpu;
	struct vnode *vp;
	seqc_t vp_seqc;

	vp = fpl->tvp;
	vp_seqc = fpl->tvp_seqc;

	VNPASS(vp->v_type == VDIR || vp->v_type == VREG || vp->v_type == VBAD, vp);
	mp = atomic_load_ptr(&vp->v_mountedhere);
	if (__predict_false(mp == NULL)) {
		return (0);
	}

	prev_mp = NULL;
	for (;;) {
		if (!vfs_op_thread_enter_crit(mp, mpcpu)) {
			if (prev_mp != NULL)
				vfs_op_thread_exit_crit(prev_mp, prev_mpcpu);
			return (cache_fpl_partial(fpl));
		}
		if (prev_mp != NULL)
			vfs_op_thread_exit_crit(prev_mp, prev_mpcpu);
		if (!vn_seqc_consistent(vp, vp_seqc)) {
			vfs_op_thread_exit_crit(mp, mpcpu);
			return (cache_fpl_partial(fpl));
		}
		if (!cache_fplookup_mp_supported(mp)) {
			vfs_op_thread_exit_crit(mp, mpcpu);
			return (cache_fpl_partial(fpl));
		}
		vp = atomic_load_ptr(&mp->mnt_rootvnode);
		if (vp == NULL) {
			vfs_op_thread_exit_crit(mp, mpcpu);
			return (cache_fpl_partial(fpl));
		}
		vp_seqc = vn_seqc_read_any(vp);
		if (seqc_in_modify(vp_seqc)) {
			vfs_op_thread_exit_crit(mp, mpcpu);
			return (cache_fpl_partial(fpl));
		}
		prev_mp = mp;
		prev_mpcpu = mpcpu;
		mp = atomic_load_ptr(&vp->v_mountedhere);
		if (mp == NULL)
			break;
	}

	vfs_op_thread_exit_crit(prev_mp, prev_mpcpu);
	fpl->tvp = vp;
	fpl->tvp_seqc = vp_seqc;
	return (0);
}

static int __noinline
cache_fplookup_cross_mount(struct cache_fpl *fpl)
{
	struct mount *mp;
	struct mount_pcpu *mpcpu;
	struct vnode *vp;
	seqc_t vp_seqc;

	vp = fpl->tvp;
	vp_seqc = fpl->tvp_seqc;

	VNPASS(vp->v_type == VDIR || vp->v_type == VREG || vp->v_type == VBAD, vp);
	mp = atomic_load_ptr(&vp->v_mountedhere);
	if (__predict_false(mp == NULL)) {
		return (0);
	}

	if (!vfs_op_thread_enter_crit(mp, mpcpu)) {
		return (cache_fpl_partial(fpl));
	}
	if (!vn_seqc_consistent(vp, vp_seqc)) {
		vfs_op_thread_exit_crit(mp, mpcpu);
		return (cache_fpl_partial(fpl));
	}
	if (!cache_fplookup_mp_supported(mp)) {
		vfs_op_thread_exit_crit(mp, mpcpu);
		return (cache_fpl_partial(fpl));
	}
	vp = atomic_load_ptr(&mp->mnt_rootvnode);
	if (__predict_false(vp == NULL)) {
		vfs_op_thread_exit_crit(mp, mpcpu);
		return (cache_fpl_partial(fpl));
	}
	vp_seqc = vn_seqc_read_any(vp);
	vfs_op_thread_exit_crit(mp, mpcpu);
	if (seqc_in_modify(vp_seqc)) {
		return (cache_fpl_partial(fpl));
	}
	mp = atomic_load_ptr(&vp->v_mountedhere);
	if (__predict_false(mp != NULL)) {
		/*
		 * There are possibly more mount points on top.
		 * Normally this does not happen so for simplicity just start
		 * over.
		 */
		return (cache_fplookup_climb_mount(fpl));
	}

	fpl->tvp = vp;
	fpl->tvp_seqc = vp_seqc;
	return (0);
}

/*
 * Check if a vnode is mounted on.
 */
static bool
cache_fplookup_is_mp(struct cache_fpl *fpl)
{
	struct vnode *vp;

	vp = fpl->tvp;
	return ((vn_irflag_read(vp) & VIRF_MOUNTPOINT) != 0);
}

/*
 * Parse the path.
 *
 * The code was originally copy-pasted from regular lookup and despite
 * clean ups leaves performance on the table. Any modifications here
 * must take into account that in case off fallback the resulting
 * nameidata state has to be compatible with the original.
 */

/*
 * Debug ni_pathlen tracking.
 */
#ifdef INVARIANTS
static void
cache_fpl_pathlen_add(struct cache_fpl *fpl, size_t n)
{

	fpl->debug.ni_pathlen += n;
	KASSERT(fpl->debug.ni_pathlen <= PATH_MAX,
	    ("%s: pathlen overflow to %zd\n", __func__, fpl->debug.ni_pathlen));
}

static void
cache_fpl_pathlen_sub(struct cache_fpl *fpl, size_t n)
{

	fpl->debug.ni_pathlen -= n;
	KASSERT(fpl->debug.ni_pathlen <= PATH_MAX,
	    ("%s: pathlen underflow to %zd\n", __func__, fpl->debug.ni_pathlen));
}

static void
cache_fpl_pathlen_inc(struct cache_fpl *fpl)
{

	cache_fpl_pathlen_add(fpl, 1);
}

static void
cache_fpl_pathlen_dec(struct cache_fpl *fpl)
{

	cache_fpl_pathlen_sub(fpl, 1);
}
#else
static void
cache_fpl_pathlen_add(struct cache_fpl *fpl, size_t n)
{
}

static void
cache_fpl_pathlen_sub(struct cache_fpl *fpl, size_t n)
{
}

static void
cache_fpl_pathlen_inc(struct cache_fpl *fpl)
{
}

static void
cache_fpl_pathlen_dec(struct cache_fpl *fpl)
{
}
#endif

static void
cache_fplookup_parse(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	struct vnode *dvp;
	char *cp;
	uint32_t hash;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	dvp = fpl->dvp;

	/*
	 * Find the end of this path component, it is either / or nul.
	 *
	 * Store / as a temporary sentinel so that we only have one character
	 * to test for. Pathnames tend to be short so this should not be
	 * resulting in cache misses.
	 *
	 * TODO: fix this to be word-sized.
	 */
	MPASS(&cnp->cn_nameptr[fpl->debug.ni_pathlen - 1] >= cnp->cn_pnbuf);
	KASSERT(&cnp->cn_nameptr[fpl->debug.ni_pathlen - 1] == fpl->nulchar,
	    ("%s: mismatch between pathlen (%zu) and nulchar (%p != %p), string [%s]\n",
	    __func__, fpl->debug.ni_pathlen, &cnp->cn_nameptr[fpl->debug.ni_pathlen - 1],
	    fpl->nulchar, cnp->cn_pnbuf));
	KASSERT(*fpl->nulchar == '\0',
	    ("%s: expected nul at %p; string [%s]\n", __func__, fpl->nulchar,
	    cnp->cn_pnbuf));
	hash = cache_get_hash_iter_start(dvp);
	*fpl->nulchar = '/';
	for (cp = cnp->cn_nameptr; *cp != '/'; cp++) {
		KASSERT(*cp != '\0',
		    ("%s: encountered unexpected nul; string [%s]\n", __func__,
		    cnp->cn_nameptr));
		hash = cache_get_hash_iter(*cp, hash);
		continue;
	}
	*fpl->nulchar = '\0';
	fpl->hash = cache_get_hash_iter_finish(hash);

	cnp->cn_namelen = cp - cnp->cn_nameptr;
	cache_fpl_pathlen_sub(fpl, cnp->cn_namelen);

#ifdef INVARIANTS
	/*
	 * cache_get_hash only accepts lengths up to NAME_MAX. This is fine since
	 * we are going to fail this lookup with ENAMETOOLONG (see below).
	 */
	if (cnp->cn_namelen <= NAME_MAX) {
		if (fpl->hash != cache_get_hash(cnp->cn_nameptr, cnp->cn_namelen, dvp)) {
			panic("%s: mismatched hash for [%s] len %ld", __func__,
			    cnp->cn_nameptr, cnp->cn_namelen);
		}
	}
#endif

	/*
	 * Hack: we have to check if the found path component's length exceeds
	 * NAME_MAX. However, the condition is very rarely true and check can
	 * be elided in the common case -- if an entry was found in the cache,
	 * then it could not have been too long to begin with.
	 */
	ndp->ni_next = cp;
}

static void
cache_fplookup_parse_advance(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	cnp->cn_nameptr = ndp->ni_next;
	KASSERT(*(cnp->cn_nameptr) == '/',
	    ("%s: should have seen slash at %p ; buf %p [%s]\n", __func__,
	    cnp->cn_nameptr, cnp->cn_pnbuf, cnp->cn_pnbuf));
	cnp->cn_nameptr++;
	cache_fpl_pathlen_dec(fpl);
}

/*
 * Skip spurious slashes in a pathname (e.g., "foo///bar") and retry.
 *
 * Lockless lookup tries to elide checking for spurious slashes and should they
 * be present is guaranteed to fail to find an entry. In this case the caller
 * must check if the name starts with a slash and call this routine.  It is
 * going to fast forward across the spurious slashes and set the state up for
 * retry.
 */
static int __noinline
cache_fplookup_skip_slashes(struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	MPASS(*(cnp->cn_nameptr) == '/');
	do {
		cnp->cn_nameptr++;
		cache_fpl_pathlen_dec(fpl);
	} while (*(cnp->cn_nameptr) == '/');

	/*
	 * Go back to one slash so that cache_fplookup_parse_advance has
	 * something to skip.
	 */
	cnp->cn_nameptr--;
	cache_fpl_pathlen_inc(fpl);

	/*
	 * cache_fplookup_parse_advance starts from ndp->ni_next
	 */
	ndp->ni_next = cnp->cn_nameptr;

	/*
	 * See cache_fplookup_dot.
	 */
	fpl->tvp = fpl->dvp;
	fpl->tvp_seqc = fpl->dvp_seqc;

	return (0);
}

/*
 * Handle trailing slashes (e.g., "foo/").
 *
 * If a trailing slash is found the terminal vnode must be a directory.
 * Regular lookup shortens the path by nulifying the first trailing slash and
 * sets the TRAILINGSLASH flag to denote this took place. There are several
 * checks on it performed later.
 *
 * Similarly to spurious slashes, lockless lookup handles this in a speculative
 * manner relying on an invariant that a non-directory vnode will get a miss.
 * In this case cn_nameptr[0] == '\0' and cn_namelen == 0.
 *
 * Thus for a path like "foo/bar/" the code unwinds the state back to "bar/"
 * and denotes this is the last path component, which avoids looping back.
 *
 * Only plain lookups are supported for now to restrict corner cases to handle.
 */
static int __noinline
cache_fplookup_trailingslash(struct cache_fpl *fpl)
{
#ifdef INVARIANTS
	size_t ni_pathlen;
#endif
	struct nameidata *ndp;
	struct componentname *cnp;
	struct namecache *ncp;
	struct vnode *tvp;
	char *cn_nameptr_orig, *cn_nameptr_slash;
	seqc_t tvp_seqc;
	u_char nc_flag;

	ndp = fpl->ndp;
	cnp = fpl->cnp;
	tvp = fpl->tvp;
	tvp_seqc = fpl->tvp_seqc;

	MPASS(fpl->dvp == fpl->tvp);
	KASSERT(cache_fpl_istrailingslash(fpl),
	    ("%s: expected trailing slash at %p; string [%s]\n", __func__, fpl->nulchar - 1,
	    cnp->cn_pnbuf));
	KASSERT(cnp->cn_nameptr[0] == '\0',
	    ("%s: expected nul char at %p; string [%s]\n", __func__, &cnp->cn_nameptr[0],
	    cnp->cn_pnbuf));
	KASSERT(cnp->cn_namelen == 0,
	    ("%s: namelen 0 but got %ld; string [%s]\n", __func__, cnp->cn_namelen,
	    cnp->cn_pnbuf));
	MPASS(cnp->cn_nameptr > cnp->cn_pnbuf);

	if (cnp->cn_nameiop != LOOKUP) {
		return (cache_fpl_aborted(fpl));
	}

	if (__predict_false(tvp->v_type != VDIR)) {
		if (!vn_seqc_consistent(tvp, tvp_seqc)) {
			return (cache_fpl_aborted(fpl));
		}
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, ENOTDIR));
	}

	/*
	 * Denote the last component.
	 */
	ndp->ni_next = &cnp->cn_nameptr[0];
	MPASS(cache_fpl_islastcn(ndp));

	/*
	 * Unwind trailing slashes.
	 */
	cn_nameptr_orig = cnp->cn_nameptr;
	while (cnp->cn_nameptr >= cnp->cn_pnbuf) {
		cnp->cn_nameptr--;
		if (cnp->cn_nameptr[0] != '/') {
			break;
		}
	}

	/*
	 * Unwind to the beginning of the path component.
	 *
	 * Note the path may or may not have started with a slash.
	 */
	cn_nameptr_slash = cnp->cn_nameptr;
	while (cnp->cn_nameptr > cnp->cn_pnbuf) {
		cnp->cn_nameptr--;
		if (cnp->cn_nameptr[0] == '/') {
			break;
		}
	}
	if (cnp->cn_nameptr[0] == '/') {
		cnp->cn_nameptr++;
	}

	cnp->cn_namelen = cn_nameptr_slash - cnp->cn_nameptr + 1;
	cache_fpl_pathlen_add(fpl, cn_nameptr_orig - cnp->cn_nameptr);
	cache_fpl_checkpoint(fpl);

#ifdef INVARIANTS
	ni_pathlen = fpl->nulchar - cnp->cn_nameptr + 1;
	if (ni_pathlen != fpl->debug.ni_pathlen) {
		panic("%s: mismatch (%zu != %zu) nulchar %p nameptr %p [%s] ; full string [%s]\n",
		    __func__, ni_pathlen, fpl->debug.ni_pathlen, fpl->nulchar,
		    cnp->cn_nameptr, cnp->cn_nameptr, cnp->cn_pnbuf);
	}
#endif

	/*
	 * If this was a "./" lookup the parent directory is already correct.
	 */
	if (cnp->cn_nameptr[0] == '.' && cnp->cn_namelen == 1) {
		return (0);
	}

	/*
	 * Otherwise we need to look it up.
	 */
	tvp = fpl->tvp;
	ncp = atomic_load_consume_ptr(&tvp->v_cache_dd);
	if (__predict_false(ncp == NULL)) {
		return (cache_fpl_aborted(fpl));
	}
	nc_flag = atomic_load_char(&ncp->nc_flag);
	if ((nc_flag & NCF_ISDOTDOT) != 0) {
		return (cache_fpl_aborted(fpl));
	}
	fpl->dvp = ncp->nc_dvp;
	fpl->dvp_seqc = vn_seqc_read_any(fpl->dvp);
	if (seqc_in_modify(fpl->dvp_seqc)) {
		return (cache_fpl_aborted(fpl));
	}
	return (0);
}

/*
 * See the API contract for VOP_FPLOOKUP_VEXEC.
 */
static int __noinline
cache_fplookup_failed_vexec(struct cache_fpl *fpl, int error)
{
	struct componentname *cnp;
	struct vnode *dvp;
	seqc_t dvp_seqc;

	cnp = fpl->cnp;
	dvp = fpl->dvp;
	dvp_seqc = fpl->dvp_seqc;

	/*
	 * Hack: delayed empty path checking.
	 */
	if (cnp->cn_pnbuf[0] == '\0') {
		return (cache_fplookup_emptypath(fpl));
	}

	/*
	 * TODO: Due to ignoring trailing slashes lookup will perform a
	 * permission check on the last dir when it should not be doing it.  It
	 * may fail, but said failure should be ignored. It is possible to fix
	 * it up fully without resorting to regular lookup, but for now just
	 * abort.
	 */
	if (cache_fpl_istrailingslash(fpl)) {
		return (cache_fpl_aborted(fpl));
	}

	/*
	 * Hack: delayed degenerate path checking.
	 */
	if (cnp->cn_nameptr[0] == '\0' && fpl->tvp == NULL) {
		return (cache_fplookup_degenerate(fpl));
	}

	/*
	 * Hack: delayed name len checking.
	 */
	if (__predict_false(cnp->cn_namelen > NAME_MAX)) {
		cache_fpl_smr_exit(fpl);
		return (cache_fpl_handled_error(fpl, ENAMETOOLONG));
	}

	/*
	 * Hack: they may be looking up foo/bar, where foo is not a directory.
	 * In such a case we need to return ENOTDIR, but we may happen to get
	 * here with a different error.
	 */
	if (dvp->v_type != VDIR) {
		error = ENOTDIR;
	}

	/*
	 * Hack: handle O_SEARCH.
	 *
	 * Open Group Base Specifications Issue 7, 2018 edition states:
	 * <quote>
	 * If the access mode of the open file description associated with the
	 * file descriptor is not O_SEARCH, the function shall check whether
	 * directory searches are permitted using the current permissions of
	 * the directory underlying the file descriptor. If the access mode is
	 * O_SEARCH, the function shall not perform the check.
	 * </quote>
	 *
	 * Regular lookup tests for the NOEXECCHECK flag for every path
	 * component to decide whether to do the permission check. However,
	 * since most lookups never have the flag (and when they do it is only
	 * present for the first path component), lockless lookup only acts on
	 * it if there is a permission problem. Here the flag is represented
	 * with a boolean so that we don't have to clear it on the way out.
	 *
	 * For simplicity this always aborts.
	 * TODO: check if this is the first lookup and ignore the permission
	 * problem. Note the flag has to survive fallback (if it happens to be
	 * performed).
	 */
	if (fpl->fsearch) {
		return (cache_fpl_aborted(fpl));
	}

	switch (error) {
	case EAGAIN:
		if (!vn_seqc_consistent(dvp, dvp_seqc)) {
			error = cache_fpl_aborted(fpl);
		} else {
			cache_fpl_partial(fpl);
		}
		break;
	default:
		if (!vn_seqc_consistent(dvp, dvp_seqc)) {
			error = cache_fpl_aborted(fpl);
		} else {
			cache_fpl_smr_exit(fpl);
			cache_fpl_handled_error(fpl, error);
		}
		break;
	}
	return (error);
}

static int
cache_fplookup_impl(struct vnode *dvp, struct cache_fpl *fpl)
{
	struct nameidata *ndp;
	struct componentname *cnp;
	struct mount *mp;
	int error;

	ndp = fpl->ndp;
	cnp = fpl->cnp;

	cache_fpl_checkpoint(fpl);

	/*
	 * The vnode at hand is almost always stable, skip checking for it.
	 * Worst case this postpones the check towards the end of the iteration
	 * of the main loop.
	 */
	fpl->dvp = dvp;
	fpl->dvp_seqc = vn_seqc_read_notmodify(fpl->dvp);

	mp = atomic_load_ptr(&dvp->v_mount);
	if (__predict_false(mp == NULL || !cache_fplookup_mp_supported(mp))) {
		return (cache_fpl_aborted(fpl));
	}

	MPASS(fpl->tvp == NULL);

	for (;;) {
		cache_fplookup_parse(fpl);

		error = VOP_FPLOOKUP_VEXEC(fpl->dvp, cnp->cn_cred);
		if (__predict_false(error != 0)) {
			error = cache_fplookup_failed_vexec(fpl, error);
			break;
		}

		error = cache_fplookup_next(fpl);
		if (__predict_false(cache_fpl_terminated(fpl))) {
			break;
		}

		VNPASS(!seqc_in_modify(fpl->tvp_seqc), fpl->tvp);

		if (fpl->tvp->v_type == VLNK) {
			error = cache_fplookup_symlink(fpl);
			if (cache_fpl_terminated(fpl)) {
				break;
			}
		} else {
			if (cache_fpl_islastcn(ndp)) {
				error = cache_fplookup_final(fpl);
				break;
			}

			if (!vn_seqc_consistent(fpl->dvp, fpl->dvp_seqc)) {
				error = cache_fpl_aborted(fpl);
				break;
			}

			fpl->dvp = fpl->tvp;
			fpl->dvp_seqc = fpl->tvp_seqc;
			cache_fplookup_parse_advance(fpl);
		}

		cache_fpl_checkpoint(fpl);
	}

	return (error);
}

/*
 * Fast path lookup protected with SMR and sequence counters.
 *
 * Note: all VOP_FPLOOKUP_VEXEC routines have a comment referencing this one.
 *
 * Filesystems can opt in by setting the MNTK_FPLOOKUP flag and meeting criteria
 * outlined below.
 *
 * Traditional vnode lookup conceptually looks like this:
 *
 * vn_lock(current);
 * for (;;) {
 *	next = find();
 *	vn_lock(next);
 *	vn_unlock(current);
 *	current = next;
 *	if (last)
 *	    break;
 * }
 * return (current);
 *
 * Each jump to the next vnode is safe memory-wise and atomic with respect to
 * any modifications thanks to holding respective locks.
 *
 * The same guarantee can be provided with a combination of safe memory
 * reclamation and sequence counters instead. If all operations which affect
 * the relationship between the current vnode and the one we are looking for
 * also modify the counter, we can verify whether all the conditions held as
 * we made the jump. This includes things like permissions, mount points etc.
 * Counter modification is provided by enclosing relevant places in
 * vn_seqc_write_begin()/end() calls.
 *
 * Thus this translates to:
 *
 * vfs_smr_enter();
 * dvp_seqc = seqc_read_any(dvp);
 * if (seqc_in_modify(dvp_seqc)) // someone is altering the vnode
 *     abort();
 * for (;;) {
 * 	tvp = find();
 * 	tvp_seqc = seqc_read_any(tvp);
 * 	if (seqc_in_modify(tvp_seqc)) // someone is altering the target vnode
 * 	    abort();
 * 	if (!seqc_consistent(dvp, dvp_seqc) // someone is altering the vnode
 * 	    abort();
 * 	dvp = tvp; // we know nothing of importance has changed
 * 	dvp_seqc = tvp_seqc; // store the counter for the tvp iteration
 * 	if (last)
 * 	    break;
 * }
 * vget(); // secure the vnode
 * if (!seqc_consistent(tvp, tvp_seqc) // final check
 * 	    abort();
 * // at this point we know nothing has changed for any parent<->child pair
 * // as they were crossed during the lookup, meaning we matched the guarantee
 * // of the locked variant
 * return (tvp);
 *
 * The API contract for VOP_FPLOOKUP_VEXEC routines is as follows:
 * - they are called while within vfs_smr protection which they must never exit
 * - EAGAIN can be returned to denote checking could not be performed, it is
 *   always valid to return it
 * - if the sequence counter has not changed the result must be valid
 * - if the sequence counter has changed both false positives and false negatives
 *   are permitted (since the result will be rejected later)
 * - for simple cases of unix permission checks vaccess_vexec_smr can be used
 *
 * Caveats to watch out for:
 * - vnodes are passed unlocked and unreferenced with nothing stopping
 *   VOP_RECLAIM, in turn meaning that ->v_data can become NULL. It is advised
 *   to use atomic_load_ptr to fetch it.
 * - the aforementioned object can also get freed, meaning absent other means it
 *   should be protected with vfs_smr
 * - either safely checking permissions as they are modified or guaranteeing
 *   their stability is left to the routine
 */
int
cache_fplookup(struct nameidata *ndp, enum cache_fpl_status *status,
    struct pwd **pwdp)
{
	struct cache_fpl fpl;
	struct pwd *pwd;
	struct vnode *dvp;
	struct componentname *cnp;
	int error;

	fpl.status = CACHE_FPL_STATUS_UNSET;
	fpl.in_smr = false;
	fpl.ndp = ndp;
	fpl.cnp = cnp = &ndp->ni_cnd;
	MPASS(ndp->ni_lcf == 0);
	KASSERT ((cnp->cn_flags & CACHE_FPL_INTERNAL_CN_FLAGS) == 0,
	    ("%s: internal flags found in cn_flags %" PRIx64, __func__,
	    cnp->cn_flags));
	MPASS(cnp->cn_nameptr == cnp->cn_pnbuf);
	MPASS(ndp->ni_resflags == 0);

	if (__predict_false(!cache_can_fplookup(&fpl))) {
		*status = fpl.status;
		SDT_PROBE3(vfs, fplookup, lookup, done, ndp, fpl.line, fpl.status);
		return (EOPNOTSUPP);
	}

	cache_fpl_checkpoint_outer(&fpl);

	cache_fpl_smr_enter_initial(&fpl);
#ifdef INVARIANTS
	fpl.debug.ni_pathlen = ndp->ni_pathlen;
#endif
	fpl.nulchar = &cnp->cn_nameptr[ndp->ni_pathlen - 1];
	fpl.fsearch = false;
	fpl.tvp = NULL; /* for degenerate path handling */
	fpl.pwd = pwdp;
	pwd = pwd_get_smr();
	*(fpl.pwd) = pwd;
	namei_setup_rootdir(ndp, cnp, pwd);
	ndp->ni_topdir = pwd->pwd_jdir;

	if (cnp->cn_pnbuf[0] == '/') {
		dvp = cache_fpl_handle_root(&fpl);
		ndp->ni_resflags = NIRES_ABS;
	} else {
		if (ndp->ni_dirfd == AT_FDCWD) {
			dvp = pwd->pwd_cdir;
		} else {
			error = cache_fplookup_dirfd(&fpl, &dvp);
			if (__predict_false(error != 0)) {
				goto out;
			}
		}
	}

	SDT_PROBE4(vfs, namei, lookup, entry, dvp, cnp->cn_pnbuf, cnp->cn_flags, true);
	error = cache_fplookup_impl(dvp, &fpl);
out:
	cache_fpl_smr_assert_not_entered(&fpl);
	cache_fpl_assert_status(&fpl);
	*status = fpl.status;
	SDT_PROBE3(vfs, fplookup, lookup, done, ndp, fpl.line, fpl.status);
	if (__predict_true(fpl.status == CACHE_FPL_STATUS_HANDLED)) {
		MPASS(error != CACHE_FPL_FAILED);
		SDT_PROBE4(vfs, namei, lookup, return, error, ndp->ni_vp, true,
		    ndp);
		if (error != 0) {
			cache_fpl_cleanup_cnp(fpl.cnp);
			MPASS(fpl.dvp == NULL);
			MPASS(fpl.tvp == NULL);
		}
		ndp->ni_dvp = fpl.dvp;
		ndp->ni_vp = fpl.tvp;
	}
	return (error);
}
