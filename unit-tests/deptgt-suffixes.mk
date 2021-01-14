# $NetBSD: deptgt-suffixes.mk,v 1.4 2020/11/21 21:54:42 rillig Exp $
#
# Tests for the special target .SUFFIXES in dependency declarations.
#
# See also:
#	varname-dot-includes.mk
#	varname-dot-libs.mk

.MAKEFLAGS: -dg1

.MAIN: all

.SUFFIXES: .custom-null

# TODO: What is the effect of this? How is it useful?
.NULL: .custom-null
.PATH.custom-null: . ..

# The order in which the suffixes are listed doesn't matter.
# Here, they are listed from source to target, just like in the transformation
# rule below it.
.SUFFIXES: .src-left .tgt-right
deptgt-suffixes.src-left:
	: Making ${.TARGET} out of nothing.
.src-left.tgt-right:
	: Making ${.TARGET} from ${.IMPSRC}.
all: deptgt-suffixes.tgt-right

# Here, the target is listed earlier than the source.
.SUFFIXES: .tgt-left .src-right
deptgt-suffixes.src-right:
	: Making ${.TARGET} out of nothing.
.src-right.tgt-left:
	: Making ${.TARGET} from ${.IMPSRC}.
all: deptgt-suffixes.tgt-left
