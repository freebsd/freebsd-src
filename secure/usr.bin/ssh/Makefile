# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

PROG=	ssh
BINOWN=	root
BINMODE=4555
MAN1=	ssh.1
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c log-client.c readconf.c clientloop.c sshconnect.c \
	sshconnect1.c sshconnect2.c

.include <bsd.own.mk>

.PATH:		${SSHSRC}
.PATH:		${SSHSRC}/lib

.if defined(MAKE_KERBEROS4) && \
	((${MAKE_KERBEROS4} == "yes") || (${MAKE_KERBEROS4} == "YES"))
DISTRIBUTION=krb4
CFLAGS+= -DKRB4
LDADD+=	 -lkrb -lcom_err
DPADD+=	 ${LIBKRB} ${LIBCOM_ERR}
.endif # MAKE_KERBEROS4

.if defined(MAKE_KERBEROS5) && \
	((${MAKE_KERBEROS5} == "yes") || (${MAKE_KERBEROS5} == "YES"))
DISTRIBUTION=krb5
CFLAGS+= -DKRB5
LDADD+=	 -lkrb5 -lasn1 -lcom_err -lmd -L${.OBJDIR}/../../../kerberos5/lib/libroken -lroken -lcrypt
DPADD+=	 ${LIBKRB5} ${LIBCOM_ERR} ${LIBASN1} ${LIBMD} ${LIBCRYPT}
.endif # MAKE_KERBEROS5

.include <bsd.prog.mk>

.if defined(X11BASE)
CFLAGS+= -DXAUTH_PATH=${X11BASE}/bin/xauth
.endif

LDADD+=	-L${.OBJDIR}/../../lib/libssh -lssh -lcrypto -lutil -lz
DPADD+=	${LIBCRYPTO} ${LIBUTIL} ${LIBZ}
