# Part of unified Makefile for building kernels.  This includes all
# the definitions that need to be included after all the % directives,
# except %RULES and things that act like they are part of %RULES
#
# Most make variables should not be defined in this file.  Instead, they
# should be defined in the kern.pre.mk so that port makefiles can
# override or augment them.
#
# $FreeBSD$
#

# XXX why are only some phony targets marked phony?
.PHONY:	all modules

clean:  kernel-clean
cleandepend:  kernel-cleandepend
cleandir:
clobber: kernel-clobber
depend: kernel-depend
install: kernel-install
reinstall: kernel-reinstall
tags:  kernel-tags

.if !defined(DEBUG)
FULLKERNEL=	${KERNEL_KO}
.else
FULLKERNEL=	${KERNEL_KO}.debug
${KERNEL_KO}: ${FULLKERNEL}
	${OBJCOPY} --strip-debug ${FULLKERNEL} ${KERNEL_KO}
install.debug reinstall.debug:
	cd ${.CURDIR}; ${MAKE} -DINSTALL_DEBUG ${.TARGET:R}
.endif

${FULLKERNEL}: ${SYSTEM_DEP} vers.o
	@rm -f ${.TARGET}
	@echo linking ${.TARGET}
	${SYSTEM_LD}
	${SYSTEM_LD_TAIL}

.if !exists(.depend)
${SYSTEM_OBJS}: vnode_if.h ${BEFORE_DEPEND:M*.h} ${MFILES:T:S/.m$/.h/}
.endif

.for mfile in ${MFILES}
# XXX the low quality .m.o rules gnerated by config are normally used
# instead of the .m.c rules here.
${mfile:T:S/.m$/.c/}: ${mfile}
	${AWK} -f $S/tools/makeobjops.awk ${mfile} -c
${mfile:T:S/.m$/.h/}: ${mfile}
	${AWK} -f $S/tools/makeobjops.awk ${mfile} -h
.endfor

miidevs.h: $S/tools/devlist2h.awk $S/dev/mii/miidevs
	${AWK} -f $S/tools/devlist2h.awk $S/dev/mii/miidevs

.if !exists(.depend)
acphy.o amphy.o bmtphy.o brgphy.o dcphy.o e1000phy.o exphy.o if_bge.o if_tx.o \
inphy.o lxtphy.o nsgphy.o nsphy.o pnaphy.o pnphy.o qsphy.o rlphy.o tdkphy.o \
tlphy.o xmphy.o: miidevs.h
.endif

kernel-clean:
	rm -f *.o *.so *.So *.ko *.s eddep errs \
	      ${FULLKERNEL} ${KERNEL_KO} linterrs makelinks tags \
	      miidevs.h vers.c vnode_if.c vnode_if.h \
	      ${MFILES:T:S/.m$/.c/} ${MFILES:T:S/.m$/.h/} \
	      ${CLEAN}

kernel-clobber:
	find . -type f ! -name version -delete

lint: ${CFILES}
	@${LINT} ${LINTKERNFLAGS} ${CFLAGS:M-[DILU]*} ${.ALLSRC}

# This is a hack.  BFD "optimizes" away dynamic mode if there are no
# dynamic references.  We could probably do a '-Bforcedynamic' mode like
# in the a.out ld.  For now, this works.
HACK_EXTRA_FLAGS?= -shared
hack.So: Makefile
	touch hack.c
	${CC} ${FMT} ${HACK_EXTRA_FLAGS} -nostdlib hack.c -o hack.So
	rm -f hack.c

# this rule stops ./assym.s in .depend from causing problems
./assym.s: assym.s

assym.s: $S/kern/genassym.sh genassym.o
	NM=${NM} sh $S/kern/genassym.sh genassym.o > ${.TARGET}

# XXX used to force -elf after CFLAGS to work around breakage of cc -aout
# (genassym.sh makes some assumptions and cc stopped satisfying them).
genassym.o: $S/$M/$M/genassym.c
	${CC} -c ${CFLAGS:N-fno-common} $S/$M/$M/genassym.c

${SYSTEM_OBJS} genassym.o vers.o: opt_global.h

kernel-depend:
.if defined(EXTRA_KERNELDEP)
	${EXTRA_KERNELDEP}
.endif
	rm -f .olddep
	if [ -f .depend ]; then mv .depend .olddep; fi
	${MAKE} _kernel-depend

# XXX this belongs elsewhere (inside GEN_CFILES if possible).
GEN_M_CFILES=	${MFILES:T:S/.m$/.c/}

# The argument list can be very long, so use make -V and xargs to
# pass it to mkdep.
_kernel-depend: assym.s vnode_if.h miidevs.h ${BEFORE_DEPEND} \
	    ${CFILES} ${SYSTEM_CFILES} ${GEN_CFILES} ${GEN_M_CFILES} \
	    ${SFILES} ${SYSTEM_SFILES} ${MFILES:T:S/.m$/.h/}
	if [ -f .olddep ]; then mv .olddep .depend; fi
	rm -f .newdep
	${MAKE} -V CFILES -V SYSTEM_CFILES -V GEN_CFILES -V GEN_M_CFILES | \
	    MKDEP_CPP="${CC} -E" CC="${CC}" xargs mkdep -a -f .newdep ${CFLAGS}
	${MAKE} -V SFILES -V SYSTEM_SFILES | \
	    MKDEP_CPP="${CC} -E" xargs mkdep -a -f .newdep ${ASM_CFLAGS}
	rm -f .depend
	mv .newdep .depend

kernel-cleandepend:
	rm -f .depend

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
	rm -f tags1
	sed -e 's,      ../,    ,' tags > tags1

kernel-install:
.if exists(${DESTDIR}/boot)
	@if [ ! -f ${DESTDIR}/boot/device.hints ] ; then \
		echo "You must set up a ${DESTDIR}/boot/device.hints file first." ; \
		exit 1 ; \
	fi
	@if [ x"`grep device.hints ${DESTDIR}/boot/defaults/loader.conf ${DESTDIR}/boot/loader.conf`" = "x" ]; then \
		echo "You must activate /boot/device.hints in loader.conf." ; \
		exit 1 ; \
	fi
.endif
	@if [ ! -f ${FULLKERNEL} ] ; then \
		echo "You must build a kernel first." ; \
		exit 1 ; \
	fi
.if exists(${DESTDIR}${KODIR})
	-thiskernel=`sysctl -n kern.bootfile` ; \
	if [ "$$thiskernel" = ${DESTDIR}${KODIR}.old/${KERNEL_KO} ] ; then \
		chflags -R noschg ${DESTDIR}${KODIR} ; \
		rm -rf ${DESTDIR}${KODIR} ; \
	else \
		if [ -d ${DESTDIR}${KODIR}.old ] ; then \
			chflags -R noschg ${DESTDIR}${KODIR}.old ; \
			rm -rf ${DESTDIR}${KODIR}.old ; \
		fi ; \
		mv ${DESTDIR}${KODIR} ${DESTDIR}${KODIR}.old ; \
		if [ "$$thiskernel" = ${DESTDIR}${KODIR}/${KERNEL_KO} ] ; then \
			sysctl kern.bootfile=${DESTDIR}${KODIR}.old/${KERNEL_KO} ; \
		fi; \
	fi
.endif
	mkdir -p ${DESTDIR}${KODIR}
.if defined(DEBUG) && defined(INSTALL_DEBUG)
	${INSTALL} -p -m 555 -o root -g wheel ${FULLKERNEL} ${DESTDIR}${KODIR}
.else
	${INSTALL} -p -m 555 -o root -g wheel ${KERNEL_KO} ${DESTDIR}${KODIR}
.endif

kernel-reinstall:
	@-chflags -R noschg ${DESTDIR}${KODIR}
.if defined(DEBUG) && defined(INSTALL_DEBUG)
	${INSTALL} -p -m 555 -o root -g wheel ${FULLKERNEL} ${DESTDIR}${KODIR}
.else
	${INSTALL} -p -m 555 -o root -g wheel ${KERNEL_KO} ${DESTDIR}${KODIR}
.endif

.if !defined(MODULES_WITH_WORLD) && !defined(NO_MODULES) && exists($S/modules)
all:	modules
clean:  modules-clean
cleandepend:  modules-cleandepend
cleandir:  modules-cleandir
clobber:  modules-clobber
depend: modules-depend
install: modules-install
.ORDER: kernel-install modules-install
reinstall: modules-reinstall
tags:  modules-tags
.endif

modules:
	cd $S/modules ; ${MKMODULESENV} ${MAKE} all

modules-clean:
	cd $S/modules ; ${MKMODULESENV} ${MAKE} clean

modules-cleandepend:
	cd $S/modules ; ${MKMODULESENV} ${MAKE} cleandepend

modules-cleandir:
	cd $S/modules ; ${MKMODULESENV} ${MAKE} cleandir

modules-clobber:	modules-clean
	rm -rf ${MKMODULESENV}

modules-depend:
	cd $S/modules ; ${MKMODULESENV} ${MAKE} depend

modules-obj:
	@mkdir -p ${.OBJDIR}/modules
	cd $S/modules ; ${MKMODULESENV} ${MAKE} obj

.if !defined(NO_MODULES_OBJ)
modules modules-depend: modules-obj
.endif

modules-tags:
	cd $S/modules ; ${MKMODULESENV} ${MAKE} tags

modules-install modules-reinstall:
	cd $S/modules ; ${MKMODULESENV} ${MAKE} install

config.o:
	${NORMAL_C}

env.o:	env.c
	${NORMAL_C}

hints.o:	hints.c
	${NORMAL_C}

vers.c: $S/conf/newvers.sh $S/sys/param.h ${SYSTEM_DEP}
	sh $S/conf/newvers.sh ${KERN_IDENT}

# XXX strictly, everything depends on Makefile because changes to ${PROF}
# only appear there, but we don't handle that.
vers.o:
	${NORMAL_C}

vnode_if.c: $S/tools/vnode_if.awk $S/kern/vnode_if.src
	${AWK} -f $S/tools/vnode_if.awk $S/kern/vnode_if.src -c

vnode_if.h: $S/tools/vnode_if.awk $S/kern/vnode_if.src
	${AWK} -f $S/tools/vnode_if.awk $S/kern/vnode_if.src -h

vnode_if.o:
	${NORMAL_C}

.include <bsd.kern.mk>
