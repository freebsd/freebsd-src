# $NetBSD: deptgt-end.mk,v 1.3 2020/08/29 17:34:21 rillig Exp $
#
# Tests for the special target .END in dependency declarations,
# which is run after making the desired targets.

.BEGIN:
	: $@

.END:
	: $@

all:
	: $@
