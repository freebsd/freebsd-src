#
#	$Id: Makefile,v 1.8 1994/08/14 16:53:33 jkh Exp $
#
# Make command line options:
#	-DCLOBBER will remove /usr/include and MOST of /usr/lib 
#	-DMAKE_LOCAL to add ./local to the SUBDIR list
#	-DMAKE_PORTS to add ./ports to the SUBDIR list
#	-DNOCLEANDIR run ${MAKE} clean, instead of ${MAKE} cleandir
# XXX1	-DNOCRYPT will prevent building of crypt versions (BROKEN RIGHT NOW)
#	-DNOOBJDIR do not run ``${MAKE} obj''
#	-DNOPROFILE do not build profiled libraries
#
# XXX1	This has not yet been implemented in FreeBSD 2.0.0, the only way
#	to build the system is with full crypt and KerberosIV
#

# Put initial settings here.
NOCRYPT=	yes
SUBDIR=

.if exists(bin)
SUBDIR+= bin
.endif
.if exists(contrib)
SUBDIR+= contrib
.endif
.if exists(etc)
SUBDIR+= etc
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
.if exists(kerberosIV) && !defined(NOCRYPT)
SUBDIR+= kerberosIV
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

world:	directories cleandist mk includes libraries tools mdec
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} The whole thing"
	@echo "--------------------------------------------------------------"
	@echo
	${MAKE} depend all install
	cd ${.CURDIR}/share/man &&		${MAKE} makedb

directories:
	@echo "--------------------------------------------------------------"
	@echo " Making directories"
	@echo "--------------------------------------------------------------"
	@echo " XXX Not yet ready in 2.0.0"
# XXX	cd ${.CURDIR}/etc &&			${MAKE} distrib-dirs

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
#XXX	cd ${.CURDIR}/gnu/lib/libg++ &&		${MAKE} beforeinstall
#XXX	cd ${.CURDIR}/gnu/usr.bin/cc26/libobjc &&	${MAKE} beforeinstall
.if !defined(NOCRYPT)
	cd ${.CURDIR}/kerberosIV/include &&	${MAKE} install
.endif
	cd ${.CURDIR}/lib/libc &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libcurses &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/libedit &&		${MAKE} beforeinstall
	cd ${.CURDIR}/lib/librpcsvc &&		${MAKE} beforeinstall

libraries:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/lib"
	@echo "--------------------------------------------------------------"
	@echo
.if defined(CLOBBER)
	find ${DESTDIR}/usr/lib \! -name '*.s[ao].*' -a \! -type d | \
		xargs rm -rf
.endif
	# XXX The whole GNU block should be doable in one command, as soon		# as libg++ works on FreeBSD 2.0 I will try that out
#XXX	cd ${.CURDIR}/gnu/lib/libg++ && \
#XXX		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/lib/libmalloc && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/lib/libreadline && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/lib/libregex && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/gnu/usr.bin/cc26/libgcc && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/lib && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	# You need the lex lib before you can build kerberosIV
#XXX	# We don't have lex in the 2.0 tree yet!
#XXX	cd ${.CURDIR}/usr.bin/lex/lib && \
#XXX		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
.if !defined(NOCRYPT)
	cd ${.CURDIR}/kerberosIV/acl && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/kerberosIV/des && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/kerberosIV/kdb && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/kerberosIV/krb && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
.endif

tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} Compiler and Make"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR}/gnu/usr.bin/cc26 && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}
	cd ${.CURDIR}/usr.bin/make && \
		${MAKE} depend all install ${CLEANDIR} ${OBJDIR}

mdec:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/mdec"
	@echo "--------------------------------------------------------------"
	@echo
	@echo " XXX Not yet ready in 2.0.0"
#XXX.if ${MACHINE} == "i386"
#XXX	# XXX Need to fix for obj case, src/sys/Makefile needs to be fixed to
#XXX	# traverse down into here and this can go away!
#XXX	cd ${.CURDIR}/sys/i386/boot &&	${MAKE} depend all install ${CLEANDIR}
#XXX.if defined (DESTDIR)
#XXX	# XXX Really need to fix the sys/i386/boot Makefile so this is not
#XXX	# necessary!!!
#XXX	cd /usr/mdec && find . | cpio -pdamuv ${DESTDIR}/usr/mdec
#XXX.endif
#XXX.endif

.include <bsd.subdir.mk>
