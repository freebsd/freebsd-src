#	$Id$

PROG =	rpc.statd
SRCS =	statd.c procs.c file.c
MAN8 =	rpc.statd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

test: test.c
	cc -o test test.c -lrpcsvc
