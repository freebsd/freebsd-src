#
#	$Id: Makefile,v 1.197 1998/06/06 00:56:23 bde Exp $
#
# While porting to the another architecture include the bootstrap instead
# of the normal build.
#
.if exists(${.CURDIR}/Makefile.${MACHINE}) && defined(BOOTSTRAP_WORLD)
.include "${.CURDIR}/Makefile.${MACHINE}"
.else
#
# Make command line options:
#	-DCLOBBER will remove /usr/include
#	-DMAKE_KERBEROS4 to build KerberosIV
#	-DALLLANG to build documentation for all languages
#	  (where available -- see share/doc/Makefile)
#
#	-DNOCLEANDIR run ${MAKE} clean, instead of ${MAKE} cleandir
#	-DNOCLEAN do not clean at all
#	-DNOTOOLS do not rebuild any tools first
#	-DNOCRYPT will prevent building of crypt versions
#	-DNOLKM do not build loadable kernel modules
#	-DNOOBJDIR do not run ``${MAKE} obj''
#	-DNOPROFILE do not build profiled libraries
#	-DNOSECURE do not go into secure subdir
#	-DNOGAMES do not go into games subdir
#	-DNOSHARE do not go into share subdir
#	-DNOINFO do not make or install info files
#	-DNOLIBC_R do not build libc_r.
#	LOCAL_DIRS="list of dirs" to add additional dirs to the SUBDIR list

#
# The intended user-driven targets are:
# buildworld  - rebuild *everything*, including glue to help do upgrades
# installworld- install everything built by "buildworld"
# world       - buildworld + installworld
# update      - convenient way to update your source tree (eg: sup/cvs)
# most        - build user commands, no libraries or include files
# installmost - install user commands, no libraries or include files
#
# Standard targets (not defined here) are documented in the makefiles in
# /usr/share/mk.  These include:
#		obj depend all install clean cleandepend cleanobj

.if (!make(world)) && (!make(buildworld)) && (!make(installworld))
.MAKEFLAGS:=	-m ${.CURDIR}/share/mk ${.MAKEFLAGS}
.endif

# Put initial settings here.
SUBDIR=

# We must do share/info early so that installation of info `dir'
# entries works correctly.  Do it first since it is less likely to
# grow dependencies on include and lib than vice versa.
.if exists(share/info)
SUBDIR+= share/info
.endif

# We must do include and lib early so that the perl *.ph generation
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
.if exists(kerberosIV) && exists(crypto) && !defined(NOCRYPT) && \
    defined(MAKE_KERBEROS4)
SUBDIR+= kerberosIV
.endif
.if exists(libexec)
SUBDIR+= libexec
.endif
.if exists(sbin)
SUBDIR+= sbin
.endif
.if exists(share) && !defined(NOSHARE)
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
.if defined(LOCAL_DIRS)
.for _DIR in ${LOCAL_DIRS}
.if exists(${_DIR}) & exists(${_DIR}/Makefile)
SUBDIR+= ${_DIR}
.endif
.endfor
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
CLEANDIR=	clean cleandepend
.else
CLEANDIR=	cleandir
.endif
.endif

.if !defined(NOCLEAN) && ${.MAKEFLAGS:M-j} == ""
_NODEPEND=	true
.endif
.if defined(_NODEPEND)
_DEPEND=	cleandepend
.else
_DEPEND=	depend
.endif

SUP?=		cvsup
SUPFLAGS?=	-g -L 2 -P -

#
# While building tools for bootstrapping, we don't need to waste time on
# shared or profiled libraries, shared linkage, or documentation, except
# when the tools won't get cleaned we must use the defaults for shared
# libraries and shared linkage (and this doesn't waste time).
# XXX actually, we do need to waste time building shared libraries.
#
.if defined(NOCLEAN)
MK_FLAGS=	-DNOINFO -DNOMAN         -DNOPROFILE
.else
MK_FLAGS=	-DNOINFO -DNOMAN -DNOPIC -DNOPROFILE -DNOSHARED
.endif

#
# world
#
# Attempt to rebuild and reinstall *everything*, with reasonable chance of
# success, regardless of how old your existing system is.
#
# >> Beware, it overwrites the local build environment! <<
#
world:
	@echo "--------------------------------------------------------------"
	@echo "make world started on `LC_TIME=C date`"
	@echo "--------------------------------------------------------------"
.if target(pre-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Making 'pre-world' target"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${MAKE} pre-world
.endif
	cd ${.CURDIR}; ${MAKE} buildworld
	cd ${.CURDIR}; ${MAKE} -B installworld
.if target(post-world)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Making 'post-world' target"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${MAKE} post-world
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@echo "make world completed on `LC_TIME=C date`"
	@echo "--------------------------------------------------------------"

.if defined(MAKEOBJDIRPREFIX)
WORLDTMP=	${MAKEOBJDIRPREFIX}${.CURDIR}/tmp
.else
WORLDTMP=	/usr/obj${.CURDIR}/tmp
.endif
STRICTTMPPATH=	${WORLDTMP}/sbin:${WORLDTMP}/usr/sbin:${WORLDTMP}/bin:${WORLDTMP}/usr/bin
TMPPATH=	${STRICTTMPPATH}:${PATH}

# XXX COMPILER_PATH is needed for finding cc1, ld and as
# XXX GCC_EXEC_PREFIX is for *crt.o.  It is probably unnecessary now
#	that LIBRARY_PATH is set.  We still can't use -nostdlib, since gcc
#	wouldn't link *crt.o or libgcc if it were used.
# XXX LD_LIBRARY_PATH is for ld.so.  It is also used by ld, although we don't
#	want that - all compile-time library paths should be resolved by gcc.
#	It fails for set[ug]id executables (are any used?).
COMPILER_ENV=	BISON_SIMPLE=${WORLDTMP}/usr/share/misc/bison.simple \
		COMPILER_PATH=${WORLDTMP}/usr/libexec:${WORLDTMP}/usr/bin \
		GCC_EXEC_PREFIX=${WORLDTMP}${SHLIBDIR}:${WORLDTMP}/usr/lib/ \
		LD_LIBRARY_PATH=${WORLDTMP}${SHLIBDIR} \
		LIBRARY_PATH=${WORLDTMP}${SHLIBDIR}:${WORLDTMP}/usr/lib

BMAKEENV=	PATH=${TMPPATH} ${COMPILER_ENV} NOEXTRADEPEND=t \
		OBJFORMAT_PATH=${WORLDTMP}/usr/libexec:/usr/libexec
XMAKEENV=	PATH=${STRICTTMPPATH} ${COMPILER_ENV} \
		OBJFORMAT_PATH=${WORLDTMP}/usr/libexec \
		CFLAGS="-nostdinc ${CFLAGS}"	# XXX -nostdlib

# used to compile and install 'make' in temporary build tree
MAKETMP=	${WORLDTMP}/make
IBMAKE=	${BMAKEENV} MAKEOBJDIR=${MAKETMP} ${MAKE} DESTDIR=${WORLDTMP}
# bootstrap make
BMAKE=	${BMAKEENV} ${WORLDTMP}/usr/bin/make DESTDIR=${WORLDTMP}
# cross make used for compilation
XMAKE=	${XMAKEENV} ${WORLDTMP}/usr/bin/make DESTDIR=${WORLDTMP}
# cross make used for final installation
IXMAKE=	${XMAKEENV} ${WORLDTMP}/usr/bin/make

#
# buildworld
#
# Attempt to rebuild the entire system, with reasonable chance of
# success, regardless of how old your existing system is.
#
buildworld:
.if !defined(NOCLEAN)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Cleaning up the temporary build tree"
	@echo "--------------------------------------------------------------"
	mkdir -p ${WORLDTMP}
	chflags -R noschg ${WORLDTMP}/
	rm -rf ${WORLDTMP}
.endif
.if !defined(NOTOOLS)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Making make"
	@echo "--------------------------------------------------------------"
	mkdir -p ${WORLDTMP}/usr/bin ${MAKETMP}
	( \
	cd ${.CURDIR}/usr.bin/make; \
		MAKEOBJDIRPREFIX=""; unset MAKEOBJDIRPREFIX; \
		${IBMAKE} -I${.CURDIR}/share/mk ${MK_FLAGS} all; \
		${IBMAKE} -I${.CURDIR}/share/mk ${MK_FLAGS} install; \
		${IBMAKE} -I${.CURDIR}/share/mk ${MK_FLAGS} clean \
	)
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Making hierarchy"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${BMAKE} hierarchy
.if !defined(NOCLEAN)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Cleaning up the obj tree"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${BMAKE} ${CLEANDIR:S/^/par-/}
.endif
.if !defined(NOOBJDIR)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding the obj tree"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${BMAKE} par-${OBJDIR}
.endif
.if !defined(NOTOOLS)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding bootstrap tools"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${BMAKE} bootstrap
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding tools necessary to build the include files"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${BMAKE} include-tools
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/include"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; SHARED=copies ${BMAKE} includes
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding bootstrap libraries"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${BMAKE} bootstrap-libraries
.if !defined(NOTOOLS)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding tools needed to build libraries"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${BMAKE} lib-tools
.endif
.if !defined(NOTOOLS)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding all other tools needed to build the world"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${BMAKE} build-tools
.endif
.if !defined(_NODEPEND)
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding dependencies"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${XMAKE} par-depend
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Building libraries"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${XMAKE} -DNOINFO -DNOMAN libraries
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Building everything.."
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${XMAKE} all

#
# installworld
#
# Installs everything compiled by a 'buildworld'.
#
installworld:
	cd ${.CURDIR}; ${IXMAKE} reinstall

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
	cd ${.CURDIR}; ${MAKE} hierarchy
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Installing everything.."
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; ${MAKE} install
.if ${MACHINE_ARCH} == "i386"
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Re-scanning the shared libraries.."
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; /sbin/ldconfig -R
.endif
	@echo
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding man page indexes"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}/share/man; ${MAKE} makedb

#
# update
#
# Update the source tree, by running sup and/or running cvs to update to the
# latest copy.
#
update:
.if defined(SUP_UPDATE)
	@echo "--------------------------------------------------------------"
	@echo "Running ${SUP}"
	@echo "--------------------------------------------------------------"
	@${SUP} ${SUPFLAGS} ${SUPFILE}
.if defined(SUPFILE1)
	@${SUP} ${SUPFLAGS} ${SUPFILE1}
.endif
.if defined(SUPFILE2)
	@${SUP} ${SUPFLAGS} ${SUPFILE2}
.endif
.endif
.if defined(CVS_UPDATE)
	@echo "--------------------------------------------------------------"
	@echo "Updating /usr/src from cvs repository" ${CVSROOT}
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}; cvs -q update -P -d
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
	cd ${.CURDIR}/bin;		${MAKE} all
	cd ${.CURDIR}/sbin;		${MAKE} all
	cd ${.CURDIR}/libexec;		${MAKE} all
	cd ${.CURDIR}/usr.bin;		${MAKE} all
	cd ${.CURDIR}/usr.sbin;		${MAKE} all
	cd ${.CURDIR}/gnu/libexec;	${MAKE} all
	cd ${.CURDIR}/gnu/usr.bin;	${MAKE} all
	cd ${.CURDIR}/gnu/usr.sbin;	${MAKE} all
#.if defined(MAKE_KERBEROS4) && !defined(NOCRYPT)
#	cd ${.CURDIR}/kerberosIV;	${MAKE} most
#.endif
#.if !defined(NOSECURE) && !defined(NOCRYPT)
#	cd ${.CURDIR}/secure;		${MAKE} most
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
	cd ${.CURDIR}/bin;		${MAKE} install
	cd ${.CURDIR}/sbin;		${MAKE} install
	cd ${.CURDIR}/libexec;		${MAKE} install
	cd ${.CURDIR}/usr.bin;		${MAKE} install
	cd ${.CURDIR}/usr.sbin;		${MAKE} install
	cd ${.CURDIR}/gnu/libexec;	${MAKE} install
	cd ${.CURDIR}/gnu/usr.bin;	${MAKE} install
	cd ${.CURDIR}/gnu/usr.sbin;	${MAKE} install
#.if defined(MAKE_KERBEROS4) && !defined(NOCRYPT)
#	cd ${.CURDIR}/kerberosIV;	${MAKE} installmost
#.endif
#.if !defined(NOSECURE) && !defined(NOCRYPT)
#	cd ${.CURDIR}/secure;		${MAKE} installmost
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
	cd ${.CURDIR}/etc;		${MAKE} distrib-dirs

#
# bootstrap - [re]build tools needed to run the actual build, this includes
# tools needed by 'make depend', as some tools are needed to generate source
# for the dependency information to be gathered from.
#
bootstrap:
.if defined(DESTDIR)
	rm -f ${DESTDIR}/usr/src/sys
	ln -s ${.CURDIR}/sys ${DESTDIR}/usr/src
	cd ${.CURDIR}/include; find -dx . | cpio -dump ${DESTDIR}/usr/include
.for d in net netinet posix4 sys vm machine
	if [ -h ${DESTDIR}/usr/include/$d ]; then \
		rm -f ${DESTDIR}/usr/include/$d ; \
	fi
.endfor
	cd ${.CURDIR}/sys; \
		find -dx net netinet posix4 sys vm -name '*.h' -o -type d | \
		cpio -dump ${DESTDIR}/usr/include
	mkdir -p ${DESTDIR}/usr/include/machine
	cd ${.CURDIR}/sys/${MACHINE_ARCH}/include; find -dx . -name '*.h' -o -type d | \
		cpio -dump ${DESTDIR}/usr/include/machine
.endif
	cd ${.CURDIR}/usr.bin/make; ${MAKE} ${MK_FLAGS} ${_DEPEND}; \
		${MAKE} ${MK_FLAGS} all; \
		${MAKE} ${MK_FLAGS} -B install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/xinstall; ${MAKE} ${MK_FLAGS} ${_DEPEND}; \
		${MAKE} ${MK_FLAGS} all; \
		${MAKE} ${MK_FLAGS} -B install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/lex; ${MAKE} bootstrap; \
		${MAKE} ${MK_FLAGS} ${_DEPEND}; \
		${MAKE} ${MK_FLAGS} -DNOLIB all; \
		${MAKE} ${MK_FLAGS} -DNOLIB -B install ${CLEANDIR}
.if !defined(NOOBJDIR)
	cd ${.CURDIR}/usr.bin/lex; ${MAKE} ${OBJDIR}
.endif
	cd ${.CURDIR}/usr.sbin/mtree; ${MAKE} ${MK_FLAGS} ${_DEPEND}; \
		${MAKE} ${MK_FLAGS} all; \
		${MAKE} ${MK_FLAGS} -B install ${CLEANDIR} ${OBJDIR}
.if defined(DESTDIR)
	cd ${.CURDIR}/include && ${MAKE} copies
.endif

#
# include-tools - generally the same as 'bootstrap', except that it's for
# things that are specifically needed to generate include files.
#
# XXX should be merged with bootstrap, it's not worth keeeping them separate.
# Well, maybe it is now.  We force 'cleandepend' here to avoid dependencies
# on cleaned away headers in ${WORLDTMP}.
#
include-tools:
.for d in usr.bin/compile_et usr.bin/rpcgen
	cd ${.CURDIR}/$d; ${MAKE} cleandepend; \
		${MAKE} ${MK_FLAGS} ${_DEPEND}; \
		${MAKE} ${MK_FLAGS} all; \
		${MAKE} ${MK_FLAGS} -B install ${CLEANDIR} ${OBJDIR}
.endfor

#
# includes - possibly generate and install the include files.
#
includes:
.if defined(CLOBBER)
	rm -rf ${DESTDIR}/usr/include/*
	mtree -deU -f ${.CURDIR}/etc/mtree/BSD.include.dist \
		-p ${DESTDIR}/usr/include
.endif
	cd ${.CURDIR}/include;			${MAKE} -B all install
	cd ${.CURDIR}/gnu/include;		${MAKE} install
	cd ${.CURDIR}/gnu/lib/libmp;		${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libobjc;		${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libreadline;	${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libregex;		${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libstdc++;	${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libg++;		${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libdialog;	${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libgmp;		${MAKE} beforeinstall
.if exists(secure) && !defined(NOCRYPT)
	cd ${.CURDIR}/secure/lib/libdes;	${MAKE} beforeinstall
.endif
.if exists(kerberosIV) && !defined(NOCRYPT) && defined(MAKE_KERBEROS4)
	cd ${.CURDIR}/kerberosIV/lib/libacl;	${MAKE} beforeinstall
	cd ${.CURDIR}/kerberosIV/lib/libkadm;	${MAKE} beforeinstall
	cd ${.CURDIR}/kerberosIV/lib/libkafs;	${MAKE} beforeinstall
	cd ${.CURDIR}/kerberosIV/lib/libkdb;	${MAKE} beforeinstall
	cd ${.CURDIR}/kerberosIV/lib/libkrb;	${MAKE} beforeinstall
	cd ${.CURDIR}/kerberosIV/lib/libtelnet; ${MAKE} beforeinstall
.else
	cd ${.CURDIR}/lib/libtelnet;		${MAKE} beforeinstall
.endif
.if exists(${.CURDIR}/lib/csu/${MACHINE})
	cd ${.CURDIR}/lib/csu/${MACHINE};	${MAKE} beforeinstall
.endif
	cd ${.CURDIR}/lib/libalias;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libc;			${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libcalendar;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libcurses;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libdisk;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libedit;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libftpio;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libmd;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libmytinfo;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libncurses;		${MAKE} beforeinstall
.if !defined(WANT_CSRG_LIBM)
	cd ${.CURDIR}/lib/msun;			${MAKE} beforeinstall
.endif
	cd ${.CURDIR}/lib/libopie;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libpcap;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/librpcsvc;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libskey;		${MAKE} beforeinstall
.if !defined(NOTCL) && exists (${.CURDIR}/contrib/tcl) && \
	exists(${.CURDIR}/usr.bin/tclsh) && exists (${.CURDIR}/lib/libtcl)
	cd ${.CURDIR}/lib/libtcl;		${MAKE} installhdrs
.endif
	cd ${.CURDIR}/lib/libtermcap;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libcom_err;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libss;		${MAKE} -B hdrs beforeinstall
	cd ${.CURDIR}/lib/libscsi;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libutil;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libvgl;		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libz;			${MAKE} beforeinstall
	cd ${.CURDIR}/usr.bin/f2c;		${MAKE} beforeinstall
	cd ${.CURDIR}/usr.bin/lex;		${MAKE} beforeinstall

#
# Declare tools if they are not required on all architectures.
#
.if ${MACHINE_ARCH} == "i386"
# aout tools:
_aout_ar	= usr.bin/ar
_aout_as	= gnu/usr.bin/as
_aout_ld	= gnu/usr.bin/ld
_aout_nm	= usr.bin/nm
_aout_ranlib	= usr.bin/ranlib
_aout_size	= usr.bin/size
_aout_strip	= usr.bin/strip
_objformat	= usr.bin/objformat
.endif

#
# lib-tools - build tools to compile and install the libraries.
#
# XXX gperf is required for cc
# XXX a new ld and tsort is required for cc
lib-tools:
.for d in				\
		gnu/usr.bin/gperf	\
		${_aout_ld}		\
		usr.bin/tsort		\
		${_aout_as}		\
		gnu/usr.bin/bison	\
		gnu/usr.bin/cc		\
		${_aout_ar}		\
		usr.bin/env		\
		usr.bin/lex/lib		\
		usr.bin/mk_cmds		\
		${_aout_nm}		\
		${_aout_ranlib}		\
		${_aout_strip}		\
		gnu/usr.bin/binutils	\
		usr.bin/uudecode	\
		${_objformat}
	cd ${.CURDIR}/$d; ${MAKE} ${MK_FLAGS} ${_DEPEND}; \
		${MAKE} ${MK_FLAGS} all; \
		${MAKE} ${MK_FLAGS} -B install; \
		${MAKE} ${MK_FLAGS:S/-DNOPIC//} -B ${CLEANDIR} ${OBJDIR}
.endfor

#
# We have to know too much about ordering and subdirs in the lib trees:
#
# To satisfy shared library linkage when only the libraries being built
# are visible:
#
# libcom_err must be built before libss.
# libcrypt and libmd must be built before libskey.
# libm must be built before libtcl.
# libmytinfo must be built before libdialog and libncurses.
# libncurses must be built before libdialog.
# libtermcap must be built before libcurses, libedit and libreadline.
#
# Some libraries are built conditionally and/or are in inconsistently
# named directories:
#
.if exists(lib/csu/${MACHINE}.pcc)
_csu=lib/csu/${MACHINE}.pcc
.else
_csu=lib/csu/${MACHINE}
.endif

.if !defined(NOSECURE) && !defined(NOCRYPT)
_libcrypt=	secure/lib/libcrypt lib/libcrypt
.else
_libcrypt=	lib/libcrypt
.endif

.if defined(WANT_CSRG_LIBM)
_libm=	lib/libm
.else
_libm=	lib/msun
.endif

#
# bootstrap-libraries - build just enough libraries for the bootstrap
# tools, and install them under ${WORLDTMP}.
#
# Build csu and libgcc early so that some tools get linked to the new
# versions (too late for the main tools, however).  Then build the
# necessary prerequisite libraries (libtermcap just needs to be before
# libcurses, and this only matters for the NOCLEAN case when NOPIC is
# not set).
#
# This is mostly wrong.  The build tools must run on the host system,
# so they should use host libraries.  We depend on the target being
# similar enough to the host for new target libraries to work on the
# host.
#
bootstrap-libraries:
.for _lib in ${_csu} gnu/usr.bin/cc/libgcc lib/libtermcap \
    gnu/lib/libregex gnu/lib/libreadline lib/libc \
    ${_libcrypt} lib/libcurses lib/libedit ${_libm} \
    lib/libmd lib/libutil lib/libz usr.bin/lex/lib
.if exists(${.CURDIR}/${_lib})
	cd ${.CURDIR}/${_lib}; \
		${MAKE} ${MK_FLAGS} ${_DEPEND}; \
		${MAKE} ${MK_FLAGS} all; \
		${MAKE} ${MK_FLAGS} -B install ${CLEANDIR} ${OBJDIR}
.endif
.endfor

#
# libraries - build all libraries, and install them under ${DESTDIR}.
#
# The ordering is not as special as for bootstrap-libraries.  Build
# the prerequisites first, then build almost everything else in
# alphabetical order.
#
libraries:
.for _lib in lib/libcom_err ${_libcrypt} ${_libm} lib/libmytinfo \
    lib/libncurses lib/libtermcap \
    gnu/lib gnu/usr.bin/cc/libgcc lib usr.bin/lex/lib usr.sbin/pcvt/keycap
.if exists(${.CURDIR}/${_lib})
	cd ${.CURDIR}/${_lib}; ${MAKE} all; ${MAKE} -B install
.endif
.endfor
.if exists(${.CURDIR}/secure/lib) && !defined(NOCRYPT) && !defined(NOSECURE)
	cd ${.CURDIR}/secure/lib; ${MAKE} all; ${MAKE} -B install
.endif
.if exists(${.CURDIR}/kerberosIV/lib) && !defined(NOCRYPT) && \
    defined(MAKE_KERBEROS4)
	cd ${.CURDIR}/kerberosIV/lib; ${MAKE} all; ${MAKE} -B install
.endif

# Exclude perl from the build-tools if NOPERL is defined.
.if defined(NOPERL)
_perl=
.else
_perl=	gnu/usr.bin/perl/perl
.endif

#
# build-tools - build and install any other tools needed to complete the
# compile and install.
# ifdef stale
# bc and cpp are required to build groff.  Otherwise, the order here is
# mostly historical, i.e., bogus.
# chmod is used to build gcc's tmpmultilib[2] at obscure times.
# endif stale
# XXX uname is a bug - the target should not depend on the host.
#
build-tools:
.for d in				\
		bin/cat 		\
		bin/chmod		\
		bin/cp 			\
		bin/date		\
		bin/dd			\
		bin/echo		\
		bin/expr		\
		bin/hostname		\
		bin/ln			\
		bin/ls			\
		bin/mkdir		\
		bin/mv			\
		bin/rm			\
		bin/test		\
		gnu/usr.bin/awk		\
		gnu/usr.bin/bc		\
		gnu/usr.bin/grep	\
		gnu/usr.bin/groff	\
		gnu/usr.bin/gzip	\
		gnu/usr.bin/man/makewhatis	\
		gnu/usr.bin/patch	\
		${_perl}		\
		gnu/usr.bin/sort	\
		gnu/usr.bin/texinfo	\
		usr.bin/basename	\
		usr.bin/cap_mkdb	\
		usr.bin/chflags		\
		usr.bin/cmp		\
		usr.bin/col		\
		usr.bin/cpp		\
		usr.bin/expand		\
		usr.bin/file2c		\
		usr.bin/find		\
		usr.bin/gencat		\
		usr.bin/id		\
		usr.bin/lorder		\
		usr.bin/m4		\
		usr.bin/mkdep		\
		usr.bin/paste		\
		usr.bin/sed		\
		${_aout_size}		\
		usr.bin/soelim		\
		usr.bin/symorder	\
		usr.bin/touch		\
		usr.bin/tr		\
		usr.bin/true		\
		usr.bin/uname		\
		usr.bin/uuencode	\
		usr.bin/vgrind		\
		usr.bin/vi		\
		usr.bin/wc		\
		usr.bin/xargs		\
		usr.bin/yacc		\
		usr.sbin/chown		\
		usr.sbin/mtree		\
		usr.sbin/zic		\
		bin/sh
	cd ${.CURDIR}/$d; ${MAKE} ${MK_FLAGS} ${_DEPEND}; \
		${MAKE} ${MK_FLAGS} all; \
		${MAKE} ${MK_FLAGS} -B install ${CLEANDIR} ${OBJDIR}
.endfor

.for __target in clean cleandepend cleandir depend obj
.for entry in ${SUBDIR}
${entry}.${__target}__D: .PHONY
	@if test -d ${.CURDIR}/${entry}.${MACHINE}; then \
		${ECHODIR} "===> ${DIRPRFX}${entry}.${MACHINE}"; \
		edir=${entry}.${MACHINE}; \
		cd ${.CURDIR}/$${edir}; \
	else \
		${ECHODIR} "===> ${DIRPRFX}${entry}"; \
		edir=${entry}; \
		cd ${.CURDIR}/$${edir}; \
	fi; \
	${MAKE} ${__target} DIRPRFX=${DIRPRFX}$${edir}/
.endfor
par-${__target}: ${SUBDIR:S/$/.${__target}__D/}
.endfor

.endif

.include <bsd.subdir.mk>
