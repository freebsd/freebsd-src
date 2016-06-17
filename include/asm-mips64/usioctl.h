/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * usema/usemaclone-related stuff.
 *
 * `Inspired' by IRIX's sys/usioctl.h
 *
 * Mike.
 */
#ifndef _ASM_USIOCTL_H
#define _ASM_USIOCTL_H

/* ioctls */
#define UIOC	('u' << 16 | 's' << 8)

#define UIOCATTACHSEMA	(UIOC|2)	/* attach to sema */
#define UIOCBLOCK	(UIOC|3)	/* block sync "intr"? */
#define UIOCABLOCK	(UIOC|4)	/* block async */
#define UIOCNOIBLOCK	(UIOC|5)	/* IRIX: block sync intr
					   Linux: block sync nointr */
#define UIOCUNBLOCK	(UIOC|6)	/* unblock sync */
#define UIOCAUNBLOCK	(UIOC|7)	/* unblock async */
#define UIOCINIT	(UIOC|8)	/* init sema (async) */

typedef struct usattach_s {
  dev_t	us_dev;		/* attach dev */
  void	*us_handle;	/* userland semaphore handle */
} usattach_t;

#endif /* _ASM_USIOCTL_H */
