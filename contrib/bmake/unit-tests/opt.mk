# $NetBSD: opt.mk,v 1.6 2020/11/18 01:06:59 sjg Exp $
#
# Tests for the command line options.

.MAKEFLAGS: -d0			# make stdout line-buffered

all: .IGNORE
	# The options from the top-level make are passed to the sub-makes via
	# the environment variable MAKEFLAGS.  This is where the " -r -k -d 0"
	# comes from.  See MainParseArg.
	${MAKE} -r -f /dev/null -V MAKEFLAGS
	@echo

	# Just to see how the custom argument parsing code reacts to a syntax
	# error.  The colon is used in the options string, marking an option
	# that takes arguments.  It is not an option by itself, though.
	${MAKE} -:
	@echo

	# See whether a '--' stops handling of command line options, like in
	# standard getopt programs.  Yes, it does, and it treats the
	# second '-f' as a target to be created.
	${MAKE} -r -f /dev/null -- -VAR=value -f /dev/null
	@echo

	# This is the normal way to print the usage of a command.
	${MAKE} -?
	@echo
