# $NetBSD: deptgt-path-suffix.mk,v 1.4 2025/06/28 22:39:28 rillig Exp $
#
# Tests for the special target .PATH.suffix in dependency declarations.

# TODO: Implementation

# expect+1: Suffix ".c" not defined (yet)
.PATH.c: ..

.SUFFIXES: .c

# Now the suffix is defined, and the path is recorded.
.PATH.c: ..

all:
	@:;
