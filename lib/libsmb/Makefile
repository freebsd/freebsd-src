# $FreeBSD: src/lib/libsmb/Makefile,v 1.1.2.2 2001/12/20 09:21:31 sheldonh Exp $

LIB=		smb

SHLIB_MAJOR=	1
SHLIB_MINOR=	0

NOMAN=

SRCS=	rcfile.c ctx.c cfopt.c subr.c nls.c rap.c mbuf.c rq.c file.c \
	print.c \
	kiconv.c \
	nb.c nb_name.c nb_net.c nbns_rq.c
CONTRIBDIR=	${.CURDIR}/../../contrib/smbfs
CFLAGS+=	-DSMB_CFG_FILE=\"/etc/nsmb.conf\" -I${CONTRIBDIR}/include

.PATH:	${CONTRIBDIR}/lib/smb

.include <bsd.lib.mk>
