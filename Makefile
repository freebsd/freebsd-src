#
#	$Id: Makefile,v 1.95 1996/08/07 13:25:54 peter Exp $
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
#	-DNOGAMES do not go into games subdir

#
# The intended user-driven targets are:
# world       - rebuild *everything*, including glue to help do upgrades.
# reinstall   - use an existing (eg: NFS mounted) build to do an update.
# update      - convenient way to update your source tree (eg: sup/cvs)
# most        - build user commands, no libraries or include files
# installmost - install user commands, no libraries or include files
# all         - run through SUBDIR and build everything.  This is an implicit
#               rule, not particularly useful for everybody.  Use 'world'.


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
.if exists(games) && !defined(NOGAMES)
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

# Handle -DNOOBJDIR, -DNOCLEAN and -DNOCLEANDIR
.if defined(NOOBJDIR)
OBJDIR=
.else
OBJDIR=		obj
.endif

.if defined(NOCLEAN)
CLEANDIR=
.else
.if defined(NOCLEANDIR)
CLEANDIR=	clean
.else
CLEANDIR=	cleandir
.endif
.endif

#
# While building tools for bootstrapping, we dont need to waste time on
# profiled libraries or man pages.  This speeds things up somewhat.
#
MK_FLAGS=	-DNOMAN -DNOPROFILE

#
# world
#
# Attempt to rebuild and reinstall *everything*, with reasonable chance of
# success, regardless of how old your existing system is.
#
# >> Beware, it overwrites the local build environment! <<
#
world:
.if target(pre-world)
	@echo "--------------------------------------------------------------"
	@echo " Making 'pre-world' target"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} pre-world
	@echo
.endif
	@echo "--------------------------------------------------------------"
	@echo " Making hierarchy"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} hierarchy
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding /usr/share/mk"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} mk
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Cleaning up the source tree"
	@echo "--------------------------------------------------------------"
.if defined(NOCLEAN)
	@echo "Not cleaning anything! I sure hope you know what you are doing!"
.else
	cd ${.CURDIR} && ${MAKE} ${CLEANDIR}
.endif
	@echo
.if !defined(NOOBJDIR)
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding the obj tree"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} obj
	@echo
.endif
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding bootstrap tools"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} bootstrap
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding tools necessary to build the include files"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} include-tools
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding /usr/include"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} includes
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding tools needed to build the libraries"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} lib-tools
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding /usr/lib"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} libraries
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding C compiler, make, symorder, sgmlfmt and zic(8)"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} build-tools
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding.. The whole thing"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} depend && ${MAKE} all install
	cd ${.CURDIR}/share/man && ${MAKE} makedb
.if target(post-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Making 'post-world' target"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} post-world
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@echo "make world completed on `date`"

#
# reinstall
#
# If you have a build server, you can NFS mount the source and obj directories
# and do a 'make reinstall' on the *client* to install new binaries from the
# most recent server build.
#
reinstall:
	@echo "--------------------------------------------------------------"
	@echo " Making hierarchy"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} hierarchy
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding /usr/share/mk"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} mk
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding /usr/include"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR} && ${MAKE} includes
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Reinstalling..  The whole thing"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR} && ${MAKE} install
	cd ${.CURDIR}/share/man && ${MAKE} makedb


#
# update
#
# Update the source tree, by running sup and/or running cvs to update to the
# latest copy.
#
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
	cd ${.CURDIR} &&  cvs -q update -P -d
.endif


#
# most
#
# Build most of the user binaries on the existing system libs and includes.
#
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

#
# installmost
#
# Install the binaries built by the 'most' target.  This does not include
# libraries or include files.
#
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

#
# ------------------------------------------------------------------------
#
# From here onwards are utility targets used by the 'make world' and
# related targets.  If your 'world' breaks, you may like to try to fix
# the problem and manually run the following targets to attempt to
# complete the build.  Beware, this is *not* guaranteed to work, you
# need to have a pretty good grip on the current state of the system
# to attempt to manually finish it.  If in doubt, 'make world' again.
#

#
# heirarchy - ensure that all the needed directories are present
#
hierarchy:
	cd ${.CURDIR}/etc &&		${MAKE} distrib-dirs

#
# mk - update the /usr/share/mk makefiles.
#
mk:
	cd ${.CURDIR}/share/mk &&	${MAKE} install

#
# bootstrap - [re]build tools needed to run the actual build, this includes
# tools needed by 'make depend', as some tools are needed to generate source
# for the dependency information to be gathered from.
#
bootstrap:
	cd ${.CURDIR}/usr.bin/make && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/xinstall && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/lex && ${MAKE} bootstrap && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} -DNOLIB all install ${CLEANDIR} ${OBJDIR}

#
# include-tools - generally the same as 'bootstrap', except that it's for
# things that are specifically needed to generate include files.
#
# XXX should be merged with bootstrap, it's not worth keeeping them seperate
#
include-tools:
	cd ${.CURDIR}/usr.bin/rpcgen && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} depend all install ${CLEANDIR} ${OBJDIR}

#
# includes - possibly generate and install the include files.
#
includes:
.if defined(CLOBBER)
	rm -rf ${DESTDIR}/usr/include/*
	mtree -deU -f ${.CURDIR}/etc/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
.endif
	cd ${.CURDIR}/include &&		${MAKE} install
	cd ${.CURDIR}/gnu/include &&		${MAKE} install
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
	cd ${.CURDIR}/lib/libftpio &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libmd &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libmytinfo &&         ${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libncurses &&		${MAKE} beforeinstall
.if !defined(WANT_CSRG_LIBM)
	cd ${.CURDIR}/lib/msun &&		${MAKE} beforeinstall
.endif
	cd ${.CURDIR}/lib/libpcap &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/librpcsvc &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libskey &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libtcl &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libtermcap &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libcom_err &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libss &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libscsi &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libutil &&		${MAKE} beforeinstall

#
# lib-tools - build tools to compile and install the libraries.
#
lib-tools:
	cd ${.CURDIR}/usr.bin/tsort && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/usr.bin/ld && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/ar && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/ranlib && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/nm && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/lex/lib && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/compile_et && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR} && \
		rm -f /usr/sbin/compile_et
	cd ${.CURDIR}/usr.bin/mk_cmds && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}

#
# libraries - build and install the libraries
#
libraries:
.if exists(lib/libcompat)
	cd ${.CURDIR}/lib/libcompat && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(lib/libncurses)
	cd ${.CURDIR}/lib/libncurses && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(lib/libtermcap)
	cd ${.CURDIR}/lib/libtermcap && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(lib)
	cd ${.CURDIR}/lib/csu/i386 && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(gnu)
	cd ${.CURDIR}/gnu/lib && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/usr.bin/cc/libgcc && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(secure) && !defined(NOCRYPT) && !defined(NOSECURE)
	cd ${.CURDIR}/secure/lib && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(lib)
	cd ${.CURDIR}/lib && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(usr.bin/lex/lib)
	cd ${.CURDIR}/usr.bin/lex/lib && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(eBones) && !defined(NOCRYPT) && defined(MAKE_EBONES)
	cd ${.CURDIR}/eBones/lib && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(usr.sbin/pcvt/keycap)
	cd ${.CURDIR}/usr.sbin/pcvt/keycap && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
.endif

#
# build-tools - build and install any other tools needed to complete the
# compile and install.
#
build-tools:
	cd ${.CURDIR}/gnu/usr.bin/cc && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/symorder && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/sgmlfmt && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR} 
	cd ${.CURDIR}/share/sgml && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR} 
	cd ${.CURDIR}/usr.sbin/zic && ${MAKE} depend && \
		${MAKE} ${MK_FLAGS} all install ${CLEANDIR} ${OBJDIR}

.include <bsd.subdir.mk>
