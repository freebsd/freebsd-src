# $FreeBSD: src/secure/usr.bin/ssh-agent/Makefile,v 1.16.6.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	ssh-agent
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
