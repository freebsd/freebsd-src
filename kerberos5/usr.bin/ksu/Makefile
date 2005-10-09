# $FreeBSD$

PROG=	ksu
.if defined(ENABLE_SUID_K5SU) || defined(_FREEFALL_CONFIG)
BINMODE=4555
PRECIOUSPROG=
.endif
NO_MAN=
SRCS=	su.c
CFLAGS+=-I${KRB5DIR}/lib/roken
.if defined(_FREEFALL_CONFIG)
CFLAGS+=-D_FREEFALL_CONFIG
.endif
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/su
