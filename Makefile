#	@(#)Makefile	5.1.1.2 (Berkeley) 5/9/91
#
#	$Id: Makefile,v 1.40 1994/02/18 02:03:17 rgrimes Exp $
#

SUBDIR=
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
.if exists(libexec)
SUBDIR+= libexec
.endif
.if exists(sbin)
SUBDIR+= sbin
.endif
.if exists(share)
SUBDIR+= share
.endif
.if exists(usr.bin)
SUBDIR+= usr.bin
.endif
.if exists(usr.sbin)
SUBDIR+= usr.sbin
.endif

# This is for people who want to have src/ports, src/local built
# automatically.  
.if defined(MAKE_LOCAL) & exists(local) & exists(local/Makefile)
SUBDIR+= local
.endif
.if defined(MAKE_PORTS) & exists(ports) & exists(ports/Makefile)
SUBDIR+= ports
.endif


# Special cases: etc sys
# Not ported: kerberosIV

#
# setenv NOCLEANDIR will prevent make cleandirs from being run
#
.if defined(NOCLEANDIR)
CLEANDIR=
.else
CLEANDIR=	cleandir
.endif

world:	directories cleandist mk includes libraries tools mdec
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} The whole thing"
	@echo "--------------------------------------------------------------"
	@echo
	make depend all install
	cd ${.CURDIR}/share/man;		make makedb

directories:
	cd ${.CURDIR}/etc;			make distrib-dirs

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
	cd $$dest; rm -rf ${SUBDIR}
	find . -name obj | xargs -n30 rm -rf
.if defined(MAKE_LOCAL) & exists(local) & exists(local/Makefile)
	# The cd is done as local may well be a symbolic link
	-cd local ; find . -name obj | xargs -n30 rm -rf
.endif
.if defined(MAKE_PORTS) & exists(ports) & exists(ports/Makefile)
	# The cd is done as local may well be a symbolic link
	-cd ports ; find . -name obj | xargs -n30 rm -rf
.endif
	make cleandir
	make obj
.endif

mk:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/share/mk"
	@echo "--------------------------------------------------------------"
.if defined(CLOBBER)
	# DONT DO THIS!! rm -rf ${DESTDIR}/usr/share/mk
	# DONT DO THIS!! mkdir ${DESTDIR}/usr/share/mk
	# DONT DO THIS!! chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/share/mk
	# DONT DO THIS!! chmod 755 ${DESTDIR}/usr/share/mk
.endif
	cd ${.CURDIR}/share/mk;			make install;

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
	cd ${.CURDIR}/include;			make install
	cd ${.CURDIR}/gnu/usr.bin/cc/libobjc;	make beforeinstall
	cd ${.CURDIR}/gnu/lib/libg++;		make beforeinstall
	cd ${.CURDIR}/lib/libcurses;		make beforeinstall
	cd ${.CURDIR}/lib/libc;			make beforeinstall

# You MUST run this the first time you get the new sources to boot strap
# the shared library tools onto you system.  This target should only
# need to be run once on a system.

bootstrapld:	directories cleandist mk includes
	@echo "--------------------------------------------------------------"
	@echo " Building new shlib compiler tools"
	@echo "--------------------------------------------------------------"
	# These tools need to be built very early due to a.out.h changes:
	# It is possible that ar is needed
	cd ${.CURDIR}/usr.bin/mkdep;	make -DNOPIC depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/usr.bin/nm;	make -DNOPIC depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/usr.bin/ranlib;	make -DNOPIC depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/usr.bin/strip;	make -DNOPIC depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/usr.bin/ld;	make -DNOPIC depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/usr.bin/as;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/usr.bin/cc;	make -DNOPIC depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/usr.bin/cc/libgcc;	make all install ${CLEANDIR} obj
	cd ${.CURDIR}/lib/csu.i386;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/lib/libc;		make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/usr.bin/ld/rtld;	make depend all install ${CLEANDIR} obj

libraries:
	# setenv NOPROFILE if you do not want profiled libraries
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/lib"
	@echo "--------------------------------------------------------------"
	@echo
.if defined(CLOBBER)
	find ${DESTDIR}/usr/lib \! -name '*.s[ao].*' -a \! -type d | xargs -n30 rm -rf
.endif
	cd ${.CURDIR}/lib;			make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/usr.bin/cc/libgcc;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/lib/libg++;		make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/lib/libregex;		make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/lib/libmalloc;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/usr.bin/lex;	make depend all install ${CLEANDIR} obj

tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} Compiler and Make"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR}/gnu/usr.bin/cc;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/usr.bin/make;	make depend all install ${CLEANDIR} obj

mdec:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/mdec"
	@echo "--------------------------------------------------------------"
	@echo
.if ${MACHINE} == "i386"
	cd ${.CURDIR}/sys/i386/boot;	make depend all install ${CLEANDIR}
.if defined (DESTDIR)
	cd /usr/mdec; find . | cpio -pdamuv ${DESTDIR}/usr/mdec
.endif
.endif

.include <bsd.subdir.mk>
