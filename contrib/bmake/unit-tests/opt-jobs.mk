# $NetBSD: opt-jobs.mk,v 1.7 2025/05/20 17:56:40 sjg Exp $
#
# Tests for the -j command line option, which creates the targets in parallel.


# The option '-j <integer>' specifies the number of targets that can be made
# in parallel.
ARGS=		0 1 2 8 08 017 0x10 -5 1000
EXPECT.0=	argument '0' to option '-j' must be a positive number (exit 2)
EXPECT.1=	1
EXPECT.2=	2
EXPECT.8=	8
EXPECT.08=	argument '08' to option '-j' must be a positive number (exit 2)
EXPECT.017=	15
EXPECT.0x10=	16
EXPECT.-5=	argument '-5' to option '-j' must be a positive number (exit 2)
EXPECT.1000=	1000

.for arg in ${ARGS}
OUTPUT!=	${MAKE} -r -f /dev/null -j ${arg} -v .MAKE.JOBS 2>&1 || echo "(exit $$?)"
.  if ${OUTPUT:[2..-1]} != ${EXPECT.${arg}}
.      warning ${arg}:${.newline}    have: ${OUTPUT:[2..-1]}${.newline}    want: ${EXPECT.${arg}}
.  endif
.endfor


# The options '-j <float>' and '-j <integer>C' multiply the given number with
# the number of available CPUs.
ARGS=		0.0 0C 0.0C .00001 .00001C 1C 1CPUs 1.2 .5e1C 07.5C 08.5C
EXPECT.0.0=	argument '0.0' to option '-j' must be a positive number (exit 2)
EXPECT.0C=	<integer>		# rounded up to 1C
EXPECT.0.0C=	argument '0.0C' to option '-j' must be a positive number (exit 2)
EXPECT..00001=	argument '.00001' to option '-j' must be a positive number (exit 2)
EXPECT..00001C=	argument '.00001C' to option '-j' must be a positive number (exit 2)
EXPECT.1C=	<integer>
EXPECT.1CPUs=	<integer>
EXPECT.1.2=	<integer>
EXPECT..5e1C=	<integer>		# unlikely to occur in practice
EXPECT.07.5C=	<integer>
EXPECT.08.5C=	argument '08.5C' to option '-j' must be a positive number (exit 2)

.if ${.MAKE.JOBS.C} == "yes"
.  for arg in ${ARGS}
OUTPUT!=	${MAKE} -r -f /dev/null -j ${arg} -v .MAKE.JOBS 2>&1 || echo "(exit $$?)"
.    if ${OUTPUT:C,^[0-9]+$,numeric,W} == numeric
OUTPUT=		<integer>
.    endif
.    if ${OUTPUT:[2..-1]} != ${EXPECT.${arg}}
.      warning ${arg}:${.newline}    have: ${OUTPUT:[2..-1]}${.newline}    want: ${EXPECT.${arg}}
.    endif
.  endfor
.endif

all: .PHONY
