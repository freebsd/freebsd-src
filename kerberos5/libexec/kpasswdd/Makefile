# $FreeBSD$

PROG=	kpasswdd

CFLAGS+=-I${KRB5DIR}/lib/roken		\
	${LDAPCFLAGS}

LDADD=	-lkadm5srv -lhdb -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err ${LDAPLDADD}

LDFLAGS=${LDAPLDFLAGS}

DPADD=	${LIBKADM5SRV} ${LIBHDB} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR} ${LDAPDPADD}

MAN=	kpasswdd.8

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kpasswd
