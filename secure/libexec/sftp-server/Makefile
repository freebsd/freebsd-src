# $FreeBSD$

PROG=   sftp-server
SRCS=   sftp-server.c sftp-common.c
MAN=	sftp-server.8
CFLAGS+=-I${SSHDIR}

DPADD+=	${LIBSSH} ${LIBCRYPTO}
LDADD+=	-lssh -lcrypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
