# $FreeBSD$

PROG=   sftp-server
SRCS=   sftp-common.c sftp-server.c
MAN=	sftp-server.8
CFLAGS+=-I${SSHDIR}

DPADD+=	${LIBSSH} ${LIBCRYPTO}
LDADD+=	-lssh -lcrypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
