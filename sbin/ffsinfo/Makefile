# @(#)Makefile    8.8 (Berkeley) 6/21/2000
#
# $TSHeader: src/sbin/ffsinfo/Makefile,v 1.3 2000/12/05 19:45:10 tomsoft Exp $
# $FreeBSD$
#

MAINTAINER=	tomsoft@FreeBSD.ORG, chm@FreeBSD.ORG

#CFLAGS+=${BDECFLAGS}
 
PROG=   ffsinfo
SRCS=   ffsinfo.c debug.c
MAN=   ffsinfo.8
 
GROWFS= ${.CURDIR}/../growfs
CFLAGS+=-DFS_DEBUG -I${GROWFS}
WARNS=	0

.PATH:  ${GROWFS}
 
.include <bsd.prog.mk>               
