# $FreeBSD: src/kerberos5/usr.bin/kpasswd/Makefile,v 1.14.32.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	kpasswd
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
