#	$Id$

SRCS+=	stdtime/asctime.c stdtime/localtime.c stdtime/strftime.c \
	stdtime/difftime.c
MAN5+=	tzfile.5

CFLAGS+= -DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone -DSTD_INSPIRED -DPCTS
CFLAGS+= -DHAVE_LONG_DOUBLE -DTZDIR=\"/usr/share/zoneinfo\"

