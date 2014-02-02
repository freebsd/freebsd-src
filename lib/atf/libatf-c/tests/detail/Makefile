# $FreeBSD$

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/lib/atf/libatf-c/detail

ATF=		${.CURDIR:H:H:H:H:H}/contrib/atf
.PATH:		${ATF}/atf-c/detail

CFLAGS+=	-I${ATF}

.for _T in	dynstr_test \
		env_test \
		fs_test \
		list_test \
		map_test \
		process_test \
		sanity_test \
		text_test \
		user_test
ATF_TESTS_C+=	${_T}
SRCS.${_T}=	${_T}.c test_helpers.c
.endfor

PROGS+=		process_helpers
SRCS.process_helpers=	process_helpers.c
MAN.process_helpers=	# defined
BINDIR.process_helpers=	${TESTSDIR}

.include <atf.test.mk>
