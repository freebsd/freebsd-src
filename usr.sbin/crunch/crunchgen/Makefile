# $FreeBSD: src/usr.sbin/crunch/crunchgen/Makefile,v 1.6.32.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	crunchgen
SRCS=	crunchgen.c crunched_skel.c
CLEANFILES+= crunched_skel.c

crunched_skel.c: crunched_main.c
	sh -e ${.CURDIR}/mkskel.sh ${.CURDIR}/crunched_main.c >crunched_skel.c

.include <bsd.prog.mk>
