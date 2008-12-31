#	@(#)Makefile	8.8 (Berkeley) 6/21/2000
#
# $TSHeader: src/sbin/growfs/Makefile,v 1.4 2000/12/05 19:45:24 tomsoft Exp $
# $FreeBSD: src/sbin/growfs/Makefile,v 1.13.18.1 2008/11/25 02:59:29 kensmith Exp $
#

#GFSDBG=

PROG=   growfs
SRCS=   growfs.c
MAN=	growfs.8

WARNS?=	6

.if defined(GFSDBG)
SRCS+=  debug.c
.endif  

.include <bsd.prog.mk>      
