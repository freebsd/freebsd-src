/* usual BSD style copyright here */
/* Written by Julian Elischer (julian@dialix.oz.au)*/
/*
 * $Id: devfsext.h,v 1.15 1997/02/22 09:44:59 peter Exp $
 */

#ifndef _SYS_DEVFSECT_H_
#define	_SYS_DEVFSECT_H_

void	*devfs_add_devswf __P((void *devsw, int minor, int chrblk, uid_t uid,
			       gid_t gid, int perms, char *fmt, ...)); 
void	*devfs_link __P((void *original, char *fmt, ...));
void	devfs_remove_dev __P((void *devnmp));

#define DV_CHR 0
#define DV_BLK 1
#define DV_DEV 2

/* XXX */
#define	UID_ROOT	0
#define	UID_BIN		3
#define	UID_UUCP	66

/* XXX */
#define	GID_WHEEL	0
#define	GID_KMEM	2
#define	GID_OPERATOR	5
#define	GID_BIN		7
#define	GID_DIALER	68

#endif /* !_SYS_DEVFSECT_H_ */
