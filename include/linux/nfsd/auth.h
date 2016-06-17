/*
 * include/linux/nfsd/auth.h
 *
 * nfsd-specific authentication stuff.
 * uid/gid mapping not yet implemented.
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#ifndef LINUX_NFSD_AUTH_H
#define LINUX_NFSD_AUTH_H

#ifdef __KERNEL__

#define nfsd_luid(rq, uid)	((u32)(uid))
#define nfsd_lgid(rq, gid)	((u32)(gid))
#define nfsd_ruid(rq, uid)	((u32)(uid))
#define nfsd_rgid(rq, gid)	((u32)(gid))

/*
 * Set the current process's fsuid/fsgid etc to those of the NFS
 * client user
 */
void		nfsd_setuser(struct svc_rqst *, struct svc_export *);

#if 0
/*
 * These must match the actual size of uid_t and gid_t
 */
#define UGID_BITS		(8 * sizeof(uid_t))
#define UGID_SHIFT		8
#define UGID_MASK		((1 << UGID_SHIFT) - 1)
#define UGID_NRENTRIES		((1 << (UGID_BITS - UGID_SHIFT)) + 1)
#define UGID_NONE		((unsigned short)-1)

typedef struct svc_uidmap {
	uid_t *			um_ruid[UGID_NRENTRIES];
	uid_t *			um_luid[UGID_NRENTRIES];
	gid_t *			um_rgid[UGID_NRENTRIES];
	gid_t *			um_lgid[UGID_NRENTRIES];
} svc_uidmap;
#endif

#endif /* __KERNEL__ */
#endif /* LINUX_NFSD_AUTH_H */
