# $FreeBSD: src/secure/usr.bin/ssh-agent/Makefile,v 1.16.10.2.4.1 2010/12/21 17:09:25 kensmith Exp $

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
