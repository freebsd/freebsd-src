/*
 * Copyright (c) 1999, Matthew Dillon.  All Rights Reserved.
 * Copyright (c) 2001, Thomas Moestl
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided under the terms of the BSD
 * Copyright as found in /usr/src/COPYRIGHT in the FreeBSD source tree.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/blist.h>
#include <sys/sysctl.h>

#include <vm/vm_param.h>
#include <vm/swap_pager.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <kvm.h>
#include <nlist.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#include "kvm_private.h"

static struct nlist kvm_swap_nl[] = {
	{ "_swapblist" },	/* new radix swap list		*/
	{ "_swdevt" },		/* list of swap devices and sizes */
	{ "_nswdev" },		/* number of swap devices */
	{ "_dmmax" },		/* maximum size of a swap block */
	{ "" }
};

#define NL_SWAPBLIST	0
#define NL_SWDEVT	1
#define NL_NSWDEV	2
#define NL_DMMAX	3

static int kvm_swap_nl_cached = 0;
static int nswdev;
static int unswdev;  /* number of found swap dev's */
static int dmmax;

static void getswapinfo_radix(kvm_t *kd, struct kvm_swap *swap_ary,
			      int swap_max, int flags);
static int kvm_getswapinfo2(kvm_t *kd, struct kvm_swap *swap_ary,
			    int swap_max, int flags);
static int  kvm_getswapinfo_kvm(kvm_t *, struct kvm_swap *, int, int);
static int  kvm_getswapinfo_sysctl(kvm_t *, struct kvm_swap *, int, int);
static int  nlist_init(kvm_t *);
static int  getsysctl(kvm_t *, char *, void *, size_t);

#define	SVAR(var) __STRING(var)	/* to force expansion */
#define	KGET(idx, var)							\
	KGET1(idx, &var, sizeof(var), SVAR(var))
#define	KGET1(idx, p, s, msg)						\
	KGET2(kvm_swap_nl[idx].n_value, p, s, msg)
#define	KGET2(addr, p, s, msg)						\
	if (kvm_read(kd, (u_long)(addr), p, s) != s)			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd))
#define	KGETN(idx, var)							\
	KGET1N(idx, &var, sizeof(var), SVAR(var))
#define	KGET1N(idx, p, s, msg)						\
	KGET2N(kvm_swap_nl[idx].n_value, p, s, msg)
#define	KGET2N(addr, p, s, msg)						\
	((kvm_read(kd, (u_long)(addr), p, s) == s) ? 1 : 0)
#define	KGETRET(addr, p, s, msg)					\
	if (kvm_read(kd, (u_long)(addr), p, s) != s) {			\
		warnx("cannot read %s: %s", msg, kvm_geterr(kd));	\
		return (0);						\
	}

#define GETSWDEVNAME(dev, str, flags)					\
	if (dev == NODEV) {						\
		strlcpy(str, "[NFS swap]", sizeof(str));		\
	} else {							\
		snprintf(						\
		    str, sizeof(str),"%s%s",				\
		    ((flags & SWIF_DEV_PREFIX) ? _PATH_DEV : ""),	\
		    devname(dev, S_IFCHR)				\
		);							\
	}

int
kvm_getswapinfo(
	kvm_t *kd, 
	struct kvm_swap *swap_ary,
	int swap_max, 
	int flags
) {
	int rv;
#ifdef DEBUG_SWAPINFO
	int i;
#endif

	/*
	 * clear cache
	 */
	if (kd == NULL) {
		kvm_swap_nl_cached = 0;
		return(0);
	}

	rv = kvm_getswapinfo2(kd, swap_ary, swap_max, flags);

	/* This is only called when the tree shall be dumped. It needs kvm. */
	if (flags & SWIF_DUMP_TREE) {
#ifdef DEBUG_SWAPINFO
		/* 
		 * sanity check: Sizes must be equal - used field must be
		 * 0 after this. Fill it with total-used before, where
		 * getswapinfo_radix will subtrat total-used.
		 * This will of course only work if there is no swap activity
		 * while we are working, so this code is normally not active.
		 */
		for (i = 0; i < unswdev; i++) {
			swap_ary[i].ksw_used =  swap_ary[i].ksw_total - 
			    swap_ary[i].ksw_used;
		}
#endif
		getswapinfo_radix(kd, swap_ary, swap_max, flags);
#ifdef DEBUG_SWAPINFO
		for (i = 0; i < unswdev; i++) {
			if (swap_ary[i].ksw_used != 0) {
				fprintf(stderr, "kvm_getswapinfo: swap size "
				    "mismatch (%d blocks)!\n", 
				    swap_ary[i].ksw_used
				);
			}
		}
		/* This is fast enough now, so just do it again. */
		rv = kvm_getswapinfo2(kd, swap_ary, swap_max, flags);
#endif
	}

	return rv;
}

static int
kvm_getswapinfo2(
	kvm_t *kd, 
	struct kvm_swap *swap_ary,
	int swap_max, 
	int flags
) {
	if (ISALIVE(kd)) {
		return kvm_getswapinfo_sysctl(kd, swap_ary, swap_max, flags);
	} else {
		return kvm_getswapinfo_kvm(kd, swap_ary, swap_max, flags);
	}
}

int
kvm_getswapinfo_kvm(
	kvm_t *kd, 
	struct kvm_swap *swap_ary,
	int swap_max, 
	int flags
) {
	int ti = 0;

	/*
	 * namelist
	 */
	if (!nlist_init(kd))
		return (-1);

	{
		struct swdevt *sw;
		int i;

		ti = unswdev;
		if (ti >= swap_max)
			ti = swap_max - 1;

		if (ti >= 0)
			bzero(swap_ary, sizeof(struct kvm_swap) * (ti + 1));

		KGET(NL_SWDEVT, sw);
		for (i = 0; i < unswdev; ++i) {
			struct swdevt swinfo;
			int ttl;

			KGET2(&sw[i], &swinfo, sizeof(swinfo), "swinfo");

			/*
			 * old style: everything in DEV_BSIZE'd chunks,
			 * convert to pages.
			 *
			 * new style: swinfo in DEV_BSIZE'd chunks but dmmax
			 * in pages.
			 *
			 * The first dmmax is never allocating to avoid 
			 * trashing the disklabels
			 */

			ttl = swinfo.sw_nblks - dmmax;

			if (ttl == 0)
				continue;

			if (i < ti) {
				swap_ary[i].ksw_total = ttl;
				swap_ary[i].ksw_used = swinfo.sw_used;
				swap_ary[i].ksw_flags = swinfo.sw_flags;
				GETSWDEVNAME(swinfo.sw_dev, 
				    swap_ary[i].ksw_devname, flags
				);
			}
			if (ti >= 0) {
				swap_ary[ti].ksw_total += ttl;
				swap_ary[ti].ksw_used += swinfo.sw_used;
			}
		}
	}

	return(ti);
}

/*
 * scanradix() - support routine for radix scanner
 */

#define TABME	tab, tab, ""

static int
scanradix(
	blmeta_t *scan, 
	daddr_t blk,
	daddr_t radix,
	daddr_t skip, 
	daddr_t count,
	kvm_t *kd,
	int dmmax, 
	int nswdev,
	struct kvm_swap *swap_ary,
	int swap_max,
	int tab,
	int flags
) {
	blmeta_t meta;
#ifdef DEBUG_SWAPINFO
	int ti = (unswdev >= swap_max) ? swap_max - 1 : unswdev;
#endif

	KGET2(scan, &meta, sizeof(meta), "blmeta_t");

	/*
	 * Terminator
	 */
	if (meta.bm_bighint == (daddr_t)-1) {
		if (flags & SWIF_DUMP_TREE) {
			printf("%*.*s(0x%06x,%d) Terminator\n", 
			    TABME,
			    blk, 
			    radix
			);
		}
		return(-1);
	}

	if (radix == BLIST_BMAP_RADIX) {
		/*
		 * Leaf bitmap
		 */
#ifdef DEBUG_SWAPINFO
		int i;
#endif

		if (flags & SWIF_DUMP_TREE) {
			printf("%*.*s(0x%06x,%d) Bitmap %08x big=%d\n", 
			    TABME,
			    blk, 
			    radix,
			    (int)meta.u.bmu_bitmap,
			    meta.bm_bighint
			);
		}

#ifdef DEBUG_SWAPINFO
		/*
		 * If not all allocated, count.
		 */
		if (meta.u.bmu_bitmap != 0) {
			for (i = 0; i < BLIST_BMAP_RADIX && i < count; ++i) {
				/*
				 * A 0 bit means allocated
				 */
				if ((meta.u.bmu_bitmap & (1 << i))) {
					int t = 0;

					if (nswdev)
						t = (blk + i) / dmmax % nswdev;
					if (t < ti)
						--swap_ary[t].ksw_used;
					if (ti >= 0)
						--swap_ary[ti].ksw_used;
				}
			}
		}
#endif
	} else if (meta.u.bmu_avail == radix) {
		/*
		 * Meta node if all free
		 */
		if (flags & SWIF_DUMP_TREE) {
			printf("%*.*s(0x%06x,%d) Submap ALL-FREE {\n", 
			    TABME,
			    blk, 
			    radix
			);
		}
#ifdef DEBUG_SWAPINFO
		/*
		 * Note: both dmmax and radix are powers of 2.  However, dmmax
		 * may be larger then radix so use a smaller increment if
		 * necessary.
		 */
		{
			int t;
			int tinc = dmmax;

			while (tinc > radix)
				tinc >>= 1;

			for (t = blk; t < blk + radix; t += tinc) {
				int u = (nswdev) ? (t / dmmax % nswdev) : 0;

				if (u < ti)
					swap_ary[u].ksw_used -= tinc;
				if (ti >= 0)
					swap_ary[ti].ksw_used -= tinc;
			}
		}
#endif
	} else if (meta.u.bmu_avail == 0) {
		/*
		 * Meta node if all used
		 */
		if (flags & SWIF_DUMP_TREE) {
			printf("%*.*s(0x%06x,%d) Submap ALL-ALLOCATED\n", 
			    TABME,
			    blk, 
			    radix
			);
		}
	} else {
		/*
		 * Meta node if not all free
		 */
		int i;
		int next_skip;

		if (flags & SWIF_DUMP_TREE) {
			printf("%*.*s(0x%06x,%d) Submap avail=%d big=%d {\n", 
			    TABME,
			    blk, 
			    radix,
			    (int)meta.u.bmu_avail,
			    meta.bm_bighint
			);
		}

		radix >>= BLIST_META_RADIX_SHIFT;
		next_skip = skip >> BLIST_META_RADIX_SHIFT;

		for (i = 1; i <= skip; i += next_skip) {
			int r;
			daddr_t vcount = (count > radix) ? radix : count;

			r = scanradix(
			    &scan[i],
			    blk,
			    radix,
			    next_skip - 1,
			    vcount,
			    kd,
			    dmmax,
			    nswdev,
			    swap_ary,
			    swap_max,
			    tab + 4,
			    flags
			);
			if (r < 0)
				break;
			blk += radix;
		}
		if (flags & SWIF_DUMP_TREE) {
			printf("%*.*s}\n", TABME);
		}
	}
	return(0);
}

static void
getswapinfo_radix(kvm_t *kd, struct kvm_swap *swap_ary, int swap_max, int flags)
{
	struct blist *swapblist = NULL;
	struct blist blcopy = { 0 };

	if (!nlist_init(kd)) {
		fprintf(stderr, "radix tree: nlist_init failed!\n");
		return;
	}

	KGET(NL_SWAPBLIST, swapblist);

	if (swapblist == NULL) {
		if (flags & SWIF_DUMP_TREE)
			printf("radix tree: NULL - no swap in system\n");
		return;
	}

	KGET2(swapblist, &blcopy, sizeof(blcopy), "*swapblist");

	if (flags & SWIF_DUMP_TREE) {
		printf("radix tree: %d/%d/%d blocks, %dK wired\n",
			blcopy.bl_free,
			blcopy.bl_blocks,
			blcopy.bl_radix,
			(int)((blcopy.bl_rootblks * sizeof(blmeta_t) + 1023)/
			    1024)
		);
	}
	scanradix(
	    blcopy.bl_root, 
	    0, 
	    blcopy.bl_radix, 
	    blcopy.bl_skip, 
	    blcopy.bl_rootblks, 
	    kd,
	    dmmax,
	    nswdev, 
	    swap_ary,
	    swap_max,
	    0,
	    flags
	);
}

#define	GETSYSCTL(kd, name, var)					\
	    getsysctl(kd, name, &(var), sizeof(var))

/* The maximum MIB length for vm.swap_info and an additional device number */
#define	SWI_MAXMIB	3

int
kvm_getswapinfo_sysctl(
	kvm_t *kd, 
	struct kvm_swap *swap_ary,
	int swap_max, 
	int flags
) {
	int ti, ttl;
	size_t mibi, len;
	int soid[SWI_MAXMIB];
	struct xswdev xsd;
	struct kvm_swap tot;

	if (!GETSYSCTL(kd, "vm.dmmax", dmmax))
		return -1;

	mibi = SWI_MAXMIB - 1;
	if (sysctlnametomib("vm.swap_info", soid, &mibi) == -1) {
		_kvm_err(kd, kd->program, "sysctlnametomib failed: %s",
		    strerror(errno));
		return -1;
	}
	bzero(&tot, sizeof(tot));
	for (unswdev = 0;; unswdev++) {
		soid[mibi] = unswdev;
		len = sizeof(xsd);
		if (sysctl(soid, mibi + 1, &xsd, &len, NULL, 0) == -1) {
			if (errno == ENOENT)
				break;
			_kvm_err(kd, kd->program, "cannot read sysctl: %s.",
			    strerror(errno));
			return -1;
		}
		if (len != sizeof(xsd)) {
			_kvm_err(kd, kd->program, "struct xswdev has unexpected "
			    "size;  kernel and libkvm out of sync?");
			return -1;
		}
		if (xsd.xsw_version != XSWDEV_VERSION) {
			_kvm_err(kd, kd->program, "struct xswdev version "
			    "mismatch; kernel and libkvm out of sync?");
			return -1;
		}

		ttl = xsd.xsw_nblks - dmmax;
		if (unswdev < swap_max - 1) {
			bzero(&swap_ary[unswdev], sizeof(swap_ary[unswdev]));
			swap_ary[unswdev].ksw_total = ttl;
			swap_ary[unswdev].ksw_used = xsd.xsw_used;
			swap_ary[unswdev].ksw_flags = xsd.xsw_flags;
			GETSWDEVNAME(xsd.xsw_dev, swap_ary[unswdev].ksw_devname,
			     flags);
		}
		tot.ksw_total += ttl;
		tot.ksw_used += xsd.xsw_used;
	}

	ti = unswdev;
	if (ti >= swap_max)
		ti = swap_max - 1;
	if (ti >= 0)
		swap_ary[ti] = tot;

        return(ti);
}

static int
nlist_init (
	kvm_t *kd
) {
	struct swdevt *sw;

	if (kvm_swap_nl_cached)
		return (1);

	if (kvm_nlist(kd, kvm_swap_nl) < 0)
		return (0);
	
	/*
	 * required entries
	 */
	if (
	    kvm_swap_nl[NL_SWDEVT].n_value == 0 ||
	    kvm_swap_nl[NL_NSWDEV].n_value == 0 ||
	    kvm_swap_nl[NL_DMMAX].n_value == 0 ||
	    kvm_swap_nl[NL_SWAPBLIST].n_type == 0
	   ) {
		return (0);
	}
	
	/*
	 * get globals, type of swap
	 */
	KGET(NL_NSWDEV, nswdev);
	KGET(NL_DMMAX, dmmax);

	/*
	 * figure out how many actual swap devices are enabled
	 */
	KGET(NL_SWDEVT, sw);
	for (unswdev = nswdev - 1; unswdev >= 0; --unswdev) {
		struct swdevt swinfo;
		
		KGET2(&sw[unswdev], &swinfo, sizeof(swinfo), "swinfo");
		if (swinfo.sw_nblks)
			break;
	}
	++unswdev;

	kvm_swap_nl_cached = 1;
	return (1);
}

static int
getsysctl (
	kvm_t *kd,
	char *name,
	void *ptr,
	size_t len
) {
	size_t nlen = len;
	if (sysctlbyname(name, ptr, &nlen, NULL, 0) == -1) {
		_kvm_err(kd, kd->program, "cannot read sysctl %s:%s", name,
		    strerror(errno));
		return (0);
	}
	if (nlen != len) {
		_kvm_err(kd, kd->program, "sysctl %s has unexpected size", name);
		return (0);
	}
	return (1);
}
