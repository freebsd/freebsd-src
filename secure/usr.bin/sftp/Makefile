# $FreeBSD$

LIBSSH=	${.OBJDIR}/../../lib/libssh/libssh.a

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-common.c sftp-glob.c progressmeter.c
CFLAGS+=-I${SSHDIR}

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ} ${LIBEDIT}
LDADD=	${LIBSSH} -lcrypt -lcrypto -lz -ledit

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
