# $FreeBSD: src/tools/regression/security/proc_to_proc/Makefile,v 1.2 2001/04/11 20:23:23 rwatson Exp $

PROG=	testuid
NOMAN=	yes

ADDITIONAL_FLAGS=-DSETSUGID_SUPPORTED
#ADDITIONAL_FLAGS=	-DSETSUGID_SUPPORTED_BUT_NO_LIBC_STUB
ADDITIONAL_FLAGS+=	-DCHECK_CRED_SET
CFLAGS+=	${ADDITIONAL_FLAGS}
SRCS=	testuid.c scenario.c

.include <bsd.prog.mk>
