# $FreeBSD$
#

PROG=   sftp-server
SRCS=   sftp-server.c sftp-common.c
MAN=	sftp-server.8

LDADD+=	-lssh -lcrypto -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
