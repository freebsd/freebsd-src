# $NetBSD: varmod-head.mk,v 1.5 2022/07/10 21:11:49 rillig Exp $
#
# Tests for the :H variable modifier, which returns the dirname of
# each of the words in the variable value.

.if ${:U a/b/c :H} != "a/b"
.  error
.endif

.if ${:U def :H} != "."
.  error
.endif

.if ${:U a.b.c :H} != "."
.  error
.endif

.if ${:U a.b/c :H} != "a.b"
.  error
.endif

.if ${:U a :H} != "."
.  error
.endif

.if ${:U a.a :H} != "."
.  error
.endif

.if ${:U .gitignore :H} != "."
.  error
.endif

.if ${:U trailing/ :H} != "trailing"
.  error
.endif

.if ${:U /abs/dir/file :H} != "/abs/dir"
.  error
.endif

.if ${:U rel/dir/file :H} != "rel/dir"
.  error
.endif

# The head of "/" was an empty string before 2020.07.20.14.50.41, leading to
# the output "before  after", with two spaces.  Since 2020.07.20.14.50.41, the
# output is "before after", discarding the empty word.
.if ${:U before/ / after/ :H} == "before after"
# OK
.elif ${:U before/ / after/ :H} == "before  after"
# No '.info' to keep the file compatible with old make versions.
_!=	echo "The modifier ':H' generates an empty word." 1>&2; echo
.else
.  error
.endif

# An empty list is split into a single empty word.
# The dirname of this empty word is ".".
.if ${:U :H} != "."
.  error
.endif

all: .PHONY
