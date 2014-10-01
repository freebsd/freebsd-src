# $NetBSD: Makefile,v 1.10 2014/01/09 02:18:10 christos Exp $

.include <bsd.own.mk>

MKMAN=	no

TESTS_SUBDIRS+=	getaddrinfo
FILES+=hosts resolv.conf

TESTSDIR=	${TESTSBASE}/lib/libc/net

TESTS_C+=	t_getprotoent
TESTS_C+=	t_ether_aton

SRCS.t_ether_aton=	aton_ether_subr.c t_ether_aton.c

aton_ether_subr.c: gen_ether_subr ${NETBSDSRCDIR}/sys/net/if_ethersubr.c
	${HOST_SH} ${.ALLSRC} ${.TARGET}

TESTS_SH+=	t_nsdispatch
TESTS_SH+=	t_protoent
TESTS_SH+=	t_servent
TESTS_SH+=	t_hostent

BINDIR=		${TESTSDIR}

PROGS+=		h_nsd_recurse
PROGS+=		h_protoent
PROGS+=		h_servent
PROGS+=		h_hostent
PROGS+=		h_dns_server
CPPFLAGS.h_hostent.c += -I${NETBSDSRCDIR}/lib/libc/net

# For easy debugging, without installing libc
#.PATH.c:${NETBSDSRCDIR}/lib/libc/net
#SRCS.h_hostent = h_hostent.c gethnamaddr.c
#DBG=-g3

LDADD.h_nsd_recurse+=	-lpthread

CLEANFILES+=	aton_ether_subr.c

.include <bsd.test.mk>
