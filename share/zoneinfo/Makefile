# $FreeBSD$

CLEANFILES+=	yearistype

.if defined(LEAPSECONDS)
LEAPFILE=	-L leapseconds
.else
LEAPFILE=
.endif

TZFILES=	africa antarctica asia australasia etcetera europe \
		factory northamerica southamerica
POSIXRULES=	America/New_York

.if defined(OLDTIMEZONES)
TZFILES+=	backward systemv
.endif

all: yearistype

beforeinstall:
	umask 022; cd ${.CURDIR}; \
	zic -D -d ${DESTDIR}/usr/share/zoneinfo -p ${POSIXRULES} \
	    -u ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${LEAPFILE} -y ${.OBJDIR}/yearistype ${TZFILES}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/zone.tab ${DESTDIR}/usr/share/zoneinfo/

.include <bsd.prog.mk>
