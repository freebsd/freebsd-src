# $FreeBSD$

PROG=	kpasswd
CFLAGS+=-I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/lib/krb5 \
	-I${.OBJDIR:H:H}/lib/libroken \
	-I${.OBJDIR:H:H}/lib/libkrb5 \
	-I${.OBJDIR:H:H}/lib/libasn1
LIBADD=	hdb krb5 roken vers crypto
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
