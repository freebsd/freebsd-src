# $FreeBSD$

PROG=	kpasswd
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkrb5 -lhx509 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/kpasswd
