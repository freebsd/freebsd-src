# $FreeBSD$

PROG=	bectl
WARNS?=	1
MAN=	bectl.8

LIBADD+= be
LIBADD+= nvpair

CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common

CFLAGS+= -DNEED_SOLARIS_BOOLEAN

.include <bsd.prog.mk>
