#	Makefile.inc,v 1.2 1994/09/13 21:26:01 wollman Exp
#	$Id: Makefile.inc,v 1.4 1997/05/03 03:50:04 jb Exp $

.PATH:	${.CURDIR}/../libc/stdtime

SRCS+=	asctime.c localtime.c strftime.c difftime.c timelocal.c strptime.c

# Only build man pages with libc.
.if ${LIB} == "c"
MAN5+=	stdtime/tzfile.5
MAN3+=	stdtime/ctime.3 stdtime/strftime.3 stdtime/time2posix.3
MAN3+=	stdtime/strptime.3

MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3
MLINKS+=time2posix.3 posix2time.3
.endif
