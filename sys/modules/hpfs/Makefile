# $FreeBSD$

.PATH:	${.CURDIR}/../../fs/hpfs
KMOD=	hpfs
SRCS=	vnode_if.h \
	hpfs_vfsops.c hpfs_vnops.c hpfs_hash.c hpfs_subr.c hpfs_lookup.c \
	hpfs_alsubr.c opt_vmpage.h
NOMAN=
CFLAGS+= -DHPFS
CLEANFILES= opt_vmpage.h

opt_vmpage.h:
	touch ${.TARGET}

.include <bsd.kmod.mk>
