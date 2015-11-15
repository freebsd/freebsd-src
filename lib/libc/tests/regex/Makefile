# $FreeBSD$

.include <bsd.own.mk>

BINDIR=		${TESTSDIR}

IMPLEMENTATION?=      -DREGEX_SPENCER

CFLAGS.h_regex+=-I${TESTSRC} -I${.CURDIR:H:H}/regex
PROGS+=		h_regex
SRCS.h_regex=	main.c split.c debug.c

NETBSD_ATF_TESTS_SH=	regex_test

FILESDIR=	${TESTSDIR}/data
FILES+=		README
FILES+=		data/anchor.in
FILES+=		data/backref.in
FILES+=		data/basic.in
FILES+=		data/bracket.in
FILES+=		data/c_comments.in
FILES+=		data/complex.in
FILES+=		data/error.in
FILES+=		data/meta.in
FILES+=		data/nospec.in
FILES+=		data/paren.in
FILES+=		data/regress.in
FILES+=		data/repet_bounded.in
FILES+=		data/repet_multi.in
FILES+=		data/repet_ordinary.in
FILES+=		data/startend.in
FILES+=		data/subexp.in
FILES+=		data/subtle.in
FILES+=		data/word_bound.in
FILES+=		data/zero.in
#FILES+=		data/att/README
FILES+=		data/att/basic.dat
FILES+=		data/att/categorization.dat
FILES+=		data/att/forcedassoc.dat
FILES+=		data/att/leftassoc.dat
FILES+=		data/att/nullsubexpr.dat
FILES+=		data/att/repetition.dat
FILES+=		data/att/rightassoc.dat

NETBSD_ATF_TESTS_C=	exhaust_test
NETBSD_ATF_TESTS_C+=	regex_att_test

.for t in ${NETBSD_ATF_TESTS_C}
CFLAGS.$t+=	-I${TESTSRC} ${IMPLEMENTATION}
.endfor

.include "../Makefile.netbsd-tests"

DPADD.regex_att_test+=	${LIBUTIL}
LDADD.regex_att_test+=	-lutil

.include <bsd.test.mk>
