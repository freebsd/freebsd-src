# $FreeBSD$

PROG=	kimpersonate
MAN=	kimpersonate.8
CFLAGS+=	-I${KRB5DIR}/lib/hx509 \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/sl \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/kafs \
		-I${.OBJDIR:H:H}/lib/libroken \
		-I${.OBJDIR:H:H}/lib/libasn1 \
		-I${.OBJDIR:H:H}/lib/libkrb5 \
		-I.
LIBADD=	krb5 roken asn1 vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
