# Generate SBOM files from definitions in the pkg-config format
#
# +++ variables +++
#
# BOMTOOL	Tool converting pkg-config files into SPDX version 2 files
# JSONLDDIR	Destination directory for the SPDX version 3 files
# SBOMDIR	Source directory for the pkg-config files
# SPDXDIR	Destination directory for the SPDX version 2 files
# SPDXTOOL	Tool converting pkg-config files into SPDX version 3 files

.if	${MK_SBOM} != "no"

BOMTOOL?=	${BTOOLSPATH:U.}/bomtool
JSONLDDIR?=	/usr/share/sbom/jsonld
SBOMDIR?=	${SRCTOP}/release/sbom/pkgconfig
SPDXDIR?=	/usr/share/sbom/spdx
SPDXTOOL?=	${BTOOLSPATH:U.}/spdxtool

.if defined(LIB)
PCFILE?=	lib${LIB}.pc
.endif

.if defined(PROG)
PCFILE?=	${PROG}.pc
.endif

.if exists(${SBOMDIR}/${PCFILE})

${PCFILE:R}.jsonld: ${SBOMDIR}/${PCFILE}
	${SPDXTOOL} ${SBOMDIR}/${PCFILE} > ${.TARGET}
	${INSTALL} -m 0644 ${.TARGET} ${DESTDIR}${JSONLDDIR}/${.TARGET}

${PCFILE:R}.spdx: ${SBOMDIR}/${PCFILE}
	${BOMTOOL} ${SBOMDIR}/${PCFILE} > ${.TARGET}
	${INSTALL} -m 0644 ${.TARGET} ${DESTDIR}${SPDXDIR}/${.TARGET}

.if !defined(NO_JSONLD_SBOM)
all: ${PCFILE:R}.jsonld
.endif

.if !defined(NO_SPDX_SBOM)
all: ${PCFILE:R}.spdx
.endif

.endif	# exists(${SBOMDIR}/${PCFILE})

.endif	# ${MK_SBOM}
