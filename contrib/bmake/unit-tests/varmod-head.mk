# $NetBSD: varmod-head.mk,v 1.3 2020/08/23 15:09:15 rillig Exp $
#
# Tests for the :H variable modifier, which returns the dirname of
# each of the words in the variable value.

all:
.for path in a/b/c def a.b.c a.b/c a a.a .gitignore a a.a
	@echo "head (dirname) of '"${path:Q}"' is '"${path:H:Q}"'"
.endfor
