#	@(#)Makefile	8.3 (Berkeley) 3/27/94
# $FreeBSD$

PROG=	mount_umap
SRCS=	mount_umap.c getmntopts.c
MAN=	mount_umap.8

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -I${.CURDIR}/../../sys -I${MOUNT}
.PATH:	${MOUNT}

.include <bsd.prog.mk>
