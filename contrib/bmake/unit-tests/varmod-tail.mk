# $NetBSD: varmod-tail.mk,v 1.5 2024/06/01 18:44:05 rillig Exp $
#
# Tests for the :T variable modifier, which returns the basename of each of
# the words in the variable value.


# If the ':T' is not directly followed by a delimiting ':' or '}', the
# ':from=to' modifier is tried as a fallback.
.if ${:U Tail :Tail=replaced} != "replaced"
.  error
.endif


all:
.for path in a/b/c def a.b.c a.b/c a a.a .gitignore a a.a trailing/
	@echo "tail (basename) of '"${path:Q}"' is '"${path:T:Q}"'"
.endfor
