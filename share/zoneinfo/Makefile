# $FreeBSD$

#
# HOW TO UPDATE THE ZONEINFO DATA
#
# In a clean directory, unpack the distribution archive and run:
#
# $ cvs -d :ext:ncvs.freebsd.org:/home/ncvs import -b 1.1.2 \
#	src/share/zoneinfo ADO tzdataYYYYt
#
# where YYYYt is the year and the version of the distribution, for
# example 2004h.
#
# Then go to the checked out source, and merge the changes from the
# previous tag to the new tag in the HEAD:
#
# $ cd ../src/share/zoneinfo
# $ cvs update -jtzdataYYYYp -j tzdataYYYYt
#
# Update it again:
# 
# $ cvs update
#
# And push it back into CVS:
#
# $ cvs ci
#

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
	zic -D -d ${DESTDIR}/usr/share/zoneinfo -p ${POSIXRULES} \
	    -u ${BINOWN} -g ${BINGRP} \
	    ${LEAPFILE} -y ${YEARISTYPE} ${TZFILES}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/zone.tab ${DESTDIR}/usr/share/zoneinfo/

.include <bsd.prog.mk>
