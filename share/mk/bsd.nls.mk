#	Based on $NetBSD: bsd.nls.mk,v 1.35 2001/11/28 20:19:08 tv Exp $
# $FreeBSD$
#
# This include file <bsd.nls.mk> handles building and installing Native
# Language Support (NLS) catalogs
#
# +++ variables +++
#
# GENCAT	A program for converting .msg files into compiled NLS
#		.cat files. [gencat -new]
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
#
# +++ targets +++
#
#	install:
#		Install compiled NLS files
#
# bsd.obj.mk: cleandir and obj

GENCAT?=	gencat -new

NLSDIR?=        ${SHAREDIR}/nls
NLSGRP?=        ${SHAREGRP}
NLSMODE?=       ${NOBINMODE}
NLSOWN?=        ${SHAREOWN}

NLS?=
NLSLINKS=

.MAIN:		all

.SUFFIXES: .cat .msg

.msg.cat:
	${GENCAT} ${.TARGET} ${.IMPSRC}

#
# .msg file pre-build rules
#
.for file in ${NLS}
.if !defined(NLSSRCDIR_${file}) && defined(NLSSRCDIR)
NLSSRCDIR_${file}=${NLSSRCDIR}
.endif
.if !defined(NLSSRCFILES_${file}) && defined(NLSSRCFILES)
NLSSRCFILES_${file}=${NLSSRCFILES}
.endif

.if defined(NLSSRCFILES_${file})
${file}:
	@rm -f ${.TARGET}
	cat ${NLSSRCDIR_${file}}/${NLSSRCFILES_${file}} > ${.TARGET}
CLEANFILES+= ${file}
.endif

.if defined(NLSLINKS_${file:C/.msg//g}) && !empty(NLSLINKS_${file:C/.msg//g})
NLSLINKS+= ${file:C/.msg//g}
.endif
.endfor

#
# .cat file build rules
#
NLSALL=		${NLS:.msg=.cat}
CLEANFILES+=	${NLSALL}

#
# installation rules
#
__nlsinstall: .USE
	${INSTALL} -o ${NLSOWN} -g ${NLSGRP} -m ${NLSMODE} \
		${.ALLSRC} ${.TARGET}

.for F in ${NLSALL}
_F:=		${DESTDIR}${NLSDIR}/${F:T:R}/${NLSNAME}.cat

${_F}:		${F} __nlsinstall			# install rule
nlsinstall::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.endfor

links-nls:
.if defined(NLSLINKS) && !empty(NLSLINKS)
.for src in ${NLSLINKS}
.for dst in ${NLSLINKS_${src}}
	ln -fs ../${src}/${NLSNAME}.cat \
		${DESTDIR}${NLSDIR}/${dst}/${NLSNAME}.cat
.endfor
.endfor
.endif

#

.if !defined(NO_NLS) && !empty(NLS)
all-nls: ${NLSALL}
.else
all-nls:
.endif

.if !defined(NO_NLS) && !empty(NLS)
realinstall:	beforeinstall nlsinstall links-nls
.else
realinstall:	beforeinstall
.endif

all:		all-nls _SUBDIR
install:	realinstall afterinstall

.if !target(all-man)
all-man: _SUBDIR
.endif

.if !target(maninstall)
maninstall: _SUBDIR
.endif

.if !target(distribute)
distribute:
.endif

.if !target(beforeinstall)
beforeinstall:
.endif

.if !target(afterinstall)
afterinstall:
.endif

.include <bsd.obj.mk>
