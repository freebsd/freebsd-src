# $FreeBSD: src/secure/usr.bin/scp/Makefile,v 1.14 2005/06/07 09:31:26 des Exp $

PROG=	scp
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
