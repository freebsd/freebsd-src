# $NetBSD: opt-no-action-touch.mk,v 1.1 2021/01/30 12:46:38 rillig Exp $
#
# Tests for combining the command line options -n (no action) and -t (touch).
# This combination is unusual and probably doesn't ever happen in practice,
# but still make needs to behave as expected.  The option -n is stronger than
# -t, so instead of being touched, the commands of the targets are printed.
#
# See also:
#	opt-touch-jobs.mk contains the same test without the option -n.

.MAKEFLAGS: -j1 -n -t
.MAKEFLAGS: opt-touch-phony
.MAKEFLAGS: opt-touch-join
.MAKEFLAGS: opt-touch-use
.MAKEFLAGS: opt-touch-make
.MAKEFLAGS: opt-touch-regular

# .PHONY targets are not touched since they do not represent actual files.
# See Job_Touch.
opt-touch-phony: .PHONY
	: Making $@.

# .JOIN targets are not touched since they do not represent actual files.
# See Job_Touch.
opt-touch-join: .JOIN
	: Making $@.

# .USE targets are not touched since they do not represent actual files.
# See Job_Touch.
opt-touch-use: .USE
	: Making use of $@.

# The attribute .MAKE is stronger than the command line option -n.  Therefore
# this target is run as usual.  It is not prefixed by '@', therefore it is
# printed before being run.
opt-touch-make: .MAKE
	echo 'Making $@.'

# Since the option -n is stronger than the option -t, this target is not
# touched either.  Without the -n, it would be touched.
opt-touch-regular:
	: Making $@.

# Since none of the above targets are actually touched, the following command
# does not output anything.
.END:
	@files=$$(ls opt-touch-* 2>/dev/null | grep -v -e '\.'); \
	[ -z "$$files" ] || { echo "created files: $$files" 1>&2; exit 1; }
