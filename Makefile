#	@(#)Makefile	5.1.1.2 (Berkeley) 5/9/91
#
#	$Id: Makefile,v 1.19 1993/10/25 21:09:32 rgrimes Exp $
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
	cd ${.CURDIR}/usr.sbin/sendmail/src;	make install
	cd ${.CURDIR}/share/man;		make makedb

directories:
	cd ${.CURDIR}/etc;			make distrib-dirs

cleandist:
.if !defined(NOCLEANDIR)
	@echo "--------------------------------------------------------------"
	@echo " Cleaning up the source tree, and rebuilding the obj tree"
	@echo "--------------------------------------------------------------"
	@echo
	here=`pwd`; dest=/usr/obj/`echo $$here | sed 's,/usr/src,,'`; \
	cd $$dest; rm -rf ${SUBDIR}
	find . -name obj | xargs -n30 rm -rf
	make cleandir
	make obj
.endif

mk:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/share/mk"
	@echo "--------------------------------------------------------------"
	# DONT DO THIS!! rm -rf ${DESTDIR}/usr/share/mk
	# DONT DO THIS!! mkdir ${DESTDIR}/usr/share/mk
	chown ${BINOWN}.${BINGRP} ${DESTDIR}/usr/share/mk
	chmod 755 ${DESTDIR}/usr/share/mk
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
	cd ${.CURDIR}/gnu/libg++;		make beforeinstall
	cd ${.CURDIR}/gnu/libregex;		make beforeinstall
	cd ${.CURDIR}/lib/libcurses;		make beforeinstall
	cd ${.CURDIR}/lib/librpc/rpc;		make beforeinstall

libraries:
	# setenv NOPROFILE if you do not want profiled libraries
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR}/usr/lib"
	@echo "--------------------------------------------------------------"
	@echo
.if defined(CLOBBER)
	rm -rf ${DESTDIR}/usr/lib
	mkdir ${DESTDIR}/usr/lib
	chown -R bin.bin ${DESTDIR}/usr/lib
	chmod 755 ${DESTDIR}/usr/lib
.endif
	cd ${.CURDIR}/lib;		make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/gcc2/libgcc;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/libg++;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/libregex;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/gnu/libmalloc;	make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/usr.bin/lex;	make depend all install ${CLEANDIR} obj

tools:
	@echo "--------------------------------------------------------------"
	@echo " Rebuilding ${DESTDIR} Compiler and Make"
	@echo "--------------------------------------------------------------"
	@echo
	cd ${.CURDIR}/gnu/gcc2;			make depend all install ${CLEANDIR} obj
	cd ${.CURDIR}/usr.bin/make;		make depend all install ${CLEANDIR} obj

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
