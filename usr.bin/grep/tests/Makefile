# $FreeBSD$

TESTSRC=	${.CURDIR}/../../../contrib/netbsd-tests/usr.bin/grep
.PATH: ${TESTSRC}

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/usr.bin/grep

ATF_TESTS_SH=	grep_test
ATF_TESTS_SH_SRC_grep_test=	t_grep.sh

FILESDIR=	${TESTSDIR}
FILES=		d_basic.out
FILES+=		d_begin_end_a.out
FILES+=		d_begin_end_b.out
FILES+=		d_binary.out
FILES+=		d_context2_a.out
FILES+=		d_context2_b.out
FILES+=		d_context2_c.out
FILES+=		d_context_a.in
FILES+=		d_context_a.out
FILES+=		d_context_b.in
FILES+=		d_context_b.out
FILES+=		d_context_c.out
FILES+=		d_context_d.out
FILES+=		d_egrep.out
FILES+=		d_file_exp.in
FILES+=		d_file_exp.out
FILES+=		d_ignore_case.out
FILES+=		d_input
FILES+=		d_invert.in
FILES+=		d_invert.out
FILES+=		d_recurse.out
FILES+=		d_recurse_symlink.err
FILES+=		d_recurse_symlink.out
FILES+=		d_whole_line.out
FILES+=		d_word_regexps.out
FILES+=		d_zgrep.out

.include <bsd.test.mk>
