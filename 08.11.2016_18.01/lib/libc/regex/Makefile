# $NetBSD: Makefile,v 1.9 2012/08/24 20:24:40 jmmv Exp $

MKMAN=	no

.include <bsd.own.mk>

TESTSDIR?=	${TESTSBASE}/lib/libc/regex
IMPLEMENTATION?=	-DREGEX_SPENCER -DSKIP_LEFTASSOC

BINDIR=		${TESTSDIR}
PROGS?=		h_regex
SRCS.h_regex=	main.c split.c debug.c
CPPFLAGS+=	-I${NETBSDSRCDIR}/lib/libc/regex ${IMPLEMENTATION}

TESTS_SH?=	t_regex
TESTS_C=	t_regex_att
TESTS_C+=	t_exhaust

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

.include <bsd.test.mk>
