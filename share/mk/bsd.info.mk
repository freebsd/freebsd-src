#	$Id: bsd.info.mk,v 1.36 1997/04/07 16:46:40 bde Exp $
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
#	clean:
#		remove *.info* Errs errs mklog ${CLEANFILES}
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

.MAIN: all

.SUFFIXES: ${ICOMPRESS_EXT} .info .texi .texinfo

.texi.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${.IMPSRC} \
		-o ${.TARGET}.new
	mv -f ${.TARGET}.new ${.TARGET}

.texinfo.info:
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} ${.IMPSRC} \
		-o ${.TARGET}.new
	mv -f ${.TARGET}.new ${.TARGET}

.PATH: ${.CURDIR} ${SRCDIR}

IFILENS= ${INFO:S/$/.info/g}

.if !defined(NOINFO)
.if !defined(NOINFOCOMPRESS)
IFILES=	${INFO:S/$/.info${ICOMPRESS_EXT}/g}
all: ${IFILES} _SUBDIR
.else
IFILES=	${IFILENS}
all: ${IFILES} _SUBDIR
.endif
.else
all:
.endif

.for x in ${INFO:S/$/.info/g}
${x:S/$/${ICOMPRESS_EXT}/}:	${x}
	${ICOMPRESS_CMD} ${.ALLSRC} > ${.TARGET}
.endfor

# What to do if there's no dir file there.  This is really gross!!!
${DESTDIR}${INFODIR}/${INFODIRFILE}:
	cd /usr/src/share/info; ${MAKE} install

# What to do if there's no dir file there.  This is really gross!!!
${DESTDIR}${INFODIR}/${INFODIRFILE}:
	@(cd /usr/src/share/info; make install)

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
	cd ${.CURDIR} ; \
		$(MAKE) install DESTDIR=${DISTDIR}/${DISTRIBUTION} SHARED=copies
.endif

.if defined(SRCS)
${INFO}.info: ${SRCS}
	${MAKEINFO} ${MAKEINFOFLAGS} -I ${.CURDIR} -I ${SRCDIR} \
		${SRCS:S/^/${SRCDIR}\//g} -o ${INFO}.info.new
	mv -f ${INFO}.info.new ${INFO}.info
.endif

depend: _SUBDIR
	@echo -n

clean: _SUBDIR
	rm -f ${INFO:S/$/.info*/g} Errs errs mklog ${CLEANFILES}

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
