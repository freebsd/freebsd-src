# $NetBSD: opt-var-literal.mk,v 1.3 2020/08/23 14:28:04 rillig Exp $
#
# Tests for the -V command line option.

VAR=	other ${VALUE} $$$$
VALUE=	value
