# $FreeBSD$

PROG=   sftp-server
SRCS=   sftp-common.c sftp-server.c
MAN=	sftp-server.8
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
