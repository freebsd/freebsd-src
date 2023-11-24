# $NetBSD: suff-transform-select.mk,v 1.3 2020/11/23 14:47:12 rillig Exp $
#
# https://gnats.netbsd.org/49086, issue 10:
# Explicit dependencies affect transformation rule selection.
#
# If issue10.e is wanted and both issue10.d and issue10.f are available,
# make should choose the .d.e rule, because .d is before .f in .SUFFIXES.
# The bug was that if issue10.d had an explicit dependency on issue10.f,
# it would choose .f.e instead.

.MAKEFLAGS: -ds

_!=	rm -f issue10.*

all: issue10.e

.c.d .d.c .d .d.e .e.d:
	: 'Making ${.TARGET} from ${.IMPSRC} (first set).'

.SUFFIXES: .c .d .e .f .g

.e .e.f .f.e:
	: 'Making ${.TARGET} from ${.IMPSRC} (second set).'

issue10.d issue10.f:
	: 'Making ${.TARGET} out of nothing.'

# XXX: see suff-bug-endless, which must be fixed first.
#.MAKEFLAGS: -dg1

# Before 24-11-2020, resolving all.e ran into an endless loop.
