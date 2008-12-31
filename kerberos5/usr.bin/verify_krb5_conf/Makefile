# $FreeBSD: src/kerberos5/usr.bin/verify_krb5_conf/Makefile,v 1.3.26.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	verify_krb5_conf
MAN=	verify_krb5_conf.8
CFLAGS+=-I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/krb5
