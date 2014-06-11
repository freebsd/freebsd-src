# $FreeBSD$

PROG=	ksu
.if defined(ENABLE_SUID_K5SU)
BINMODE=4555
PRECIOUSPROG=
.endif
MAN=
SRCS=	su.c
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBHX509} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lhx509 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

NO_PIE=	yes

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/su
