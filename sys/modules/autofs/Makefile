# $FreeBSD$
# $Id: Makefile,v 1.10 2004/09/01 07:03:32 bright Exp $


SRCDIR=        ${.CURDIR}/../../fs/autofs

.PATH: ${SRCDIR}

CFLAGS+=	-I${SRCDIR}

KMOD=	autofs

SRCS=	vnode_if.h \
	autofs_vfsops.c autofs_util.c autofs_vnops.c

.include <bsd.kmod.mk>
