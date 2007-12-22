# $FreeBSD$

.PATH: ${.CURDIR}/../../fs/coda

KMOD=	coda5
SRCS=	vnode_if.h \
	coda_fbsd.c coda_namecache.c coda_psdev.c coda_subr.c \
	coda_venus.c coda_vfsops.c coda_vnops.c \
	opt_coda.h

CFLAGS+= -DCODA5_MODULE

.include <bsd.kmod.mk>
