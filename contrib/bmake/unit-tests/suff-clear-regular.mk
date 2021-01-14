# $NetBSD: suff-clear-regular.mk,v 1.2 2020/12/07 00:53:30 rillig Exp $
#
# https://gnats.netbsd.org/49086, issue 4:
# Suffix rules do not become regular rules when .SUFFIXES is cleared.

all: .a .a.b .b.a

.SUFFIXES: .a .b .c

# At this point, .a and .b are known suffixes, therefore the following
# targets are interpreted as transformation rules.
.a .a.b .b.a:
	: 'Making ${.TARGET} from ${.IMPSRC}.'

# The empty .SUFFIXES discards all previous suffixes.
# This means the above rules should be turned into regular targets.
.SUFFIXES:

# XXX: As of 2020-10-20, the result is unexpected.
# XXX: .a.b is still a transformation rule.
# XXX: .a belongs to "Files that are only sources".
# XXX: .a.b belongs to "Files that are only sources".
# XXX: .b.a belongs to "Files that are only sources".
# XXX: .a is listed in "Transformations".
# XXX: .a.b is listed in "Transformations".
# XXX: .b.a is listed in "Transformations".
# XXX: don't know how to make .a
# XXX: don't know how to make .a.b
# XXX: don't know how to make .b.a
#.MAKEFLAGS: -dg1
