# $FreeBSD: src/secure/libexec/sftp-server/Makefile,v 1.10 2004/02/05 22:44:24 ru Exp $

PROG=   sftp-server
SRCS=   sftp-common.c sftp-server.c
MAN=	sftp-server.8
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
