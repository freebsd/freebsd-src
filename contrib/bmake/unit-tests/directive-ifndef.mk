# $NetBSD: directive-ifndef.mk,v 1.9 2023/10/19 18:24:33 rillig Exp $
#
# Tests for the .ifndef directive, which can be used for multiple-inclusion
# guards.  In contrast to C, where #ifndef and #define nicely line up the
# macro name, there is no such syntax in make.  Therefore, it is more
# common to use .if !defined(GUARD) instead.
#
# See also:
#	directive-include-guard.mk

.ifndef GUARD
GUARD=	# defined
# expect+1: guarded section
.  info guarded section
.endif

.ifndef GUARD
GUARD=	# defined
.  info guarded section
.endif

.if !defined(GUARD)
GUARD=	# defined
.  info guarded section
.endif


# The '.ifndef' directive can be used with multiple arguments, even negating
# them.  Since these conditions are confusing for humans, they should be
# replaced with easier-to-understand plain '.if' directives.
DEFINED=
.ifndef UNDEFINED && UNDEFINED
.else
.  error
.endif
.ifndef UNDEFINED && DEFINED
.  error
.endif
.ifndef DEFINED && DEFINED
.  error
.endif
.ifndef !UNDEFINED && !UNDEFINED
.  error
.endif
.ifndef !UNDEFINED && !DEFINED
.  error
.endif
.ifndef !DEFINED && !DEFINED
.else
.  error
.endif


# The negation from the 'if-not-defined' directive only applies to bare words,
# but not to numbers, quoted strings or expressions.  Those are evaluated
# without extra negation, just like in a plain '.if' directive.
.ifndef 0
.  error
.endif
.ifndef 1
.else
.  error
.endif
.ifndef ""
.  error
.endif
.ifndef "word"
.else
.  error
.endif
.ifndef ${:UUNDEFINED}
.else
.  error
.endif
.ifndef ${:UDEFINED}
.  error
.endif
.ifndef ${:U0}
.  error
.endif
.ifndef ${:U1}
.else
.  error
.endif


all:
