#	$Id: bsd.info.mk,v 1.45 1997/10/12 18:54:34 wosch Exp $
#
# The include file <bsd.info.mk> handles installing GNU (tech)info files.
# Texinfo is a documentation system that uses a single source
# file to produce both on-line information and printed output.
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
# DVIPS		A program which convert a TeX DVI file to PostScript [dvips]
#
# DVIPS2ASCII	A program to convert a PostScript file which was prior
#		converted from a TeX DVI file to ascii/latin1 [dvips2ascii]
#
# FORMATS 	Indicates which output formats will be generated
#               (info, dvi, latin1, ps, html).  [info]
#
# ICOMPRESS_CMD	Program to compress info files. Output is to
#		stdout. [${COMPRESS_CMD}]
#
# INFO		texinfo files, without suffix.  [set in Makefile] 
#
# INFO2HTML	A program for converting GNU info files into HTML files
#		[info2html]
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
# TEX		A program for converting tex files into dvi files [tex]
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
INFO2HTML?=	info2html
TEX?=		tex
DVIPS?=		dvips
DVIPS2ASCII?=	dvips2ascii

.MAIN: all

.SUFFIXES: ${ICOMPRESS_EXT} .info .texi .texinfo .dvi .ps .latin1 .html

# What to do if there's no dir file there.  This is really gross!!!
${DESTDIR}${INFODIR}/${INFODIRFILE}:
	@(cd /usr/src/share/info; make install)

.texi.info .texinfo.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${.IMPSRC} \
		-o ${.TARGET}

.texi.dvi .texinfo.dvi:
	env TEXINPUTS=${.CURDIR}:${SRCDIR}:$$TEXINPUTS \
		${TEX} ${.IMPSRC} </dev/null
	env TEXINPUTS=${.CURDIR}:${SRCDIR}:$$TEXINPUTS \
		${TEX} ${.IMPSRC} </dev/null

.texinfo.latin1 .texi.latin1:
	perl -npe 's/(^\s*\\input\s+texinfo\s+)/$$1\n@tex\n\\global\\hsize=120mm\n@end tex\n\n/' ${.IMPSRC} >> ${.IMPSRC:T:R}-la.texi
	env TEXINPUTS=${.CURDIR}:${SRCDIR}:$$TEXINPUTS \
		${TEX} ${.IMPSRC:T:R}-la.texi </dev/null
	env TEXINPUTS=${.CURDIR}:${SRCDIR}:$$TEXINPUTS \
		${TEX} ${.IMPSRC:T:R}-la.texi </dev/null
	${DVIPS} -o /dev/stdout ${.IMPSRC:T:R}-la.dvi | \
		${DVIPS2ASCII} > ${.TARGET}.new
	mv -f ${.TARGET}.new ${.TARGET}

.dvi.ps:
	${DVIPS} -o ${.TARGET} ${.IMPSRC} 	

.info.html:
	${INFO2HTML} ${.IMPSRC}
	ln -f ${.TARGET:R}.info.Top.html ${.TARGET} 

.PATH: ${.CURDIR} ${SRCDIR}


.for _f in ${FORMATS}
IFILENS+= ${INFO:S/$/.${_f}/g}
CLEANFILES+=${INFO:S/$/.${_f}*/g}
.endfor

.if !defined(NOINFO)
.if !defined(NOINFOCOMPRESS)
IFILES=	${IFILENS:S/$/${ICOMPRESS_EXT}/g:S/.html${ICOMPRESS_EXT}/.html/g}
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


# tex garbage
.if ${FORMATS:Mps} || ${FORMATS:Mdvi} || ${FORMATS:Mlatin1}
.for _f in aux cp fn ky log out pg toc tp vr dvi
CLEANFILES+=	${INFO:S/$/.${_f}/g} ${INFO:S/$/-la.${_f}/g}
.endfor
CLEANFILES+= ${INFO:S/$/-la.texi/g}
.endif

.if ${FORMATS:Mhtml}
CLEANFILES+= ${INFO:S/$/.info.*.html/g} ${INFO:S/$/.info/g}
.endif


.if !defined(NOINFO) && defined(INFO)
install: ${INSTALLINFODIRS} _SUBDIR
.if ${IFILES:N*.html}
	${INSTALL} ${COPY} -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} \
		${IFILES:N*.html} ${DESTDIR}${INFODIR}
.endif
.if ${FORMATS:Mhtml}
	${INSTALL} ${COPY} -o ${INFOOWN} -g ${INFOGRP} -m ${INFOMODE} \
		${INFO:S/$/.info.*.html/g} ${DESTDIR}${INFODIR}
.endif
.else
install:
.endif

.if !target(maninstall)
maninstall: _SUBDIR
.endif

.include <bsd.dep.mk>
.include <bsd.obj.mk>
