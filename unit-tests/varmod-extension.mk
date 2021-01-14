# $NetBSD: varmod-extension.mk,v 1.4 2020/12/20 22:57:40 rillig Exp $
#
# Tests for the :E variable modifier, which returns the filename extension
# of each word in the variable.

all:
.for path in a/b/c def a.b.c a.b/c a a.a .gitignore a a.a trailing/
	@echo "extension of '"${path:Q}"' is '"${path:E:Q}"'"
.endfor
