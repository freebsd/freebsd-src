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
# $ svn commit	# Commit message: "Tag of tzdata2008X"
#
# Merge-from-vendor
#
# $ cd ~/svn/head/contrib/tzdata
# $ svn update
# $ svn merge -c X --accept=postpone \
#	svn+ssh://svn.freebsd.org/base/vendor/tzdata/dist .
# $ svn commit	# Commit message: "MFV of tzdata2008X"
#

CLEANFILES+=	yearistype
CLEANDIRS+=	builddir
CONTRIBDIR=	${.CURDIR}/../../contrib/tzdata/
.PATH:		${CONTRIBDIR}

.if defined(LEAPSECONDS)
LEAPFILE=	-L ${CONTRIBDIR}leapseconds
.else
LEAPFILE=
.endif

TZFILES=	africa antarctica asia australasia etcetera europe \
		factory northamerica southamerica
POSIXRULES=	America/New_York

.if defined(OLDTIMEZONES)
TZFILES+=	backward systemv
.endif

TZFILES:=	${TZFILES:S/^/${CONTRIBDIR}/}

TZBUILDDIR=	${.OBJDIR}/builddir
TZBUILDSUBDIRS=	\
		Africa \
		America/Argentina \
		America/Indiana \
		America/Kentucky \
		America/North_Dakota \
		Antarctica \
		Arctic \
		Asia \
		Atlantic \
		Australia \
		Etc \
		Europe \
		Indian \
		Pacific \
		SystemV

all: zoneinfo

.PHONY: zoneinfo
zoneinfo: yearistype ${TDATA}
	mkdir -p ${TZBUILDDIR}
	cd ${TZBUILDDIR}; mkdir -p ${TZBUILDSUBDIRS}
	umask 022; cd ${.CURDIR}; \
	zic -D -d ${TZBUILDDIR} -p ${POSIXRULES} -m ${NOBINMODE} \
	    ${LEAPFILE} -y ${.OBJDIR}/yearistype ${TZFILES}

beforeinstall:
	cd ${TZBUILDDIR} && \
	    find * -type f -print -exec ${INSTALL} \
	    -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    \{} ${DESTDIR}/usr/share/zoneinfo/\{} \;
	${INSTALL} -o ${BINOWN} -g ${BINGRP} -m ${NOBINMODE} \
	    ${CONTRIBDIR}/zone.tab ${DESTDIR}/usr/share/zoneinfo/

afterinstall:
#
# If the file /var/db/zoneinfo exists, and it is owned by root:wheel,
# and the contents of it exists in /usr/share/zoneinfo, then reinstall
# it.
#
	@if [ -f ${DESTDIR}/var/db/zoneinfo -a -O ${DESTDIR}/var/db/zoneinfo \
	    -a -G ${DESTDIR}/var/db/zoneinfo ]; then \
		zf=$$(cat ${DESTDIR}/var/db/zoneinfo); \
		if [ -f ${DESTDIR}/usr/share/zoneinfo/$${zf} ]; then \
			if [ ! -z "${DESTDIR}" ]; then \
				optC="-C ${DESTDIR}"; \
			fi; \
			echo "Updating /etc/localtime"; \
			tzsetup $${optC} -r; \
		fi; \
	else \
		echo "Run tzsetup(8) manually to update /etc/localtime."; \
	fi

.include <bsd.prog.mk>
