#	from: @(#)bsd.subdir.mk	5.9 (Berkeley) 2/1/91
#	$Id: bsd.subdir.mk,v 1.11.2.3 1998/03/07 13:18:04 jkh Exp $
#
# The include file <bsd.subdir.mk> contains the default targets
# for building subdirectories. It has the same seven targets
# as <bsd.prog.mk>:
#	all, clean, cleandir, depend, install, lint, and tags.
#
# For all of the directories listed in the variable SUBDIRS, the
# specified directory will be visited and the target made. There is
# also a default target which allows the command "make subdir" where
# subdir is any directory listed in the variable SUBDIRS.
#
#
# +++ variables +++
#
# DISTRIBUTION	Name of distribution. [bin]
#
# SUBDIR	A list of subdirectories that should be built as well.
#		Each of the targets will execute the same target in the
#		subdirectories.
#
# +++ targets +++
#
#	distribute:
# 		This is a variant of install, which will
# 		put the stuff into the right "distribution".
#
#	afterdistribute, afterinstall, all, beforeinstall, checkdpadd,
#	clean, cleandepend, cleandir, depend, install, lint, maninstall,
#	obj, objlink, realinstall, tags
#


.MAIN: all

_SUBDIRUSE: .USE
	@for entry in ${SUBDIR}; do \
		(if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			${ECHODIR} "===> ${DIRPRFX}$${entry}.${MACHINE}"; \
			edir=$${entry}.${MACHINE}; \
			cd ${.CURDIR}/$${edir}; \
		else \
			${ECHODIR} "===> ${DIRPRFX}$$entry"; \
			edir=$${entry}; \
			cd ${.CURDIR}/$${edir}; \
		fi; \
		${MAKE} ${.TARGET:realinstall=install} DIRPRFX=${DIRPRFX}$$edir/); \
	done

${SUBDIR}::
	@if test -d ${.TARGET}.${MACHINE}; then \
		cd ${.CURDIR}/${.TARGET}.${MACHINE}; \
	else \
		cd ${.CURDIR}/${.TARGET}; \
	fi; \
	${MAKE} all


.for __target in all checkdpadd clean cleandir depend lint \
		 maninstall obj objlink
.if !target(${__target})
${__target}: _SUBDIRUSE
.endif
.endfor

.if !target(tags)
.if defined(TAGS)
tags:
	@cd ${.CURDIR} && gtags ${GTAGSFLAGS}
.if defined(HTML)
	@cd ${.CURDIR} && htags ${HTAGSFLAGS}
.endif
.else
tags:	_SUBDIRUSE
.endif
.endif

.if !target(cleandepend)
cleandepend:	_SUBDIRUSE
.if defined(TAGS)
	@rm -f ${.CURDIR}/GTAGS ${.CURDIR}/GRTAGS
.if defined(HTML)
	@rm -rf ${.CURDIR}/HTML
.endif
.endif
.endif

.if !target(install)
.if !target(beforeinstall)
beforeinstall:
.endif
.if !target(afterinstall)
afterinstall:
.endif
install: afterinstall
afterinstall: realinstall
realinstall: beforeinstall _SUBDIRUSE
.endif

DISTRIBUTION?=	bin
.if !target(afterdistribute)
afterdistribute:
.endif
.if !target(distribute)
distribute: _SUBDIRUSE
.for dist in ${DISTRIBUTION}
	cd ${.CURDIR} ; ${MAKE} afterdistribute DESTDIR=${DISTDIR}/${dist}
.endfor
.endif
