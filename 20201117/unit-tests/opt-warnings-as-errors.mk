# $NetBSD: opt-warnings-as-errors.mk,v 1.4 2020/11/09 20:50:56 rillig Exp $
#
# Tests for the -W command line option, which turns warnings into errors.

.MAKEFLAGS: -W

.warning message 1
.warning message 2

_!=	echo 'parsing continues' 1>&2

all:
	@:;
