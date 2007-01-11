# $FreeBSD: src/secure/usr.bin/ssh-agent/Makefile,v 1.14.2.1 2006/07/08 14:37:18 des Exp $

PROG=	ssh-agent
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
