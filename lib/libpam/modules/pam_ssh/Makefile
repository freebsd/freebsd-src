# PAM module for SSH
# $FreeBSD: src/lib/libpam/modules/pam_ssh/Makefile,v 1.2.2.4.2.1 2002/07/16 12:33:25 des Exp $

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SRCS=		pam_ssh.c
CFLAGS+=	-I${SSHSRC}
DPADD=		${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL}
LDADD=		-lssh -lcrypto -lcrypt -lutil
MAN=		pam_ssh.8

.include <bsd.lib.mk>
