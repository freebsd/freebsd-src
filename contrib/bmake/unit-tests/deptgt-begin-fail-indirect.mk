# $NetBSD: deptgt-begin-fail-indirect.mk,v 1.1 2020/11/24 19:02:59 rillig Exp $
#
# Test for a .BEGIN target whose dependency results in an error.
# This stops make immediately and does not build the main targets.
#
# Between 2005-05-08 and 2020-11-24, a failing dependency of the .BEGIN node
# would not stop make from running the main targets.  In the end, the exit
# status was even 0.

.BEGIN: failing

failing: .PHONY .NOTMAIN
	false

all:
	: This is not made.
