# $FreeBSD$

PROG=	kinit

CFLAGS+=-I${KRB5DIR}/lib/roken

LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}

.include <bsd.prog.mk>

.PATH:	${KRB5DIR}/kuser
