# $FreeBSD$

PROG=	kadmind

SRCS=	kadmind.c			\
	server.c			\
	kadm_conn.c

CFLAGS+=-I${KRB5DIR}/lib/roken		\
	-I${KRB5DIR}/lib/krb5		\
	-I${KRB5DIR}/lib/asn1		\
	${LDAPCFLAGS}

LDADD=	-lkrb5 -lroken ${LIBVERS} -lkadm5srv -lhdb \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

LDFLAGS=${LDAPLDFLAGS}

DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBVERS} ${LIBKADM5SRV} ${LIBHDB} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}

MAN=	kadmind.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kadmin
