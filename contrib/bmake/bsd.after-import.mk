# $Id: bsd.after-import.mk,v 1.22 2025/03/09 02:56:42 sjg Exp $

# This makefile is for use when integrating bmake into a BSD build
# system.  Use this makefile after importing bmake.
# It will bootstrap the new version,
# capture the generated files we need, and add an after-import
# target to allow the process to be easily repeated.

# The goal is to allow the benefits of autoconf without
# the overhead of running configure.

.MAIN: all
all: _makefile _utmakefile
all: after-import

# we rely on bmake
.if !defined(.PARSEDIR)
.error this makefile requires bmake
.endif

_this := ${MAKEFILE:tA} 
BMAKE_SRC := ${.PARSEDIR}

# it helps to know where the top of the tree is.
.if !defined(SRCTOP)
srctop := ${.MAKE.MAKEFILES:M*src/share/mk/sys.mk:H:H:H}
.if empty(srctop)
# likely locations?
.for d in contrib/bmake external/bsd/bmake/dist
.if ${BMAKE_SRC:M*/$d} != ""
srctop := ${BMAKE_SRC:tA:S,/$d,,}
.endif
.endfor
.endif
.if !empty(srctop)
SRCTOP := ${srctop}
.endif
.endif

# This lets us match what boot-strap does
.if defined(.MAKE.OS)
HOST_OS:= ${.MAKE.OS}
.elif !defined(HOST_OS)
HOST_OS!= uname
.endif

BOOTSTRAP_ARGS = \
	--prefix /usr \
	--share /usr/share

.if !empty(DEFAULT_SYS_PATH)
BOOTSTRAP_ARGS += --with-default-sys-path='${DEFAULT_SYS_PATH}'
.endif

# run boot-strap with minimal influence
bootstrap:	${BMAKE_SRC}/boot-strap ${MAKEFILE}
	HOME=/ ${BMAKE_SRC}/boot-strap -o ${HOST_OS} ${BOOTSTRAP_ARGS} ${BOOTSTRAP_XTRAS}
	touch ${.TARGET}

# Makefiles need a little more tweaking than say config.h
MAKEFILE_SED = 	sed -e '/^MACHINE/d' \
	-e '/include.*VERSION/d' \
	-e '/^CC=/s,=,?=,' \
	-e '/^PROG/ { s,=,?=,;s,bmake,$${.CURDIR:T},; }' \
	-e '/^\..*include  *</ { s,<\([a-z]\),<bsd.\1,;/autoconf/d; }' \
	-e 's,${SRCTOP},$${SRCTOP},g'

.ifdef WITH_SPACE_ASSIGN
.space := ${x x:L:S/x//Wg}
.else
.space =
# this line noise achieves the equivalent of negating
# /^(.if|.elif|#)|==/
#
MAKEFILE_SED += \
	-e '/^\.[e ]*l*if/!{/^\#/!{/==/!s,^\([^[:space:]][^=]*\)[[:space:]][[:space:]]*\([!?:+]*=\),\1\2,;};}' \

.endif

# These are the simple files we want to capture
configured_files= config.h Makefile.config unit-tests/Makefile.config

# FreeBSD has dropped their tag with svn
.if ${HOST_OS:NFreeBSD} == ""
ECHO_TAG= :
.else
ECHO_TAG?= echo
.endif

after-import: bootstrap ${MAKEFILE}
.for f in ${configured_files:M*.[ch]}
	@echo Capturing $f
	@mkdir -p ${${.CURDIR}/$f:L:H}
	@(${ECHO_TAG} '/* $$${HOST_OS}$$ */'; cat ${HOST_OS}/$f) > ${.CURDIR}/$f
.endfor
.for f in ${configured_files:M*Makefile*}
	@echo Capturing $f
	@mkdir -p ${${.CURDIR}/$f:L:H}
	@(echo '# This is a generated file, do NOT edit!'; \
	echo '# See ${_this:S,${SRCTOP}/,,}'; \
	echo '#'; ${ECHO_TAG} '# $$${HOST_OS}$$'; echo; \
	echo 'SRCTOP${.space}?= $${.CURDIR:${${.CURDIR}/$f:L:H:S,${SRCTOP}/,,:C,[^/]+,H,g:S,/,:,g}}'; echo; \
	${MAKEFILE_SED} ${HOST_OS}/$f ) > ${.CURDIR}/$f
.endfor

.if make(all)
_makefile _utmakefile: bootstrap ${MAKEFILE}
.endif

# this needs the most work
_makefile:
	@echo Generating ${.CURDIR}/Makefile
	@(echo '# This is a generated file, do NOT edit!'; \
	echo '# See ${_this:S,${SRCTOP}/,,}'; \
	echo '#'; ${ECHO_TAG} '# $$${HOST_OS}$$'; \
	echo; echo 'SRCTOP${.space}?= $${.CURDIR:${.CURDIR:S,${SRCTOP}/,,:C,[^/]+,H,g:S,/,:,g}}'; \
	echo; echo '# look here first for config.h'; \
	echo 'CFLAGS${.space}+= -I$${.CURDIR}'; echo; \
	echo '# for after-import'; \
	echo 'CLEANDIRS${.space}+= ${HOST_OS}'; \
	echo 'CLEANFILES${.space}+= bootstrap'; echo; \
	${MAKEFILE_SED} \
	${1 2:L:@n@-e '/start-delete$n/,/end-delete$n/d'@} \
	${BMAKE_SRC}/Makefile; \
	echo; echo '# override some simple things'; \
	echo 'BINDIR${.space}= /usr/bin'; \
	echo 'MANDIR${.space}= ${MANDIR:U/usr/share/man}'; \
	echo; echo '# make sure we get this'; \
	echo 'CFLAGS${.space}+= $${COPTS.$${.IMPSRC:T}}'; \
	echo; echo 'after-import: ${_this:S,${SRCTOP},\${SRCTOP},}'; \
	echo '	cd $${.CURDIR} && $${.MAKE} -f ${_this:S,${SRCTOP},\${SRCTOP},}'; \
	echo ) > ${.TARGET}
	@cmp -s ${.TARGET} ${.CURDIR}/Makefile || \
	    mv ${.TARGET} ${.CURDIR}/Makefile

_utmakefile:
	@echo Generating ${.CURDIR}/unit-tests/Makefile
	@mkdir -p ${.CURDIR}/unit-tests
	@(echo '# This is a generated file, do NOT edit!'; \
	echo '# See ${_this:S,${SRCTOP}/,,}'; \
	echo '#'; ${ECHO_TAG} '# $$${HOST_OS}$$'; \
	${MAKEFILE_SED} \
	-e '/^UNIT_TESTS/s,=.*,= $${srcdir},' \
	${BMAKE_SRC}/unit-tests/Makefile ) > ${.TARGET}
	@cmp -s ${.TARGET} ${.CURDIR}/unit-tests/Makefile || \
	    mv ${.TARGET} ${.CURDIR}/unit-tests/Makefile


.include <bsd.obj.mk>

