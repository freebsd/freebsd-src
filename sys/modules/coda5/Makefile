# $FreeBSD$

.PATH: ${.CURDIR}/../../coda

KMOD=	coda5
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

opt_coda.h:
	echo "#define CODA_COMPAT_5" > ${.TARGET}

.include <bsd.kmod.mk>
