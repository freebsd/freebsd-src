#	@(#)Makefile	8.8 (Berkeley) 6/21/2000
#
# $TSHeader: src/sbin/growfs/Makefile,v 1.4 2000/12/05 19:45:24 tomsoft Exp $
# $FreeBSD: src/sbin/growfs/Makefile,v 1.5 2003/06/11 23:24:31 obrien Exp $
#

MAINTAINER=	tomsoft@FreeBSD.ORG, chm@FreeBSD.ORG

#GFSDBG=YES  

PROG=   growfs
SRCS=   growfs.c
MAN=	growfs.8

WARNS?=	0

.if defined(GFSDBG)
SRCS+=  debug.c
CFLAGS+=-DFS_DEBUG
.endif  

.include <bsd.prog.mk>      
