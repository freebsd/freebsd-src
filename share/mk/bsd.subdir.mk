#	from: @(#)bsd.subdir.mk	5.9 (Berkeley) 2/1/91
# $FreeBSD$
#
# The include file <bsd.subdir.mk> contains the default targets
# for building subdirectories. 
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
# SUBDIR_CHANGE A directory-tree that contains overrides for
#               corresponding build subdirs.
#		Each override is a file containing one subdirname per line:
#                  'subdirlist'  is a pure override
#		   'subdirdrop'  drops directories from the build
#		   'subdiradd'   adds directories to the build
#
# +++ targets +++
#
#	distribute:
# 		This is a variant of install, which will
# 		put the stuff into the right "distribution".
#
#	afterdistribute, afterinstall, all, all-man, beforeinstall, checkdpadd,
#	clean, cleandepend, cleandir, depend, install, lint, maninstall,
#	obj, objlink, realinstall, regress, tags
#

.if !target(__initialized__)
__initialized__:
.if exists(${.CURDIR}/../Makefile.inc)
.include "${.CURDIR}/../Makefile.inc"
.endif
.endif  

.MAIN: all

.if defined(SUBDIR_CHANGE) && !empty(SUBDIR_CHANGE) && \
	exists(${SUBDIR_CHANGE}/${DIRPRFX}/subdirlist)
SUBDIR!=cat ${SUBDIR_CHANGE}/${DIRPRFX}/subdirlist
.endif

.if defined(SUBDIR_CHANGE) && !empty(SUBDIR_CHANGE) && \
	exists(${SUBDIR_CHANGE}/${DIRPRFX}/subdiradd)
_SUBDIR_EXTRA!=cat ${SUBDIR_CHANGE}/${DIRPRFX}/subdiradd
.endif

_SUBDIRUSE: .USE
	@for entry in ${SUBDIR} ${_SUBDIR_EXTRA}; do \
		(if ! (test -f ${SUBDIR_CHANGE}/${DIRPRFX}/subdirdrop && \
			grep -w $${entry} \
			    ${SUBDIR_CHANGE}/${DIRPRFX}/subdirdrop \
				> /dev/null); then \
			if test -d ${.CURDIR}/$${entry}.${MACHINE_ARCH}; then \
				${ECHODIR} \
				    "===> ${DIRPRFX}$${entry}.${MACHINE_ARCH}"; \
				edir=$${entry}.${MACHINE_ARCH}; \
				cd ${.CURDIR}/$${edir}; \
			else \
				${ECHODIR} "===> ${DIRPRFX}$$entry"; \
				edir=$${entry}; \
				cd ${.CURDIR}/$${edir}; \
			fi; \
			${MAKE} ${.TARGET:realinstall=install} \
				SUBDIR_CHANGE=${SUBDIR_CHANGE} \
				DIRPRFX=${DIRPRFX}$$edir/; \
			fi; \
		); \
	done

${SUBDIR}::
	@if test -d ${.TARGET}.${MACHINE_ARCH}; then \
		cd ${.CURDIR}/${.TARGET}.${MACHINE_ARCH}; \
	else \
		cd ${.CURDIR}/${.TARGET}; \
	fi; \
	${MAKE} all


.for __target in all all-man checkdpadd clean cleandepend cleandir depend lint \
		 maninstall obj objlink regress tags
.if !target(${__target})
${__target}: _SUBDIRUSE
.endif
.endfor

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
