# $FreeBSD: src/secure/usr.bin/ssh-agent/Makefile,v 1.16.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	ssh-agent
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
