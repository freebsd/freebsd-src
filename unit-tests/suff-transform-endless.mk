# $NetBSD: suff-transform-endless.mk,v 1.4 2020/11/23 14:47:12 rillig Exp $

# https://gnats.netbsd.org/49086, issue 6:
# Transformation search can end up in an infinite loop.
#
# There is no file or target from which issue6.f could be made, so
# this should fail.  The bug is that because rules .e.f, .d.e and .e.d
# exist, make would try to make .f from .e and then infinitely try
# to do .e from .d and vice versa.

.MAKEFLAGS: -ds

all: issue6.f

.c.d .d.c .d .d.e .e.d:
	: 'Making ${.TARGET} from ${.IMPSRC}.'

.SUFFIXES: .c .d .e .f

.e .e.f .f.e:
	: 'Making ${.TARGET} from ${.IMPSRC}.'

# XXX: As of 2020-10-20, the result is unexpected.
# XXX: .d.c is not a transformation rule.
# XXX: .d is not a transformation rule.
# XXX: .e.d is not a transformation rule.
# XXX: .c.d is listed as "Files that are only sources".
# XXX: .d.e is listed as "Files that are only sources".
# XXX: The suffixes .d and .f both have the number 3.
# XXX: .c.d is not listed as "Transformations".
# XXX: .d.c is not listed as "Transformations".
# XXX: .d is not listed as "Transformations".
# XXX: .d.e is not listed as "Transformations".
# XXX: .e.d is not listed as "Transformations".
# XXX: Found 'all' as '(not found)'
# XXX: trying all.e, all.e, all.f, all.e, all.e, repeatedly.
#.MAKEFLAGS: -dg1

# Before 24-11-2020, resolving all.e ran into an endless loop.
