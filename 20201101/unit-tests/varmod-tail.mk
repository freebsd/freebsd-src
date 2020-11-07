# $NetBSD: varmod-tail.mk,v 1.3 2020/08/23 15:09:15 rillig Exp $
#
# Tests for the :T variable modifier, which returns the basename of each of
# the words in the variable value.

all:
.for path in a/b/c def a.b.c a.b/c a a.a .gitignore a a.a
	@echo "tail (basename) of '"${path:Q}"' is '"${path:T:Q}"'"
.endfor
