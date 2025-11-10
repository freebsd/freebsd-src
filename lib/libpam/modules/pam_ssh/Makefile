# PAM module for SSH

SSHDIR=	${SRCTOP}/crypto/openssh

PACKAGE=	ssh

LIB=		pam_ssh
MANNODEV=	pam_ssh.8
SRCS=		pam_ssh.c

WARNS?=	5
CFLAGS+= -I${SSHDIR} -include ssh_namespace.h
SRCS+=	 ssh_namespace.h

LIBADD=	ssh

.include <bsd.lib.mk>

.PATH: ${SSHDIR}
