# PAM module for SSH
# $FreeBSD: src/lib/libpam/modules/pam_ssh/Makefile,v 1.25.12.1 2010/02/10 00:26:20 kensmith Exp $

SSHDIR=	${.CURDIR}/../../../../crypto/openssh

LIB=	pam_ssh
MAN=	pam_ssh.8
SRCS=	pam_ssh.c

WARNS?=	0
CFLAGS+= -I${SSHDIR} -include ssh_namespace.h

DPADD=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lssh -lcrypto -lcrypt

.include <bsd.lib.mk>

.PATH: ${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
