#	@(#)Makefile	8.1 (Berkeley) 6/4/93

LIB=resolv
CFLAGS+=-DDEBUG -DLIBC_SCCS

.PATH:	${.CURDIR}/../libc/net

SRCS=gethostnamadr.c res_mkquery.c res_query.c res_send.c

.include <bsd.lib.mk>
