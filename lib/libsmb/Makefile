# $FreeBSD$

LIB=		smb

SHLIB_MAJOR=	1
SHLIB_MINOR=	0

NOMAN=

SRCS=	rcfile.c ctx.c cfopt.c subr.c nls.c rap.c mbuf.c rq.c file.c \
	print.c \
	kiconv.c \
	nb.c nb_name.c nb_net.c nbns_rq.c
CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/lib/smb

.include <bsd.lib.mk>
