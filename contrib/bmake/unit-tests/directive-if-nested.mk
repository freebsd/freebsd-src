# $NetBSD: directive-if-nested.mk,v 1.2 2025/01/03 05:00:37 rillig Exp $
#
# Tests for deeply nested .if directives.  By default, memory for 128 nested
# .if directives is pre-allocated, any deeper nesting is reallocated.
#
# See also:
#	Cond_EvalLine

GEN=	directive-if-nested.inc

all: set-up test tear-down

set-up: .PHONY
	@{ printf '.if %s\n' ${:U:range=1000};				\
	   printf '.info deeply nested .if directives\n';		\
	   printf '.endif # %s\n' ${:U:range=1000:[-1..1]};		\
	   printf '\n';							\
	   printf 'all:\n';						\
	} > ${GEN}

test: .PHONY
	@${MAKE} -f ${GEN}

tear-down: .PHONY
	@rm -f ${GEN}
