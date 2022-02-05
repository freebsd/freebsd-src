# $NetBSD: opt-raw.mk,v 1.3 2022/01/23 16:09:38 rillig Exp $
#
# Tests for the -r command line option, which skips the system-defined default
# rules from <sys.mk>.

# To provide a clean testing environment without unintended side effects,
# these unit tests run make with the option '-r' by default.  This means there
# are no predefined suffixes and no predefined tools.

.if defined(CC)
.  error
.endif

all: .PHONY
