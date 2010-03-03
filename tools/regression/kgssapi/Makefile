# $FreeBSD$

PROG=	gsstest
NO_MAN=
WARNS?=	2
DPADD=	${LIBGSSAPI} ${LIBGSSAPI_KRB5}
LDADD=	-lgssapi -lgssapi_krb5
DEBUG_FLAGS= -g -O0

.include <bsd.prog.mk>
