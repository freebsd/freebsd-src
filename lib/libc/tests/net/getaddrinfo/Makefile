PACKAGE=	tests

.include <bsd.own.mk>

BINDIR=		${TESTSDIR}

ATF_TESTS_SH=	getaddrinfo_test
ATF_TESTS_C=	getaddrinfo

PROGS=		h_gai

FILESGROUPS+=	${PACKAGE}DATA_FILES
${PACKAGE}DATA_FILESPACKAGE=	tests

${PACKAGE}DATA_FILESDIR=	${TESTSDIR}/data

${PACKAGE}DATA_FILES+=          data/basics_v4.exp
${PACKAGE}DATA_FILES+=          data/basics_v4_only.exp
${PACKAGE}DATA_FILES+=          data/basics_v4v6.exp
${PACKAGE}DATA_FILES+=          data/basics_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+=          data/no_host_v4.exp
${PACKAGE}DATA_FILES+=          data/no_host_v4_only.exp
${PACKAGE}DATA_FILES+=          data/no_host_v4v6.exp
${PACKAGE}DATA_FILES+=          data/no_host_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+=          data/no_serv_v4.exp
${PACKAGE}DATA_FILES+=          data/no_serv_v4_only.exp
${PACKAGE}DATA_FILES+=          data/no_serv_v4v6.exp
${PACKAGE}DATA_FILES+=          data/no_serv_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+=          data/scoped.exp
${PACKAGE}DATA_FILES+=          data/scoped_v4_only.exp
${PACKAGE}DATA_FILES+=          data/scoped_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+=          data/sock_raw_v4.exp
${PACKAGE}DATA_FILES+=          data/sock_raw_v4_only.exp
${PACKAGE}DATA_FILES+=          data/sock_raw_v4v6.exp
${PACKAGE}DATA_FILES+=          data/sock_raw_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+=          data/spec_fam_v4.exp
${PACKAGE}DATA_FILES+=          data/spec_fam_v4_only.exp
${PACKAGE}DATA_FILES+=          data/spec_fam_v4v6.exp
${PACKAGE}DATA_FILES+=          data/spec_fam_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+=          data/unsup_fam.exp
${PACKAGE}DATA_FILES+=          data/unsup_fam_v4_only.exp
${PACKAGE}DATA_FILES+=          data/unsup_fam_v4v6_prefer_v4.exp


.include "../../Makefile.netbsd-tests"

.include <bsd.test.mk>
