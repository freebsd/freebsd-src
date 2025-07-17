
# Part of a unified Makefile for building kernels.  This part includes all
# the definitions that need to be after all the % directives except %RULES
# and ones that act like they are part of %RULES.
#
# Most make variables should not be defined in this file.  Instead, they
# should be defined in the kern.pre.mk so that port makefiles can
# override or augment them.

.if defined(DTS) || defined(DTSO) || defined(FDT_DTS_FILE)
.include "dtb.build.mk"

KERNEL_EXTRA+=	${DTB} ${DTBO}
CLEAN+=		${DTB} ${DTBO}

kernel-install: _dtbinstall
.ORDER: beforeinstall _dtbinstall
.endif

# In case the config had a makeoptions DESTDIR...
.if defined(DESTDIR)
MKMODULESENV+=	DESTDIR="${DESTDIR}"
.endif
SYSDIR?= ${S:C;^[^/];${.CURDIR}/&;:tA}
MKMODULESENV+=	KERNBUILDDIR="${.CURDIR}" SYSDIR="${SYSDIR}"
MKMODULESENV+=  MODULE_TIED=yes

.if defined(CONF_CFLAGS)
MKMODULESENV+=	CONF_CFLAGS="${CONF_CFLAGS}"
.endif

.if defined(WITH_CTF)
MKMODULESENV+=	WITH_CTF="${WITH_CTF}"
.endif

.if !empty(KCSAN_ENABLED)
MKMODULESENV+=	KCSAN_ENABLED="yes"
.endif

.if defined(GCOV_CFLAGS)
MKMODULESENV+=	GCOV_CFLAGS="${GCOV_CFLAGS}"
.endif

.if !empty(COMPAT_FREEBSD32_ENABLED)
MKMODULESENV+=	COMPAT_FREEBSD32_ENABLED="yes"
.endif

# Allow overriding the kernel debug directory, so kernel and user debug may be
# installed in different directories. Setting it to "" restores the historical
# behavior of installing debug files in the kernel directory.
KERN_DEBUGDIR?=	${DEBUGDIR}

.MAIN: all

.if !defined(NO_MODULES)
# Default prefix used for modules installed from ports
LOCALBASE?=	/usr/local

LOCAL_MODULES_DIR?= ${LOCALBASE}/sys/modules

# Default to installing all modules installed by ports unless overridden
# by the user.
.if !defined(LOCAL_MODULES) && exists(${LOCAL_MODULES_DIR})
LOCAL_MODULES!= ls ${LOCAL_MODULES_DIR}
.endif
.endif

.for target in all clean cleandepend cleandir clobber depend install \
    ${_obj} reinstall tags
${target}: kernel-${target}
.if !defined(NO_MODULES)
${target}: modules-${target}
modules-${target}:
.if !defined(MODULES_WITH_WORLD) && exists($S/modules)
	cd $S/modules; ${MKMODULESENV} ${MAKE} \
	    ${target:S/^reinstall$/install/:S/^clobber$/cleandir/}
.endif
.for module in ${LOCAL_MODULES}
	@${ECHODIR} "===> ${module} (${target:S/^reinstall$/install/:S/^clobber$/cleandir/})"
	@cd ${LOCAL_MODULES_DIR}/${module}; ${MKMODULESENV} ${MAKE} \
	    DIRPRFX="${module}/" \
	    ${target:S/^reinstall$/install/:S/^clobber$/cleandir/}
.endfor
.endif
.endfor

# Handle ports (as defined by the user) that build kernel modules
.if !defined(NO_MODULES) && defined(PORTS_MODULES)
#
# The ports tree needs some environment variables defined to match the new kernel
#
# SRC_BASE is how the ports tree refers to the location of the base source files
.if !defined(SRC_BASE)
SRC_BASE=	${SYSDIR:H:tA}
.endif
# OSVERSION is used by some ports to determine build options
.if !defined(OSRELDATE)
# Definition copied from src/Makefile.inc1
OSRELDATE!=	awk '/^\#define[[:space:]]*__FreeBSD_version/ { print $$3 }' \
		    ${MAKEOBJDIRPREFIX}${SRC_BASE}/include/osreldate.h
.endif
# Keep the related ports builds in the obj directory so that they are only rebuilt once per kernel build
#
# Ports search for some dependencies in PATH, so add the location of the
# installed files
WRKDIRPREFIX?=	${.OBJDIR}
PORTSMODULESENV=\
	env \
	-u CC \
	-u CXX \
	-u CPP \
	-u MAKESYSPATH \
	-u MK_AUTO_OBJ \
	-u MAKEOBJDIR \
	MAKEFLAGS="${MAKEFLAGS:M*:tW:S/^-m /-m_/g:S/ -m / -m_/g:tw:N-m_*:NMK_AUTO_OBJ=*}" \
	SYSDIR=${SYSDIR} \
	PATH=${PATH}:${LOCALBASE}/bin:${LOCALBASE}/sbin \
	SRC_BASE=${SRC_BASE} \
	OSVERSION=${OSRELDATE} \
	WRKDIRPREFIX=${WRKDIRPREFIX}

# The WRKDIR needs to be cleaned before building, and trying to change the target
# with a :C pattern below results in install -> instclean
all:
.for __i in ${PORTS_MODULES}
	@${ECHO} "===> Ports module ${__i} (all)"
	cd ${PORTSDIR:U/usr/ports}/${__i}; ${PORTSMODULESENV} ${MAKE} -B clean build
.endfor

.for __target in install reinstall clean
${__target}: ports-${__target}
ports-${__target}:
.for __i in ${PORTS_MODULES}
	@${ECHO} "===> Ports module ${__i} (${__target})"
	cd ${PORTSDIR:U/usr/ports}/${__i}; ${PORTSMODULESENV} ${MAKE} -B ${__target:C/(re)?install/deinstall reinstall/}
.endfor
.endfor
.endif

# Generate the .bin (booti images) kernel as an extra build output.
# The targets and rules to generate these appear in Makefile.$MACHINE
# if the platform supports it.
.if ${MK_KERNEL_BIN} != "no"
KERNEL_EXTRA+= ${KERNEL_KO}.bin
KERNEL_EXTRA_INSTALL+= ${KERNEL_KO}.bin
CLEAN+=	${KERNEL_KO}.bin
.endif

.ORDER: kernel-install modules-install

beforebuild: .PHONY
kernel-all: beforebuild .WAIT ${KERNEL_KO} ${KERNEL_EXTRA}

kernel-cleandir: kernel-clean kernel-cleandepend

kernel-clobber:
	find . -maxdepth 1 ! -type d ! -name version -delete

kernel-obj:

.if !defined(NO_MODULES)
modules: modules-all
modules-depend: beforebuild
modules-all: beforebuild

.if !defined(NO_MODULES_OBJ)
modules-all modules-depend: modules-obj
.endif
.endif

.if !defined(DEBUG)
FULLKERNEL=	${KERNEL_KO}
.else
FULLKERNEL=	${KERNEL_KO}.full
${KERNEL_KO}: ${FULLKERNEL} ${KERNEL_KO}.debug
	${OBJCOPY} --strip-debug --add-gnu-debuglink=${KERNEL_KO}.debug \
	    ${FULLKERNEL} ${.TARGET}
${KERNEL_KO}.debug: ${FULLKERNEL}
	${OBJCOPY} --only-keep-debug ${FULLKERNEL} ${.TARGET}
install.debug reinstall.debug: gdbinit
	cd ${.CURDIR}; ${MAKE} ${.TARGET:R}

# Install gdbinit files for kernel debugging.
gdbinit:
	grep -v '# XXX' ${S}/../tools/debugscripts/dot.gdbinit | \
	    sed "s:MODPATH:${.OBJDIR}/modules:" > .gdbinit
	cp ${S}/../tools/debugscripts/gdbinit.kernel ${.CURDIR}
.if exists(${S}/../tools/debugscripts/gdbinit.${MACHINE_CPUARCH})
	cp ${S}/../tools/debugscripts/gdbinit.${MACHINE_CPUARCH} \
	    ${.CURDIR}/gdbinit.machine
.endif
.endif

${FULLKERNEL}: ${SYSTEM_DEP} vers.o
	@rm -f ${.TARGET}
	@echo linking ${.TARGET}
	${SYSTEM_LD}
.if !empty(MD_ROOT_SIZE_CONFIGURED) && defined(MFS_IMAGE)
	@sh ${S}/tools/embed_mfs.sh ${.TARGET} ${MFS_IMAGE}
.endif
.if ${MK_CTF} != "no"
	@echo ${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ...
	@${CTFMERGE} ${CTFFLAGS} -o ${.TARGET} ${SYSTEM_OBJS} vers.o
.endif
.if !defined(DEBUG)
	${OBJCOPY} --strip-debug ${.TARGET}
.endif
	${SYSTEM_LD_TAIL}

OBJS_DEPEND_GUESS+=	offset.inc assym.inc vnode_if.h ${BEFORE_DEPEND:M*.h} \
			${MFILES:T:S/.m$/.h/}

.for mfile in ${MFILES}
# XXX the low quality .m.o rules gnerated by config are normally used
# instead of the .m.c rules here.
${mfile:T:S/.m$/.c/}: ${mfile}
	${AWK} -f $S/tools/makeobjops.awk ${mfile} -c
${mfile:T:S/.m$/.h/}: ${mfile}
	${AWK} -f $S/tools/makeobjops.awk ${mfile} -h
.endfor

kernel-clean:
	rm -f *.o *.so *.pico *.ko *.s eddep errs \
	    ${FULLKERNEL} ${KERNEL_KO} ${KERNEL_KO}.debug \
	    tags vers.c \
	    vnode_if.c vnode_if.h vnode_if_newproto.h vnode_if_typedef.h \
	    ${MFILES:T:S/.m$/.c/} ${MFILES:T:S/.m$/.h/} \
	    ${CLEAN}

# This is a hack.  BFD "optimizes" away dynamic mode if there are no
# dynamic references.  We could probably do a '-Bforcedynamic' mode like
# in the a.out ld.  For now, this works.
force-dynamic-hack.c:
	:> ${.TARGET}

force-dynamic-hack.pico: force-dynamic-hack.c Makefile
	${CC} ${CCLDFLAGS} -shared ${CFLAGS} -nostdlib \
	    force-dynamic-hack.c -o ${.TARGET}

offset.inc: $S/kern/genoffset.sh genoffset.o
	NM='${NM}' NMFLAGS='${NMFLAGS}' sh $S/kern/genoffset.sh genoffset.o > ${.TARGET}

genoffset.o: $S/kern/genoffset.c
	${CC} -c ${NOSAN_CFLAGS:N-flto*:N-fno-common} \
	    -fcommon $S/kern/genoffset.c

# genoffset_test.o is not actually used for anything - the point of compiling it
# is to exercise the CTASSERT that checks that the offsets in the offset.inc
# _lite struct(s) match those in the original(s). 
genoffset_test.o: $S/kern/genoffset.c offset.inc
	${CC} -c ${NOSAN_CFLAGS:N-flto*:N-fno-common} \
	    -fcommon -DOFFSET_TEST $S/kern/genoffset.c -o ${.TARGET}

assym.inc: $S/kern/genassym.sh genassym.o genoffset_test.o
	NM='${NM}' NMFLAGS='${NMFLAGS}' sh $S/kern/genassym.sh genassym.o > ${.TARGET}

genassym.o: $S/$M/$M/genassym.c  offset.inc
	${CC} -c ${NOSAN_CFLAGS:N-flto*:N-fno-common} \
	    -fcommon $S/$M/$M/genassym.c

OBJS_DEPEND_GUESS+= opt_global.h
genoffset.o genassym.o vers.o: opt_global.h

.if !empty(.MAKE.MODE:Unormal:Mmeta) && empty(.MAKE.MODE:Unormal:Mnofilemon)
_meta_filemon=	1
.endif
.if ${MK_DIRDEPS_BUILD} == "no"
# Skip reading .depend when not needed to speed up tree-walks and simple
# lookups.  For install, only do this if no other targets are specified.
# Also skip generating or including .depend.* files if in meta+filemon mode
# since it will track dependencies itself.  OBJS_DEPEND_GUESS is still used
# for _meta_filemon but not for _SKIP_DEPEND.
.if !defined(NO_SKIP_DEPEND) && \
    ((!empty(.MAKEFLAGS:M-V) && empty(.MAKEFLAGS:M*DEP*)) || \
    ${.TARGETS:M*obj} == ${.TARGETS} || \
    ${.TARGETS:M*clean*} == ${.TARGETS} || \
    ${.TARGETS:M*install*} == ${.TARGETS})
_SKIP_DEPEND=	1
.endif
.if defined(_SKIP_DEPEND) || defined(_meta_filemon)
.MAKE.DEPENDFILE=	/dev/null
.endif
.endif

kernel-depend: .depend
SRCS=	assym.inc offset.inc vnode_if.h ${BEFORE_DEPEND} ${CFILES} \
	${SYSTEM_CFILES} ${GEN_CFILES} ${SFILES} \
	${MFILES:T:S/.m$/.h/}
DEPENDOBJS+=	${SYSTEM_OBJS} genassym.o genoffset.o genoffset_test.o
DEPENDOBJS+=	${CLEAN:M*.o}
DEPENDFILES=	${DEPENDOBJS:O:u:C/^/.depend./}
.if ${MAKE_VERSION} < 20160220
DEPEND_MP?=	-MP
.endif
.if defined(_SKIP_DEPEND)
# Don't bother reading any .meta files
${DEPENDOBJS}:	.NOMETA
.depend:	.NOMETA
# Unset these to avoid looping/statting on them later.
.undef DEPENDOBJS
.undef DEPENDFILES
.endif	# defined(_SKIP_DEPEND)
DEPEND_CFLAGS+=	-MD ${DEPEND_MP} -MF.depend.${.TARGET}
DEPEND_CFLAGS+=	-MT${.TARGET}
.if !defined(_meta_filemon)
.if !empty(DEPEND_CFLAGS)
# Only add in DEPEND_CFLAGS for CFLAGS on files we expect from DEPENDOBJS
# as those are the only ones we will include.
DEPEND_CFLAGS_CONDITION= "${DEPENDOBJS:M${.TARGET}}" != ""
CFLAGS+=	${${DEPEND_CFLAGS_CONDITION}:?${DEPEND_CFLAGS}:}
.endif
.for __depend_obj in ${DEPENDFILES}
.if ${MAKE_VERSION} < 20160220
.sinclude "${.OBJDIR}/${__depend_obj}"
.else
.dinclude "${.OBJDIR}/${__depend_obj}"
.endif
.endfor
.endif	# !defined(_meta_filemon)

# Always run 'make depend' to generate dependencies early and to avoid the
# need for manually running it.  For the kernel this is mostly a NOP since
# all dependencies are correctly added or accounted for.  This is mostly to
# ensure downstream uses of kernel-depend are handled.
beforebuild: kernel-depend

# Guess some dependencies for when no ${DEPENDFILE}.OBJ is generated yet.
# For meta+filemon the .meta file is checked for since it is the dependency
# file used.
.for __obj in ${DEPENDOBJS:O:u}
.if defined(_meta_filemon)
_depfile=	${.OBJDIR}/${__obj}.meta
.else
_depfile=	${.OBJDIR}/.depend.${__obj}
.endif
.if !exists(${_depfile})
.if ${SYSTEM_OBJS:M${__obj}}
${__obj}: ${OBJS_DEPEND_GUESS}
.endif
${__obj}: ${OBJS_DEPEND_GUESS.${__obj}}
.elif defined(_meta_filemon)
# For meta mode we still need to know which file to depend on to avoid
# ambiguous suffix transformation rules from .PATH.  Meta mode does not
# use .depend files.  We really only need source files, not headers since
# they are typically in SRCS/beforebuild already.  For target-specific
# guesses do include headers though since they may not be in SRCS.
.if ${SYSTEM_OBJS:M${__obj}}
${__obj}: ${OBJS_DEPEND_GUESS:N*.h}
.endif
${__obj}: ${OBJS_DEPEND_GUESS.${__obj}}
.endif	# !exists(${_depfile})
.endfor

.NOPATH: .depend ${DEPENDFILES}

.depend: .PRECIOUS ${SRCS}

_ILINKS= machine
.if ${MACHINE} != ${MACHINE_CPUARCH} && ${MACHINE} != "arm64"
_ILINKS+= ${MACHINE_CPUARCH}
.endif
.if ${MACHINE_CPUARCH} == "i386" || ${MACHINE_CPUARCH} == "amd64"
_ILINKS+= x86
.endif
.if ${MACHINE_CPUARCH} == "amd64"
_ILINKS+= i386
.endif

# Ensure that the link exists without depending on it when it exists.
# Ensure that debug info references the path in the source tree.
.for _link in ${_ILINKS}
.if !exists(${.OBJDIR}/${_link})
${SRCS} ${DEPENDOBJS}: ${_link}
.endif
.if ${_link} == "machine"
CFLAGS+= -fdebug-prefix-map=./machine=${SYSDIR}/${MACHINE}/include
.else
CFLAGS+= -fdebug-prefix-map=./${_link}=${SYSDIR}/${_link}/include
.endif
.endfor

${_ILINKS}:
	@case ${.TARGET} in \
	machine) \
		path=${S}/${MACHINE}/include ;; \
	*) \
		path=${S}/${.TARGET}/include ;; \
	esac ; \
	${ECHO} ${.TARGET} "->" $$path ; \
	ln -fns $$path ${.TARGET}

# .depend needs include links so we remove them only together.
kernel-cleandepend: .PHONY
	rm -f .depend .depend.* ${_ILINKS}

kernel-tags:
	@ls .depend.* > /dev/null 2>&1 || \
	    { echo "you must make all first"; exit 1; }
	sh $S/conf/systags.sh

kernel-install: .PHONY
	@if [ ! -f ${KERNEL_KO} ] ; then \
		echo "You must build a kernel first." ; \
		exit 1 ; \
	fi
.if exists(${DESTDIR}${KODIR})
	-thiskernel=`sysctl -n kern.bootfile || echo /boot/kernel/kernel` ; \
	if [ ! "`dirname "$$thiskernel"`" -ef ${DESTDIR}${KODIR} ] ; then \
		chflags -R noschg ${DESTDIR}${KODIR} ; \
		rm -rf ${DESTDIR}${KODIR} ; \
		rm -rf ${DESTDIR}${KERN_DEBUGDIR}${KODIR} ; \
	else \
		if [ -d ${DESTDIR}${KODIR}.old ] ; then \
			chflags -R noschg ${DESTDIR}${KODIR}.old ; \
			rm -rf ${DESTDIR}${KODIR}.old ; \
		fi ; \
		mv ${DESTDIR}${KODIR} ${DESTDIR}${KODIR}.old ; \
		if [ -n "${KERN_DEBUGDIR}" -a \
		     -d ${DESTDIR}${KERN_DEBUGDIR}${KODIR} ]; then \
			rm -rf ${DESTDIR}${KERN_DEBUGDIR}${KODIR}.old ; \
			mv ${DESTDIR}${KERN_DEBUGDIR}${KODIR} ${DESTDIR}${KERN_DEBUGDIR}${KODIR}.old ; \
		fi ; \
		sysctl kern.bootfile=${DESTDIR}${KODIR}.old/"`basename "$$thiskernel"`" ; \
	fi
.endif
	mkdir -p ${DESTDIR}${KODIR}
	${INSTALL} -p -m ${KMODMODE} -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_KO} ${DESTDIR}${KODIR}/
.if defined(DEBUG) && !defined(INSTALL_NODEBUG) && ${MK_KERNEL_SYMBOLS} != "no"
	mkdir -p ${DESTDIR}${KERN_DEBUGDIR}${KODIR}
	${INSTALL} -p -m ${KMODMODE} -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_KO}.debug ${DESTDIR}${KERN_DEBUGDIR}${KODIR}/
.endif
.if defined(KERNEL_EXTRA_INSTALL)
	${INSTALL} -p -m ${KMODMODE} -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_EXTRA_INSTALL} ${DESTDIR}${KODIR}/
.endif



kernel-reinstall:
	@-chflags -R noschg ${DESTDIR}${KODIR}
	${INSTALL} -p -m ${KMODMODE} -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_KO} ${DESTDIR}${KODIR}/
.if defined(DEBUG) && !defined(INSTALL_NODEBUG) && ${MK_KERNEL_SYMBOLS} != "no"
	${INSTALL} -p -m ${KMODMODE} -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_KO}.debug ${DESTDIR}${KERN_DEBUGDIR}${KODIR}/
.endif

config.o env.o hints.o vers.o vnode_if.o:
	${NORMAL_C}
	${NORMAL_CTFCONVERT}

NEWVERS_ENV+= MAKE="${MAKE}"
.if ${MK_REPRODUCIBLE_BUILD} != "no"
NEWVERS_ARGS+= -R
.endif
vers.c: .NOMETA_CMP $S/conf/newvers.sh $S/sys/param.h ${SYSTEM_DEP:Nvers.*}
	${NEWVERS_ENV} sh $S/conf/newvers.sh ${NEWVERS_ARGS} ${KERN_IDENT}

vnode_if.c: $S/tools/vnode_if.awk $S/kern/vnode_if.src
	${AWK} -f $S/tools/vnode_if.awk $S/kern/vnode_if.src -c

vnode_if.h vnode_if_newproto.h vnode_if_typedef.h: $S/tools/vnode_if.awk \
    $S/kern/vnode_if.src
vnode_if.h: vnode_if_newproto.h vnode_if_typedef.h
	${AWK} -f $S/tools/vnode_if.awk $S/kern/vnode_if.src -h
vnode_if_newproto.h:
	${AWK} -f $S/tools/vnode_if.awk $S/kern/vnode_if.src -p
vnode_if_typedef.h:
	${AWK} -f $S/tools/vnode_if.awk $S/kern/vnode_if.src -q

.if ${MFS_IMAGE:Uno} != "no"
.if empty(MD_ROOT_SIZE_CONFIGURED)
embedfs_${MFS_IMAGE:T:R}.o: ${MFS_IMAGE} $S/dev/md/embedfs.S
	${CC} ${CFLAGS} ${ACFLAGS} -DMFS_IMAGE=\""${MFS_IMAGE}"\" -c \
	    $S/dev/md/embedfs.S -o ${.TARGET}
.endif
.endif

.include "kern.mk"
