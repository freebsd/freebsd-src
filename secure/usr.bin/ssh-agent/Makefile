.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	ssh-agent
SRCS=	ssh-agent.c ssh-pkcs11-client.c $(SKSRCS)
PACKAGE=	ssh

LIBADD=	crypto ssh

.if defined(LOCALBASE)
CFLAGS+= -DDEFAULT_PKCS11_WHITELIST='"/usr/lib*/*,${LOCALBASE}/lib*/*"'
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
