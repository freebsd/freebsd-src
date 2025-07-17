# $NetBSD: suff-add-later.mk,v 1.2 2020/10/21 08:18:24 rillig Exp $
#
# https://gnats.netbsd.org/49086, issue 5:
# Adding more suffixes does not turn existing rules into suffix rules.

.MAKEFLAGS: -ds

all: issue5a.d issue5b.c issue5c issue5d.e issue5e.d

.SUFFIXES: .c

# At this point, only .c is a suffix, therefore the following are all regular
# rules.
.c.d .d.c .d .d.e .e.d:
	: 'Making ${.TARGET} from ${.IMPSRC}.'

# Adding .d and .e as suffixes should turn the above regular rules into
# suffix rules.
.SUFFIXES: .d .e

issue5a.c issue5b.d issue5c.d issue5d.d issue5e.e:
	: 'Making ${.TARGET} out of nothing.'

# XXX: As of 2020-10-20, the result is unexpected.
# XXX: .d.c is not a transformation rule but a regular target.
# XXX: .d is not a transformation rule but a regular target.
# XXX: .e.d is not a transformation but a regular target.
# XXX: .c.d is listed as "Files that are only sources".
# XXX: .d.e is listed as "Files that are only sources".
# XXX: The suffixes .c and .e both have the number 2.
# XXX: don't know how to make issue5a.d (even though .c.d is a transformation
# rule and issue5a.c can be readily made)
#.MAKEFLAGS: -dg1
.MAKEFLAGS: -d0
