# $NetBSD: dep.mk,v 1.6 2026/06/09 08:27:08 rillig Exp $
#
# Tests for dependency declarations, such as "target: sources".

all: .PHONY
	@${MAKE} -f ${MAKEFILE} parse-only-colon || echo "sub exit status $$?"
	@${MAKE} -f ${MAKEFILE} parse-semicolon-in-source
	@${MAKE} -f ${MAKEFILE} parse-semicolon || echo "sub exit status $$?"


.if make(parse-only-colon)
parse-only-colon: .PHONY

# As soon as a target is defined using one of the dependency operators, it is
# restricted to this dependency operator and cannot use the others anymore.
only-colon:
# expect+1: Inconsistent operator for only-colon
only-colon!
# expect+1: Inconsistent operator for only-colon
only-colon::
# Ensure that the target still has the original operator.  If it hadn't, there
# would be another error message.
only-colon:

.endif


# Before parse.c 1.158 from 2009-10-07, the parser broke dependency lines at
# the first ';', without parsing expressions as such.  It interpreted the
# first ';' as the separator between the dependency and its commands, and the
# '^' as a shell command.
.if make(parse-semicolon-in-source)
parse-semicolon-in-source: .PHONY

.  for file in parse-semicolon-in-source
parse-semicolon-in-source:	${file:S;^;renamed-;g}
	@echo "Making ${.TARGET} from ${.ALLSRC}."
renamed-parse-semicolon-in-source:
	@echo "Making ${.TARGET} out of nothing."
.  endfor

.endif


# Before parse.c 1.757 from 2026-06-09, the parser counted "$(" and "${" as
# opening a brace level, and ")" and "}" as closing it.  It didn't count a
# plain "(" or "{" as opening, though.  This simple counting resulted in
# parse errors on modifiers containing parentheses or braces, as well as
# semicolons.
.if make(parse-semicolon)
parse-semicolon: .PHONY

semicolon-in-modifier.${:Utarget:S;target;param;}:; @echo '$@: after semicolon'
parse-semicolon: semicolon-in-modifier.param

parentheses-in-modifier.${:U123:C;(.);\1-\1;g}:; @echo '$@: after semicolon'
parse-semicolon: parentheses-in-modifier.1-12-23-3

var-semicolon.$;:; @echo '$@: after semicolon'
parse-semicolon: var-semicolon.

# Using actual dollars in target names is still tricky and not recommended,
# due to multiple levels of expansion, that's where the 8 dollars come from.
# expect: make: Malformed expression at "$$-rest"
double-dollar.$$$;-$$$$-rest:; @echo '$@: after semicolon'
parse-semicolon: double-dollar.$$$$-$$$$$$$$-rest

.endif
