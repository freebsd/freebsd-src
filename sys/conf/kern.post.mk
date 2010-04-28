# $FreeBSD$

# Part of a unified Makefile for building kernels.  This part includes all
# the definitions that need to be after all the % directives except %RULES
# and ones that act like they are part of %RULES.
#
# Most make variables should not be defined in this file.  Instead, they
# should be defined in the kern.pre.mk so that port makefiles can
# override or augment them.

# In case the config had a makeoptions DESTDIR...
.if defined(DESTDIR)
MKMODULESENV+=	DESTDIR="${DESTDIR}"
.endif
SYSDIR?= ${S:C;^[^/];${.CURDIR}/&;}
MKMODULESENV+=	KERNBUILDDIR="${.CURDIR}" SYSDIR="${SYSDIR}"

.if defined(CONF_CFLAGS)
MKMODULESENV+=	CONF_CFLAGS="${CONF_CFLAGS}"
.endif

.if defined(WITH_CTF)
MKMODULESENV+=	WITH_CTF="${WITH_CTF}"
.endif

.MAIN: all

.for target in all clean cleandepend cleandir clobber depend install \
    obj reinstall tags
${target}: kernel-${target}
.if !defined(MODULES_WITH_WORLD) && !defined(NO_MODULES) && exists($S/modules)
${target}: modules-${target}
modules-${target}:
	cd $S/modules; ${MKMODULESENV} ${MAKE} \
	    ${target:S/^reinstall$/install/:S/^clobber$/cleandir/}
.endif
.endfor

# Handle out of tree ports 
.if !defined(NO_MODULES) && defined(PORTS_MODULES)
PORTSMODULESENV=SYSDIR=${SYSDIR}
.for __target in all install reinstall clean
${__target}: ports-${__target}
ports-${__target}:
.for __i in ${PORTS_MODULES}
	cd $${PORTSDIR:-/usr/ports}/${__i}; ${PORTSMODULESENV} ${MAKE} -B ${__target:C/install/deinstall reinstall/:C/reinstall/deinstall reinstall/}
.endfor
.endfor
.endif

.ORDER: kernel-install modules-install

kernel-all: ${KERNEL_KO} ${KERNEL_EXTRA}

kernel-cleandir: kernel-clean kernel-cleandepend

kernel-clobber:
	find . -maxdepth 1 ! -type d ! -name version -delete

kernel-obj:

.if !defined(MODULES_WITH_WORLD) && !defined(NO_MODULES) && exists($S/modules)
modules: modules-all

.if !defined(NO_MODULES_OBJ)
modules-all modules-depend: modules-obj
.endif
.endif

.if !defined(DEBUG)
FULLKERNEL=	${KERNEL_KO}
.else
FULLKERNEL=	${KERNEL_KO}.debug
${KERNEL_KO}: ${FULLKERNEL} ${KERNEL_KO}.symbols
	${OBJCOPY} --strip-debug --add-gnu-debuglink=${KERNEL_KO}.symbols\
	    ${FULLKERNEL} ${.TARGET}
${KERNEL_KO}.symbols: ${FULLKERNEL}
	${OBJCOPY} --only-keep-debug ${FULLKERNEL} ${.TARGET}
install.debug reinstall.debug: gdbinit
	cd ${.CURDIR}; ${MAKE} ${.TARGET:R}

# Install gdbinit files for kernel debugging.
gdbinit:
	grep -v '# XXX' ${S}/../tools/debugscripts/dot.gdbinit | \
	    sed "s:MODPATH:${.OBJDIR}/modules:" > .gdbinit
	cp ${S}/../tools/debugscripts/gdbinit.kernel ${.CURDIR}
.if exists(${S}/../tools/debugscripts/gdbinit.${MACHINE_ARCH})
	cp ${S}/../tools/debugscripts/gdbinit.${MACHINE_ARCH} \
	    ${.CURDIR}/gdbinit.machine
.endif
.endif

${FULLKERNEL}: ${SYSTEM_DEP} vers.o
	@rm -f ${.TARGET}
	@echo linking ${.TARGET}
	${SYSTEM_LD}
	@${SYSTEM_CTFMERGE}
.if !defined(DEBUG)
	${OBJCOPY} --strip-debug ${.TARGET}
.endif
	${SYSTEM_LD_TAIL}
.if defined(MFS_IMAGE)
	@sh ${S}/tools/embed_mfs.sh ${FULLKERNEL} ${MFS_IMAGE}
.endif

.if !exists(${.OBJDIR}/.depend)
${SYSTEM_OBJS}: assym.s vnode_if.h ${BEFORE_DEPEND:M*.h} ${MFILES:T:S/.m$/.h/}
.endif

LNFILES=	${CFILES:T:S/.c$/.ln/}

.for mfile in ${MFILES}
# XXX the low quality .m.o rules gnerated by config are normally used
# instead of the .m.c rules here.
${mfile:T:S/.m$/.c/}: ${mfile}
	${AWK} -f $S/tools/makeobjops.awk ${mfile} -c
${mfile:T:S/.m$/.h/}: ${mfile}
	${AWK} -f $S/tools/makeobjops.awk ${mfile} -h
.endfor

kernel-clean:
	rm -f *.o *.so *.So *.ko *.s eddep errs \
	    ${FULLKERNEL} ${KERNEL_KO} ${KERNEL_KO}.symbols \
	    linterrs makelinks tags vers.c \
	    vnode_if.c vnode_if.h vnode_if_newproto.h vnode_if_typedef.h \
	    ${MFILES:T:S/.m$/.c/} ${MFILES:T:S/.m$/.h/} \
	    ${CLEAN}

lint: ${LNFILES}
	${LINT} ${LINTKERNFLAGS} ${CFLAGS:M-[DILU]*} ${.ALLSRC} 2>&1 | \
	    tee -a linterrs

# This is a hack.  BFD "optimizes" away dynamic mode if there are no
# dynamic references.  We could probably do a '-Bforcedynamic' mode like
# in the a.out ld.  For now, this works.
HACK_EXTRA_FLAGS?= -shared
hack.So: Makefile
	:> hack.c
	${CC} ${HACK_EXTRA_FLAGS} -nostdlib hack.c -o hack.So
	rm -f hack.c

# This rule stops ./assym.s in .depend from causing problems.
./assym.s: assym.s

assym.s: $S/kern/genassym.sh genassym.o
	NM='${NM}' sh $S/kern/genassym.sh genassym.o > ${.TARGET}

genassym.o: $S/$M/$M/genassym.c
	${CC} -c ${CFLAGS:N-fno-common} $S/$M/$M/genassym.c

${SYSTEM_OBJS} genassym.o vers.o: opt_global.h

kernel-depend: .depend
# The argument list can be very long, so use make -V and xargs to
# pass it to mkdep.
SRCS=	assym.s vnode_if.h ${BEFORE_DEPEND} ${CFILES} \
	${SYSTEM_CFILES} ${GEN_CFILES} ${SFILES} \
	${MFILES:T:S/.m$/.h/}
.depend: .PRECIOUS ${SRCS}
	rm -f .newdep
	${MAKE} -V CFILES -V SYSTEM_CFILES -V GEN_CFILES | \
	    MKDEP_CPP="${CC} -E" CC="${CC}" xargs mkdep -a -f .newdep ${CFLAGS}
	${MAKE} -V SFILES | \
	    MKDEP_CPP="${CC} -E" xargs mkdep -a -f .newdep ${ASM_CFLAGS}
	rm -f .depend
	mv .newdep .depend

_ILINKS= machine
.if ${MACHINE} != ${MACHINE_ARCH}
_ILINKS+= ${MACHINE_ARCH}
.endif

# Ensure that the link exists without depending on it when it exists.
.for _link in ${_ILINKS}
.if !exists(${.OBJDIR}/${_link})
${SRCS}: ${_link}
.endif
.endfor

${_ILINKS}:
	@case ${.TARGET} in \
	machine) \
		path=${S}/${MACHINE}/include ;; \
	${MACHINE_ARCH}) \
		path=${S}/${MACHINE_ARCH}/include ;; \
	esac ; \
	${ECHO} ${.TARGET} "->" $$path ; \
	ln -s $$path ${.TARGET}

# .depend needs include links so we remove them only together.
kernel-cleandepend:
	rm -f .depend ${_ILINKS}

links:
	egrep '#if' ${CFILES} | sed -f $S/conf/defines | \
	    sed -e 's/:.*//' -e 's/\.c/.o/' | sort -u > dontlink
	${MAKE} -V CFILES | tr -s ' ' '\12' | sed 's/\.c/.o/' | \
	    sort -u | comm -23 - dontlink | \
	    sed 's,../.*/\(.*.o\),rm -f \1;ln -s ../GENERIC/\1 \1,' > makelinks
	sh makelinks; rm -f dontlink

kernel-tags:
	@[ -f .depend ] || { echo "you must make depend first"; exit 1; }
	sh $S/conf/systags.sh

kernel-install:
	@if [ ! -f ${KERNEL_KO} ] ; then \
		echo "You must build a kernel first." ; \
		exit 1 ; \
	fi
.if exists(${DESTDIR}${KODIR})
	-thiskernel=`sysctl -n kern.bootfile` ; \
	if [ ! "`dirname "$$thiskernel"`" -ef ${DESTDIR}${KODIR} ] ; then \
		chflags -R noschg ${DESTDIR}${KODIR} ; \
		rm -rf ${DESTDIR}${KODIR} ; \
	else \
		if [ -d ${DESTDIR}${KODIR}.old ] ; then \
			chflags -R noschg ${DESTDIR}${KODIR}.old ; \
			rm -rf ${DESTDIR}${KODIR}.old ; \
		fi ; \
		mv ${DESTDIR}${KODIR} ${DESTDIR}${KODIR}.old ; \
		sysctl kern.bootfile=${DESTDIR}${KODIR}.old/"`basename "$$thiskernel"`" ; \
	fi
.endif
	mkdir -p ${DESTDIR}${KODIR}
	${INSTALL} -p -m 555 -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_KO} ${DESTDIR}${KODIR}
.if defined(DEBUG) && !defined(INSTALL_NODEBUG)
	${INSTALL} -p -m 555 -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_KO}.symbols ${DESTDIR}${KODIR}
.endif
.if defined(KERNEL_EXTRA_INSTALL)
	${INSTALL} -p -m 555 -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_EXTRA_INSTALL} ${DESTDIR}${KODIR}
.endif



kernel-reinstall:
	@-chflags -R noschg ${DESTDIR}${KODIR}
	${INSTALL} -p -m 555 -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_KO} ${DESTDIR}${KODIR}
.if defined(DEBUG) && !defined(INSTALL_NODEBUG)
	${INSTALL} -p -m 555 -o ${KMODOWN} -g ${KMODGRP} ${KERNEL_KO}.symbols ${DESTDIR}${KODIR}
.endif

config.o env.o hints.o vers.o vnode_if.o:
	${NORMAL_C}
	@[ -z "${CTFCONVERT}" -o -n "${NO_CTF}" ] || ${CTFCONVERT} ${CTFFLAGS} ${.TARGET}

config.ln env.ln hints.ln vers.ln vnode_if.ln:
	${NORMAL_LINT}

vers.c: $S/conf/newvers.sh $S/sys/param.h ${SYSTEM_DEP}
	MAKE=${MAKE} sh $S/conf/newvers.sh ${KERN_IDENT}

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

# XXX strictly, everything depends on Makefile because changes to ${PROF}
# only appear there, but we don't handle that.

.include "kern.mk"
