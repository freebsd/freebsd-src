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

BOMTOOL?=	bomtool
JSONLDDIR?=	/usr/share/sbom/spdx-3.0.1
SBOMDIR?=	${SRCTOP}/share/sbom/pkgconfig
SPDXDIR?=	/usr/share/sbom/spdx-2.2
SPDXTOOL?=	spdxtool

. if defined(LIB)
SBOMNAME?=	lib${LIB}
SBOMTAGS=	package=${PACKAGE:Uutilities},lib
. elif defined(PROG)
SBOMNAME?=	${PROG}
SBOMTAGS=	package=${PACKAGE:Uutilities}
. else
SBOMNAME=
SBOMTAGS=
. endif # defined(PROG) || defined(LIB)

. if !empty(SBOMNAME)
SBOMFILE?=	${SBOMNAME}.pc
. endif # !empty(SBOMNAME)
SBOM_TAG_ARGS=	-T ${SBOMTAGS:ts,:[*]}

. if !empty(SBOMFILE) && exists(${SBOMDIR}/${SBOMFILE})

.  if !defined(NO_JSONLD_SBOM)
all: ${SBOMNAME}.jsonld

${SBOMNAME}.jsonld: ${SBOMDIR}/${SBOMFILE}
	${SPDXTOOL} ${SBOMDIR}/${SBOMFILE} > ${.TARGET}

jsonldinstall: .PHONY ${SBOMNAME}.jsonld
	${INSTALL} ${SBOM_TAG_ARGS} -m 0644 ${SBOMNAME}.jsonld \
		${DESTDIR}${JSONLDDIR}/${SBOMNAME}.jsonld

realinstall: jsonldinstall
.ORDER: beforeinstall jsonldinstall
.  endif # !defined(NO_JSONLD_SBOM)

.  if !defined(NO_SPDX_SBOM)
all: ${SBOMNAME}.spdx

${SBOMNAME}.spdx: ${SBOMDIR}/${SBOMFILE}
	${BOMTOOL} ${SBOMDIR}/${SBOMFILE} > ${.TARGET}

spdxinstall: .PHONY ${SBOMNAME}.spdx
	${INSTALL} ${SBOM_TAG_ARGS} -m 0644 ${SBOMNAME}.spdx \
		${DESTDIR}${SPDXDIR}/${SBOMFILE:R}.spdx

realinstall: spdxinstall
.ORDER: beforeinstall spdxinstall
.  endif # !defined(NO_SPDX_SBOM)

. endif	# exists(${SBOMDIR}/${SBOMFILE})

.endif	# ${MK_SBOM}
