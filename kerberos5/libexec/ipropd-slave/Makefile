# $FreeBSD$

PROG=	ipropd-slave

SRCS=	\
	ipropd_slave.c		\
	kadm5_err.h

CFLAGS+=-I${KRB5DIR}/lib/roken	\
	-I${KRB5DIR}/lib/krb5	\
	-I${KRB5DIR}/lib/asn1	\
	-I.			\
	${LDAPCFLAGS}

LDADD=	-lkadm5srv -lkrb5 -lhdb -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

LDFLAGS=${LDAPLDFLAGS}

DPADD=	${LIBKADM5SRV} ${LIBKRB5} ${LIBHDB} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}

NOMAN=	yes

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/lib/kadm5
