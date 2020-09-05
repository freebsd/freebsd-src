# $NetBSD: opt-warnings-as-errors.mk,v 1.3 2020/08/23 14:28:04 rillig Exp $
#
# Tests for the -W command line option, which turns warnings into errors.

.warning message 1
.warning message 2

_!=	echo 'parsing continues' 1>&2

all:
	@:;
