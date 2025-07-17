# $NetBSD: varname-dot-make-jobs.mk,v 1.5 2023/09/10 16:25:32 sjg Exp $
#
# Tests for the special .MAKE.JOBS variable, which is defined in jobs mode
# only.  There it contains the number of jobs that may run in parallel.

.MAIN: all

echo: .PHONY
	@echo ${.MAKE.JOBS:Uundefined}

all:
	@${MAKE} -r -f ${MAKEFILE} echo
	@${MAKE} -r -f ${MAKEFILE} echo -j1
	@${MAKE} -r -f ${MAKEFILE} echo -j5
	@${MAKE} -r -f ${MAKEFILE} echo -j20
	@${MAKE} -r -f ${MAKEFILE} echo -j00000000000000000000000000000001

.if !make(echo) && ${.MAKE.JOBS.C} == "yes"
# These results will not be static, we need NCPU
# to compute expected results.
all:	jC

NCPU!= ${MAKE} -r -f /dev/null -jC -V .MAKE.JOBS

# If -j arg is floating point or ends in C;
# .MAKE.JOBS is a multiple of _SC_NPROCESSORS_ONLN
# No news is good news here.
jCvals ?= 1 1.2 2

jC:
	@for j in ${jCvals}; do \
	e=`echo "${NCPU} * $$j" | bc | sed 's/\.[0-9]*//'`; \
	g=`${MAKE} -r -f /dev/null -V .MAKE.JOBS -j$${j}C`; \
	test $$g = $$e || echo "$$g != $$e"; \
	done

.endif

# expect: undefined
# expect: 1
# expect: 5
# expect: 20
# expect: 1
