.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	sftp
SRCS=	sftp.c sftp-usergroup.c progressmeter.c $(SFTP_CLIENT_SRCS) $(SKSRCS)
PACKAGE=	ssh

LIBADD=	ssh edit

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
