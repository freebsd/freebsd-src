# $NetBSD: varmod-extension.mk,v 1.3 2020/08/23 15:09:15 rillig Exp $
#
# Tests for the :E variable modifier, which returns the filename extension
# of each word in the variable.

all:
.for path in a/b/c def a.b.c a.b/c a a.a .gitignore a a.a
	@echo "extension of '"${path:Q}"' is '"${path:E:Q}"'"
.endfor
