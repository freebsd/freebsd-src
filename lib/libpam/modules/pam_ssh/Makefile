# PAM module for SSH
# $FreeBSD$

SSHSRC=	${.CURDIR}/../../../../crypto/openssh

LIB=		pam_ssh
SHLIB_NAME=	pam_ssh.so
SRCS=		log-client.c pam_ssh.c
CFLAGS+=	-Wall -I${SSHSRC}
DPADD+=		${LIBCRYPTO} ${LIBCRYPT} ${LIBUTIL} ${LIBZ} ${LIBGCC_PIC}
LDADD+=		-lcrypto -lcrypt -lutil -lz -lgcc_pic
INTERNALLIB=	yes
INTERNALSTATICLIB=yes

.include <bsd.lib.mk>

.PATH:		${SSHSRC}/pam_ssh
.PATH:		${SSHSRC}
