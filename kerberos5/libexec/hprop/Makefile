# $FreeBSD$

PROG=	hprop

SRCS=	\
	hprop.c			\
	mit_dump.c		\
	v4_dump.c

CFLAGS+=-I${KRB5DIR}/lib/roken	\
	-I${KRB5DIR}/lib/krb5	\
	-I${KRB5DIR}/lib/asn1

LDADD=	-lkrb5 -lhdb -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

DPADD=	${LIBKRB5} ${LIBHDB} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}

MAN=	hprop.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kdc
