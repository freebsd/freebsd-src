/*
 *  linux/fs/filesystems.c
 *
 *  Copyright (C) 1991, 1992  Linus Torvalds
 *
 *  nfsservctl system-call when nfsd is not compiled in.
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/smp_lock.h>
#include <linux/kmod.h>
#include <linux/nfsd/interface.h>

#if ! defined(CONFIG_NFSD)
struct nfsd_linkage *nfsd_linkage;

long
asmlinkage sys_nfsservctl(int cmd, void *argp, void *resp)
{
	int ret = -ENOSYS;
	
#if defined(CONFIG_MODULES)
	lock_kernel();

	if (nfsd_linkage ||
	    (request_module ("nfsd") == 0 && nfsd_linkage)) {
		__MOD_INC_USE_COUNT(nfsd_linkage->owner);
		unlock_kernel();
		ret = nfsd_linkage->do_nfsservctl(cmd, argp, resp);
		__MOD_DEC_USE_COUNT(nfsd_linkage->owner);
	} else
		unlock_kernel();
#endif
	return ret;
}
EXPORT_SYMBOL(nfsd_linkage);

#endif /* CONFIG_NFSD */
