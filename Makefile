#
#	$Id: Makefile,v 1.35 1995/01/19 22:41:25 wollman Exp $
#
# Make command line options:
#	-DCLOBBER will remove /usr/include and MOST of /usr/lib 
#	-DMAKE_LOCAL to add ./local to the SUBDIR list
#	-DMAKE_PORTS to add ./ports to the SUBDIR list
#	-DMAKE_EBONES to build eBones (KerberosIV)
#
#	-DNOCLEANDIR run ${MAKE} clean, instead of ${MAKE} cleandir
#	-DNOCRYPT will prevent building of crypt versions
#	-DNOLKM do not build loadable kernel modules
#	-DNOOBJDIR do not run ``${MAKE} obj''
#	-DNOPROFILE do not build profiled libraries
#	-DNOSECURE do not go into secure subdir

# Put initial settings here.
SUBDIR=

# Must be first for "distribute" to work
.if exists(release)
SUBDIR+= release
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
.if exists(include)
SUBDIR+= include
.endif
.if exists(lib)
SUBDIR+= lib
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
.if defined(NOCLEANDIR)
CLEANDIR=	clean
.else 
CLEANDIR=	cleandir
.endif

world:	hierarchy mk cleandist includes lib-tools libraries tools
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} The whole thing"
	@echo "--------------------------------------------------------------"
	@echo
	${MAKE} depend all install
	cd ${.CURDIR}/share/man &&		${MAKE} makedb


hierarchy:
	@echo "--------------------------------------------------------------"
	@echo " Making hierarchy"
	@echo "--------------------------------------------------------------"
	cd ${.CURDIR}/release &&		${MAKE} hierarchy

update:
.if defined(SUP_UPDATE)
	@echo "--------------------------------------------------------------"
	@echo "Running sup"
	@echo "--------------------------------------------------------------"
	@sup -v ${SUPFILE}
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
	rm -rf ${DESTDIR}/usr/include
	mkdir ${DESTDIR}/usr/include
	chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/include
	chmod 755 ${DESTDIR}/usr/include
.endif
	cd ${.CURDIR}/include &&		${MAKE} install
	cd ${.CURDIR}/gnu/include &&		${MAKE}	install
	cd ${.CURDIR}/gnu/lib/libreadline &&	${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libg++ &&         ${MAKE} beforeinstall
	cd ${.CURDIR}/gnu/lib/libdialog &&      ${MAKE} beforeinstall
.if exists(eBones) && !defined(NOCRYPT) && defined(MAKE_EBONES)
	cd ${.CURDIR}/eBones/include &&		${MAKE} beforeinstall
.endif
	cd ${.CURDIR}/lib/libc &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libcurses &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libedit &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libmd &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libmytinfo &&		${MAKE}	beforeinstall
	cd ${.CURDIR}/lib/libncurses &&		${MAKE}	beforeinstall
.if defined(WANT_MSUN)
	cd ${.CURDIR}/lib/msun &&		${MAKE} beforeinstall
.endif
	cd ${.CURDIR}/lib/librpcsvc &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libskey &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libtermcap &&		${MAKE}	beforeinstall
	cd ${.CURDIR}/lib/libcom_err &&		${MAKE} beforeinstall

lib-tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding tools needed to build the libraries
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR}/usr.bin/ar && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/ranlib && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/nm && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/compile_et && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR} && \
		rm -f /usr/sbin/compile_et
	cd ${.CURDIR}/usr.bin/mk_cmds && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}

libraries:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/lib"
	@echo "--------------------------------------------------------------"
	@echo
.if defined(CLOBBER)
	find ${DESTDIR}/usr/lib \! -name '*.s[ao].*' -a \! -type d | \
		xargs rm -rf
.endif
	cd ${.CURDIR}/gnu/lib && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/usr.bin/cc/libgcc && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
.if exists(secure) && !defined(NOCRYPT) && !defined(NOSECURE)
	cd ${.CURDIR}/secure/lib && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
.endif
.if exists(lib)
	cd ${.CURDIR}/lib/csu/i386 && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/lib && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
.endif
	cd ${.CURDIR}/usr.bin/lex/lib && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
.if exists(eBones) && !defined(NOCRYPT) && defined(MAKE_EBONES)
	cd ${.CURDIR}/eBones/des && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/eBones/acl && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/eBones/kdb && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/eBones/krb && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
.endif

tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} Compiler and Make"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR}/gnu/usr.bin/cc && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/make && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}

.include <bsd.subdir.mk>
