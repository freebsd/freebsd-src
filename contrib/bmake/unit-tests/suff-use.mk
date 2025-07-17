# $NetBSD: suff-use.mk,v 1.2 2022/02/09 21:09:24 rillig Exp $
#
# This test combines a .USE node with suffix rules, trying to add an
# additional command before and after successful compilation of a .c file.
#
# History:
#	make-2001.11.12.21.58.18
#	| : 'Making demo.c out of nothing'
#	| make: don't know how to make demo.o. Stop
#	|
#	| make: stopped in <curdir>
#	| exit status 2
#	make-2007.10.11.21.19.28
#
#	make-2014.08.23.15.05.40
#	| : 'Making demo.c out of nothing'
#	| : 'Compiling demo.c to demo.o'
#	| exit status 0
#	make-2014.09.05.06.57.20
#
#	make-2014.09.07.20.55.34
#	| : 'Making demo.c out of nothing'
#	| make: don't know how to make demo.o. Stop
#	|
#	| make: stopped in <curdir>
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
