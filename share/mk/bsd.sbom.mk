# Generate SBOM files from definitions in the pkg-config format
#
# +++ variables +++
#
# BOMTOOL	Tool converting pkg-config files into SPDX version 2 files
# SBOMDIR	Source directory for the pkg-config files
# SPDXDIR	Destination directory for the SPDX version 2 files

.if	${MK_SBOM} != "no"

BOMTOOL?=	bomtool
SBOMDIR?=	${SRCTOP}/release/sbom/pkgconfig
SPDXDIR?=	/usr/share/sbom/spdx

.if defined(LIB)
PCFILE?=	lib${LIB}.pc
SBOM_TAG_ARGS=	${LIB_TAG_ARGS}
.endif

.if defined(PROG)
PCFILE?=	${PROG}.pc
SBOM_TAG_ARGS=	${TAG_ARGS}
.endif

.if !empty(PCFILE) && exists(${SBOMDIR}/${PCFILE})

${PCFILE:R}.spdx: ${SBOMDIR}/${PCFILE}
	${BOMTOOL} ${SBOMDIR}/${PCFILE} > ${.TARGET}

spdxinstall: .PHONY ${PCFILE:R}.spdx
	${INSTALL} ${SBOM_TAG_ARGS} -m 0644 ${PCFILE:R}.spdx \
		${DESTDIR}${SPDXDIR}/${PCFILE:R}.spdx

.if !defined(NO_SPDX_SBOM)
all: ${PCFILE:R}.spdx

realinstall: spdxinstall
.ORDER: beforeinstall spdxinstall
.endif

.endif	# exists(${SBOMDIR}/${PCFILE})

.endif	# ${MK_SBOM}
