#	$FreeBSD$

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN8 =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBKVM}
LDADD=	-lrpcsvc -lutil -lkvm

.include <bsd.prog.mk>
