#	$Id: Makefile,v 1.1 1993/09/24 13:03:14 rgrimes Exp $

CFLAGS+= -I${.CURDIR}

LIB=	gnumalloc
SRCS+=	malloc.c cfree.c calloc.c morecore.c
SRCS+=	memalign.c valloc.c mcheck.c mtrace.c mstats.c vm-limit.c
SRCS+=	ralloc.c
NOMAN=	noman

.include <bsd.lib.mk>
