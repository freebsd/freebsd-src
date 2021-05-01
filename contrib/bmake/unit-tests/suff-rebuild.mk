# $NetBSD: suff-rebuild.mk,v 1.6 2020/11/21 12:01:16 rillig Exp $
#
# Demonstrates what happens to transformation rules (called inference rules
# by POSIX) when all suffixes are deleted.

all: suff-rebuild-example

.MAKEFLAGS: -dpst

.SUFFIXES:

.SUFFIXES: .a .b .c

suff-rebuild-example.a:
	: Making ${.TARGET} out of nothing.

.a.b:
	: Making ${.TARGET} from ${.IMPSRC}.
.b.c:
	: Making ${.TARGET} from ${.IMPSRC}.
.c:
	: Making ${.TARGET} from ${.IMPSRC}.

# XXX: At a quick glance, the code in SuffUpdateTarget looks as if it were
# possible to delete the suffixes in the middle of the makefile, add back
# the suffixes from before, and have the transformation rules preserved.
#
# As of 2020-09-25, uncommenting the following line results in the error
# message "don't know how to make suff-rebuild-example" though.
#
# If this is a bug, the actual cause is probably that when a suffix
# transformation rule is defined, it is not added to the global list of
# targets, see Suff_EndTransform.  Later, UpdateTargets iterates over exactly
# this global list of targets though.
#
# If UpdateTargets were to iterate over 'transforms' as well, it still
# wouldn't work because the condition 'ptr == target->name' skips these
# transformation rules.

#.SUFFIXES:

# Add the suffixes back.  It should not matter that the order of the suffixes
# is different from before.
.SUFFIXES: .c .b .a
