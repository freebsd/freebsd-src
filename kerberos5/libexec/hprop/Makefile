# $FreeBSD: src/kerberos5/libexec/hprop/Makefile,v 1.16.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	hprop
MAN=	hprop.8
SRCS=	hprop.c mit_dump.c v4_dump.c
CFLAGS+=-I${KRB5DIR}/lib/roken
CFLAGS+=-I${KRB5DIR}/lib/krb5
CFLAGS+=-I${KRB5DIR}/lib/asn1
CFLAGS+=-I${KRB5DIR}/lib/hx509
CFLAGS+=-I${KRB5DIR}/lib/ntlm
CFLAGS+=-I${KRB5DIR}/kdc
CFLAGS+=-I${.OBJDIR}/../../lib/libkrb5
DPADD=	${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lhdb -lkrb5 -lhx509 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kdc
