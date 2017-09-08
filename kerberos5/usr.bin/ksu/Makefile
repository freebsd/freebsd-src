# $FreeBSD$

PROG=	ksu
.if defined(ENABLE_SUID_K5SU)
BINMODE=4555
PRECIOUSPROG=
.endif
MAN=
SRCS=	su.c
CFLAGS+=-I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/lib/krb5 \
	-I${KRB5DIR}/lib/kafs \
	-I${.OBJDIR:H:H}/lib/libroken \
	-I${.OBJDIR:H:H}/lib/libkrb5 \
	-I${.OBJDIR:H:H}/lib/libasn1
LIBADD=	kafs5 krb5 roken vers crypto crypt

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/su
