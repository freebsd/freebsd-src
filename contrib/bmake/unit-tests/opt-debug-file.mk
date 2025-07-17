# $NetBSD: opt-debug-file.mk,v 1.11 2024/06/30 15:21:24 rillig Exp $
#
# Tests for the -dF command line option, which redirects the debug log
# to a file instead of writing it to stderr.

# Enable debug logging for variable assignments and evaluation (-dv)
# and redirect the debug logging to the given file.
.MAKEFLAGS: -dvFopt-debug-file.debuglog

# This output goes to the debug log file.
VAR=	value ${:Uexpanded}

# Hide the logging output for the remaining actions.
# Before main.c 1.362 from 2020-10-03, it was not possible to disable debug
# logging again.  Since then, an easier way is the undocumented option '-d0'.
.MAKEFLAGS: -dF/dev/null

# Make sure that the debug logging file contains some logging.
DEBUG_OUTPUT:=	${:!cat opt-debug-file.debuglog!}
# Grmbl.  Because of the := operator in the above line, the variable
# value contains ${:Uexpanded}.  This expression is expanded
# when it is used in the condition below.  Therefore, be careful when storing
# untrusted input in variables.
#.MAKEFLAGS: -dc -dFstderr
.if !${DEBUG_OUTPUT:tW:M*VAR = value expanded*}
.  error ${DEBUG_OUTPUT}
.endif

# To get the unexpanded text that was actually written to the debug log
# file, the content of that log file must not be stored in a variable.
#
# XXX: In the :M modifier, a dollar is escaped using '$$', not '\$'.  This
# escaping scheme unnecessarily differs from all other modifiers.
.if !${:!cat opt-debug-file.debuglog!:tW:M*VAR = value $${:Uexpanded}*}
.  error
.endif

.MAKEFLAGS: -d0


# See Parse_Error.
.MAKEFLAGS: -dFstdout
# expect+1: This goes to stderr only, once.
.  info This goes to stderr only, once.
.MAKEFLAGS: -dFstderr
# expect+1: This goes to stderr only, once.
.  info This goes to stderr only, once.
.MAKEFLAGS: -dFopt-debug-file.debuglog
# expect+1: This goes to stderr, and in addition to the debug log.
.  info This goes to stderr, and in addition to the debug log.
.MAKEFLAGS: -dFstderr -d0c
.if ${:!cat opt-debug-file.debuglog!:Maddition:[#]} != 1
.  error
.endif


# See Main_ParseArgLine, which calls Error.
.MAKEFLAGS: -dFstdout
# expect: make: Unterminated quoted string [make 'This goes to stdout only, once.]
.MAKEFLAGS: 'This goes to stdout only, once.
.MAKEFLAGS: -dFstderr
# expect: make: Unterminated quoted string [make 'This goes to stderr only, once.]
.MAKEFLAGS: 'This goes to stderr only, once.
.MAKEFLAGS: -dFopt-debug-file.debuglog
# expect: make: Unterminated quoted string [make 'This goes to stderr, and in addition to the debug log.]
.MAKEFLAGS: 'This goes to stderr, and in addition to the debug log.
.MAKEFLAGS: -dFstderr -d0c
.if ${:!cat opt-debug-file.debuglog!:MUnterminated:[#]} != 1
.  error
.endif


# If the debug log file cannot be opened, make prints an error message and
# exits immediately since the debug log file is usually selected from the
# command line.
_:=	${:!rm opt-debug-file.debuglog!}
.MAKEFLAGS: -dF/nonexistent-6f21c672-a22d-4ef7/opt-debug-file.debuglog
