/*
 * linux/fs/nfsd/nfsctl.c
 *
 * Syscall interface to knfsd.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/config.h>
#include <linux/module.h>
#include <linux/version.h>

#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/fcntl.h>
#include <linux/net.h>
#include <linux/in.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>

#include <linux/nfs.h>
#include <linux/sunrpc/svc.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/cache.h>
#include <linux/nfsd/xdr.h>
#include <linux/nfsd/syscall.h>

#include <asm/uaccess.h>
#include <linux/smp.h>
#include <linux/smp_lock.h>
#include <linux/init.h>

static int	nfsctl_svc(struct nfsctl_svc *data);
static int	nfsctl_addclient(struct nfsctl_client *data);
static int	nfsctl_delclient(struct nfsctl_client *data);
static int	nfsctl_export(struct nfsctl_export *data);
static int	nfsctl_unexport(struct nfsctl_export *data);
static int	nfsctl_getfh(struct nfsctl_fhparm *, __u8 *);
static int	nfsctl_getfd(struct nfsctl_fdparm *, __u8 *);
static int	nfsctl_getfs(struct nfsctl_fsparm *, struct knfsd_fh *);
#ifdef notyet
static int	nfsctl_ugidupdate(struct nfsctl_ugidmap *data);
#endif

extern struct seq_operations nfs_exports_op;
static int exports_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &nfs_exports_op);
}
static struct file_operations exports_operations = {
	open:		exports_open,
	read:		seq_read,
	llseek:		seq_lseek,
	release:	seq_release,
};

void proc_export_init(void)
{
	struct proc_dir_entry *entry;
	if (!proc_mkdir("fs/nfs", 0))
		return;
	entry = create_proc_entry("fs/nfs/exports", 0, NULL);
	if (entry)
		entry->proc_fops =  &exports_operations;
}

static inline int
nfsctl_svc(struct nfsctl_svc *data)
{
	return nfsd_svc(data->svc_port, data->svc_nthreads);
}

static inline int
nfsctl_addclient(struct nfsctl_client *data)
{
	return exp_addclient(data);
}

static inline int
nfsctl_delclient(struct nfsctl_client *data)
{
	return exp_delclient(data);
}

static inline int
nfsctl_export(struct nfsctl_export *data)
{
	return exp_export(data);
}

static inline int
nfsctl_unexport(struct nfsctl_export *data)
{
	return exp_unexport(data);
}

#ifdef notyet
static inline int
nfsctl_ugidupdate(nfs_ugidmap *data)
{
	return -EINVAL;
}
#endif

static inline int
nfsctl_getfs(struct nfsctl_fsparm *data, struct knfsd_fh *res)
{
	struct sockaddr_in	*sin;
	struct svc_client	*clp;
	int			err = 0;

	if (data->gd_addr.sa_family != AF_INET)
		return -EPROTONOSUPPORT;
	sin = (struct sockaddr_in *)&data->gd_addr;
	if (data->gd_maxlen > NFS3_FHSIZE)
		data->gd_maxlen = NFS3_FHSIZE;
	exp_readlock();
	if (!(clp = exp_getclient(sin)))
		err = -EPERM;
	else
		err = exp_rootfh(clp, 0, 0, data->gd_path, res, data->gd_maxlen);
	exp_unlock();
	return err;
}

static inline int
nfsctl_getfd(struct nfsctl_fdparm *data, __u8 *res)
{
	struct sockaddr_in	*sin;
	struct svc_client	*clp;
	int			err = 0;
	struct	knfsd_fh	fh;

	if (data->gd_addr.sa_family != AF_INET)
		return -EPROTONOSUPPORT;
	if (data->gd_version < 2 || data->gd_version > NFSSVC_MAXVERS)
		return -EINVAL;
	sin = (struct sockaddr_in *)&data->gd_addr;

	exp_readlock();
	if (!(clp = exp_getclient(sin)))
		err = -EPERM;
	else
		err = exp_rootfh(clp, 0, 0, data->gd_path, &fh, NFS_FHSIZE);
	exp_unlock();

	if (err == 0) {
		if (fh.fh_size > NFS_FHSIZE)
			err = -EINVAL;
		else {
			memset(res,0, NFS_FHSIZE);
			memcpy(res, &fh.fh_base, fh.fh_size);
		}
	}

	return err;
}

static inline int
nfsctl_getfh(struct nfsctl_fhparm *data, __u8 *res)
{
	struct sockaddr_in	*sin;
	struct svc_client	*clp;
	int			err = 0;
	struct knfsd_fh		fh;

	if (data->gf_addr.sa_family != AF_INET)
		return -EPROTONOSUPPORT;
	if (data->gf_version < 2 || data->gf_version > NFSSVC_MAXVERS)
		return -EINVAL;
	sin = (struct sockaddr_in *)&data->gf_addr;

	exp_readlock();
	if (!(clp = exp_getclient(sin)))
		err = -EPERM;
	else
		err = exp_rootfh(clp, to_kdev_t(data->gf_dev), data->gf_ino, NULL, &fh, NFS_FHSIZE);
	exp_unlock();

	if (err == 0) {
		if (fh.fh_size > NFS_FHSIZE)
			err = -EINVAL;
		else {
			memset(res,0, NFS_FHSIZE);
			memcpy(res, &fh.fh_base, fh.fh_size);
		}
	}

	return err;
}

#ifdef CONFIG_NFSD
#define handle_sys_nfsservctl sys_nfsservctl
#endif

static struct {
	int argsize, respsize;
}  sizes[] = {
	/* NFSCTL_SVC        */ { sizeof(struct nfsctl_svc), 0 },
	/* NFSCTL_ADDCLIENT  */ { sizeof(struct nfsctl_client), 0},
	/* NFSCTL_DELCLIENT  */ { sizeof(struct nfsctl_client), 0},
	/* NFSCTL_EXPORT     */ { sizeof(struct nfsctl_export), 0},
	/* NFSCTL_UNEXPORT   */ { sizeof(struct nfsctl_export), 0},
	/* NFSCTL_UGIDUPDATE */ { sizeof(struct nfsctl_uidmap), 0},
	/* NFSCTL_GETFH      */ { sizeof(struct nfsctl_fhparm), NFS_FHSIZE},
	/* NFSCTL_GETFD      */ { sizeof(struct nfsctl_fdparm), NFS_FHSIZE},
	/* NFSCTL_GETFS      */ { sizeof(struct nfsctl_fsparm), sizeof(struct knfsd_fh)},
};
#define CMD_MAX (sizeof(sizes)/sizeof(sizes[0])-1)

long
asmlinkage handle_sys_nfsservctl(int cmd, void *opaque_argp, void *opaque_resp)
{
	struct nfsctl_arg *	argp = opaque_argp;
	union nfsctl_res *	resp = opaque_resp;
	struct nfsctl_arg *	arg = NULL;
	union nfsctl_res *	res = NULL;
	int			err;
	int			argsize, respsize;

	lock_kernel ();

	err = -EPERM;
	if (!capable(CAP_SYS_ADMIN)) {
		goto done;
	}
	err = -EINVAL;
	if (cmd<0 || cmd > CMD_MAX)
		goto done;
	err = -EFAULT;
	argsize = sizes[cmd].argsize + (int)&((struct nfsctl_arg *)0)->u;
	respsize = sizes[cmd].respsize;	/* maximum */
	if (!access_ok(VERIFY_READ, argp, argsize)
	 || (resp && !access_ok(VERIFY_WRITE, resp, respsize))) {
		goto done;
	}
	err = -ENOMEM;	/* ??? */
	if (!(arg = kmalloc(sizeof(*arg), GFP_USER)) ||
	    (resp && !(res = kmalloc(sizeof(*res), GFP_USER)))) {
		goto done;
	}

	err = -EINVAL;
	copy_from_user(arg, argp, argsize);
	if (arg->ca_version != NFSCTL_VERSION) {
		printk(KERN_WARNING "nfsd: incompatible version in syscall.\n");
		goto done;
	}

	switch(cmd) {
	case NFSCTL_SVC:
		err = nfsctl_svc(&arg->ca_svc);
		break;
	case NFSCTL_ADDCLIENT:
		err = nfsctl_addclient(&arg->ca_client);
		break;
	case NFSCTL_DELCLIENT:
		err = nfsctl_delclient(&arg->ca_client);
		break;
	case NFSCTL_EXPORT:
		err = nfsctl_export(&arg->ca_export);
		break;
	case NFSCTL_UNEXPORT:
		err = nfsctl_unexport(&arg->ca_export);
		break;
#ifdef notyet
	case NFSCTL_UGIDUPDATE:
		err = nfsctl_ugidupdate(&arg->ca_umap);
		break;
#endif
	case NFSCTL_GETFH:
		err = nfsctl_getfh(&arg->ca_getfh, res->cr_getfh);
		break;
	case NFSCTL_GETFD:
		err = nfsctl_getfd(&arg->ca_getfd, res->cr_getfh);
		break;
	case NFSCTL_GETFS:
		err = nfsctl_getfs(&arg->ca_getfs, &res->cr_getfs);
		respsize = res->cr_getfs.fh_size+ (int)&((struct knfsd_fh*)0)->fh_base;
		break;
	default:
		err = -EINVAL;
	}

	if (!err && resp && respsize)
		copy_to_user(resp, res, respsize);

done:
	if (arg)
		kfree(arg);
	if (res)
		kfree(res);

	unlock_kernel ();
	return err;
}

EXPORT_NO_SYMBOLS;
MODULE_AUTHOR("Olaf Kirch <okir@monad.swb.de>");
MODULE_LICENSE("GPL");

#ifdef MODULE
struct nfsd_linkage nfsd_linkage_s = {
	do_nfsservctl: handle_sys_nfsservctl,
	owner: THIS_MODULE,
};
#endif

/*
 * Initialize the module
 */
static int __init
nfsd_init(void)
{
	printk(KERN_INFO "Installing knfsd (copyright (C) 1996 okir@monad.swb.de).\n");
#ifdef MODULE
	nfsd_linkage = &nfsd_linkage_s;
#endif
	nfsd_stat_init();	/* Statistics */
	nfsd_cache_init();	/* RPC reply cache */
	nfsd_export_init();	/* Exports table */
	nfsd_lockd_init();	/* lockd->nfsd callbacks */
	proc_export_init();
	return 0;
}

/*
 * Clean up the mess before unloading the module
 */
static void __exit
nfsd_exit(void)
{
#ifdef MODULE
	nfsd_linkage = NULL;
#endif
	nfsd_export_shutdown();
	nfsd_cache_shutdown();
	remove_proc_entry("fs/nfs/exports", NULL);
	remove_proc_entry("fs/nfs", NULL);
	nfsd_stat_shutdown();
	nfsd_lockd_shutdown();
}

module_init(nfsd_init);
module_exit(nfsd_exit);
