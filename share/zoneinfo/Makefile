#	$Id: Makefile,v 1.15 1997/03/09 09:41:44 bde Exp $

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
YEARISTYPE=	${.OBJDIR}/yearistype
.else
YEARISTYPE=	${.CURDIR}/yearistype
.endif

all: yearistype

yearistype: yearistype.sh
	cp ${.ALLSRC} ${.TARGET}
	chmod +x ${.TARGET}

beforeinstall:
	umask 022; cd ${.CURDIR}; \
	zic -d ${DESTDIR}/usr/share/zoneinfo -p ${POSIXRULES} \
	    ${LEAPFILE} -y ${YEARISTYPE} ${TZFILES}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/zone.tab ${DESTDIR}/usr/share/zoneinfo/

afterinstall:
	chown -R ${BINOWN}:${BINGRP} ${DESTDIR}/usr/share/zoneinfo/*

.include <bsd.prog.mk>
