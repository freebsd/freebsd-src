#	@(#)Makefile	8.8 (Berkeley) 6/21/2000
#
# $TSHeader: src/sbin/growfs/Makefile,v 1.4 2000/12/05 19:45:24 tomsoft Exp $
# $FreeBSD$
#

MAINTAINER=	tomsoft@FreeBSD.ORG, chm@FreeBSD.ORG

#GFSDBG=YES  
#CFLAGS+=${BDECFLAGS}

PROG=   growfs
SRCS=   growfs.c
MAN=	growfs.8

.if defined(GFSDBG)
SRCS+=  debug.c
CFLAGS+=-DFS_DEBUG
.endif  
WARNS=	0

.include <bsd.prog.mk>      
