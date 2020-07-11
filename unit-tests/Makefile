# $Id: Makefile,v 1.60 2020/07/10 00:48:32 sjg Exp $
#
# $NetBSD: Makefile,v 1.63 2020/07/09 22:40:14 sjg Exp $
#
# Unit tests for make(1)
#
# The main targets are:
#
# all:
#	run all the tests
# test:
#	run 'all', and compare to expected results
# accept:
#	move generated output to expected results
#
# Settable variables
#
# TEST_MAKE
#	The make program to be tested.
#
#
# Adding a test case
#
# Each feature should get its own set of tests in its own suitably
# named makefile (*.mk), with its own set of expected results (*.exp),
# and it should be added to the TESTS list.
#
# Any added files must also be added to src/distrib/sets/lists/tests/mi.
# Makefiles that are not added to TESTS must be ignored in
# src/tests/usr.bin/make/t_make.sh (example: include-sub).
#

# Each test is in a sub-makefile.
# Keep the list sorted.
TESTS+=		comment
TESTS+=		cond-late
TESTS+=		cond-short
TESTS+=		cond1
TESTS+=		cond2
TESTS+=		dollar
TESTS+=		doterror
TESTS+=		dotwait
TESTS+=		error
TESTS+=		# escape	# broken by reverting POSIX changes
TESTS+=		export
TESTS+=		export-all
TESTS+=		export-env
TESTS+=		forloop
TESTS+=		forsubst
TESTS+=		hash
TESTS+=		# impsrc	# broken by reverting POSIX changes
TESTS+=		include-main
TESTS+=		misc
TESTS+=		moderrs
TESTS+=		modmatch
TESTS+=		modmisc
TESTS+=		modorder
TESTS+=		modts
TESTS+=		modword
TESTS+=		order
TESTS+=		# phony-end	# broken by reverting POSIX changes
TESTS+=		posix
TESTS+=		# posix1	# broken by reverting POSIX changes
TESTS+=		qequals
TESTS+=		# suffixes	# broken by reverting POSIX changes
TESTS+=		sunshcmd
TESTS+=		sysv
TESTS+=		ternary
TESTS+=		unexport
TESTS+=		unexport-env
TESTS+=		varcmd
TESTS+=		varmisc
TESTS+=		varmod-edge
TESTS+=		varquote
TESTS+=		varshell

# Override make flags for certain tests; default is -k.
FLAGS.doterror=		# none
FLAGS.order=		-j1

# Some tests need extra post-processing.
SED_CMDS.modmisc+=	-e 's,\(substitution error:\).*,\1 (details omitted),'
SED_CMDS.varshell+=	-e 's,^[a-z]*sh: ,,'
SED_CMDS.varshell+=	-e '/command/s,No such.*,not found,'

# End of the configuration section.

.MAIN: all

.-include "Makefile.config"

UNIT_TESTS:=	${.PARSEDIR}
.PATH: ${UNIT_TESTS}

OUTFILES=	${TESTS:=.out}

all: ${OUTFILES}

CLEANFILES+=		*.rawout *.out *.status *.tmp *.core *.tmp
CLEANFILES+=		obj*.[och] lib*.a	# posix1.mk
CLEANFILES+=		issue* .[ab]*		# suffixes.mk
CLEANRECURSIVE+=	dir dummy		# posix1.mk

clean:
	rm -f ${CLEANFILES}
.if !empty(CLEANRECURSIVE)
	rm -rf ${CLEANRECURSIVE}
.endif

TEST_MAKE?=	${.MAKE}
TOOL_SED?=	sed
TOOL_TR?=	tr
TOOL_DIFF?=	diff
DIFF_FLAGS?=	-u

.if defined(.PARSEDIR)
# ensure consistent results from sort(1)
LC_ALL=		C
LANG=		C
.export LANG LC_ALL
.endif

# the tests are actually done with sub-makes.
.SUFFIXES: .mk .rawout .out
.mk.rawout:
	@echo ${TEST_MAKE} ${FLAGS.${.TARGET:R}:U-k} -f ${.IMPSRC}
	-@cd ${.OBJDIR} && \
	{ ${TEST_MAKE} ${FLAGS.${.TARGET:R}:U-k} -f ${.IMPSRC} \
	  2>&1 ; echo $$? >${.TARGET:R}.status ; } > ${.TARGET}.tmp
	@mv ${.TARGET}.tmp ${.TARGET}

# Post-process the test output so that the results can be compared.
#
# always pretend .MAKE was called 'make'
_SED_CMDS+=	-e 's,^${TEST_MAKE:T:S,.,\\.,g}[][0-9]*:,make:,'
_SED_CMDS+=	-e 's,${TEST_MAKE:S,.,\\.,g},make,'
# replace anything after 'stopped in' with unit-tests
_SED_CMDS+=	-e '/stopped/s, /.*, unit-tests,'
# strip ${.CURDIR}/ from the output
_SED_CMDS+=	-e 's,${.CURDIR:S,.,\\.,g}/,,g'
_SED_CMDS+=	-e 's,${UNIT_TESTS:S,.,\\.,g}/,,g'

.rawout.out:
	@echo postprocess ${.TARGET}
	@${TOOL_SED} ${_SED_CMDS} ${SED_CMDS.${.TARGET:R}} \
	  < ${.IMPSRC} > ${.TARGET}.tmp
	@echo "exit status `cat ${.TARGET:R}.status`" >> ${.TARGET}.tmp
	@mv ${.TARGET}.tmp ${.TARGET}

# Compare all output files
test:	${OUTFILES} .PHONY
	@failed= ; \
	for test in ${TESTS}; do \
	  ${TOOL_DIFF} ${DIFF_FLAGS} ${UNIT_TESTS}/$${test}.exp $${test}.out \
	  || failed="$${failed}$${failed:+ }$${test}" ; \
	done ; \
	if [ -n "$${failed}" ]; then \
	  echo "Failed tests: $${failed}" ; false ; \
	else \
	  echo "All tests passed" ; \
	fi

accept:
	@for test in ${TESTS}; do \
	  cmp -s ${UNIT_TESTS}/$${test}.exp $${test}.out \
	  || { echo "Replacing $${test}.exp" ; \
	       cp $${test}.out ${UNIT_TESTS}/$${test}.exp ; } \
	done

.if exists(${TEST_MAKE})
${TESTS:=.rawout}: ${TEST_MAKE}
.endif

.-include <obj.mk>
