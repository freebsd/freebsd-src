#	@(#)Makefile	8.2 (Berkeley) 3/27/94

PROG=	mount_nfs
SRCS=	mount_nfs.c getmntopts.c
MAN8=	mount_nfs.0

MOUNT=	${.CURDIR}/../mount
CFLAGS+= -DNFS -I${MOUNT}
.PATH:	${MOUNT}

DPADD=	${LIBRPC}
LDADD=	-lrpc

.include <bsd.prog.mk>
