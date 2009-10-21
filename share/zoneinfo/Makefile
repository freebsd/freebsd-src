# $FreeBSD$

#
# HOW TO UPDATE THE ZONEINFO DATA
#
# With the use of subversion, this is a little bit simpler than the CVS method.
#
# Import the new sources to the vendor branch:
#
# $ cd ~/svn/vendor/tzdata/dist
# $ tar zxvf /usr/ports/distfile/tzdata2008X.tar.gz
#   (check with "svn stat" and "svn diff" if it all makes sense)
# $ svn commit	# Commit message: "Vendor import of tzdata2008X (+details)" 
#
# Tag it
#
# $ cd ~/svn/vendor/tzdata
# $ svn cp svn+ssh://svn.freebsd.org/base/vendor/tzdata/dist \
#	svn+ssh://svn.freebsd.org/base/vendor/tzdata/tzdata2008X
# $ svn update	# Commit message: "Tag of tzdata2008X"
#
# Merge-from-vendor
#
# $ cd ~/svn/head/share/zoneinfo
# $ svn update
# $ svn merge -c X --accept=postpone \
#	svn+ssh://svn.freebsd.org/base/vendor/tzdata/dist .
# $ svn update	# Commit message: "MFV of tzdata2008X"
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

all: yearistype

beforeinstall:
	umask 022; cd ${.CURDIR}; \
	zic -D -d ${DESTDIR}/usr/share/zoneinfo -p ${POSIXRULES} \
	    -u ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${LEAPFILE} -y ${.OBJDIR}/yearistype ${TZFILES}
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${.CURDIR}/zone.tab ${DESTDIR}/usr/share/zoneinfo/

afterinstall:
	@#
	@# If the file /var/db/zoneinfo exists, and it is owned by root:wheel,
	@# and the contents of it exists in /usr/share/zoneinfo, then reinstall
	@# it.
	@#
	@if [ -f ${DESTDIR}/var/db/zoneinfo -a -O ${DESTDIR}/var/db/zoneinfo \
	    -a -G ${DESTDIR}/var/db/zoneinfo ]; then \
		zf=$$(cat ${DESTDIR}/var/db/zoneinfo); \
		if [ -f ${DESTDIR}/usr/share/zoneinfo/$${zf} ]; then \
			if [ ! -z "${DESTDIR}" ]; then \
				optC="-C ${DESTDIR}"; \
			fi; \
			echo "Updating /etc/localtime"; \
			tzsetup ${optC} -r; \
		fi; \
	else \
		echo "Run tzsetup(8) manually to update /etc/localtime."; \
	fi

.include <bsd.prog.mk>
