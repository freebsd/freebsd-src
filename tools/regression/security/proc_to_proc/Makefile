# $FreeBSD: src/tools/regression/security/proc_to_proc/Makefile,v 1.2.8.1 2005/02/13 07:23:35 obrien Exp $

PROG=	testuid
NOMAN=

ADDITIONAL_FLAGS=-DSETSUGID_SUPPORTED
#ADDITIONAL_FLAGS=	-DSETSUGID_SUPPORTED_BUT_NO_LIBC_STUB
ADDITIONAL_FLAGS+=	-DCHECK_CRED_SET
CFLAGS+=	${ADDITIONAL_FLAGS}
SRCS=	testuid.c scenario.c

.include <bsd.prog.mk>
