# kern.post.mk
#
# Unified Makefile for building kenrels.  This includes all the definitions
# that need to be included after all the % directives, except %RULES and
# things that act like they are part of %RULES
#
# Most make variables should not be defined in this file.  Instead, they
# should be defined in the kern.pre.mk so that port makefiles can
# override or augment them.
#
# $FreeBSD$
#

.PHONY:	all modules

depend: kernel-depend
clean:  kernel-clean
cleandepend:  kernel-cleandepend
clobber: kernel-clobber
tags:  kernel-tags
install: kernel-install
install.debug: kernel-install.debug
reinstall: kernel-reinstall
reinstall.debug: kernel-reinstall.debug

.if !defined(DEBUG)
FULLKERNEL=	${KERNEL_KO}
.else
FULLKERNEL=	${KERNEL_KO}.debug
${KERNEL_KO}: ${FULLKERNEL}
	${OBJCOPY} --strip-debug ${FULLKERNEL} ${KERNEL_KO}
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
${mfile:T:S/.m$/.h/}: ${mfile}
	perl5 $S/kern/makeobjops.pl -h ${mfile}
.endfor

kernel-clean:
	rm -f *.o *.so *.So *.ko *.s eddep errs \
	      ${FULLKERNEL} ${KERNEL_KO} linterrs makelinks \
	      setdef[01].c setdefs.h tags \
	      vers.c vnode_if.c vnode_if.h \
	      ${MFILES:T:S/.m$/.c/} ${MFILES:T:S/.m$/.h/} \
	      ${CLEAN}

kernel-clobber:
	find . -type f ! -name version -delete

locore.o: $S/$M/$M/locore.s assym.s
	${NORMAL_S}

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
	NM=${NM} OBJFORMAT=elf sh $S/kern/genassym.sh genassym.o > ${.TARGET}

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

_kernel-depend: assym.s vnode_if.h ${BEFORE_DEPEND} \
	    ${CFILES} ${SYSTEM_CFILES} ${GEN_CFILES} ${SFILES} \
	    ${SYSTEM_SFILES} ${MFILES:T:S/.m$/.h/}
	if [ -f .olddep ]; then mv .olddep .depend; fi
	rm -f .newdep
	#
	# The argument list can be very long, use make -V and xargs to
	# pass it to mkdep.
	${MAKE} -V CFILES -V SYSTEM_CFILES -V GEN_CFILES | xargs \
	    env MKDEP_CPP="${CC} -E" CC="${CC}" mkdep -a -f .newdep ${CFLAGS}
	${MAKE} -V SFILES -V SYSTEM_SFILES | xargs \
	    env MKDEP_CPP="${CC} -E" mkdep -a -f .newdep ${ASM_CFLAGS}
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

kernel-install kernel-install.debug:
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
	@if [ ! -f ${KERNEL_KO}${.TARGET:S/kernel-install//} ] ; then \
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
			sysctl -w kern.bootfile=${DESTDIR}${KODIR}.old/${KERNEL_KO} ; \
		fi; \
	fi
.endif
	mkdir -p ${DESTDIR}${KODIR}
	install -c -m 555 -o root -g wheel \
		${KERNEL_KO}${.TARGET:S/kernel-install//} ${DESTDIR}${KODIR}

kernel-reinstall kernel-reinstall.debug:
	@-chflags -R noschg ${DESTDIR}${KODIR}
	install -c -m 555 -o root -g wheel \
		${KERNEL_KO}${.TARGET:S/kernel-reinstall//} ${DESTDIR}${KODIR}

.if !defined(MODULES_WITH_WORLD) && !defined(NO_MODULES) && exists($S/modules)
all:	modules
depend: modules-depend
clean:  modules-clean
cleandepend:  modules-cleandepend
cleandir:  modules-cleandir
clobber:  modules-clobber
tags:  modules-tags
install: modules-install
install.debug: modules-install.debug
reinstall: modules-reinstall
reinstall.debug: modules-reinstall.debug
.endif

modules:
	@mkdir -p ${.OBJDIR}/modules
	cd $S/modules ; env ${MKMODULESENV} ${MAKE} obj ; \
	    env ${MKMODULESENV} ${MAKE} all

modules-depend:
	@mkdir -p ${.OBJDIR}/modules
	cd $S/modules ; env ${MKMODULESENV} ${MAKE} obj ; \
	    env ${MKMODULESENV} ${MAKE} depend

modules-clean:
	cd $S/modules ; env ${MKMODULESENV} ${MAKE} clean

modules-cleandepend:
	cd $S/modules ; env ${MKMODULESENV} ${MAKE} cleandepend

modules-clobber:	modules-clean
	rm -rf ${MKMODULESENV}

modules-cleandir:
	cd $S/modules ; env ${MKMODULESENV} ${MAKE} cleandir

modules-tags:
	cd $S/modules ; env ${MKMODULESENV} ${MAKE} tags

modules-install modules-reinstall:
	cd $S/modules ; env ${MKMODULESENV} ${MAKE} install

modules-install.debug modules-reinstall.debug:
	cd $S/modules ; env ${MKMODULESENV} ${MAKE} install.debug

config.o:
	${NORMAL_C}

vers.c: $S/conf/newvers.sh $S/sys/param.h ${SYSTEM_DEP}
	sh $S/conf/newvers.sh ${KERN_IDENT} ${IDENT}

# XXX strictly, everything depends on Makefile because changes to ${PROF}
# only appear there, but we don't handle that.
vers.o:
	${NORMAL_C}

hints.o:	hints.c
	${NORMAL_C}

env.o:	env.c
	${NORMAL_C}

vnode_if.c: $S/kern/vnode_if.pl $S/kern/vnode_if.src
	perl5 $S/kern/vnode_if.pl -c $S/kern/vnode_if.src

vnode_if.h: $S/kern/vnode_if.pl $S/kern/vnode_if.src
	perl5 $S/kern/vnode_if.pl -h $S/kern/vnode_if.src

vnode_if.o:
	${NORMAL_C}

.include <bsd.kern.mk>
