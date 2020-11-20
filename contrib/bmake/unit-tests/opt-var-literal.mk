# $NetBSD: opt-var-literal.mk,v 1.4 2020/11/09 20:50:56 rillig Exp $
#
# Tests for the -V command line option.

.MAKEFLAGS: -V VAR -V VALUE

VAR=	other ${VALUE} $$$$
VALUE=	value
