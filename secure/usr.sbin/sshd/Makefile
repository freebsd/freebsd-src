.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

CONFS=	moduli sshd_config
CONFSDIR=	/etc/ssh
PROG=	sshd
SRCS=	sshd.c \
	platform-listen.c \
	servconf.c sshpty.c srclimit.c groupaccess.c auth2-methods.c \
	dns.c fatal.c compat.c utf8.c authfd.c canohost.c \
	$(SKSRCS)
PACKAGE=	ssh

MAN=	sshd.8 sshd_config.5

# Don't rebuild based on moduli.c
moduli: .MADE

LIBADD=	ssh util

.if ${MK_KERBEROS_SUPPORT} != "no"
.if ${MK_MITKRB5} == "no"
LIBADD+=	gssapi_krb5 gssapi krb5
.else
LIBADD+=	gssapi_krb5 krb5
.endif
.endif

.if ${MK_TCP_WRAPPERS} != "no"
LIBADD+=	wrap
.endif

LIBADD+=	crypto

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
