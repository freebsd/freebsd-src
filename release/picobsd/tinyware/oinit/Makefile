# $FreeBSD: src/release/picobsd/tinyware/oinit/Makefile,v 1.3.8.1 2005/02/13 07:23:16 obrien Exp $
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
NOMAN=

.include <bsd.prog.mk>


