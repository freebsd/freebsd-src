# $FreeBSD$

PROG=	kadmind

SRCS=	kadmind.c			\
	server.c			\
	kadm_conn.c

CFLAGS+=-I${KRB5DIR}/lib/roken		\
	-I${KRB5DIR}/lib/krb5		\
	-I${KRB5DIR}/lib/asn1		\
	${LDAPCFLAGS}

LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

LDFLAGS=${LDAPLDFLAGS}

DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}

MAN=	kadmind.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kadmin
