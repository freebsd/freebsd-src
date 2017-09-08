# $FreeBSD$

PROG=	hprop
MAN=	hprop.8
SRCS=	hprop.c mit_dump.c
CFLAGS+=-I${KRB5DIR}/lib/roken
CFLAGS+=-I${KRB5DIR}/lib/krb5
CFLAGS+=-I${KRB5DIR}/lib/asn1
CFLAGS+=-I${KRB5DIR}/lib/hx509
CFLAGS+=-I${KRB5DIR}/lib/ntlm
CFLAGS+=-I${KRB5DIR}/lib/wind
CFLAGS+=-I${KRB5DIR}/lib/hdb
CFLAGS+=-I${KRB5DIR}/base
CFLAGS+=-I${KRB5DIR}/kdc
CFLAGS+=-I${.OBJDIR:H:H}/lib/libkrb5
CFLAGS+=-I${.OBJDIR:H:H}/lib/libroken
CFLAGS+=-I${.OBJDIR:H:H}/lib/libasn1
CFLAGS+=-I${.OBJDIR:H:H}/lib/libhx509
CFLAGS+=-I${.OBJDIR:H:H}/lib/libheimntlm
CFLAGS+=-I${.OBJDIR:H:H}/lib/libwind
CFLAGS+=-I${.OBJDIR:H:H}/lib/libhdb
CFLAGS+=-I${.OBJDIR:H:H}/lib/libheimbase
LIBADD=	hdb krb5 roken vers
DPADD=	${LDAPDPADD}
LDADD=	${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
