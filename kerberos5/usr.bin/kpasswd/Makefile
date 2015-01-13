# $FreeBSD$

PROG=	kpasswd
CFLAGS+=-I${KRB5DIR}/lib/roken
LIBADD=	hdb krb5 roken vers crypto

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
