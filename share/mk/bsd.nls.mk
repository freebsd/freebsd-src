# $FreeBSD$
#
# This include file <bsd.nls.mk> handles building and installing Native
# Language Support (NLS) catalogs
#
# +++ variables +++
#
# GENCAT	A program for converting .msg files into compiled NLS
#		.cat files. [gencat]
#
# NLS		Source or intermediate .msg files. [set in Makefile]
#
# NLSDIR	Base path for National Language Support files
#		installation. [${SHAREDIR}/nls]
#
# NLSGRP	National Language Support files group. [${SHAREGRP}]
#
# NLSMODE	National Language Support files mode. [${NOBINMODE}]
#
# NLSOWN	National Language Support files owner. [${SHAREOWN}]
#
# NO_NLS	Do not make or install NLS files. [not set]

.if !target(__<bsd.init.mk>__)
.error bsd.nls.mk cannot be included directly.
.endif

GENCAT?=	gencat

.SUFFIXES: .cat .msg

.msg.cat:
	${GENCAT} ${.TARGET} ${.IMPSRC}

.if defined(NLS) && !empty(NLS) && !defined(NO_NLS)

#
# .msg file pre-build rules
#
NLSSRCDIR?=	${.CURDIR}
.for file in ${NLS}
.if defined(NLSSRCFILES)
NLSSRCFILES_${file}?= ${NLSSRCFILES}
.endif
.if defined(NLSSRCFILES_${file})
NLSSRCDIR_${file}?= ${NLSSRCDIR}
${file}.msg: ${NLSSRCFILES_${file}:S/^/${NLSSRCDIR_${file}}\//}
	@rm -f ${.TARGET}
	cat ${.ALLSRC} > ${.TARGET}
CLEANFILES+= ${file}.msg
.endif
.endfor

#
# .cat file build rules
#
NLS:=		${NLS:=.cat}
CLEANFILES+=	${NLS}
FILESGROUPS?=	FILES
FILESGROUPS+=	NLS
NLSDIR?=	${SHAREDIR}/nls

#
# installation rules
#
.for file in ${NLS}
NLSNAME_${file:T}= ${file:T:R}/${NLSNAME}.cat
.if defined(NLSLINKS_${file:R}) && !empty(NLSLINKS_${file:R})
NLSLINKS+=	${file:R}
.endif
.for dst in ${NLSLINKS_${file:R}}
SYMLINKS+=	../${file:R}/${NLSNAME}.cat ${NLSDIR}/${dst}/${NLSNAME}.cat
.endfor
.endfor

.endif # defined(NLS) && !empty(NLS) && !defined(NO_NLS)
