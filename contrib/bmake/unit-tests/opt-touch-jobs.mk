# $NetBSD: opt-touch-jobs.mk,v 1.2 2021/01/30 12:14:08 rillig Exp $
#
# Tests for the -t command line option in jobs mode.

.MAKEFLAGS: -j1
.MAKEFLAGS: -t
.MAKEFLAGS: opt-touch-phony
.MAKEFLAGS: opt-touch-join
.MAKEFLAGS: opt-touch-use
.MAKEFLAGS: opt-touch-make

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

# Even though it is listed last, in the output it appears first.
# This is because it is the only node that actually needs to be run.
# The "is up to date" of the other nodes happens after all jobs have
# finished, by Make_Run > MakePrintStatusList > MakePrintStatus.
opt-touch-make: .MAKE
	: Making $@.

.END:
	@files=$$(ls opt-touch-* 2>/dev/null | grep -v -e '\.' -e '\*'); \
	[ -z "$$files" ] || { echo "created files: $$files" 1>&2; exit 1; }
