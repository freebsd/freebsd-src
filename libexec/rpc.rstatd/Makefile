#	$Id: Makefile,v 1.3 1997/02/22 14:22:11 peter Exp $

PROG =	rpc.rstatd
SRCS =	rstatd.c rstat_proc.c
MAN8 =	rpc.rstatd.8

DPADD=	${LIBRPCSVC} ${LIBUTIL} ${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lrpcsvc -lutil -lkvm -ldevstat

.include <bsd.prog.mk>
