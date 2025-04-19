# $NetBSD: posix.mk,v 1.5 2025/04/13 09:44:58 rillig Exp $
#
# This file is included in all tests that start with a ".POSIX:" line,
# even when the "-r" option is given.

# The makefile containing the POSIX definitions is not supposed to contain a
# ".POSIX:" line, but even if it does, this must not lead to an endless loop
# by including it over and over again.
.POSIX:

# The file <posix.mk> is not intended to be used as a top-level makefile, and
# it is not supposed to define any targets, only rules.
# expect: make: no target to make.
