# $FreeBSD: src/kerberos5/usr.bin/ksu/Makefile,v 1.13 2004/02/05 18:51:52 ru Exp $

PROG=	ksu
.if defined(ENABLE_SUID_K5SU)
BINMODE=4555
INSTALLFLAGS=-fschg
.endif
NOMAN=
SRCS=	su.c
CFLAGS+=-I${KRB5DIR}/lib/roken
DPADD=	${LIBKAFS5} ${LIBKRB5} ${LIBROKEN} ${LIBVERS} \
	${LIBASN1} ${LIBCRYPTO} ${LIBCRYPT} ${LIBCOM_ERR}
LDADD=	-lkafs5 -lkrb5 -lroken ${LIBVERS} \
	-lasn1 -lcrypto -lcrypt -lcom_err

.include <bsd.prog.mk>

.PATH: ${KRB5DIR}/appl/su
