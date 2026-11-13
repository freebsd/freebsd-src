# Generate SBOM files from definitions in the pkg-config format
#
# +++ variables +++
#
# BOMTOOL	Tool converting pkg-config files into SPDX version 2 files
# JSONLDDIR	Destination directory for the SPDX version 3 files
# SBOMDATA	Location for the SBOM meta-data
# SBOMDIR	Destination directory for the SBOM files
# SBOMBASE	Prefix for the SBOM files belonging to the base system
# SPDXDIR	Destination directory for the SPDX version 2 files
# SPDXTOOL	Tool converting pkg-config files into SPDX version 3 files

.if	${MK_SBOM} != "no"

BOMTOOL?=	bomtool
JSONLDDIR?=	${SBOMDIR}/spdx-3.0.1
SBOMBASE?=	FreeBSD-
SBOMDATA?=	Makefile.sbom
SBOMDIR?=	/usr/share/sbom
SPDXDIR?=	${SBOMDIR}/spdx-2.2
SPDXTOOL?=	spdxtool
TOOLARGS=

# meta-data
. if exists(${.CURDIR}/${SBOMDATA})

FREEBSD_COPYRIGHT!=grep ^Copyright ${SRCTOP}/COPYRIGHT | head -1
SBOM_COPYRIGHT?=${FREEBSD_COPYRIGHT}
SBOM_DESC?=	${SBOM_NAME}
SBOM_LICENSE?=	BSD-2-Clause
SBOM_MAINTAINER?=
SBOM_VERSION?=	${_REVISION}
SBOM_URL?=	https://www.FreeBSD.org
SBOM_SOURCE?=	https://cgit.FreeBSD.org/src/tree/${RELDIR}

.  if defined(LIB)
SBOM_NAME?=	${SBOMBASE}lib${LIB}
SBOM_KIND?=
SBOMTAGS=	package=${PACKAGE:Uutilities},lib
.  elif defined(SHLIB)
SBOM_NAME?=	${SBOMBASE}lib${SHLIB}
SBOM_KIND?=
SBOMTAGS=	package=${PACKAGE:Uutilities},lib
.  elif defined(PROG)
SBOM_NAME?=	${SBOMBASE}${PROG}
SBOM_KIND?=	tool
SBOMTAGS=	package=${PACKAGE:Uutilities}
.  elif defined(SCRIPTS) && defined(PACKAGE)
SBOM_NAME?=	${SBOMBASE}${PACKAGE}
SBOM_KIND?=	tool
SBOMTAGS=	package=${PACKAGE}
.  else
SBOM_NAME?=
SBOM_KIND?=
SBOMTAGS=
.  endif

.  include "${.CURDIR}/${SBOMDATA}"

.  if !empty(SBOM_NAME) && !empty(SBOM_DESC) && !empty(SBOM_VERSION)
SBOMFILE?=	${OBJTOP}/share/sbom/${SBOM_NAME}.pc
.  endif # !empty(SBOM_NAME)
SBOM_TAG_ARGS=	-T ${SBOMTAGS:ts,:[*]}

.  if !empty(SBOMFILE)

_SBOMREQUIRES=
.   for l in ${LIBADD}
.    if ${_INTERNALLIBS:M${l}} != ${l}
_SBOMREQUIRES+=	${SBOMBASE}lib${l}
.    endif #${_INTERNALLIBS:M${l}} != ${l}
.   endfor
SBOM_REQUIRES?=	${_SBOMREQUIRES:S/ /,/gW}
SBOM_PROVIDES?=

${SBOMFILE}: ${.CURDIR}/${SBOMDATA}
	mkdir -p ${SBOMFILE:H}
	(echo "# ${FREEBSD_COPYRIGHT}"; echo "#";			\
	echo "# BSD-2-Clause"; echo "#";				\
	echo "Copyright: ${SBOM_COPYRIGHT}";				\
	echo "Name: ${SBOM_NAME}";					\
	echo "Description: ${SBOM_DESC}";				\
	echo "Version: ${SBOM_VERSION}";				\
	echo "License: ${SBOM_LICENSE}";				\
	[ -z "${SBOM_MAINTAINER}" ] || echo "Maintainer: ${SBOM_MAINTAINER}";	\
	[ -z "${SBOM_SOURCE}" ] || echo "Source: ${SBOM_SOURCE}";	\
	[ -z "${SBOM_URL}" ] || echo "URL: ${SBOM_URL}";		\
	[ -z "${SBOM_KIND}" ] || echo "Kind: ${SBOM_KIND}";		\
	[ -z "${SBOM_REQUIRES}" ] || echo "Requires: ${SBOM_REQUIRES}"; \
	[ -z "${SBOM_PROVIDES}" ] || echo "Provides: ${SBOM_PROVIDES}") \
	> ${.TARGET}
# Libraries installing symlinks to themselves also need a .pc file
.   for s t in ${SYMLINKS}
.    if defined(LIB) && ${s} == lib${LIB}.so && ${SBOMBASE}${t:T:C/\.so$/.pc/} != ${SBOMFILE:T}
	ln -sf ${SBOMFILE:T} ${SBOMFILE:H}/${SBOMBASE}${t:T:C/\.so$/.pc/}
.    endif
.   endfor

.   if !defined(NO_JSONLD_SBOM)
JSONLDFILE?= ${SBOMFILE:R}.jsonld

all: ${JSONLDFILE}

${JSONLDFILE}: ${SBOMFILE}
	${SPDXTOOL} ${TOOLARGS} ${SBOMFILE} > ${.TARGET}

afterinstall: jsonldinstall
.ORDER: realinstall jsonldinstall
jsonldinstall: .PHONY ${JSONLDFILE}
	${INSTALL} ${SBOM_TAG_ARGS} -m 0644 ${JSONLDFILE} \
		${DESTDIR}${JSONLDDIR}/${JSONLDFILE:T}
.   endif # !defined(NO_JSONLD_SBOM)

.   if !defined(NO_SPDX_SBOM)
SPDXFILE?= ${SBOMFILE:R}.spdx

all: ${SPDXFILE}

${SPDXFILE}: ${SBOMFILE}
	${BOMTOOL} ${TOOLARGS} ${SBOMFILE} > ${.TARGET}

afterinstall: spdxinstall
.ORDER: realinstall spdxinstall
spdxinstall: .PHONY ${SPDXFILE}
	${INSTALL} ${SBOM_TAG_ARGS} -m 0644 ${SPDXFILE} \
		${DESTDIR}${SPDXDIR}/${SPDXFILE:T}
.   endif # !defined(NO_SPDX_SBOM)

.  endif # !empty(SBOM_NAME) && !empty(SBOM_DESC) && !empty(SBOM_VERSION)
. endif # exists(${.CURDIR}/${SBOMDATA})
.endif # ${MK_SBOM}
