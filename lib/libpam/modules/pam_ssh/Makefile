# PAM module for SSH
# $FreeBSD: src/lib/libpam/modules/pam_ssh/Makefile,v 1.2 2000/03/06 15:28:30 peter Exp $

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SHLIB_NAME=	pam_ssh.so
SRCS=		log-client.c pam_ssh.c
CFLAGS+=	-Wall -I${SSHSRC}
DPADD+=		${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ} ${LIBGCC_PIC}
LDADD+=		-L${.OBJDIR}/../../../../secure/lib/libssh -lssh -lcrypto -lcrypt -lutil -lz -lgcc_pic
INTERNALLIB=	yes
INTERNALSTATICLIB=yes

.include <bsd.lib.mk>

.PATH:		${SSHSRC}/pam_ssh
.PATH:		${SSHSRC}
