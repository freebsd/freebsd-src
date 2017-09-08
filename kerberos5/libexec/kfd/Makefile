# $FreeBSD$

PROG=	kfd
MAN=	kfd.8
CFLAGS+=	-I${KRB5DIR}/lib/asn1 \
		-I${KRB5DIR}/lib/roken \
		-I${KRB5DIR}/lib/krb5 \
		-I${.OBJDIR:H:H}/lib/libroken \
		-I${.OBJDIR:H:H}/lib/libasn1 \
		-I${.OBJDIR:H:H}/lib/libkrb5
LIBADD=	krb5 roken vers

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/kf
