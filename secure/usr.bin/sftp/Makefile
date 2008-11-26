# $FreeBSD: src/secure/usr.bin/sftp/Makefile,v 1.12.2.2.6.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	sftp
SRCS=   sftp.c sftp-client.c sftp-common.c sftp-glob.c progressmeter.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ} ${LIBEDIT} ${LIBNCURSES}
LDADD=	-lssh -lcrypt -lcrypto -lz -ledit -lncurses

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
