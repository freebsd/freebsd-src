# $FreeBSD$

LIB=		pam_opieaccess
SHLIB_NAME=	${LIB}.so.${SHLIB_MAJOR}
SRCS=		${LIB}.c
DPADD=		${LIBOPIE}
LDADD=		-lopie
MAN=		pam_opieaccess.8

.include <bsd.lib.mk>
