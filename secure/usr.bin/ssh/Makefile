# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

PROG=	ssh
BINOWN=	root
BINMODE=4555
MAN1=	ssh.1
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c sshconnect.c log-client.c readconf.c clientloop.c

.include <bsd.own.mk>

.PATH:		${SSHSRC}
.PATH:		${SSHSRC}/lib

.if defined(MAKE_KERBEROS4) && \
	((${MAKE_KERBEROS4} == "yes") || (${MAKE_KERBEROS4} == "YES"))
CFLAGS+= -DKRB4
LDADD+=	 -lkrb -lcom_err
DPADD+=	 ${LIBKRB} ${LIBCOM_ERR}
.endif # MAKE_KERBEROS4

.if defined(MAKE_KERBEROS5) && \
	((${MAKE_KERBEROS5} == "yes") || (${MAKE_KERBEROS5} == "YES"))
CFLAGS+= -DKRB5
LDADD+=	 -lkrb5 -lcom_err
DPADD+=	 ${LIBKRB5} ${LIBCOM_ERR}
.endif # MAKE_KERBEROS5

.include <bsd.prog.mk>

LDADD+=	-L${.OBJDIR}/../../lib/libssh -lssh -lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
