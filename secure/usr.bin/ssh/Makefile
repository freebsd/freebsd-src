# $FreeBSD$
#

PROG=	ssh
.if defined(ENABLE_SUID_SSH)
BINMODE=4555
.endif
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c

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

.if defined(X11BASE)
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

LDADD+=	${LIBSSH} -lcrypto -lutil -lz
DPADD+=	${LIBSSH} ${LIBCRYPTO} ${LIBUTIL} ${LIBZ}

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
