# $FreeBSD: src/secure/usr.bin/sftp/Makefile,v 1.12 2005/06/07 09:31:27 des Exp $

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-common.c sftp-glob.c progressmeter.c
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ} ${LIBEDIT}
LDADD=	-lssh -lcrypt -lcrypto -lz -ledit

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
