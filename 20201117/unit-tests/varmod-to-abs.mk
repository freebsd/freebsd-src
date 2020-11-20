# $NetBSD: varmod-to-abs.mk,v 1.5 2020/11/15 05:48:17 rillig Exp $
#
# Tests for the :tA variable modifier, which returns the absolute path for
# each of the words in the variable value.

# TODO: Implementation

# Between 2016-06-03 and 2020-11-14, it was possible to trick the :tA modifier
# into resolving completely unrelated absolute paths by defining a global
# variable with the same name as the path that is to be resolved.  There were
# a few restrictions though: The "redirected" path had to start with a slash,
# and it had to exist (see ModifyWord_Realpath).
#
# This unintended behavior was caused by cached_realpath using a GNode for
# keeping the cache, just like the GNode for global variables.
.MAKEFLAGS: -dd
does-not-exist.c=	/dev/null
.info ${does-not-exist.c:L:tA}
.info ${does-not-exist.c:L:tA}

# The output of the following line is modified by the global _SED_CMDS in
# unit-tests/Makefile.  See the .rawout file for the truth.
.info ${MAKEFILE:tA}

.MAKEFLAGS: -d0

all:
	@:;
