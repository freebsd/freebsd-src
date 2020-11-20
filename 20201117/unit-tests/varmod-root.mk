# $NetBSD: varmod-root.mk,v 1.3 2020/08/23 15:09:15 rillig Exp $
#
# Tests for the :R variable modifier, which returns the filename root
# without the extension.

all:
.for path in a/b/c def a.b.c a.b/c a a.a .gitignore a a.a
	@echo "root of '"${path:Q}"' is '"${path:R:Q}"'"
.endfor
