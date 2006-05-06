# PAM module for SSH
# $FreeBSD: src/lib/libpam/modules/pam_ssh/Makefile,v 1.20 2005/06/07 09:31:26 des Exp $

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=	pam_ssh
MAN=	pam_ssh.8
SRCS=	pam_ssh.c

WARNS?=	0
CFLAGS+= -I${SSHSRC}

DPADD=	${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT}
LDADD=	-lssh -lcrypto -lcrypt

.include <bsd.lib.mk>
