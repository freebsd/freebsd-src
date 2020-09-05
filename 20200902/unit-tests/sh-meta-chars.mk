# $NetBSD: sh-meta-chars.mk,v 1.2 2020/08/16 14:25:16 rillig Exp $
#
# Tests for running shell commands that contain meta-characters.
#
# These meta-characters decide whether the command is run by the shell
# or executed directly via execv.  See Cmd_Exec for details.

# TODO: Implementation

all:
	@:;
