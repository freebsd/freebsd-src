#	$Id: Makefile,v 1.3 1998/10/16 04:30:42 peter Exp $

.PATH:	${.CURDIR}/../../coda
KMOD=	coda
SRCS=	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c opt_devfs.h vcoda.h
NOMAN=
VFS_KLD=

CLEANFILES+=	vcoda.h opt_devfs.h

opt_devfs.h:
	touch opt_devfs.h

vcoda.h:
	echo "#define NVCODA 4" > vcoda.h

.include <bsd.kmod.mk>
