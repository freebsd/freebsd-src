# $FreeBSD$

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-int.c sftp-common.c sftp-glob.c
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPTO}
LDADD=	-lssh -lcrypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
