# $FreeBSD$

PROG=	bectl
MAN=	bectl.8

LIBADD+= be
LIBADD+= nvpair

CFLAGS+= -I${SRCTOP}/cddl/contrib/opensolaris/lib/libzfs/common
CFLAGS+= -I${SRCTOP}/sys/cddl/compat/opensolaris

CFLAGS+= -DNEED_SOLARIS_BOOLEAN

.include <bsd.prog.mk>
