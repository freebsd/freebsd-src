# $FreeBSD: src/tools/regression/security/proc_to_proc/Makefile,v 1.4.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	testuid
NO_MAN=

ADDITIONAL_FLAGS=-DSETSUGID_SUPPORTED
#ADDITIONAL_FLAGS=	-DSETSUGID_SUPPORTED_BUT_NO_LIBC_STUB
ADDITIONAL_FLAGS+=	-DCHECK_CRED_SET
CFLAGS+=	${ADDITIONAL_FLAGS}
SRCS=	testuid.c scenario.c

.include <bsd.prog.mk>
