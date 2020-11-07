# $NetBSD: suff-clear-single.mk,v 1.1 2020/10/20 20:36:53 rillig Exp $
#
# https://gnats.netbsd.org/49086, issue 3:
# Single suffix rules remain active after .SUFFIXES is cleared.
#
# There's a rule for issue3.a, but .a is no longer a known suffix when
# targets are being made, so issue3 should not get made.

all: issue3

.SUFFIXES: .a .b .c

.a .a.b .b.a:
	: 'Making ${.TARGET} from ${.IMPSRC}.'

.SUFFIXES:

issue3.a:
	: 'There is a bug if you see this.'
