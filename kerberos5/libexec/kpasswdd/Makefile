# $FreeBSD: src/kerberos5/libexec/kpasswdd/Makefile,v 1.18.28.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	kpasswdd
MAN=	kpasswdd.8
CFLAGS+=-I${KRB5DIR}/lib/roken ${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
