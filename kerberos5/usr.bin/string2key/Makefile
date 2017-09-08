# $FreeBSD$

PROG=	string2key
MAN=	string2key.8
CFLAGS+=	-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/wind \
		-I${KRB5DIR}/lib/hx509 \
		-I${KRB5DIR}/lib/hdb \
		-I${KRB5DIR}/lib/ntlm \
		-I${KRB5DIR}/base \
		-I${.OBJDIR:H:H}/lib/libkrb5 \
		-I${.OBJDIR:H:H}/lib/libroken \
		-I${.OBJDIR:H:H}/lib/libasn1 \
		-I${.OBJDIR:H:H}/lib/libwind \
		-I${.OBJDIR:H:H}/lib/libhx509 \
		-I${.OBJDIR:H:H}/lib/libhdb \
		-I${.OBJDIR:H:H}/lib/libheimntlm
LIBADD=	krb5 roken crypto vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
