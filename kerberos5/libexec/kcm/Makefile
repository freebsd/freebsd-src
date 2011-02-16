# $FreeBSD: src/kerberos5/libexec/kcm/Makefile,v 1.1.2.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	kcm
MAN=	kcm.8

SRCS=	acl.c \
	acquire.c \
	cache.c \
	client.c \
	config.c \
	connect.c \
	cursor.c \
	events.c \
	glue.c \
	headers.h \
	kcm_locl.h \
	kcm_protos.h \
	log.c \
	main.c \
	protocol.c \
	renew.c

CFLAGS+=-I${KRB5DIR}/lib/krb5 -I${KRB5DIR}/lib/asn1 -I${KRB5DIR}/lib/roken \
	-I${KRB5DIR}/kcm ${LDAPCFLAGS}
DPADD=	${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}
LDADD=	-lkrb5 -lhx509 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}
LDFLAGS=${LDAPLDFLAGS}

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kcm
