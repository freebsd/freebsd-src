# $FreeBSD: src/kerberos5/usr.bin/kpasswd/Makefile,v 1.14.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	kpasswd
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
