# $NetBSD: dir.mk,v 1.7 2020/10/31 21:30:03 rillig Exp $
#
# Tests for dir.c.

.MAKEFLAGS: -m /		# hide /usr/share/mk from the debug log

# Dependency lines may use braces for expansion.
# See DirExpandCurly for the implementation.
all: {one,two,three}

# XXX: The above dependency line is parsed as a single node named
# "{one,two,three}".  There are no individual targets "one", "two", "three"
# yet.  The node exists but is not a target since it never appeared
# on the left-hand side of a dependency operator.  However, it is listed
# in .ALLTARGETS (which lists all nodes, not only targets).
.if target(one)
.  error
.endif
.if target({one,two,three})
.  error
.endif
.if ${.ALLTARGETS:M{one,two,three}} != "{one,two,three}"
.  error
.endif

one:
	: 1
two:
	: 2
three:
	: 3

# The braces may start in the middle of a word.
all: f{our,ive}

four:
	: 4
five:
	: 5
six:
	: 6

# Nested braces work as expected since 2020-07-31 19:06 UTC.
# They had been broken at least since 2003-01-01, probably even longer.
all: {{thi,fou}r,fif}teen

thirteen:
	: 13
fourteen:
	: 14
fifteen:
	: 15

# There may be multiple brace groups side by side.
all: {pre-,}{patch,configure}

pre-patch patch pre-configure configure:
	: $@

# Empty pieces are allowed in the braces.
all: {fetch,extract}{,-post}

fetch fetch-post extract extract-post:
	: $@

# The expansions may have duplicates.
# These are merged together because of the dependency line.
all: dup-{1,1,1,1,1,1,1}

dup-1:
	: $@

# Other than in Bash, the braces are also expanded if there is no comma.
all: {{{{{{{{{{single-word}}}}}}}}}}

single-word:
	: $@

# Demonstrate debug logging for filename expansion, especially curly braces.
.MAKEFLAGS: -dd
# The below line does not call Dir_Expand yet.
# It is expanded only when necessary, that is, when the 'debug' target is
# indeed made.
debug: {{thi,fou}r,fif}twen
# Therefore, keep the debug logging active.

.PHONY: one two three four five six
.PHONY: thirteen fourteen fifteen
.PHONY: single-word
.PHONY: pre-patch patch pre-configure configure
.PHONY: fetch fetch-post extract extract-post
.PHONY: dup-1 single-word
.PHONY: all
