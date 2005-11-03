# $FreeBSD: src/secure/usr.bin/ssh-keyscan/Makefile,v 1.9 2005/06/07 09:31:28 des Exp $

PROG=	ssh-keyscan
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
