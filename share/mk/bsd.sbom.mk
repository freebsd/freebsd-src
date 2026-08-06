# Generate SBOM files from definitions in the pkg-config format
#
# +++ variables +++
#
# BOMTOOL	Tool converting pkg-config files into SPDX version 2 files
# JSONLDDIR	Destination directory for the SPDX version 3 files
# SBOMDIR	Destination directory for the SBOM files
# SPDXDIR	Destination directory for the SPDX version 2 files
# SPDXTOOL	Tool converting pkg-config files into SPDX version 3 files

.if	${MK_SBOM} != "no"

BOMTOOL?=	bomtool
JSONLDDIR?=	${SBOMDIR}/spdx-3.0.1
SBOMDIR?=	/usr/share/sbom
SPDXDIR?=	${SBOMDIR}/spdx-2.2
SPDXTOOL?=	spdxtool
TOOLARGS=	--define-variable=FREEBSD_VERSION=${OS_REVISION}
_UCLFILE=	${SRCTOP}/packages/${PACKAGE:Uutilities}/${PACKAGE:Ucommon}.ucl

# meta-data
.if exists(_UCLFILE)
SBOM_COPYRIGHT!=sed -n '/^ \* Copyright /{ s/^ \* //; p; }' ${_UCLFILE}
SBOM_DESC!=	sed -n '/^comment *= *"/{ s/^comment *= *"\(.*\)"$$/\1/; p; }' ${_UCLFILE}
SBOM_LICENSE!=	sed -n '/^ \* SPDX-License-Identifier: */{ s/^ \* SPDX-License-Identifier: //; p; }' ${_UCLFILE}
.endif # exists(_UCLFILE)
SBOM_COPYRIGHT?=Copyright (c) 2026 The FreeBSD Project
SBOM_DESC?=	${SBOM_NAME}
SBOM_LICENSE?=	BSD-2-Clause
SBOM_VERSION?=	${FREEBSD_RELEASE}
SBOM_SOURCE?=	https://cgit.FreeBSD.org/src/tree
.if !empty(MAN)
SBOM_URL?=	https://man.FreeBSD.org/cgi/man.cgi?query=${MAN:S,\.\([0-9a-z]\)\+,(\1),}
.endif

. if defined(LIB)
SBOM_NAME?=	lib${LIB}
SBOM_KIND?=
SBOMTAGS=	package=${PACKAGE:Uutilities},lib
. elif defined(PROG)
SBOM_NAME?=	${PROG}
SBOM_KIND?=	tool
SBOMTAGS=	package=${PACKAGE:Uutilities}
. else
SBOM_NAME=
SBOM_KIND?=
SBOMTAGS=
. endif # defined(PROG) || defined(LIB)

. if !empty(SBOM_NAME)
SBOMFILE?=	${SBOM_NAME}.pc
. endif # !empty(SBOM_NAME)
SBOM_TAG_ARGS=	-T ${SBOMTAGS:ts,:[*]}

. if !empty(SBOMFILE)

_SBOMREQUIRES=
.  for l in ${LIBADD}
_SBOMREQUIRES+=	lib${l}
.  endfor
SBOMREQUIRES=	${_SBOMREQUIRES:S/ /,/gW}
TOOLARGS+=	--define-variable=SBOM_REQUIRES=${SBOMREQUIRES:Q}

sbom/${SBOMFILE}:
	mkdir -p ${.OBJDIR}/sbom
	(echo "# Copyright (c) 2026 The FreeBSD Project"; echo "#";	\
	echo "# ${SBOM_LICENSE}"; echo "#";				\
	echo "Copyright: ${SBOM_COPYRIGHT}";				\
	echo "Name: ${SBOM_NAME}";					\
	echo "Description: ${SBOM_DESC}";				\
	echo "Version: ${SBOM_VERSION}";				\
	echo "License: ${SBOM_LICENSE}";				\
	[ -z "${SBOM_SOURCE}" ] || echo "Source: ${SBOM_SOURCE}";	\
	[ -z "${SBOM_URL}" ] || echo "URL: ${SBOM_URL}";		\
	[ -z "${SBOM_KIND}" ] || echo "Kind: ${SBOM_KIND}";		\
	[ -z "${SBOM_REQUIRES}" ] || echo "Requires: ${SBOM_REQUIRES}") \
	> ${.TARGET}

.  if !defined(NO_JSONLD_SBOM)
JSONLDFILE?= ${SBOMFILE:R}.jsonld

all: sbom/${JSONLDFILE}

sbom/${JSONLDFILE}: sbom/${SBOMFILE}
	${SPDXTOOL} ${TOOLARGS} sbom/${SBOMFILE} > ${.TARGET}

jsonldinstall: .PHONY sbom/${SBOM_NAME}.jsonld
	${INSTALL} ${SBOM_TAG_ARGS} -m 0644 sbom/${SBOMFILE:R}.jsonld \
		${DESTDIR}${JSONLDDIR}/${SBOMFILE:R}.jsonld

realinstall: jsonldinstall
.ORDER: beforeinstall jsonldinstall
.  endif # !defined(NO_JSONLD_SBOM)

.  if !defined(NO_SPDX_SBOM)
SPDXFILE?= ${SBOMFILE:R}.spdx

all: sbom/${SPDXFILE}

sbom/${SPDXFILE}: sbom/${SBOMFILE}
	${BOMTOOL} ${TOOLARGS} sbom/${SBOMFILE} > ${.TARGET}

spdxinstall: .PHONY sbom/${SPDXFILE}
	${INSTALL} ${SBOM_TAG_ARGS} -m 0644 sbom/${SPDXFILE} \
		${DESTDIR}${SPDXDIR}/${SPDXFILE}

realinstall: spdxinstall
.ORDER: beforeinstall spdxinstall
.  endif # !defined(NO_SPDX_SBOM)

. endif	# !empty(SBOMFILE)

.endif	# ${MK_SBOM}
