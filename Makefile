#	@(#)Makefile	5.1.1.2 (Berkeley) 5/9/91
#
#	$Id: Makefile,v 1.55 1994/06/15 21:30:28 adam Exp $
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

# This contains both libraries and includes, which stuff below depends
# upon.
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

# Where is the c-compiler source.  Change this, and gnu/usr.bin/Makefile if you
# want to use another cc (gcc-2.5.8 for instance)
CCDIR=		${.CURDIR}/gnu/usr.bin/cc
#CCDIR=		${.CURDIR}/gnu/usr.bin/cc25

world:	directories cleandist mk includes libraries tools mdec
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} The whole thing"
	@echo "--------------------------------------------------------------"
	@echo
	make depend all install
	cd ${.CURDIR}/share/man;		make makedb

directories:
	@echo "--------------------------------------------------------------"
	@echo " Making directories"
	@echo "--------------------------------------------------------------"
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
	cd ${.CURDIR}/share/mk;			make clean all install;

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
	cd ${.CURDIR}/include;			make clean all install
	cd ${CCDIR}/libobjc;			make beforeinstall
	cd ${.CURDIR}/gnu/lib/libg++;		make beforeinstall
	cd ${.CURDIR}/gnu/lib/libreadline;	make beforeinstall
	cd ${.CURDIR}/lib/libcurses;		make beforeinstall
	cd ${.CURDIR}/lib/libc;			make beforeinstall
.if !defined(NOCRYPT) && exists(${.CURDIR}/kerberosIV)
	cd ${.CURDIR}/kerberosIV/include;	make clean all install
.endif

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
	cd ${CCDIR};			make -DNOPIC depend all install ${CLEANDIR} obj
	cd ${CCDIR}/libgcc;		make all install ${CLEANDIR} obj
	cd ${.CURDIR}/lib/csu.i386;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/lib/libc;		make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/usr.bin/ld/rtld;	make depend all install ${CLEANDIR} obj

#    Standard database make routines are slow especially for big passwd files.
#    Moreover, *pwd.db bases are too big and waste root space.  You can have
#    much faster routines with small *pwd.db, but loose binary compatibility
#    with previous versions and with other BSD-like systems.  If you want to
#    setup much faster routines, define envirnoment variable (f.e. 'setenv
#    PW_COMPACT' in csh) and use target into /usr/src/Makefile.  If you will
#    want to return this changes back, use the same target without defining
#    PW_COMPACT.

bootstrappwd:   #directories
	-rm -f ${.CURDIR}/lib/libc/obj/getpwent.o ${.CURDIR}/lib/libc/getpwent.o
	cd ${.CURDIR}/lib/libc; make all
	-rm -f ${.CURDIR}/usr.sbin/pwd_mkdb/obj/pwd_mkdb.o ${.CURDIR}/usr.sbin/pwd_mkdb/pwd_mkdb.o
	cd ${.CURDIR}/usr.sbin/pwd_mkdb; make all install ${CLEANDIR}
	cp /etc/master.passwd /etc/mp.t; pwd_mkdb /etc/mp.t
	SLIB=`basename ${.CURDIR}/lib/libc/obj/libc.so.*`; \
		cp ${.CURDIR}/lib/libc/obj/$$SLIB /usr/lib/$$SLIB.tmp; \
		mv /usr/lib/$$SLIB.tmp /usr/lib/$$SLIB
	cd ${.CURDIR}/lib/libc; make install ${CLEANDIR}
	cd ${.CURDIR}/usr.bin/passwd; make clean all install ${CLEANDIR}
	cd ${.CURDIR}/usr.bin/chpass; make clean all install ${CLEANDIR}
	cd ${.CURDIR}/bin; make clean all install ${CLEANDIR}
	cd ${.CURDIR}/sbin; make clean all install ${CLEANDIR}
	@echo "--------------------------------------------------------------"
	@echo " Do a reboot now because all daemons need restarting"
	@echo "--------------------------------------------------------------"

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
	cd ${CCDIR}/libgcc;			make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/lib/libg++;		make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/lib/libregex;		make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/lib/libmalloc;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/lib/libreadline;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/usr.bin/lex;	make depend all install ${CLEANDIR} obj
.if exists(${.CURDIR}/kerberosIV) && !defined(NOCRYPT)
	cd ${.CURDIR}/kerberosIV/des;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/kerberosIV/krb;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/kerberosIV/kdb;	make depend all install ${CLEANDIR} obj
.endif

tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} Compiler and Make"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${CCDIR};			make depend all install ${CLEANDIR} obj
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
