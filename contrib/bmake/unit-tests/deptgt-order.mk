# $NetBSD: deptgt-order.mk,v 1.3 2021/06/17 15:25:33 rillig Exp $
#
# Tests for the special target .ORDER in dependency declarations.

all one two three: .PHONY

two: one
	: 'Making $@ out of $>.'
three: two
	: 'Making $@ out of $>.'

# This .ORDER creates a circular dependency since 'three' depends on 'one'
# but 'one' is supposed to be built after 'three'.
.ORDER: three one

# XXX: The circular dependency should be detected here.
all: three
	: 'Making $@ out of $>.'
