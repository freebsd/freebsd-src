# $NetBSD: opt-var-expanded.mk,v 1.4 2020/11/09 20:50:56 rillig Exp $
#
# Tests for the -v command line option.

.MAKEFLAGS: -v VAR -v VALUE

VAR=	other ${VALUE} $$$$
VALUE=	value
