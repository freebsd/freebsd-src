/*
 * $Id: sliceio.h,v 1.1 1998/04/19 23:32:43 julian Exp $
 */

#ifndef	_SYS_SLICEIO_H_
#define	_SYS_SLICEIO_H_

#ifndef KERNEL
#include <sys/types.h>
#endif
#include <sys/ioccom.h>
#define SLCTYPE_SIZE 16
#define	SLCNAME_SIZE 32

struct sliceinfo {
	u_int64_t	size;
	u_int32_t	blocksize;
	char		type[SLCTYPE_SIZE];		/* e.g. sd or raw*/
	char		hint[SLCTYPE_SIZE];		/* e.g. mbr or ""*/
	char		handler[SLCTYPE_SIZE];		/* e.g. mbr or "" */
	char		devicename[SLCNAME_SIZE];	/* e.g. sd0s1a */
};

struct subsliceinfo {
	struct sliceinfo wholesliceinfo;	/* size of the whole slice */
	int		slicenumber;		/* which subslice we are on */
	u_int64_t	offset;			/* where that subslice starts */
	struct sliceinfo subsliceinfo;		/* info about that subslice */
};

#define SLCIOCRESET	_IO('S', 0)		/* reset and reprobe. */
#define	SLCIOCINQ	_IOR('S', 2, struct sliceinfo)	/* info on container */
#define	SLCIOCMOD	_IOW('S', 3, struct sliceinfo) /* force container */
#define	SLCIOCGETSUB	_IOWR('S', 4, struct subsliceinfo) /* get sub info */
#define	SLCIOCSETSUB	_IOWR('S', 5, struct subsliceinfo) /* set sub info */
#define	SLCIOCTRANSBAD	_IOWR('S', 6, daddr_t)	/* map bad144 sector */

#endif /* !_SYS_SLICEIO_H_ */
