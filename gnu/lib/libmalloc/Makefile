#	$Id: Makefile,v 1.3 1993/11/09 20:25:14 paul Exp $

SHLIB_MAJOR=1
SHLIB_MINOR=0

CFLAGS+= -I${.CURDIR}

LIB=	gnumalloc
SRCS+=	malloc.c cfree.c calloc.c morecore.c
SRCS+=	memalign.c valloc.c mcheck.c mtrace.c mstats.c vm-limit.c
SRCS+=	ralloc.c
NOMAN=	noman

.include <bsd.lib.mk>
