# $FreeBSD: src/secure/usr.bin/ssh-agent/Makefile,v 1.16.10.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ssh-agent
SRCS=	ssh-agent.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

# required when linking with a dynamic libssh 
SRCS+=	roaming_dummy.c

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
