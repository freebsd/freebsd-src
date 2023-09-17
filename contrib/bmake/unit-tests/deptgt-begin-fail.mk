# $NetBSD: deptgt-begin-fail.mk,v 1.1 2020/11/24 19:02:59 rillig Exp $
#
# Test for a .BEGIN target whose command results in an error.
# This stops make immediately and does not build the main targets.

.BEGIN:
	false

all:
	: This is not made.
