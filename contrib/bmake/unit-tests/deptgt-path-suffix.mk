# $NetBSD: deptgt-path-suffix.mk,v 1.3 2021/12/13 23:38:54 rillig Exp $
#
# Tests for the special target .PATH.suffix in dependency declarations.

# TODO: Implementation

# expect+1: Suffix '.c' not defined (yet)
.PATH.c: ..

.SUFFIXES: .c

# Now the suffix is defined, and the path is recorded.
.PATH.c: ..

all:
	@:;
