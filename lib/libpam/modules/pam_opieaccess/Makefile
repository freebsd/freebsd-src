# $FreeBSD: src/lib/libpam/modules/pam_opieaccess/Makefile,v 1.4.30.1 2008/11/25 02:59:29 kensmith Exp $

LIB=	pam_opieaccess
SRCS=	${LIB}.c
MAN=	pam_opieaccess.8

DPADD=	${LIBOPIE}
LDADD=	-lopie

.include <bsd.lib.mk>
