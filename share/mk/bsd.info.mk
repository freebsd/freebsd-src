#	$Id: bsd.info.mk,v 1.43 1997/10/09 18:14:18 wosch Exp $
#
# The include file <bsd.info.mk> handles installing GNU (tech)info files.
# Texinfo is a documentation system that uses a single source
# file to produce both on-line information and printed output.
# <bsd.info.mk> includes the files <bsd.dep.mk> and <bsd.obj.mk>.
#
#
# +++ variables +++
#
# CLEANFILES	Additional files to remove for the clean and cleandir targets.
#
# DESTDIR	Change the tree where the info files gets installed. [not set]
#
# DISTRIBUTION	Name of distribution. [info]
#
# FORMATS 	Indicates which output formats will be generated
#               (info, dvi, latin1, ps).  [info]
#
# ICOMPRESS_CMD	Program to compress info files. Output is to
#		stdout. [${COMPRESS_CMD}]
#
# INFO		???
#
# INFODIR	Base path for GNU's hypertext system
#		called Info (see info(1)). [${SHAREDIR}/info]
#
# INFODIRFILE	Top level node/index for info files. [dir]
#
# INFOGRP	Info group. [${SHAREGRP}]
#
# INFOMODE	Info mode. [${NOBINMODE}]
#
# INFOOWN	Info owner. [${SHAREOWN}]
#
# INFOSECTION	??? [Miscellaneous]
#
# INFOTMPL	??? [${INFODIR}/dir-tmpl]
#
# INSTALLINFO	??? [install-info]
#
# INSTALLINFODIRS	???
#
# MAKEINFO	A program for converting GNU Texinfo files into Info
#		file. [makeinfo]
#
# MAKEINFOFLAGS		Options for ${MAKEINFO} command. [--no-split]
#
# NOINFO	Do not make or install info files. [not set]
#
# NOINFOCOMPRESS	If you do not want info files be
#			compressed when they are installed. [not set]
#
#
# +++ targets +++
#
#	depend:
#		Dummy target, do nothing.
#
#	distribute:
#		This is a variant of install, which will
#		put the stuff into the right "distribution".
#
#	install:
#		Install the info files.
#
#	maninstall:
#		Dummy target, do nothing.
#
#
# bsd.obj.mk: cleandir and obj

.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif

MAKEINFO?=	makeinfo
MAKEINFOFLAGS+=	--no-split # simplify some things, e.g., compression
SRCDIR?=	${.CURDIR}
INFODIRFILE?=   dir
INFOTMPL?=      ${INFODIR}/dir-tmpl
INSTALLINFO?=   install-info
INFOSECTION?=   Miscellaneous
ICOMPRESS_CMD?=	${COMPRESS_CMD}
ICOMPRESS_EXT?=	${COMPRESS_EXT}
FORMATS?=	info

.MAIN: all

.SUFFIXES: ${ICOMPRESS_EXT} .info .texi .texinfo .dvi .ps .latin1

# What to do if there's no dir file there.  This is really gross!!!
${DESTDIR}${INFODIR}/${INFODIRFILE}:
	@(cd /usr/src/share/info; make install)

.texi.info .texinfo.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${.IMPSRC} \
		-o ${.TARGET}

.texi.dvi .texinfo.dvi:
	env TEXINPUTS=${.CURDIR}:${SRCDIR}:$$TEXINPUTS \
		tex ${.IMPSRC} </dev/null
	env TEXINPUTS=${.CURDIR}:${SRCDIR}:$$TEXINPUTS \
		tex ${.IMPSRC} </dev/null

.texinfo.latin1 .texi.latin1:
	perl -npe 's/(^\s*\\input\s+texinfo\s+)/$$1\n@tex\n\\global\\hsize=120mm\n@end tex\n\n/' ${.IMPSRC} >> ${.IMPSRC:T:R}-la.texi
	env TEXINPUTS=${.CURDIR}:${SRCDIR}:$$TEXINPUTS \
		tex ${.IMPSRC:T:R}-la.texi </dev/null
	env TEXINPUTS=${.CURDIR}:${SRCDIR}:$$TEXINPUTS \
		tex ${.IMPSRC:T:R}-la.texi </dev/null
	dvips -o /dev/stdout ${.IMPSRC:T:R}-la.dvi | \
		dvips2ascii > ${.TARGET}.new
	mv -f ${.TARGET}.new ${.TARGET}

.dvi.ps:
	dvips -o ${.TARGET} ${.IMPSRC} 	

.PATH: ${.CURDIR} ${SRCDIR}

.for _f in ${FORMATS}
IFILENS+= ${INFO:S/$/.${_f}/g}
.endfor

.if !defined(NOINFO)
.if !defined(NOINFOCOMPRESS)
.for _f in ${FORMATS}
IFILES+=	${INFO:S/$/.${_f}${ICOMPRESS_EXT}/g}
.endfor
all: ${IFILES} _SUBDIR
.else
IFILES=	${IFILENS}
all: ${IFILES} _SUBDIR
.endif
.else
all:
.endif

.for _f in ${FORMATS}
.for x in ${INFO:S/$/.${_f}/g}
${x:S/$/${ICOMPRESS_EXT}/}:	${x}
	${ICOMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
.endfor
.endfor

.for x in ${INFO}
INSTALLINFODIRS+= ${x:S/$/-install/}
${x:S/$/-install/}: ${DESTDIR}${INFODIR}/${INFODIRFILE}
	${INSTALLINFO} --defsection=${INFOSECTION} \
		       --defentry=${INFOENTRY_${x}} \
		       ${x}.info ${DESTDIR}${INFODIR}/${INFODIRFILE}
.endfor

.PHONY: ${INSTALLINFODIRS}

# The default is "info" and it can never be "bin"
DISTRIBUTION?=	info
.if ${DISTRIBUTION} == "bin"
DISTRIBUTION=	info
.endif

.if !target(distribute)
distribute: _SUBDIR
.for dist in ${DISTRIBUTION}
	cd ${.CURDIR} ; $(MAKE) install DESTDIR=${DISTDIR}/${dist} SHARED=copies
.endfor
.endif

.if defined(SRCS)
CLEANFILES+=	${INFO}.texi
${INFO}.texi: ${SRCS}
	cat ${.ALLSRC} > ${.TARGET}
.endif

depend: _SUBDIR
	@echo -n

.for _f in ${FORMATS}
CLEANFILES+=${INFO:S/$/.${_f}*/g} ${INFO:S/$/-la.${_f}*/g}
.endfor
CLEANFILES+= ${INFO:S/$/-la.texi/g}

# tex garbage
.for _f in aux cp fn ky log out pg toc tp vr dvi
CLEANFILES+=	${INFO:S/$/.${_f}/g} ${INFO:S/$/-la.${_f}/g}
.endfor


.if !defined(NOINFO) && defined(INFO)
install: ${INSTALLINFODIRS} _SUBDIR
	${INSTALL} ${COPY} -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} \
		${IFILES} ${DESTDIR}${INFODIR}
.else
install:
.endif

.if !target(maninstall)
maninstall: _SUBDIR
.endif

.include <bsd.dep.mk>
.include <bsd.obj.mk>
