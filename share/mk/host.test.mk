#
# If MK_RUN_TESTS is "yes"; we want to run tests for "host"
# during the build - and fail the build if any test fails.
#
# HOST_RUN_TESTS can be used to filter which tests should be built and
# run.  It should be a list of glob patterns to apply to RELDIR,
# so "*" would mean all.
# 
# For the DIRDEPS_BUILD, HOST_RUN_TESTS defaults to "*" as the
# selection of tests to build and run is taken controlled at level 0
# in local.dirdeps.mk
#

.if !target(__<bsd.test.mk>__)
.error ${.PARSEFILE} cannot be included directly.
.endif

all:

.if ${.MAKE.LEVEL} > 0 && !empty(_TESTS) && ${MACHINE:Nhost*} == ""

# allow for customization
.-include <local.host.test.mk>

.if ${MK_DIRDEPS_BUILD} == "yes"
# orchestration choices are already made 
HOST_RUN_TESTS ?= *
.else
.if empty(RELDIR)
RELDIR:= ${.CURDIR:S,${SRCTOP}/,,:S,${OBJTOP}/,,}
.endif
.endif

.if ${HOST_RUN_TESTS:Uno:@x@${RELDIR:M$x}@} != ""
all: run-tests
.endif

KYUA?= kyua

# we need to make sure kyua-report can find the results
KYUA_RESULTS?= ${.OBJDIR}/kyua.results
KYUA_ARGS?= --results-file=${KYUA_RESULTS}
KYUA_ENV?= HOME=${KYUA_HOME} TMPDIR=${.OBJDIR}
KYUA_FLAGS?= --config none --loglevel=${KYUA_LOGLEVEL:Uinfo}
KYUA_HOME?= ${OBJTOP}

.if make(debug*)
KYUA_LOGLEVEL?= debug
.endif

# some tests have files they need
.if ${${PACKAGE}FILES:U:NKyuafile} != ""
run-tests run-tests.log: link-test-files
link-test-files: ${${PACKAGE}FILES:NKyuafile}
	@for f in ${.ALLSRC:N*Kyuafile:M*/*}; do \
	    ln -sf $$f .; \
	done
.endif

# we do not want to stage any of this
RUN_TESTS_LOG= run-tests.log
MK_STAGING= no
META_XTRAS+= ${RUN_TESTS_LOG}

run-tests: ${RUN_TESTS_LOG}

# This is the main event.
# Run kyua-test followed by kyua-report.
# If we have any test failues we want to run kyua-report --verbose
# Also on fail, we rename run-tests.log to run-tests.err so we save the
# output but the target will be out-of-date.
# We prepend ${.OBJDIR}:${.OBJDIR:H}: to PATH seen by kyua
# so tests for things like cat, cp, cmp etc can find the one we just built
# rather than the one from the host.
${RUN_TESTS_LOG}: ${_TESTS} Kyuafile
	@( export PATH=${.OBJDIR}:${.OBJDIR:H}:${PATH}; \
	rm -f ${KYUA_RESULTS}; \
	${KYUA_ENV} ${KYUA} ${KYUA_FLAGS} test ${KYUA_ARGS} -k ${.OBJDIR}/Kyuafile --build-root=${.OBJDIR} && \
	${KYUA_ENV} ${KYUA} ${KYUA_FLAGS} report ${KYUA_ARGS} ) > ${.TARGET} || \
	{ mv ${.TARGET} ${.TARGET:R}.err; \
	${KYUA_ENV} ${KYUA} ${KYUA_FLAGS} report ${KYUA_ARGS} --verbose --results-filter broken,failed; echo See ${.TARGET:R:tA}.err; \
	exit 1; }

# make kyua-debug KYUA_DEBUG_ARGS=app:test
kyua-debug:
	@(export PATH=${.OBJDIR}:${.OBJDIR:H}:${PATH}; \
	${KYUA_ENV} ${KYUA} ${KYUA_FLAGS} debug ${KYUA_DEBUG_ARGS}) || true

.endif
