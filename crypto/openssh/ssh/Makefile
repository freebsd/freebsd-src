#	$OpenBSD: Makefile,v 1.42 2002/06/20 19:56:07 stevesk Exp $

.PATH:		${.CURDIR}/..

PROG=	ssh
BINOWN=	root

#BINMODE?=4555

BINDIR=	/usr/bin
MAN=	ssh.1 ssh_config.5
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c

.include <bsd.own.mk> # for AFS

.if (${KERBEROS5:L} == "yes")
CFLAGS+= -DKRB5 -I${DESTDIR}/usr/include/kerberosV
LDADD+=  -lkrb5 -lasn1 -lcom_err
DPADD+=  ${LIBKRB5} ${LIBASN1} ${LIBCOM_ERR}
.endif # KERBEROS5

.if (${KERBEROS:L} == "yes")
CFLAGS+= -DKRB4 -I${DESTDIR}/usr/include/kerberosIV
LDADD+=	 -lkrb
DPADD+=	 ${LIBKRB}
.if (${AFS:L} == "yes")
CFLAGS+= -DAFS
LDADD+=  -lkafs
DPADD+=  ${LIBKAFS}
.endif # AFS
.endif # KERBEROS

.include <bsd.prog.mk>

LDADD+=	-lcrypto -lz -ldes
DPADD+=	${LIBCRYPTO} ${LIBZ} ${LIBDES}
