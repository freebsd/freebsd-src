# PAM module for SSH
# $FreeBSD: src/lib/libpam/modules/pam_ssh/Makefile,v 1.13 2002/04/10 18:07:04 ru Exp $

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SRCS=		pam_ssh.c
CFLAGS+=	-I${SSHSRC}
DPADD=		${LIBSSH} ${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL}
LDADD=		-lssh -lcrypto -lcrypt -lutil
MAN=		pam_ssh.8

.include <bsd.lib.mk>
