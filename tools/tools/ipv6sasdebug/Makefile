# $FreeBSD$

.PATH:		${.CURDIR}/../../../sys/netinet6 

BINDIR?=	/usr/bin

PROG=		ipv6sasdebug
MAN=

SRCS=		main.c stub.c ctl.c in6_src.c
SRCS+=		opt_inet.h opt_inet6.h opt_mpath.h
CLEANFILES+=	opt_inet.h opt_inet6.h opt_mpath.h

CFLAGS+=	-I${.CURDIR}
CFLAGS+=	-I${.CURDIR}/../../../sys

opt_inet.h:
	echo "#define INET 1" > opt_inet.h

opt_inet6.h:
	echo "#define INET6 1" > opt_inet6.h

opt_mpath.h:
	echo "#include \"stub.h\"" > opt_mpath.h

.include <bsd.prog.mk>
