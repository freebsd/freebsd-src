# Generate SBOM files from definitions in the pkg-config format
#
# +++ variables +++
#
# BOMTOOL	Tool converting pkg-config files into SPDX version 2 files
# JSONLDDIR	Destination directory for the SPDX version 3 files
# SPDXDIR	Destination directory for the SPDX version 2 files
# SPDXTOOL	Tool converting pkg-config files into SPDX version 3 files

.if	${MK_SBOM} != "no"

BOMTOOL?=	bomtool
JSONLDDIR?=	/usr/share/sbom/jsonld
SPDXDIR?=	/usr/share/sbom/spdx
SPDXTOOL?=	spdxtool

.if defined(LIB)
PCFILE?=	${LIB}.pc
.endif

.if defined(PROG)
PCFILE?=	${PROG}.pc
.endif

.if exists(${.CURDIR}/${PCFILE})

${PCFILE:R}.jsonld: ${.CURDIR}/${PCFILE}
	${SPDXTOOL} ${.CURDIR}/${PCFILE} > ${.TARGET}
	${INSTALL} -m 0644 ${.TARGET} ${DESTDIR}${JSONLDDIR}/${.TARGET}

${PCFILE:R}.spdx: ${.CURDIR}/${PCFILE}
	${BOMTOOL} ${.CURDIR}/${PCFILE} > ${.TARGET}
	${INSTALL} -m 0644 ${.TARGET} ${DESTDIR}${SPDXDIR}/${.TARGET}

.if !defined(NO_JSONLD_SBOM)
all: ${PCFILE:R}.jsonld
.endif

.if !defined(NO_SPDX_SBOM)
all: ${PCFILE:R}.spdx
.endif

.endif	# exists(${.CURDIR}/${PCFILE})

.endif	# ${MK_SBOM}
