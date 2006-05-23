# $FreeBSD: src/secure/usr.bin/ssh-keyscan/Makefile,v 1.7 2004/01/07 11:17:23 des Exp $

PROG=	ssh-keyscan
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
