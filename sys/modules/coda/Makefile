# $FreeBSD$

.PATH: ${.CURDIR}/../../coda

KMOD=	coda
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

CLEANFILES=	opt_coda.h

opt_coda.h:
	echo > ${.TARGET}

.include <bsd.kmod.mk>
