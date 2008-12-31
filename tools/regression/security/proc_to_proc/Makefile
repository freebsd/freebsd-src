# $FreeBSD: src/tools/regression/security/proc_to_proc/Makefile,v 1.4.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	testuid
NO_MAN=

ADDITIONAL_FLAGS=-DSETSUGID_SUPPORTED
#ADDITIONAL_FLAGS=	-DSETSUGID_SUPPORTED_BUT_NO_LIBC_STUB
ADDITIONAL_FLAGS+=	-DCHECK_CRED_SET
CFLAGS+=	${ADDITIONAL_FLAGS}
SRCS=	testuid.c scenario.c

.include <bsd.prog.mk>
