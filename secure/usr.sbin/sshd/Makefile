# $FreeBSD$
#

SSHSRC=	${.CURDIR}/../../../crypto/openssh
LOGINSRC= ${.CURDIR}/../../../usr.bin/login

PROG=	sshd
BINOWN=	root
BINMODE=555
MAN8=	sshd.8

SRCS=	auth-passwd.c auth-rh-rsa.c auth-rhosts.c auth-rsa.c \
	key.c log-server.c login.c login_access.c pty.c servconf.c \
	serverloop.c sshd.c \

CFLAGS+= -DLIBWRAP -DLOGIN_ACCESS -I${LOGINSRC}

.include <bsd.own.mk>

.PATH:		${SSHSRC} ${LOGINSRC}

.if defined(MAKE_KERBEROS4) && \
	((${MAKE_KERBEROS4} == "yes") || (${MAKE_KERBEROS4} == "YES"))
DISTRIBUTION=krb4
CFLAGS+= -DKRB4
SRCS+=	auth-krb4.c
LDADD+=	 -lkrb -lcom_err
DPADD+=	 ${LIBKRB} ${LIBCOM_ERR}
.endif # MAKE_KERBEROS4

.if defined(MAKE_KERBEROS5) && \
	((${MAKE_KERBEROS5} == "yes") || (${MAKE_KERBEROS5} == "YES"))
DISTRIBUTION=krb5
CFLAGS+= -DKRB5
SRCS+=	auth-krb5.c
LDADD+=	 -lkrb5 -lasn1 -lcom_err -lmd -L${.OBJDIR}/../../../kerberos5/lib/libroken -lroken
DPADD+=	 ${LIBKRB5} ${LIBCOM_ERR} ${LIBASN1} ${LIBMD}
.endif # MAKE_KERBEROS5

.include <bsd.prog.mk>

LDADD+=	-L${.OBJDIR}/../../lib/libssh -lssh -lcrypt -lcrypto -lutil -lz -lwrap
DPADD+=	${LIBCRYPT} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ} ${LIBWRAP}
