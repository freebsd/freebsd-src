# This is a generated file, do NOT edit!
# See contrib/bmake/bsd.after-import.mk
#
# $FreeBSD$

SRCTOP?= ${.CURDIR:H:H:H}

# $Id: Makefile.in,v 1.44 2013/08/28 22:09:29 sjg Exp $
#
# $NetBSD: Makefile,v 1.38 2013/08/28 21:56:50 sjg Exp $
#
# Unit tests for make(1)
# The main targets are:
# 
# all:	run all the tests
# test:	run 'all', capture output and compare to expected results
# accept: move generated output to expected results
#
# Adding a test case.  
# Each feature should get its own set of tests in its own suitably
# named makefile which should be added to SUBFILES to hook it in.
# 

srcdir= ${SRCTOP}/contrib/bmake/unit-tests

.MAIN: all

UNIT_TESTS:= ${srcdir}

# Simple sub-makefiles - we run them as a black box
# keep the list sorted.
SUBFILES= \
	comment \
	cond1 \
	error \
	export \
	export-all \
	export-env \
	doterror \
	dotwait \
	forloop \
	forsubst \
	hash \
	misc \
	moderrs \
	modmatch \
	modmisc \
	modorder \
	modts \
	modword \
	order \
	phony-end \
	posix \
	qequals \
	sunshcmd \
	sysv \
	ternary \
	unexport \
	unexport-env \
	varcmd

all: ${SUBFILES}

flags.doterror=
flags.order=-j1

# the tests are actually done with sub-makes.
.PHONY: ${SUBFILES}
.PRECIOUS: ${SUBFILES}
${SUBFILES}:
	-@${.MAKE} ${flags.$@:U-k} -f ${UNIT_TESTS}/$@

clean:
	rm -f *.out *.fail *.core

.sinclude <bsd.obj.mk>

TEST_MAKE?= ${.MAKE}
TOOL_SED?= sed
TOOL_TR?= tr
TOOL_DIFF?= diff
DIFF_FLAGS?= -u

.if defined(.PARSEDIR)
# ensure consistent results from sort(1)
LC_ALL= C
LANG= C
.export LANG LC_ALL
.endif

# The driver.
# We always pretend .MAKE was called 'make' 
# and strip ${.CURDIR}/ from the output
# and replace anything after 'stopped in' with unit-tests
# so the results can be compared.
test:
	@echo "${TEST_MAKE} -f ${MAKEFILE} > ${.TARGET}.out 2>&1"
	@cd ${.OBJDIR} && ${TEST_MAKE} -f ${MAKEFILE} 2>&1 | \
	${TOOL_TR} -d '\015' | \
	${TOOL_SED} -e 's,^${TEST_MAKE:T:C/\./\\\./g}[^:]*:,make:,' \
	-e '/stopped/s, /.*, unit-tests,' \
	-e 's,${.CURDIR:C/\./\\\./g}/,,g' \
	-e 's,${UNIT_TESTS:C/\./\\\./g}/,,g' > ${.TARGET}.out || { \
	tail ${.TARGET}.out; mv ${.TARGET}.out ${.TARGET}.fail; exit 1; }
	${TOOL_DIFF} ${DIFF_FLAGS} ${UNIT_TESTS}/${.TARGET}.exp ${.TARGET}.out

accept:
	mv test.out ${srcdir}/test.exp

