# $NetBSD: suff-main-several.mk,v 1.1 2020/11/22 20:36:17 rillig Exp $
#
# Demonstrate that an inference rule is considered the main target if its
# suffixes are not known at the point of declaration.

.MAKEFLAGS: -dmps

.1.2 .1.3 .1.4:
	: Making ${.TARGET} from ${.IMPSRC}.

# At this point, the above targets are normal targets.
# The target '.1.2' is now the default main target.

next-main:
	: Making ${.TARGET}

# At this point, 'next-main' is just a regular target.

.SUFFIXES: .1 .2 .3 .4

# Since the targets '.1.2', '.1.3' and '.1.4' have now been turned into
# transformation rules, 'next-main' is the default main target now.

.SUFFIXES: # clear all

# At this point, 'next-main' is still the default main target, even though
# it is not the first regular target anymore.

# Define and undefine the suffixes, changing their order.
# XXX: This should have no effect, but as of 2020-11-22, it does.
# For some reason, mentioning the suffixes in reverse order disables them.
.SUFFIXES: .4 .3 .2 .1
.SUFFIXES: # none
.SUFFIXES: .1 .2 .3 .4
.SUFFIXES: # none
.SUFFIXES: .4 .3 .2 .1

suff-main-several.1:
	: Making ${.TARGET} out of nothing.
next-main: suff-main-several.{2,3,4}

.MAKEFLAGS: -d0 -dg1
