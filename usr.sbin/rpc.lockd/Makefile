#	$Id$

PROG =	rpc.lockd
SRCS =	lockd.c procs.c
MAN8 =	rpc.lockd.8

DPADD=	${LIBRPCSVC}
LDADD=	-lrpcsvc

.include <bsd.prog.mk>

test: test.c
	cc -o test test.c -lrpcsvc

