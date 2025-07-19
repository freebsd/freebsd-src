# $NetBSD: deptgt-begin.mk,v 1.8 2025/06/30 21:44:39 rillig Exp $
#
# Tests for the special target .BEGIN in dependency declarations,
# which is a container for commands that are run before any other
# commands from the shell lines.

.BEGIN:
	: $@

# To register a custom action to be run at the beginning, the simplest way is
# to directly place some commands on the '.BEGIN' target.  This doesn't scale
# though, since the ':' dependency operator prevents that any other place may
# add its commands after this.
#
# There are several ways to resolve this situation, which are detailed below.
# expect+3: warning: duplicate script for target ".BEGIN" ignored
# expect-9: warning: using previous script for ".BEGIN" defined here
.BEGIN:
	: Making another $@.

# One way to run commands at the beginning is to define a custom target and
# make the .BEGIN depend on that target.  This way, the commands from the
# custom target are run even before the .BEGIN target.
.BEGIN: before-begin
before-begin: .PHONY .NOTMAIN
	: Making $@ before .BEGIN.

# Another way is to define a custom target and make that a .USE dependency.
# For the .BEGIN target, .USE dependencies do not work though, since in
# Compat_MakeAll, the .USE and .USEBEFORE nodes are expanded right after the
# .BEGIN target has been made, which is too late.
.BEGIN: use
use: .USE .NOTMAIN
	: Making $@ from a .USE dependency.

# Same as with .USE, but run the commands before the main commands from the
# .BEGIN target.
#
# For the .BEGIN target, .USEBEFORE dependencies do not work though, since in
# Compat_MakeAll, the .USE and .USEBEFORE nodes are expanded right after the
# .BEGIN target has been made, which is too late.
.BEGIN: use-before
use-before: .USEBEFORE .NOTMAIN
	: Making $@ from a .USEBEFORE dependency.

all:
	: $@

_!=	echo : parse time 1>&2
