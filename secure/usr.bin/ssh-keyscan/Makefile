# $FreeBSD: src/secure/usr.bin/ssh-keyscan/Makefile,v 1.9.2.2 2006/10/11 09:31:49 des Exp $

PROG=	ssh-keyscan
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBMD} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lmd -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
