#	Makefile.inc,v 1.2 1994/09/13 21:26:01 wollman Exp
#	$Id: Makefile.inc,v 1.6 1997/10/15 16:16:21 bde Exp $

.PATH:	${.CURDIR}/../libc/stdtime

SRCS+=	asctime.c difftime.c localtime.c strftime.c strptime.c timelocal.c

.if ${LIB} == "c"
MAN3+=	ctime.3 strftime.3 strptime.3 time2posix.3
MAN5+=	tzfile.5

MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3
MLINKS+=time2posix.3 posix2time.3
.endif
