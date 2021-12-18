# $NetBSD: varmod-root.mk,v 1.5 2021/12/05 22:31:58 rillig Exp $
#
# Tests for the :R variable modifier, which returns the filename root
# without the extension.

.if ${a/b/c:L:R} != "a/b/c"
.  error
.endif

.if ${def:L:R} != "def"
.  error
.endif

.if ${a.b.c:L:R} != "a.b"
.  error
.endif

.if ${a.b/c:L:R} != "a"
.  error
.endif

.if ${a:L:R} != "a"
.  error
.endif

.if ${a.a:L:R} != "a"
.  error
.endif

.if ${.gitignore:L:R} != ""
.  error
.endif

.if ${trailing/:L:R} != "trailing/"
.  error
.endif

all:
