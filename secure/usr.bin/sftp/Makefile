# $FreeBSD$
#

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-int.c sftp-common.c sftp-glob.c

LDADD=	-lssh -lcrypto
DPADD=	${LIBSSH} ${LIBCRYPTO}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
