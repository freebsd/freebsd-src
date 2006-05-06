# $FreeBSD: src/secure/usr.bin/ssh-keygen/Makefile,v 1.15 2005/06/07 09:31:27 des Exp $

PROG=	ssh-keygen
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
