# $NetBSD: suff-lookup.mk,v 1.2 2020/10/24 03:18:22 rillig Exp $
#
# Demonstrate name resolution for suffixes.
#
# See also:
#	FindSuffByName

.MAKEFLAGS: -ds

all: suff-lookup.cc

.SUFFIXES: .c .cc .ccc

# Register '.short' before '.sho'.  When searching for the transformation
# '.sho.c', the suffix '.short' must not be found even though it starts with
# the correct characters.
.SUFFIXES: .short .sho .dead-end

# From long to short suffix.
.ccc.cc:
	: 'Making ${.TARGET} from ${.IMPSRC}.'

# From short to long suffix.
.c.ccc:
	: 'Making ${.TARGET} from ${.IMPSRC}.'

.short.c:
	: 'Making ${.TARGET} from ${.IMPSRC}.'
.sho.c:
	: 'Making ${.TARGET} from ${.IMPSRC}.'
.dead-end.short:
	: 'Making ${.TARGET} from ${.IMPSRC}.'

suff-lookup.sho:
	: 'Making ${.TARGET} out of nothing.'

# Deleting all suffixes and adding them again rebuilds all of the above
# transformation rules.
.SUFFIXES:
.SUFFIXES: .c .cc .ccc .short .sho .dead-end
