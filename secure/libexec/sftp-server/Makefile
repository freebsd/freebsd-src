# $FreeBSD$
#

PROG=   sftp-server
SRCS=   sftp-server.c log-server.c
MAN=	sftp-server.8

LDADD+=	${LIBSSH} -lcrypto
DPADD+=	${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
