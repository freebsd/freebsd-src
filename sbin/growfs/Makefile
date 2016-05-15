#	@(#)Makefile	8.8 (Berkeley) 6/21/2000
#
# $TSHeader: src/sbin/growfs/Makefile,v 1.4 2000/12/05 19:45:24 tomsoft Exp $
# $FreeBSD$
#

.include <src.opts.mk>

.PATH:	${.CURDIR}/../mount

PACKAGE=runtime
PROG=   growfs
SRCS=   growfs.c getmntopts.c
MAN=	growfs.8
CFLAGS+=-I${.CURDIR}/../mount

.if defined(GFSDBG)
SRCS+=	debug.c
CFLAGS+= -DFS_DEBUG
NO_WCAST_ALIGN= yes
.endif  

LIBADD=	util

.if ${MK_TESTS} != "no"
SUBDIR+=    tests
.endif

.include <bsd.prog.mk>
