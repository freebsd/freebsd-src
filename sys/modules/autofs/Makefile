# $FreeBSD$
# $Id: Makefile,v 1.11 2004/09/08 08:12:21 bright Exp $


SRCDIR=        ${.CURDIR}/../../fs/autofs

.PATH: ${SRCDIR}

CFLAGS+=	-I${SRCDIR}

KMOD=	autofs

SRCS=	vnode_if.h \
	autofs_vfsops.c autofs_util.c autofs_vnops.c

.include <bsd.kmod.mk>
