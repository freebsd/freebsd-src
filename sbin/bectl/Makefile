# $FreeBSD$

PROG=	bectl
MAN=	bectl.8

SRCS=	bectl.c bectl_jail.c bectl_list.c

LIBADD+= be
LIBADD+= jail
LIBADD+= nvpair
LIBADD+= util

CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris
CFLAGS+= -I${SRCTOP}/sys/cddl/contrib/opensolaris/uts/common

CFLAGS+= -DNEED_SOLARIS_BOOLEAN

.include <bsd.prog.mk>
