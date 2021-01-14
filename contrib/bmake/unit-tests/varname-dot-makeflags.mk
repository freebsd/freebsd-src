# $NetBSD: varname-dot-makeflags.mk,v 1.1 2020/12/01 20:37:30 rillig Exp $
#
# Tests for the special .MAKEFLAGS variable, which collects almost all
# command line arguments and passes them on to any child processes via
# the environment variable MAKEFLAGS (without leading '.').

# When options are parsed, the option and its argument are appended as
# separate words to .MAKEFLAGS.  Special characters in the option argument
# are not quoted though.  It seems to have not been necessary at least from
# 1993 until 2020.
.MAKEFLAGS: -d00000 -D"VARNAME WITH SPACES"

all:
	echo "$$MAKEFLAGS"
	@:;
