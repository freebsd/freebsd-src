# $NetBSD: Makefile,v 1.3 2012/06/03 23:19:11 joerg Exp $

NOMAN=		# defined

.include <bsd.own.mk>

SUBDIR=		director slave tests check_files

TESTSDIR=	${TESTSBASE}/lib/libcurses

TEST_TERMINFO=	atf.terminfo
TERMINFO_DB=	terminfo
TERMINFODIR=	${TESTSDIR}

FILESDIR=	${TESTSDIR}
FILES=		${TERMINFO_DB}.cdb

TESTS_SH=	t_curses

HOME=		${TESTDIR}

CLEANFILES+=	${TERMINFO_DB}.cdb

realall: ${TERMINFO_DB}.cdb

${TERMINFO_DB}.cdb: ${TOOL_TIC} ${TEST_TERMINFO}
	${TOOL_TIC} -o ${.TARGET} ${.CURDIR}/${TEST_TERMINFO}

.include <bsd.test.mk>
#.include <bsd.subdir.mk>
