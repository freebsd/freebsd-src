# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh

PROG=	sshd
BINOWN=	root
BINMODE=555
MAN8=	sshd.8

SRCS=	sshd.c auth-rhosts.c auth-passwd.c auth-rsa.c auth-rh-rsa.c \
	pty.c log-server.c login.c servconf.c serverloop.c

CFLAGS=	-DLIBWRAP

.include <bsd.own.mk>

.PATH:		${SSHSRC}

.if defined(MAKE_KERBEROS4) && \
	((${MAKE_KERBEROS4} == "yes") || (${MAKE_KERBEROS4} == "YES"))
CFLAGS+= -DKRB4
SRCS+=	auth-krb4.c
LDADD+=	 -lkrb -lcom_err
DPADD+=	 ${LIBKRB} ${LIBCOM_ERR}
.endif # MAKE_KERBEROS4

.if defined(MAKE_KERBEROS5) && \
	((${MAKE_KERBEROS5} == "yes") || (${MAKE_KERBEROS5} == "YES"))
CFLAGS+= -DKRB5
SRCS+=	auth-krb5.c
LDADD+=	 -lkrb5 -lcom_err
DPADD+=	 ${LIBKRB5} ${LIBCOM_ERR}
.endif # MAKE_KERBEROS5

.include <bsd.prog.mk>

LDADD+=	-L${.OBJDIR}/../../lib/libssh -lssh -lcrypt -lcrypto -lutil -lz -lwrap
DPADD+=	${LIBCRYPT} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ} ${LIBWRAP}
