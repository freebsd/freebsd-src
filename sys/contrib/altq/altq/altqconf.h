/*	$OpenBSD: altqconf.h,v 1.1 2001/06/27 05:28:36 kjc Exp $	*/
/*	$NetBSD: altqconf.h,v 1.2 2001/05/30 11:57:16 mrg Exp $	*/

#if defined(_KERNEL_OPT) || defined(__OpenBSD__)

#if defined(_KERNEL_OPT)
#include "opt_altq_enabled.h"
#endif

#include <sys/conf.h>

#ifdef ALTQ
#define	NALTQ	1
#else
#define	NALTQ	0
#endif

cdev_decl(altq);

#ifdef __OpenBSD__
#define cdev_altq_init(c,n) { \
	dev_init(c,n,open), dev_init(c,n,close), (dev_type_read((*))) enodev, \
	(dev_type_write((*))) enodev, dev_init(c,n,ioctl), \
	(dev_type_stop((*))) enodev, 0, (dev_type_select((*))) enodev, \
	(dev_type_mmap((*))) enodev }
#else
#define	cdev_altq_init(x,y)	cdev__oci_init(x,y)
#endif
#endif /* defined(_KERNEL_OPT) || defined(__OpenBSD__) */
