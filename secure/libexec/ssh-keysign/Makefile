# $FreeBSD: src/secure/libexec/ssh-keysign/Makefile,v 1.6.2.2 2006/10/11 09:31:48 des Exp $

PROG=	ssh-keysign
SRCS=	ssh-keysign.c readconf.c
MAN=	ssh-keysign.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
.if defined(ENABLE_SUID_SSH)
BINMODE=4511
.endif

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBMD} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lmd -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
