#	$Id: Makefile,v 1.2 1993/11/10 03:45:40 smace Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN8 =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBKVM}
LDADD=	-lrpcsvc -lutil -lkvm

.include <bsd.prog.mk>
