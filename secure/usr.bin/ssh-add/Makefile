# $FreeBSD: src/secure/usr.bin/ssh-add/Makefile,v 1.16.10.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	ssh-add
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
