# $NetBSD: suff.mk,v 1.3 2025/01/14 21:39:25 rillig Exp $
#
# Demonstrate suffix rules and dependency resolution.


# Circumvent the file system cache.
.if !make(init) && !make(step*)
all:
	@${MAKE} -f ${MAKEFILE} init
	@${MAKE} -f ${MAKEFILE} step1
.endif


.if make(init)
init:
.  if ${.PARSEDIR:tA} != ${.CURDIR:tA}
${:U}!=		cd ${MAKEFILE:H} && cp a*.mk ${.CURDIR}
.  endif
.endif


.if make(step1)
step1: .PHONY edge-case.to everything

.MAKEFLAGS: -dsv

.SUFFIXES: .from .to

.from.to:
	: Making ${.TARGET} from ${.ALLSRC}.

# When making this target, ${.ARCHIVE} is undefined, but there's no warning.
# expect: Var_Parse: ${.ARCHIVE}.additional (eval)
edge-case.to: ${.PREFIX}${.ARCHIVE}.additional

edge-case.from edge-case.additional:
	: Making ${.TARGET} out of nothing.

everything: .PHONY a*.mk
	: Making ${.TARGET} from ${.ALLSRC}.
.endif
