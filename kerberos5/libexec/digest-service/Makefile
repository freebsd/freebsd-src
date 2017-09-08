# $FreeBSD$

PROG=	digest-service
MAN=
CFLAGS+=	-I${KRB5DIR}/base \
		-I${KRB5DIR}/kdc \
		-I${KRB5DIR}/lib/asn1 \
		-I${.OBJDIR:H:H}/lib/libasn1 \
		-I${KRB5DIR}/lib/krb5 \
		-I${.OBJDIR:H:H}/lib/libkrb5 \
		-I${KRB5DIR}/lib/ipc \
		-I${KRB5DIR}/lib/wind \
		-I${.OBJDIR:H:H}/lib/libwind \
		-I${KRB5DIR}/lib/roken \
		-I${.OBJDIR:H:H}/lib/libroken \
		-I${KRB5DIR}/lib/hx509 \
		-I${.OBJDIR:H:H}/lib/libhx509 \
		-I${KRB5DIR}/lib/hdb \
		-I${.OBJDIR:H:H}/lib/libhdb \
		-I${KRB5DIR}/lib/ntlm \
		-I${.OBJDIR:H:H}/lib/libheimntlm
LIBADD=	hdb kdc heimipcs krb5 roken asn1 crypto vers heimntlm
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
