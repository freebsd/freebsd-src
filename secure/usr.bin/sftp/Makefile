# $FreeBSD: src/secure/usr.bin/sftp/Makefile,v 1.9 2004/02/26 11:26:46 des Exp $

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-common.c sftp-glob.c progressmeter.c
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
