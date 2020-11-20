# $NetBSD: directive-include.mk,v 1.4 2020/11/03 17:17:31 rillig Exp $
#
# Tests for the .include directive, which includes another file.

# TODO: Implementation

.MAKEFLAGS: -dc

# All included files are recorded in the variable .MAKE.MAKEFILES.
# In this test, only the basenames of the files are compared since
# the directories can differ.
.include "/dev/null"
.if ${.MAKE.MAKEFILES:T} != "${.PARSEFILE} null"
.  error
.endif

# Each file is recorded only once in the variable .MAKE.MAKEFILES.
# Between 2015-11-26 and 2020-10-31, the very last file could be repeated,
# due to an off-by-one bug in ParseTrackInput.
.include "/dev/null"
.if ${.MAKE.MAKEFILES:T} != "${.PARSEFILE} null"
.  error
.endif

.include "nonexistent.mk"
.include "/dev/null"		# size 0
# including a directory technically succeeds, but shouldn't.
#.include "."			# directory

all:
	@:;
