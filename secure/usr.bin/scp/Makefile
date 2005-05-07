# $FreeBSD: src/secure/usr.bin/scp/Makefile,v 1.12 2004/01/07 11:17:23 des Exp $

PROG=	scp
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
