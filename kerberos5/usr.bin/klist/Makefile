# $FreeBSD: src/kerberos5/usr.bin/klist/Makefile,v 1.15.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	klist
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kuser
