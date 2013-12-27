# $FreeBSD$

.include <bsd.init.mk>

TESTSDIR=	${TESTSBASE}/lib/atf/libatf-c

ATF=		${.CURDIR:H:H:H:H}/contrib/atf
.PATH:		${ATF}/atf-c
.PATH:		${ATF}/atf-c/detail

CFLAGS+=	-I${ATF}

# macros_test.c contains a double 'const const' which will be gone with
# the import of atf-0.18.
# TODO(jmmv): Remove this workaround once we do that update.
CFLAGS.clang+=	-Wno-duplicate-decl-specifier

FILESDIR=	${TESTSDIR}
FILES=		macros_h_test.c
FILES+=		unused_test.c

# Tests in atf-c.

.for _T in	atf_c_test \
		build_test \
		check_test \
		config_test \
		error_test \
		macros_test \
		tc_test \
		tp_test \
		utils_test
ATF_TESTS_C+=	${_T}
SRCS.${_T}=	${_T}.c test_helpers.c
.endfor

ATF_TESTS_SH=	pkg_config_test

# Tests in atf-c/detail.

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
