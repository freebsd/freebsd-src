# PAM module for SSH
# $FreeBSD: src/lib/libpam/modules/pam_ssh/Makefile,v 1.2.2.3 2001/06/11 15:28:49 markm Exp $

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SHLIB_NAME=	pam_ssh.so
SRCS=		log-client.c pam_ssh.c
CFLAGS+=	-Wall -I${SSHSRC}
DPADD+=		${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ}
LDADD+=		-L${.OBJDIR}/../../../../secure/lib/libssh -lssh -lcrypto -lcrypt -lutil -lz
INTERNALLIB=	yes
INTERNALSTATICLIB=yes

.include <bsd.lib.mk>

.PATH:		${SSHSRC}/pam_ssh
.PATH:		${SSHSRC}
