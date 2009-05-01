# $FreeBSD: src/secure/usr.bin/ssh-add/Makefile,v 1.16.8.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	ssh-add
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
