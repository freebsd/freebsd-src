# $NetBSD: suff-rebuild.mk,v 1.2 2020/10/18 16:12:39 rillig Exp $
#
# Demonstrates what happens to transformation rules (called inference rules
# by POSIX) when all suffixes are deleted.

all: suff-rebuild-example

.SUFFIXES:

.SUFFIXES: .a .b .c

suff-rebuild-example.a:
	: from nothing to a

.a.b:
	: from a to b
.b.c:
	: from b to c
.c:
	: from c to nothing

# XXX: At a quick glance, the code in SuffScanTargets looks as if it were
# possible to delete the suffixes in the middle of the makefile, add back
# the suffixes from before, and have the transformation rules preserved.
#
# As of 2020-09-25, uncommenting the following line results in the error
# message "don't know how to make suff-rebuild-example" though.
#
#.SUFFIXES:

# Add the suffixes back.  It should not matter that the order of the suffixes
# is different from before.
.SUFFIXES: .c .b .a
