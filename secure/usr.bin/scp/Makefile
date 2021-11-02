# $FreeBSD$

.include <src.opts.mk>
.include "${SRCTOP}/secure/ssh.mk"

PROG=	scp
SRCS=	scp.c sftp-common.c sftp-client.c sftp-glob.c progressmeter.c
PACKAGE=	ssh

LIBADD=	ssh

.if ${MK_LDNS} != "no"
CFLAGS+=	-DHAVE_LDNS=1
#DPADD+=	${LIBLDNS}
#LDADD+=	-lldns
.endif

.include <bsd.prog.mk>

.PATH:	${SSHDIR}
