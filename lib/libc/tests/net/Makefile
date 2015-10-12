# $FreeBSD$

.include <bsd.own.mk>

BINDIR=		${TESTSDIR}

NETBSD_ATF_TESTS_C=	getprotoent_test
NETBSD_ATF_TESTS_C+=	ether_aton_test

SRCS.t_ether_aton=	aton_ether_subr.c t_ether_aton.c

aton_ether_subr.c: gen_ether_subr ${.CURDIR:H:H:H:H}/sys/net/if_ethersubr.c
	${HOST_SH} ${.ALLSRC} ${.TARGET}

# TODO: hostent_test
NETBSD_ATF_TESTS_SH=	nsdispatch_test
NETBSD_ATF_TESTS_SH+=	protoent_test
NETBSD_ATF_TESTS_SH+=	servent_test

PROGS=		h_nsd_recurse
PROGS+=		h_protoent
PROGS+=		h_servent
PROGS+=		h_dns_server

DPADD.h_nsd_recurse+=	${LIBPTHREAD}
LDADD.h_nsd_recurse+=	-lpthread

CLEANFILES+=	aton_ether_subr.c

.include "../Makefile.netbsd-tests"

# TODO: the testcases needs to be ported to FreeBSD
#TESTS_SUBDIRS=	getaddrinfo
FILES=	hosts
FILES+=	resolv.conf

.include <bsd.test.mk>
