# $FreeBSD: src/secure/libexec/ssh-pkcs11-helper/Makefile,v 1.1.2.2.2.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	ssh-pkcs11-helper
SRCS=	ssh-pkcs11.c ssh-pkcs11-helper.c
SRCS+=	roaming_dummy.c
MAN=	ssh-pkcs11-helper.8
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPT} ${LIBCRYPTO} ${LIBZ}
LDADD=	-lssh -lcrypt -lcrypto -lz

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
