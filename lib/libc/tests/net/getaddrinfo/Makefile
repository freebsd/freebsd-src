PACKAGE=	tests

.include <bsd.own.mk>

BINDIR=		${TESTSDIR}

ATF_TESTS_SH=  	getaddrinfo_test getaddrinfo_test_v6
ATF_TESTS_C=	getaddrinfo

SRCS.h_gai= h_gai.c
PROGS=		h_gai

FILESGROUPS+=	${PACKAGE}DATA_FILES
${PACKAGE}DATA_FILESPACKAGE=	tests

${PACKAGE}DATA_FILESDIR=	${TESTSDIR}/data

${PACKAGE}DATA_FILES+= basics_v4.exp
${PACKAGE}DATA_FILES+= basics_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+= basics_v4v6_prefer_v6.exp
${PACKAGE}DATA_FILES+= no_host_v4.exp
${PACKAGE}DATA_FILES+= no_host_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+= no_host_v4v6_prefer_v6.exp
${PACKAGE}DATA_FILES+= no_serv_v4.exp
${PACKAGE}DATA_FILES+= no_serv_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+= no_serv_v4v6_prefer_v6.exp
${PACKAGE}DATA_FILES+= scoped_v4.exp
${PACKAGE}DATA_FILES+= scoped_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+= scoped_v4v6_prefer_v6.exp
${PACKAGE}DATA_FILES+= sock_raw_v4.exp
${PACKAGE}DATA_FILES+= sock_raw_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+= sock_raw_v4v6_prefer_v6.exp
${PACKAGE}DATA_FILES+= spec_fam_v4.exp
${PACKAGE}DATA_FILES+= spec_fam_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+= spec_fam_v4v6_prefer_v6.exp
${PACKAGE}DATA_FILES+= unsup_fam_v4.exp
${PACKAGE}DATA_FILES+= unsup_fam_v4v6_prefer_v4.exp
${PACKAGE}DATA_FILES+= unsup_fam_v4v6_prefer_v6.exp


.include "../../Makefile.netbsd-tests"

.include <bsd.test.mk>
