# $NetBSD: suff-use.mk,v 1.1 2022/02/07 22:43:50 rillig Exp $
#
# This test combines a .USE node with suffix rules, trying to add an
# additional command before and after successful compilation of a .c file.
#
# History:
#	bin/make-2001.11.12.21.58.18-plain
#	| : 'Making demo.c out of nothing'
#	| make: don't know how to make demo.o. Stop
#	|
#	| make: stopped in /home/rillig/proj/make-archive
#	| exit status 2
#	bin/make-2007.10.11.21.19.28-plain
#
#	bin/make-2014.08.23.15.05.40-plain
#	| : 'Making demo.c out of nothing'
#	| : 'Compiling demo.c to demo.o'
#	| exit status 0
#	bin/make-2014.09.05.06.57.20-plain
#
#	bin/make-2014.09.07.20.55.34-plain
#	| : 'Making demo.c out of nothing'
#	| make: don't know how to make demo.o. Stop
#	|
#	| make: stopped in /home/rillig/proj/make-archive
#	| exit status 2
#	...
#
# See also:
#	https://gnats.netbsd.org/20993


.SUFFIXES: .c .o

all: demo.o

.c.o:
	: 'Compiling ${.IMPSRC} to ${.TARGET}'

demo.c:
	: 'Making ${.TARGET} out of nothing'

using-before: .USEBEFORE
	: 'Before making ${.TARGET} from ${.ALLSRCS}'

using-after: .USE
	: 'After making ${.TARGET} from ${.ALLSRCS}'

# expect: make: don't know how to make demo.o (continuing)
.c.o: using-before using-after
