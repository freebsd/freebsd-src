# $NetBSD: Makefile,v 1.1 2011/01/12 02:58:40 pgoyette Exp $

MKMAN=	no

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/net/getaddrinfo

TESTS_SH+=	t_getaddrinfo

BINDIR=		${TESTSDIR}

PROGS=		h_gai

FILESDIR=	${TESTSDIR}/data

FILES+=		basics_v4.exp	basics_v4v6.exp
FILES+=		no_host_v4.exp	no_host_v4v6.exp
FILES+=		no_serv_v4.exp	no_serv_v4v6.exp
FILES+=		sock_raw_v4.exp	sock_raw_v4v6.exp
FILES+=		spec_fam_v4.exp	spec_fam_v4v6.exp
FILES+=		scoped.exp
FILES+=		unsup_fam.exp

.include <bsd.test.mk>
