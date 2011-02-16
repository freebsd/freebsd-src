# $FreeBSD: src/secure/usr.bin/ssh-keygen/Makefile,v 1.17.14.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	ssh-keygen
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
