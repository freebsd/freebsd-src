# $FreeBSD$
#

.include <bsd.own.mk>

PROG=	ssh
CFLAGS+=-I${SSHDIR} -include ssh_namespace.h
LINKS=	${BINDIR}/ssh ${BINDIR}/slogin
MAN=	ssh.1 ssh_config.5
MLINKS=	ssh.1 slogin.1

SRCS=	ssh.c readconf.c clientloop.c sshtty.c \
	sshconnect.c sshconnect1.c sshconnect2.c mux.c \
	roaming_common.c

# gss-genr.c really belongs in libssh; see src/secure/lib/libssh/Makefile
SRCS+=	gss-genr.c

DPADD=	${LIBSSH} ${LIBUTIL} ${LIBZ}
LDADD=	-lssh -lutil -lz

.if ${MK_KERBEROS_SUPPORT} != "no"
CFLAGS+= -DGSSAPI -DHAVE_GSSAPI_GSSAPI_H=1 -DKRB5 -DHEIMDAL
DPADD+=	 ${LIBGSSAPI}
LDADD+=	 -lgssapi
.endif

.if defined(X11BASE) || defined(LOCALBASE)
# Recommended /etc/make.conf setting is X11BASE=${LOCALBASE} for x.org
# 7.x upgrade on <= 6.2, but LOCALBASE has moved out of scope of src/
# so we need to provide the default for users with old make.conf
# settings.
LOCALBASE?=	/usr/local

# Users may override either LOCALBASE or X11BASE to move the location
# of xauth
X11BASE?=	${LOCALBASE}
CFLAGS+= -DXAUTH_PATH=\"${X11BASE}/bin/xauth\"
.endif

DPADD+=	${LIBCRYPT} ${LIBCRYPTO}
LDADD+=	-lcrypt -lcrypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}

${OBJS} ${POBJS} ${SOBJS}: ssh_namespace.h
