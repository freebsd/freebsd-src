# $NetBSD: varmod-root.mk,v 1.4 2020/12/20 22:57:40 rillig Exp $
#
# Tests for the :R variable modifier, which returns the filename root
# without the extension.

all:
.for path in a/b/c def a.b.c a.b/c a a.a .gitignore a a.a trailing/
	@echo "root of '"${path:Q}"' is '"${path:R:Q}"'"
.endfor
