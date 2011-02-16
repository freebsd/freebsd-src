# $FreeBSD: src/kerberos5/libexec/ipropd-master/Makefile,v 1.18.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	ipropd-master
NO_MAN=
SRCS=	ipropd_common.c ipropd_master.c kadm5_err.h
CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I. ${LDAPCFLAGS}
DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lkadm5srv -lhdb -lkrb5 -lhx509 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

foo::
	echo ${LIBHX509}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/lib/kadm5
