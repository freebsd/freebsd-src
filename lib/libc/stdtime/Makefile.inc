#	Makefile.inc,v 1.2 1994/09/13 21:26:01 wollman Exp
#	$Id: Makefile.inc,v 1.5 1997/08/09 15:43:51 joerg Exp $

.PATH:	${.CURDIR}/../libc/stdtime

SRCS+=	asctime.c localtime.c strftime.c difftime.c timelocal.c strptime.c

.if ${LIB} == "c"
MAN5+=	tzfile.5
MAN3+=	ctime.3 strftime.3 time2posix.3
MAN3+=	strptime.3

MLINKS+=ctime.3 asctime.3 ctime.3 difftime.3 ctime.3 gmtime.3 \
	ctime.3 localtime.3 ctime.3 mktime.3
MLINKS+=time2posix.3 posix2time.3
.endif
