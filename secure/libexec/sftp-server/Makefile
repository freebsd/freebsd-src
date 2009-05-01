# $FreeBSD: src/secure/libexec/sftp-server/Makefile,v 1.15.2.1.4.1 2009/04/15 03:14:26 kensmith Exp $

PROG=   sftp-server
SRCS=   sftp-server.c sftp-common.c sftp-server-main.c
MAN=	sftp-server.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
