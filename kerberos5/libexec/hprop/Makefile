# $FreeBSD$

PROG=	hprop

SRCS=	\
	hprop.c			\
	mit_dump.c		\
	v4_dump.c

CFLAGS+=-I${KRB5DIR}/lib/roken	\
	-I${KRB5DIR}/lib/krb5	\
	-I${KRB5DIR}/lib/asn1

LDADD=	-lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

DPADD=	${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}

LDFLAGS=${LDAPLDFLAGS}

MAN=	hprop.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kdc
