# $FreeBSD$

LIB=		smb

SHLIB_MAJOR=	1

NOMAN=

SRCS=	rcfile.c ctx.c cfopt.c subr.c nls.c rap.c mbuf.c rq.c file.c \
	print.c \
	kiconv.c \
	nb.c nb_name.c nb_net.c nbns_rq.c
CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-DSMB_CFG_FILE=\"/etc/nsmb.conf\" -I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/lib/smb

.include <bsd.lib.mk>
