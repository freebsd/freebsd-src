# $FreeBSD$
#

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-int.c sftp-common.c sftp-glob.c scp-common.c

LDADD+=	-lssh -lcrypto -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
