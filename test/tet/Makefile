#
# Build TET from source.
#
# $Id: Makefile 3024 2014-04-18 16:20:11Z jkoshy $
#

TOP=	../..

.include "${TOP}/mk/elftoolchain.tetvars.mk"

TET_BUILD_MARKER=	.tet-build-done
TET_PATCH_MARKER=	.tet-patch-done

.MAIN:	all

.PHONY:	all clean clobber depend test

.if exists(${TET_ROOT}/configure)

#
# The TET source tree was present.
#

all: ${TET_BUILD_MARKER}

${TET_BUILD_MARKER}: ${TET_PATCH_MARKER}
	cd ${TET_ROOT} && sh ./configure -t lite
	cd ${TET_ROOT}/src && ${MAKE} all install
	touch ${TET_BUILD_MARKER}

${TET_PATCH_MARKER}:
	for f in patches/*.patch; do	\
		patch -p0 < $${f};	\
	done
	touch ${TET_PATCH_MARKER}

clean:
	cd ${TET_ROOT}/src && ${MAKE} clean
	rm -f ${TET_BUILD_MARKER}

clobber:
	rm -rf ${TET_ROOT} ${TET_BUILD_MARKER} ${TET_PATCH_MARKER}

cleandepend depend test:	.SILENT
	true

.else

#
# The TET source tree was not detected.
#

all depend test:	.SILENT
	echo "ERROR: The distribution of TET v${TET_VERSION} is missing."
	echo
	echo "Please download the distribution from:"
	echo "	${TET_DOWNLOAD_URL}"
	echo "and unpack it into directory \"${TET_ROOT}\"."
	echo
	false

clean clobber:
	rm -f ${TET_BUILD_MARKER} ${TET_PATCH_MARKER}
.endif
