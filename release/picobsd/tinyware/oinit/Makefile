# $FreeBSD: src/release/picobsd/tinyware/oinit/Makefile,v 1.5.20.1 2009/04/15 03:14:26 kensmith Exp $
#
PROG=		oinit

SH_PATH?=	/bin/sh
SH_NAME?=	-sh
SH_ARG?=	/etc/rc

CFLAGS=		-DSH_PATH=\"${SH_PATH}\" -DSH_NAME=\"${SH_NAME}\" \
		-DSH_ARG=\"${SH_ARG}\"


#CFLAGS+=	-DUSE_HISTORY
#CFLAGS+=	-DOINIT_RC=\"/etc/oinit.rc\"

#LDADD=		-lutil -ledit -ltermcap
LDADD=-lutil
NO_MAN=

.include <bsd.prog.mk>


