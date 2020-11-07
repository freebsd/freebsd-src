# $NetBSD: directive-include-fatal.mk,v 1.2 2020/09/13 10:20:11 rillig Exp $
#
# Test for the .include directive combined with fatal errors.
#
# At 2020-09-13, the code in Parse_File that sets "fatals = 0" looked
# suspicious, as if it were possible to suppress fatal errors by including
# another file.  It was a false alarm though, since Parse_File only handles
# the top-level makefiles from the command line.  Any included files are
# handled by Parse_include_file instead, and that function does not reset
# the "fatals" counter.

# Using an undefined variable in a condition generates a fatal error.
.if ${UNDEF}
.endif

# Including another file does not reset the global variable "fatals".
# The exit status will be 1.
.include "/dev/null"

# Adding another file to be included has no effect either.
# When the command line is parsed, the additional file is only enqueued
# in the global "makefiles" variable, but not immediately run through
# Parse_File.
.MAKEFLAGS: -f "/dev/null"

all:
	@:;
