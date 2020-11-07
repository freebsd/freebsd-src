# $NetBSD: opt-var-expanded.mk,v 1.3 2020/08/23 14:28:04 rillig Exp $
#
# Tests for the -v command line option.

VAR=	other ${VALUE} $$$$
VALUE=	value
