# bsd.subdir_ports.mk	Thu Feb 17 17:26:04 MEZ 1994	by Julian Stacey <stacey@guug.de>
#	Copyright Julian Stacey, Munich Dec. 93, Free Software, No Liability.
#	For details see `Legalities' in /sys/Makefile.

# This make include file was inspired by bsd.subdir.mk.
# Some differences:
#	- Additional to SUBDIR, has been added G_SUBDIR,
#	  G_SUBDIR directories re made with gmake rather than make.
# 	- This make include file is tolerant of missing directories.
#	  ( many people will only have partially populated ports trees )
#	  This behaviour is very different from from bsd.subdir.mk,
#	  that assumes all directories are present (even games).
#	  (When bsd.subdir.mk fails to find a directory the cd fails,
#	  the make saturates the cpu, recursive makes end in a series
#	  of `cannot fork`s, also ruining performance for other users.

# This commentary supplements ./bsd.README
#	Notes by Julian Stacey <stacey@guug.de> regarding :
#		bsd.prog.mk		dependency	_PROGSUBDIR:	&
#		bsd.subdir.mk		dependency	_SUBDIRUSE:	.
#		bsd.subdir_ports.mk	dependency	_SUBDIRUSE:	.
#
#	Name	Description				Example
#	DIRPRFX	source tree prefix		bin/	(when in bin/cat)
#	MACHINE 					i386
#	.TARGET					all
#	.CURDIR current source directory
#	(though nate had expected it be what obj points to)
#	ref 940126_2031_39.nate
#	entry	dependency currently being made		all
#	edir    evaluated next directory,		libcsu.i386
#
#	The {} in ---{> & <}--- are there so the % key in vi can be used,
#	thus allowing easy matching within long make logs.
#	The >< in ---{> & <}--- are retained for ornament.
#	The --- in ---{> & <}--- are retained both for ornament, &
#	to contribute to the uniqueness of forward & backward search keys.
#	The brackets () allow subsequent subsidiary makes to ignore possible
#	error results from previous sibling makes.
#	A "cd ${.CURDIR} ;" after the subsidiary make is not needed, as nothing
#	else happens inside the sub shell invoked by the ( ),
#	it has thus been omitted to assist shell interpreter speed.

MFLAGS	+= -i	# to be removed when ports tree error free
#	used to be MFLAGS ?= -i but something was always defining
#	MFLAGS, so -i was never seen.
GMAKE		= gmake
MAKE		?= make
MAKE_CMD	= ${MAKE} ${MFLAGS}
GMAKE_CMD	= ${GMAKE} ${MFLAGS}
AVOID		?= "AVOID set in bsd.subdir_ports.mk avoids broken packages."

# Define WAIT_OK if you are prepared for the make to hang
# on an open window awaiting a response.

# You cant win with DESTDIR, once all packages use DESTDIR, no problem,
# till then its a 4 way choice between throwing all stuff in
#	- ${DESTDIR}/usr/local,
#	- ${DESTDIR}/usr/gnu,
#	- ${DESTDIR}/
#	- wherever the package Makefile thinks it wants to put it.
.if defined(DESTDIR)
DESTDIR := ${DESTDIR}/usr/local
.else
DESTDIR	= /usr/local
.endif

BINDIR?= ${DESTDIR}/bin
# maybe BINDIR & DESTDIR interfere on some packages
LIBDIR?= ${DESTDIR}/lib
# maybe useful to have a default LIBDIR ?

# Labels _SUBDIRUSE & ${SUBDIR} supercede labels in bsd.subdir.mk,

DIR_EXIST = -e
# DIR_EXIST	-e rather than -d, as it may be a sym link to a directory
FILE_EXIST = -r
# FILE_EXIST	not -f as it might be a sym link to a local
#		work_in_progress_Makefile

_SUBDIRUSE: .USE
	@# echo Starting directories that use BSD ${MAKE} 
	@for entry in ${SUBDIR} ; do \
		( \
		if test ${DIR_EXIST} ${.CURDIR}/$${entry}.${MACHINE} ; then \
			echo "Starting machine specific" > /dev/null ; \
			edir=$${entry}.${MACHINE} ; \
		else \
			echo "Starting generic" > /dev/null ; \
			edir=$${entry} ; \
		fi ;\
		echo -n "---{> $${edir}:	" ;\
		if test ${FILE_EXIST} ${.CURDIR}/$${edir}/Makefile ; then \
			cmd="${MAKE_CMD} \
				${.TARGET:realinstall=install} \
				 DIRPRFX=${DIRPRFX}$${edir}/"; \
			echo "$$cmd"; \
			cd ${.CURDIR}/$${edir} ; \
			$$cmd ; \
		else \
			echo "Warning missing directory or Makefile." ;\
		fi ;\
		echo "<}--- $${edir}" ;\
		) \
	done
	@# echo Finished BSD ${MAKE} directories, Starting ${GMAKE} directories.
	@for entry in ${G_SUBDIR}; do \
		( \
		if test ${DIR_EXIST} ${.CURDIR}/$${entry}.${MACHINE} ; then \
			echo "Starting machine specific" > /dev/null ; \
			edir=$${entry}.${MACHINE} ; \
		else \
			echo "Starting generic" > /dev/null ; \
			edir=$${entry} ; \
		fi ;\
		echo -n "---{> $${edir}:	" ;\
		if test ${FILE_EXIST} ${.CURDIR}/$${edir}/Makefile ; then \
			cmd="${GMAKE_CMD} MAKE=${GMAKE} \
				${.TARGET:realinstall=install} \
				DIRPRFX=${DIRPRFX}$${edir}/"; \
			echo "$$cmd"; \
			cd ${.CURDIR}/$${edir} ; \
			$$cmd ; \
		else \
			echo "Warning missing directory or Makefile." ;\
		fi ;\
		echo "<}--- $${edir}" ;\
		) \
	done
	@# echo Finished ${GMAKE} directories.

${SUBDIR}::
	@# echo Starting directories that use BSD ${MAKE} 
	if test ${DIR_EXIST} ${.CURDIR}/${.TARGET}.${MACHINE} ; then \
		echo "Starting machine specific" > /dev/null ; \
		edir=${.CURDIR}/${.TARGET}.${MACHINE};\
	else \
		edir=${.CURDIR}/${.TARGET};\
		echo "Starting generic" > /dev/null ; \
	fi ;\
	echo -n "---{> $${edir}:	" ;\
	if test ${FILE_EXIST} ${.CURDIR}/$${edir}/Makefile ; then \
		cmd="${MAKE_CMD} -f $${edir}/Makefile all";\
		echo "$$cmd"; \
		cd ${.CURDIR}/$${edir} ; \
		$$cmd ; \
	else \
		echo "Warning missing directory or Makefile." ;\
	fi ;\
	echo "<}--- $${edir}" ;\

${G_SUBDIR}::
	@# echo Starting directories that use BSD ${MAKE} 
	if test ${DIR_EXIST} ${.CURDIR}/${.TARGET}.${MACHINE} ; then \
		echo "Starting machine specific" > /dev/null ; \
		edir=${.CURDIR}/${.TARGET}.${MACHINE};\
	else \
		edir=${.CURDIR}/${.TARGET};\
		echo "Starting generic" > /dev/null ; \
	fi ;\
	echo -n "---{> $${edir}:	" ;\
	if test ${FILE_EXIST} ${.CURDIR}/$${edir}/Makefile ; then \
		cmd="${GMAKE_CMD} MAKE=${GMAKE} -f $${edir}/Makefile all";\
		echo "$$cmd"; \
		cd ${.CURDIR}/$${edir} ; \
		$$cmd ; \
	else \
		echo "Warning missing directory or Makefile." ;\
	fi ;\
	echo "<}--- $${edir}" ;\

.include <bsd.subdir.mk>

.if !target(pkg)
pkg: _SUBDIRUSE
.endif

.if !target(world)
world: _SUBDIRUSE
# note if we append `all' after `_SUBDIRUSE'
# then with several levels of SUBDIR makes, we'd get far too many `make all's
.endif

# End Of File
