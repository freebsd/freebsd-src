#	from: @(#)bsd.subdir.mk	5.9 (Berkeley) 2/1/91
#	$Id: bsd.port.subdir.mk,v 1.26 1998/11/08 10:29:53 asami Exp $
#
# The include file <bsd.port.subdir.mk> contains the default targets
# for building ports subdirectories. 
#
#
# +++ variables +++
#
# STRIP		The flag passed to the install program to cause the binary
#		to be stripped.  This is to be used when building your
#		own install script so that the entire system can be made
#		stripped/not-stripped using a single knob. [-s]
#
# ECHO_MSG	Used to print all the '===>' style prompts - override this
#		to turn them off [echo].
#
# OPSYS		Get the operating system type [`uname -s`]
#
# SUBDIR	A list of subdirectories that should be built as well.
#		Each of the targets will execute the same target in the
#		subdirectories.
#
#
# +++ targets +++
#
#	README.html:
#		Creating README.html for package.
#
#	afterinstall, all, beforeinstall, build, checksum, clean,
#	clean-for-cdrom, clean-restricted, configure, deinstall,
#	depend, depends, describe, extract, fetch, fetch-list, ignorelist,
#	install, package, package-loop, readmes, realinstall, reinstall, tags
#


.MAIN: all

.if !defined(DEBUG_FLAGS)
STRIP?=	-s
.endif

.if !defined(OPSYS)	# XXX !!
OPSYS!=	uname -s
.endif

ECHO_MSG?=	echo

_SUBDIRUSE: .USE
	@for entry in ${SUBDIR}; do \
		OK=""; \
		for dud in $$DUDS; do \
			if [ $${dud} = $${entry} ]; then \
				OK="false"; \
				${ECHO_MSG} "===> ${DIRPRFX}$${entry} skipped"; \
			fi; \
		done; \
		if test -d ${.CURDIR}/$${entry}.${MACHINE}; then \
			edir=$${entry}.${MACHINE}; \
		elif test -d ${.CURDIR}/$${entry}; then \
			edir=$${entry}; \
		else \
			OK="false"; \
			${ECHO_MSG} "===> ${DIRPRFX}$${entry} non-existent"; \
		fi; \
		if [ "$$OK" = "" ]; then \
			${ECHO_MSG} "===> ${DIRPRFX}$${edir}"; \
			cd ${.CURDIR}/$${edir}; \
			${MAKE} ${.TARGET:realinstall=install} \
				DIRPRFX=${DIRPRFX}$$edir/; \
		fi; \
	done

${SUBDIR}::
	@if test -d ${.TARGET}.${MACHINE}; then \
		cd ${.CURDIR}/${.TARGET}.${MACHINE}; \
	else \
		cd ${.CURDIR}/${.TARGET}; \
	fi; \
	${MAKE} all

.for __target in all fetch fetch-list package package-loop extract configure \
		 build clean clean-for-cdrom clean-restricted \
		 deinstall depend depends describe distclean \
		 reinstall tags checksum \
		 ignorelist
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

IGNOREDIR=	CVS distfiles packages pkg templates

.if !target(checksubdirs)
.if defined(PORTSTOP)
checksubdirs: checksubdir _SUBDIRUSE
.else
checksubdirs: checksubdir
.endif
.endif

.if !target(checksubdir)
checksubdir:
	@for d in *; do \
	  if [ -d "$$d" ]; then \
	    found=0; \
	    for s in ${SUBDIR} ${IGNOREDIR}; do \
	      if [ "x$$s" = "x$$d" ]; then \
	        found=1; \
	        break; \
	      fi; \
	    done; \
	    if [ $$found = 0 ]; then \
	      ${ECHO} "Warning: directory $$d not in SUBDIR"; \
	    fi; \
	  fi; \
	done
.endif

.if !target(readmes)
readmes: readme _SUBDIRUSE
.endif

.if !target(readme)
readme:
	@rm -f README.html
	@make README.html
.endif

.if (${OPSYS} == "NetBSD")
PORTSDIR ?= /usr/opt
.else
PORTSDIR ?= /usr/ports
.endif
TEMPLATES ?= ${PORTSDIR}/templates
.if defined(PORTSTOP)
README=	${TEMPLATES}/README.top
.else
README=	${TEMPLATES}/README.category
.endif

HTMLIFY=	sed -e 's/&/\&amp;/g' -e 's/>/\&gt;/g' -e 's/</\&lt;/g'

README.html:
	@echo "===>  Creating README.html"
	@> $@.tmp
.for entry in ${SUBDIR}
.if defined(PORTSTOP)
	@echo -n '<a href="'${entry}/README.html'">'"`echo ${entry} | ${HTMLIFY}`"'</a>: ' >> $@.tmp
.else
	@echo -n '<a href="'${entry}/README.html'">'"`cd ${entry}; make package-name | ${HTMLIFY}`</a>: " >> $@.tmp
.endif
.if exists(${entry}/pkg/COMMENT)
	@${HTMLIFY} ${entry}/pkg/COMMENT >> $@.tmp
.else
	@echo "(no description)" >> $@.tmp
.endif
.endfor
	@sort -t '>' +1 -2 $@.tmp > $@.tmp2
.if exists(${.CURDIR}/pkg/DESCR)
	@${HTMLIFY} ${.CURDIR}/pkg/DESCR > $@.tmp3
.else
	@> $@.tmp3
.endif
	@cat ${README} | \
		sed -e 's/%%CATEGORY%%/'"`basename ${.CURDIR}`"'/g' \
			-e '/%%DESCR%%/r$@.tmp3' \
			-e '/%%DESCR%%/d' \
			-e '/%%SUBDIR%%/r$@.tmp2' \
			-e '/%%SUBDIR%%/d' \
		> $@
	@rm -f $@.tmp $@.tmp2 $@.tmp3
