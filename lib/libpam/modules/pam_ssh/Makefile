# PAM module for SSH
# $FreeBSD: src/lib/libpam/modules/pam_ssh/Makefile,v 1.2.2.4 2002/04/04 12:29:36 des Exp $

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SHLIB_NAME=	pam_ssh.so
SRCS=		pam_ssh.c
CFLAGS+=	-I${SSHSRC}
DPADD=		${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}
LDADD=		-L${.OBJDIR}/../../../../secure/lib/libssh -lssh -lcrypto \
		-lcrypt -lutil -lz
MAN=		pam_ssh.8

.include <bsd.lib.mk>
