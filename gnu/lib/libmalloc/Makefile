#	$Id: Makefile,v 1.2 1993/10/19 18:22:34 rich Exp $

SHLIB_MAJOR=0
SHLIB_MINOR=0

CFLAGS+= -I${.CURDIR}

LIB=	gnumalloc
SRCS+=	malloc.c cfree.c calloc.c morecore.c
SRCS+=	memalign.c valloc.c mcheck.c mtrace.c mstats.c vm-limit.c
SRCS+=	ralloc.c
NOMAN=	noman

.include <bsd.lib.mk>
