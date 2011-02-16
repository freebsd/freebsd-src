# $FreeBSD: src/secure/usr.bin/ssh-keyscan/Makefile,v 1.11.10.2.4.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	ssh-keyscan
SRCS=	ssh-keyscan.c roaming_dummy.c
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
