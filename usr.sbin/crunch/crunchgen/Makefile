# $FreeBSD: src/usr.sbin/crunch/crunchgen/Makefile,v 1.6.36.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	crunchgen
SRCS=	crunchgen.c crunched_skel.c
CLEANFILES+= crunched_skel.c

crunched_skel.c: crunched_main.c
	sh -e ${.CURDIR}/mkskel.sh ${.CURDIR}/crunched_main.c >crunched_skel.c

.include <bsd.prog.mk>
