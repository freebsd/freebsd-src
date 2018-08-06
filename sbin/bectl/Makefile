# $FreeBSD$

PROG=	bectl
MAN=	bectl.8

SRCS=	bectl.c bectl_jail.c

LIBADD+= be
LIBADD+= jail
LIBADD+= nvpair
LIBADD+= util

CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris

CFLAGS+= -DNEED_SOLARIS_BOOLEAN

.include <bsd.prog.mk>
