# $FreeBSD: src/secure/usr.bin/sftp/Makefile,v 1.15.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-common.c sftp-glob.c progressmeter.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ} ${LIBEDIT} ${LIBNCURSES}
LDADD=	-lssh -lcrypt -lcrypto -lz -ledit -lncurses

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
