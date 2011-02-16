# $FreeBSD: src/secure/libexec/ssh-keysign/Makefile,v 1.8.10.2.4.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	ssh-keysign
SRCS=	ssh-keysign.c readconf.c roaming_dummy.c
MAN=	ssh-keysign.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
.if defined(ENABLE_SUID_SSH)
BINMODE=4511
.endif

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
