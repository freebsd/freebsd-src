# $FreeBSD: src/usr.sbin/crunch/crunchgen/Makefile,v 1.4.8.1 2002/04/11 19:41:39 murray Exp $

PROG=	crunchgen
SRCS=	crunchgen.c crunched_skel.c
CLEANFILES+= crunched_skel.c

crunched_skel.c: crunched_main.c
	sh -e ${.CURDIR}/mkskel.sh ${.CURDIR}/crunched_main.c >crunched_skel.c

.include <bsd.prog.mk>
