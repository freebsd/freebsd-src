#
#	$Id: Makefile,v 1.81 1996/06/19 21:19:56 nate Exp $
#
# Make command line options:
#	-DCLOBBER will remove /usr/include
#	-DMAKE_LOCAL to add ./local to the SUBDIR list
#	-DMAKE_PORTS to add ./ports to the SUBDIR list
#	-DMAKE_EBONES to build eBones (KerberosIV)
#
#	-DNOCLEANDIR run ${MAKE} clean, instead of ${MAKE} cleandir
#	-DNOCLEAN do not clean at all
#	-DNOCRYPT will prevent building of crypt versions
#	-DNOLKM do not build loadable kernel modules
#	-DNOOBJDIR do not run ``${MAKE} obj''
#	-DNOPROFILE do not build profiled libraries
#	-DNOSECURE do not go into secure subdir

# Put initial settings here.
SUBDIR=

# We must do include and lib first so that the perl *.ph generation
# works correctly as it uses the header files installed by this.
.if exists(include)
SUBDIR+= include
.endif
.if exists(lib)
SUBDIR+= lib
.endif

.if exists(bin)
SUBDIR+= bin
.endif
.if exists(contrib)
SUBDIR+= contrib
.endif
.if exists(games)
SUBDIR+= games
.endif
.if exists(gnu)
SUBDIR+= gnu
.endif
.if exists(eBones) && !defined(NOCRYPT) && defined(MAKE_EBONES)
SUBDIR+= eBones
.endif
.if exists(libexec)
SUBDIR+= libexec
.endif
.if exists(sbin)
SUBDIR+= sbin
.endif
.if exists(share)
SUBDIR+= share
.endif
.if exists(sys)
SUBDIR+= sys
.endif
.if exists(usr.bin)
SUBDIR+= usr.bin
.endif
.if exists(usr.sbin)
SUBDIR+= usr.sbin
.endif
.if exists(secure) && !defined(NOCRYPT) && !defined(NOSECURE)
SUBDIR+= secure
.endif
.if exists(lkm) && !defined(NOLKM)
SUBDIR+= lkm
.endif

# etc must be last for "distribute" to work
.if exists(etc) && make(distribute)
SUBDIR+= etc
.endif

# These are last, since it is nice to at least get the base system
# rebuilt before you do them.
.if defined(MAKE_LOCAL) & exists(local) & exists(local/Makefile)
SUBDIR+= local
.endif
.if defined(MAKE_PORTS) & exists(ports) & exists(ports/Makefile)
SUBDIR+= ports
.endif

# Handle the -DNOOBJDIR and -DNOCLEANDIR
.if defined(NOOBJDIR)
OBJDIR=
.else
OBJDIR=		obj
.endif

.if defined(NOCLEAN)
CLEANDIR=
WORLD_CLEANDIST=obj
.else
WORLD_CLEANDIST=cleandist
.if defined(NOCLEANDIR)
CLEANDIR=	clean
.else
CLEANDIR=	cleandir
.endif
.endif

MK_FLAGS=	-DNOMAN -DNOPROFILE

world:	hierarchy mk $(WORLD_CLEANDIST) bootstrap include-tools includes lib-tools libraries build-tools
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} The whole thing"
	@echo "--------------------------------------------------------------"
	@echo
	${MAKE} depend all install
	cd ${.CURDIR}/share/man &&		${MAKE} makedb
	@echo "make world completed on `date`"

bootstrap:
	cd ${.CURDIR}/usr.bin/xlint && ${MAKE} ${MK_FLAGS} lint1 lint2 xlint
	cd ${.CURDIR}/usr.bin/xlint/lint1 && ${MAKE} ${MK_FLAGS} install
	cd ${.CURDIR}/usr.bin/xlint/lint2 && ${MAKE} ${MK_FLAGS} install
	cd ${.CURDIR}/usr.bin/xlint/xlint && ${MAKE} ${MK_FLAGS} install
	cd ${.CURDIR}/usr.bin/lex && ${MAKE} ${MK_FLAGS} bootstrap && \
		${MAKE} ${MK_FLAGS} all install

reinstall:	hierarchy mk includes
	@echo "--------------------------------------------------------------"
	@echo " Reinstall ${DESTDIR} The whole thing"
	@echo "--------------------------------------------------------------"
	@echo
	${MAKE} install
	cd ${.CURDIR}/share/man &&		${MAKE} makedb

hierarchy:
	@echo "--------------------------------------------------------------"
	@echo " Making hierarchy"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}/etc &&		${MAKE} distrib-dirs

update:
.if defined(SUP_UPDATE)
	@echo "--------------------------------------------------------------"
	@echo "Running sup"
	@echo "--------------------------------------------------------------"
	@sup -v ${SUPFILE}
.if defined(SUPFILE1)
	@sup -v ${SUPFILE1}
.endif
.if defined(SUPFILE2)
	@sup -v ${SUPFILE2}
.endif
.endif
.if defined(CVS_UPDATE)
	@echo "--------------------------------------------------------------"
	@echo "Updating /usr/src from cvs repository" ${CVSROOT}
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} &&  cvs update -P -d
.endif

cleandist:
.if !defined(NOCLEANDIR)
	@echo "--------------------------------------------------------------"
	@echo " Cleaning up the source tree, and rebuilding the obj tree"
	@echo "--------------------------------------------------------------"
	@echo
	here=`pwd`; dest=/usr/obj`echo $$here | sed 's,^/usr/src,,'`; \
	if test -d /usr/obj -a ! -d $$dest; then \
		mkdir -p $$dest; \
	else \
		true; \
	fi; \
	cd $$dest && rm -rf ${SUBDIR}
	find . -name obj | xargs rm -rf
.if defined(MAKE_LOCAL) & exists(local) & exists(local/Makefile)
	# The cd is done as local may well be a symbolic link
	-cd local && find . -name obj | xargs rm -rf
.endif
.if defined(MAKE_PORTS) & exists(ports) & exists(ports/Makefile)
	# The cd is done as local may well be a symbolic link
	-cd ports && find . -name obj | xargs rm -rf
.endif
	${MAKE} cleandir
	${MAKE} obj
.endif

installmost:
	@echo "--------------------------------------------------------------"
	@echo " Installing programs only"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}/bin	&&	${MAKE} ${.MAKEFLAGS} install
	cd ${.CURDIR}/sbin	&&	${MAKE} ${.MAKEFLAGS} install
	cd ${.CURDIR}/libexec	&&	${MAKE} ${.MAKEFLAGS} install
	cd ${.CURDIR}/usr.bin	&&	${MAKE} ${.MAKEFLAGS} install
	cd ${.CURDIR}/usr.sbin	&&	${MAKE} ${.MAKEFLAGS} install
	cd ${.CURDIR}/gnu/libexec &&	${MAKE} ${.MAKEFLAGS} install
	cd ${.CURDIR}/gnu/usr.bin &&	${MAKE} ${.MAKEFLAGS} install
	cd ${.CURDIR}/gnu/usr.sbin &&	${MAKE} ${.MAKEFLAGS} install
#.if defined(MAKE_EBONES) && !defined(NOCRYPT)
#	cd ${.CURDIR}/eBones	&&	${MAKE} ${.MAKEFLAGS} installmost
#.endif
#.if !defined(NOSECURE) && !defined(NOCRYPT)
#	cd ${.CURDIR}/secure	&&	${MAKE} ${.MAKEFLAGS} installmost
#.endif

most:
	@echo "--------------------------------------------------------------"
	@echo " Building programs only"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}/bin	&&	${MAKE} ${.MAKEFLAGS} all
	cd ${.CURDIR}/sbin	&&	${MAKE} ${.MAKEFLAGS} all
	cd ${.CURDIR}/libexec	&&	${MAKE} ${.MAKEFLAGS} all
	cd ${.CURDIR}/usr.bin	&&	${MAKE} ${.MAKEFLAGS} all
	cd ${.CURDIR}/usr.sbin	&&	${MAKE} ${.MAKEFLAGS} all
	cd ${.CURDIR}/gnu/libexec &&	${MAKE} ${.MAKEFLAGS} all
	cd ${.CURDIR}/gnu/usr.bin &&	${MAKE} ${.MAKEFLAGS} all
	cd ${.CURDIR}/gnu/usr.sbin &&	${MAKE} ${.MAKEFLAGS} all
#.if defined(MAKE_EBONES) && !defined(NOCRYPT)
#	cd ${.CURDIR}/eBones	&&	${MAKE} ${.MAKEFLAGS} most
#.endif
#.if !defined(NOSECURE) && !defined(NOCRYPT)
#	cd ${.CURDIR}/secure	&&	${MAKE} ${.MAKEFLAGS} most
#.endif

mk:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/share/mk"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}/share/mk &&		${MAKE} install

includes:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/include"
	@echo "--------------------------------------------------------------"
	@echo
.if defined(CLOBBER)
	rm -rf ${DESTDIR}/usr/include/*
	mtree -deU -f ${.CURDIR}/etc/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
.endif
	cd ${.CURDIR}/include &&		${MAKE} install
	cd ${.CURDIR}/gnu/include &&		${MAKE}	install
	cd ${.CURDIR}/gnu/lib/libreadline &&	${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libregex &&	${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libg++ &&         ${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libdialog &&      ${MAKE} beforeinstall
.if exists(eBones) && !defined(NOCRYPT) && defined(MAKE_EBONES)
	cd ${.CURDIR}/eBones/include &&		${MAKE} beforeinstall
	cd ${.CURDIR}/eBones/lib/libkrb &&	${MAKE} beforeinstall
	cd ${.CURDIR}/eBones/lib/libkadm &&	${MAKE} beforeinstall
.endif
	cd ${.CURDIR}/lib/libc &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libcurses &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libedit &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libmd &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libmytinfo &&		${MAKE}	beforeinstall
	cd ${.CURDIR}/lib/libncurses &&		${MAKE}	beforeinstall
.if !defined(WANT_CSRG_LIBM)
	cd ${.CURDIR}/lib/msun &&		${MAKE} beforeinstall
.endif
	cd ${.CURDIR}/lib/libpcap &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/librpcsvc &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libskey &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libtermcap &&		${MAKE}	beforeinstall
	cd ${.CURDIR}/lib/libcom_err &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libss &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libscsi &&		${MAKE}	beforeinstall
	cd ${.CURDIR}/lib/libutil &&		${MAKE}	beforeinstall

lib-tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding tools needed to build the libraries"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR}/usr.bin/xinstall && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/usr.bin/ld && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/ar && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/ranlib && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/nm && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/lex/lib && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/compile_et && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR} && \
		rm -f /usr/sbin/compile_et
	cd ${.CURDIR}/usr.bin/mk_cmds && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}

libraries:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/lib"
	@echo "--------------------------------------------------------------"
	@echo
.if exists(lib/libcompat)
	cd ${.CURDIR}/lib/libcompat && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(lib/libncurses)
	cd ${.CURDIR}/lib/libncurses && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(lib/libtermcap)
	cd ${.CURDIR}/lib/libtermcap && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(gnu)
	cd ${.CURDIR}/gnu/lib && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/usr.bin/cc/libgcc && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(secure) && !defined(NOCRYPT) && !defined(NOSECURE)
	cd ${.CURDIR}/secure/lib && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(lib)
	cd ${.CURDIR}/lib/csu/i386 && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/lib && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(usr.sbin/lex/lib)
	cd ${.CURDIR}/usr.bin/lex/lib && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(eBones) && !defined(NOCRYPT) && defined(MAKE_EBONES)
	cd ${.CURDIR}/eBones/lib && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(usr.sbin/pcvt/keycap)
	cd ${.CURDIR}/usr.sbin/pcvt/keycap && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
.endif

include-tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuild tools necessary to build the include files"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR}/usr.bin/xinstall && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/rpcgen && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}

build-tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} C compiler, make, symorder, sgmlfmt and zic(8)"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR}/gnu/usr.bin/cc && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/make && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/symorder && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/sgmlfmt && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR} 
	cd ${.CURDIR}/share/sgml && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR} 
	cd ${.CURDIR}/usr.sbin/zic && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}

.include <bsd.subdir.mk>
