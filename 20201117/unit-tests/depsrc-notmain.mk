# $NetBSD: depsrc-notmain.mk,v 1.3 2020/09/05 15:57:12 rillig Exp $
#
# Tests for the special source .NOTMAIN in dependency declarations,
# which prevents the associated target from becoming the default target
# to be made.

ignored: .NOTMAIN
	: ${.TARGET}

all:
	: ${.TARGET}
