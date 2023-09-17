# $NetBSD: suff-main.mk,v 1.1 2020/10/18 16:33:18 rillig Exp $
#
# Demonstrate that an inference rule is considered the main target if its
# suffixes are not known at the point of declaration.

.1.2:
	: Making ${.TARGET} from ${.IMPSRC}.

# At this point, the target '.1.2' is a normal target.
# Since it is the first target in the first dependency declaration,
# it becomes the main target.

next-main:
	: Making ${.TARGET}

# At this point, 'next-main' is effectively ignored.

# Declaring both '.1' and '.2' as suffixes turns the '.1.2' target into an
# inference rule (OP_TRANSFORM).  As a side effect, this target is no longer
# a candidate for the main target.  Therefore the next target is selected as
# the main target, which in this case is 'next-main'.
.SUFFIXES: .1 .2
