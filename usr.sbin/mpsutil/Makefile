# $FreeBSD$

PROG=	mpsutil
SRCS=	mpsutil.c mps_cmd.c mps_show.c
#	mpt_flash.c
MAN=	mpsutil.8

WARNS?= 3

LIBADD=	cam util
LINKS=	${BINDIR}/mpsutil ${BINDIR}/mprutil

CFLAGS+= -I${.CURDIR}/../../sys -I. -DUSE_MPT_IOCTLS -g


# Here be dragons
.ifdef DEBUG
CFLAGS+= -DDEBUG
.endif

.include <bsd.prog.mk>
