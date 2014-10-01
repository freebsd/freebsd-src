# $NetBSD: Makefile,v 1.2 2014/08/22 16:45:32 apb Exp $

# The tests for make(1) are maintained in src/usr.bin/make/unit-tests
# (UNIT_TESTS_DISTDIR).  We copy them verbatim to ${FILESDIR}/unit-tests
# (UNIT_TESTS_DIR) at install time.
#
# The t_make .sh script (installed as ${FILESDIR}/t_make) is the only
# thing that knows about Atf.  At run time, the t_make simply invokes
# the Makefile in UNIT_TESTS_DIR for each test.

NOMAN=			# defined

.include <bsd.own.mk>

TESTSDIR=		${TESTSBASE}/usr.bin/make

TESTS_SH=		t_make

FILESDIR=		${TESTSDIR}

# Where we install the copies of the files for the tests
UNIT_TESTS_DIR=		${FILESDIR}/unit-tests

# Where we obtain the original files for the tests
UNIT_TESTS_DISTDIR=	${NETBSDSRCDIR}/usr.bin/make/unit-tests

# Which original files to copy
DISTFILES!=	(cd ${UNIT_TESTS_DISTDIR} && echo Makefile *.mk *.exp)

# Instruct bsd.files.mk to make the copies
.for f in ${DISTFILES}
_file := ${UNIT_TESTS_DISTDIR}/${f}
FILES+= ${UNIT_TESTS_DISTDIR}/${f}
FILESNAME_${_file} := ${_file:T}
FILESDIR_${_file} := ${UNIT_TESTS_DIR}
.endfor

.include <bsd.test.mk>
