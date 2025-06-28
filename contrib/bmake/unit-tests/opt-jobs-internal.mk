# $NetBSD: opt-jobs-internal.mk,v 1.6 2025/05/23 21:05:56 rillig Exp $
#
# Tests for the (intentionally undocumented) internal -J command line option.

# This test expects
.MAKE.ALWAYS_PASS_JOB_QUEUE= no

all: .PHONY
	@${MAKE} -f ${MAKEFILE} -j1 direct
	@${MAKE} -f ${MAKEFILE} -j1 direct-syntax
	@${MAKE} -f ${MAKEFILE} -j1 direct-open
	@${MAKE} -f ${MAKEFILE} -j1 indirect-open
	@${MAKE} -f ${MAKEFILE} -j1 indirect-expr
	@${MAKE} -f ${MAKEFILE} -j1 indirect-comment
	@${MAKE} -f ${MAKEFILE} -j1 indirect-silent-comment
	@${MAKE} -f ${MAKEFILE} -j1 indirect-expr-empty

detect-mode: .PHONY
	@mode=parallel
	@echo ${HEADING}: mode=$${mode:-compat}

# expect: direct: mode=parallel
direct: .PHONY
	@mode=parallel
	@echo ${.TARGET}: mode=$${mode:-compat}

# expect: make: error: invalid internal option "-J garbage" in "<curdir>"
direct-syntax: .PHONY
	@${MAKE} -f ${MAKEFILE} -J garbage unexpected-target || :

# expect: direct-open: mode=compat
direct-open: .PHONY
	@${MAKE} -f ${MAKEFILE} -J 31,32 detect-mode HEADING=${.TARGET}

# expect: indirect-open: mode=compat
indirect-open: .PHONY
	@${MAKE:U} -f ${MAKEFILE} detect-mode HEADING=${.TARGET}

# When a command in its unexpanded form contains the expression "${MAKE}"
# without any modifiers, the file descriptors get passed around.
# expect: indirect-expr: mode=parallel
indirect-expr: .PHONY
	@${MAKE} -f ${MAKEFILE} detect-mode HEADING=${.TARGET}

# The "# make" comment starts directly after the leading tab and is thus not
# considered a shell command line. No file descriptors are passed around.
# expect: indirect-comment: mode=compat
indirect-comment: .PHONY
	# make
	@${MAKE:U} -f ${MAKEFILE} detect-mode HEADING=${.TARGET}

# When the "# make" comment is prefixed with "@", it becomes a shell command.
# As that shell command contains the plain word "make", the file descriptors
# get passed around.
# expect: indirect-silent-comment: mode=parallel
indirect-silent-comment: .PHONY
	@# make
	@${MAKE:U} -f ${MAKEFILE} detect-mode HEADING=${.TARGET}

# When a command in its unexpanded form contains the plain word "make", the
# file descriptors get passed around.
# expect: indirect-expr-empty: mode=parallel
indirect-expr-empty: .PHONY
	@${:D make}
	@${MAKE:U} -f ${MAKEFILE} detect-mode HEADING=${.TARGET}
