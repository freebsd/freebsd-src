#	@(#)Makefile	8.1 (Berkeley) 6/8/93

# Change the line below for your time zone (after finding the zone you want in
# the time zone files, or adding it to a time zone file).
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -l rightzone

LOCALTIME=	US/Pacific

# If you want something other than Eastern United States time as a template
# for handling POSIX-style time zone environment variables,
# change the line below (after finding the zone you want in the
# time zone files, or adding it to a time zone file).
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -p rightzone

POSIXRULES=	US/Pacific

# Use an absolute path name for TZDIR unless you're just testing the software.

TZDIR=	${DESTDIR}/usr/share/zoneinfo

# If you always want time values interpreted as "seconds since the epoch
# (not counting leap seconds)", use
# 	REDO=		posix_only
# below.  If you always want right time values interpreted as "seconds since
# the epoch" (counting leap seconds)", use
#	REDO=		right_only
# below.  If you want both sets of data available, with leap seconds not
# counted normally, use
#	REDO=		posix_right
# below.  If you want both sets of data available, with leap seconds counted
# normally, use
#	REDO=		right_posix
# below.

REDO=		right_only

# If you're running on a System V-style system and don't want lint grief,
# add
#	-DUSG
# to the end of the "CFLAGS=" line.
#
# If you're running on a system where "strchr" is known as "index",
# (for example, a 4.[012]BSD system), add
#	-Dstrchr=index
# to the end of the "CFLAGS=" line.
#
# If you're running on a system with a "mkdir" function, feel free to add
#	-Demkdir=mkdir
# to the end of the "CFLAGS=" line
#
# If you want to use System V compatibility code, add
#	-DUSG_COMPAT
# to the end of the "CFLAGS=" line.
#
# If your system has a "GMT offset" field in its "struct tm"s
# (or if you decide to add such a field in your system's "time.h" file),
# add the name to a define such as
#	-DTM_GMTOFF=tm_gmtoff
# or
#	-DTM_GMTOFF=_tm_gmtoff
# to the end of the "CFLAGS=" line.
#
# If your system has a "GMT offset" field in its "struct tm"s
# (or if you decide to add such a field in your system's "time.h" file),
# add the name to a define such as
#	-DTM_ZONE=tm_zone
# or
#	-DTM_ZONE=_tm_zone
# to the end of the "CFLAGS=" line.
#
# If you want code inspired by certain emerging standards, add
#	-DSTD_INSPIRED
# to the end of the "CFLAGS=" line.
#
# If you want Source Code Control System ID's left out of object modules, add
#	-DNOID
# to the end of the "CFLAGS=" line.
#
# If you'll never want to handle solar-time-based time zones, add
#	-DNOSOLAR
# to the end of the "CFLAGS=" line
# (and comment out the "SDATA=" line below).
#
# If you want to allocate state structures in localtime, add
#	-DALL_STATE
# to the end of the "CFLAGS=" line.
#
# If you want an "altzone" variable (a la System V Release 3.1), add
#	-DALTZONE
# to the end of the "CFLAGS=" line.
#
# If you want a "gtime" function (a la MACH), add
#	-DCMUCS
# to the end of the "CFLAGS=" line

.PATH:	${.CURDIR}/datfiles
CFLAGS=	-DTM_GMTOFF=tm_gmtoff -DTM_ZONE=tm_zone
PROG=	zic
MAN5=	tzfile.0

SRCS=	zic.c scheck.c ialloc.c

YDATA=	africa antarctica asia australasia europe northamerica \
	southamerica pacificnew etcetera factory
NDATA=	systemv
#SDATA=	solar87 solar88 solar89
TDATA=	${YDATA} ${NDATA} ${SDATA}
DATA=	${YDATA} ${NDATA} ${SDATA} leapseconds
USNO=	usno1988 usno1989

posix_only: ${TDATA}
	(cd ${.CURDIR}/datfiles; \
	    ../obj/zic -d ${TZDIR} -L /dev/null ${TDATA})

right_only: leapseconds ${TDATA}
	(cd ${.CURDIR}/datfiles; \
	    ../obj/zic -d ${TZDIR} -L leapseconds ${TDATA})

other_two: leapseconds ${TDATA}
	(cd ${.CURDIR}/datfiles; \
	    ../obj/zic -d ${TZDIR}/posix -L /dev/null ${TDATA})
	(cd ${.CURDIR}/datfiles; \
	    ../obj/zic -d ${TZDIR}/right -L leapseconds ${TDATA})

posix_right: posix_only other_two

right_posix: right_only other_two

install: maninstall ${DATA} ${REDO}
	(cd ${.CURDIR}/datfiles && ../obj/zic -d ${TZDIR} -p ${POSIXRULES})
	install -c -o ${BINOWN} -g ${BINGRP} -m 444 \
	    ${TZDIR}/${LOCALTIME} ${DESTDIR}/etc/localtime
	chown -R ${BINOWN}.${BINGRP} ${TZDIR}
	chmod -R a-w ${TZDIR}

.include <bsd.prog.mk>
