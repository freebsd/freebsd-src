# $FreeBSD$

PROG=	kpasswdd

CFLAGS+=-I${KRB5DIR}/lib/roken		\
	${LDAPCFLAGS}

LDADD=	-lkrb5 -lroken ${LIBVERS} -lkadm5srv -lhdb \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

LDFLAGS=${LDAPLDFLAGS}

DPADD=	${LIBKRB5} ${LIBROKEN} ${LIBVERS} ${LIBKADM5SRV} ${LIBHDB} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}

MAN=	kpasswdd.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kpasswd
