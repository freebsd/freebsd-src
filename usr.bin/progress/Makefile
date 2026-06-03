.include <src.opts.mk>

FTP=	${SRCTOP}/usr.bin/ftp
TNFTP=	${SRCTOP}/contrib/tnftp

.PATH:	${TNFTP}/src

PROG=	progress
SRCS=	progress.c progressbar.c

CFLAGS+= -I${.CURDIR} -I${FTP} -I${TNFTP} -I${TNFTP}/src -DSTANDALONE_PROGRESS
LIBADD=	netbsd

WARNS?= 2

.include <bsd.prog.mk>
