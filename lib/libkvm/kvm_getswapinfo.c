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

#define NL_SWAPBLIST	0
#define NL_SWDEVT	1
#define NL_NSWDEV	2
#define NL_DMMAX	3

static int kvm_swap_nl_cached = 0;
static int nswdev;
static int unswdev;  /* number of found swap dev's */
static int dmmax;

static int kvm_getswapinfo2(kvm_t *kd, struct kvm_swap *swap_ary,
			    int swap_max, int flags);
static int  kvm_getswapinfo_kvm(kvm_t *, struct kvm_swap *, int, int);
static int  kvm_getswapinfo_sysctl(kvm_t *, struct kvm_swap *, int, int);
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

	/*
	 * clear cache
	 */
	if (kd == NULL) {
		kvm_swap_nl_cached = 0;
		return(0);
	}

	if (ISALIVE(kd)) {
		return kvm_getswapinfo_sysctl(kd, swap_ary, swap_max, flags);
	} else {
		return -1;
	}
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
