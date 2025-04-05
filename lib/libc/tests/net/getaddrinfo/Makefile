PACKAGE=	tests

.include <bsd.own.mk>

BINDIR=		${TESTSDIR}

ATF_TESTS_SH=	t_getaddrinfo
ATF_TESTS_C=	getaddrinfo

PROGS=		h_gai

FILESGROUPS+=	${PACKAGE}DATA_FILES
${PACKAGE}DATA_FILESPACKAGE=	tests

${PACKAGE}DATA_FILESDIR=	${TESTSDIR}/data

${PACKAGE}DATA_FILES+=		basics_v4.exp	basics_v4v6.exp
${PACKAGE}DATA_FILES+=		no_host_v4.exp	no_host_v4v6.exp
${PACKAGE}DATA_FILES+=		no_serv_v4.exp	no_serv_v4v6.exp
${PACKAGE}DATA_FILES+=		sock_raw_v4.exp	sock_raw_v4v6.exp
${PACKAGE}DATA_FILES+=		spec_fam_v4.exp	spec_fam_v4v6.exp
${PACKAGE}DATA_FILES+=		scoped.exp
${PACKAGE}DATA_FILES+=		unsup_fam.exp

.include "../../Makefile.netbsd-tests"

.include <bsd.test.mk>
