# $FreeBSD$

PROG=	mpsutil
SRCS=	mpsutil.c mps_cmd.c mps_show.c
MAN=	mpsutil.8

WARNS?= 3

#LIBADD=	cam util
LINKS=	${BINDIR}/mpsutil ${BINDIR}/mprutil
MLINKS=	mpsutil.8 mprutil.8

CFLAGS+= -I${.CURDIR}/../../sys -I. -DUSE_MPT_IOCTLS


# Here be dragons
.ifdef DEBUG
CFLAGS+= -DDEBUG
.endif

.include <bsd.prog.mk>
