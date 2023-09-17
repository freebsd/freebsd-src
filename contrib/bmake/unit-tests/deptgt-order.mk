# $NetBSD: deptgt-order.mk,v 1.4 2021/12/13 23:38:54 rillig Exp $
#
# Tests for the special target .ORDER in dependency declarations.

all one two three: .PHONY

two: one
	: 'Making $@ out of $>.'
three: two
	: 'Making $@ out of $>.'

# This .ORDER creates a circular dependency since 'three' depends on 'one'
# but 'one' is supposed to be built after 'three'.
.MAKEFLAGS: -dp
.ORDER: three one
.MAKEFLAGS: -d0

# XXX: The circular dependency should be detected here.
all: three
	: 'Making $@ out of $>.'
