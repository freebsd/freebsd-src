# $NetBSD: deptgt-suffixes.mk,v 1.3 2020/08/28 04:05:35 rillig Exp $
#
# Tests for the special target .SUFFIXES in dependency declarations.
#
# See also:
#	varname-dot-includes.mk
#	varname-dot-libs.mk

.MAKEFLAGS: -dg1

.SUFFIXES: .custom-null

# TODO: What is the effect of this? How is it useful?
.NULL: .custom-null
.PATH.custom-null: . ..

all:
	@:;
