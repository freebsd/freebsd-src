
PACKAGE=ufs
PROG=	tunefs
SRCS=	tunefs.c getmntopts.c
LIBADD=	ufs
MAN=	tunefs.8

MOUNT=	${SRCTOP}/sbin/mount
CFLAGS+=	-I${MOUNT}
.PATH:	${MOUNT}

WARNS?=	3

.include <bsd.prog.mk>
