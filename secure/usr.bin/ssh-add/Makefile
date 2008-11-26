# $FreeBSD: src/secure/usr.bin/ssh-add/Makefile,v 1.14.2.1.6.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	ssh-add
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
