# $NetBSD: depsrc-precious.mk,v 1.3 2020/11/15 20:20:58 rillig Exp $
#
# Tests for the special source .PRECIOUS in dependency declarations, which
# is only relevant if the commands for the target fail or are interrupted.
# In such a case, the target file is usually removed, to avoid having
# half-finished files with a timestamp suggesting the file were up-to-date.
#
# For targets marked with .PRECIOUS, the target file is not removed.
# The author of the makefile is then responsible for avoiding the above
# situation, in which the target would be wrongly considered up-to-date,
# just because its timestamp says so.

# TODO: Implementation

all:
	@:;
