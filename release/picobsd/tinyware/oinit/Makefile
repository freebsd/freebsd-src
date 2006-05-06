# $FreeBSD: src/release/picobsd/tinyware/oinit/Makefile,v 1.5 2004/12/21 08:47:06 ru Exp $
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


