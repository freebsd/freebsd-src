#	$FreeBSD$

NOBIN=
CLEANFILES+=	yearistype

.if defined(LEAPSECONDS)
LEAPFILE=	-L leapseconds
.else
LEAPFILE=
.endif

TZFILES=	africa antarctica asia australasia etcetera europe \
		factory northamerica southamerica systemv
POSIXRULES=	America/New_York

.if defined(OLDTIMEZONES)
TZFILES+=	backward
.endif

.if exists(${.OBJDIR}/yearistype)
YEARISTYPE=${.OBJDIR}/yearistype
.else
YEARISTYPE=${.CURDIR}/yearistype
.endif

all:	yearistype
depend:

yearistype:	yearistype.sh
	cp ${.CURDIR}/yearistype.sh yearistype
	chmod +x yearistype

afterinstall:	yearistype
	umask 022; cd ${.CURDIR}; \
	zic -d ${DESTDIR}/usr/share/zoneinfo -p ${POSIXRULES} \
		${LEAPFILE} -y ${YEARISTYPE} ${TZFILES}
	chown -R bin.bin ${DESTDIR}/usr/share/zoneinfo/*
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 ${.CURDIR}/zone.tab \
		${DESTDIR}/usr/share/zoneinfo

.include <bsd.prog.mk>
