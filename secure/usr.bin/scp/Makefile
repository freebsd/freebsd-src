.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	scp
SRCS=	scp.c progressmeter.c $(SFTP_CLIENT_SRCS) $(SKSRCS)
PACKAGE=	ssh

LIBADD=	ssh

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
