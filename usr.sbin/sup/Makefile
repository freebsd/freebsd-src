# Copyright (c) 1992,1991 Carnegie Mellon University
# All Rights Reserved.
# 
# Permission to use, copy, modify and distribute this software and its
# documentation is hereby granted, provided that both the copyright
# notice and this permission notice appear in all copies of the
# software, derivative works or modified versions, and any portions
# thereof, and that both notices appear in supporting documentation.
#
# CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
# CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
# ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
#
# Carnegie Mellon requests users of this software to return to
#
#  Software Distribution Coordinator   or   Software.Distribution@CS.CMU.EDU
#  School of Computer Science
#  Carnegie Mellon University
#  Pittsburgh PA 15213-3890
#
# any improvements or extensions that they make and grant Carnegie the rights
# to redistribute these changes.
######################################################################
# Makefile to build sup (the client side), supfilesrv (the repository
# side, and supscan (used by the repository. If you only want to sup
# files from CMU, just build sup.
# The header files: c.h, libc.h and sysent.h are only
# necessary if you are compiling on a non-Mach system. Likewise the
# files in libextra.a are normally found in libcs.a on a Mach system.
# DOPRINT_VA is used by vprintf.c and should be defined if your version
# of libc/doprnt.c defines the routine _doprnt_va. If it defines _doprnt
# instead, leave DORPINT_VA undefined.
######################################################################
#
# If you need to build a sup for export outside of North America use
# 	"make EXPORTABLE_SYSTEM=true"
# this will remove (not use) any vestiges of crypt code that is present
# on the system.
#
# If you have crypt/crypt.c and /usr/lib/libcrypt.a, you will be building
# a system that uses the SUP crypt mechanism by default.
#
SITE			= NETBSD
#SITE			= CMUCS
RENAMELOG		= \"/usr/local/etc/sup.moved\"
NETBSD_DEFINES		= -UMACH -DVAR_TMP -DHAS_DAEMON
AFS_DEFINES		= -DAFS -I/usr/afsws/include
OSF_DEFINES		= -UMACH -DOSF -D_BSD -noshrlib -g -DNEED_VSNPRINTF -DVAR_TMP
CMUCS_DEFINES		= -DMACH -DDOPRINT_VA -DNEED_VPRINTF
NON_MACH_DEFINES	= -UMACH
#DEFS			= -UCMUCS -UCMU  ${${SITE}_DEFINES}
DEFS			= -UCMUCS -UCMU  ${NETBSD_DEFINES} -DRENAMELOG=${RENAMELOG}

#INSTALLATION PARAMETERS
NETBSD_BINDIR		= /usr/local/bin
NETBSD_MAN1		= /usr/local/man/man1
NETBSD_MAN8		= /usr/local/man/man8

CFLAGS			= ${DEFS} -O -I.

SUPCL			= supcmain.o supcvers.o supcparse.o supcname.o \
			  supcmisc.o supcmeat.o
SUPS			= scm.o scmio.o stree.o log.o supmsg.o netcrypt.o
EXTRA			= atoo.o errmsg.o expand.o ffilecopy.o filecopy.o \
			  nxtarg.o path.o quit.o run.o salloc.o skipto.o  \
			  vprintf.o


PROGRAMS		= sup supscan supfilesrv
MAN1			= sup.1
MAN8			= supservers.8

AFS_LIBPATH		= /usr/afs/lib
AFS_LIBS		= -L${AFS_LIBPATH}/afs -lkauth -lprot -L${AFS_LIBPATH} -lubik -lauth -lrxkad -lsys -ldes -lrx -llwp -lcmd -lcom_err -lc ${AFS_LIBPATH}/afs/util.a

.if exists(/usr/lib/libcrypt.a) && exists(${.CURDIR}/crypt/crypt.c) && !defined(EXPORTABLE_SYSTEM)
USE_CRYPT		= yes
.endif

.if defined(USE_CRYPT)
NETBSD_LIBS		= -lcipher -lcrypt -lutil
.else
NETBSD_LIBS		= -lcrypt -lutil
.endif
CMUCS_LIBS		= -lsys
OSF_LIBS		= -lbsd
LIBS			= libextra.a
sup_OFILES		= ${SUPCL} ${SUPS}
supfilesrv_OFILES	= supfilesrv.o scan.o ${SUPS}
supfilesrv_LIBS		= libextra.a 
supscan_OFILES		= supscan.o stree.o scan.o


all: ${PROGRAMS}
.if defined(USE_CRYPT)
	@echo "WARNING: You have built a NON-exportable version of sup because it uses crypt()!"
	@echo "	To build a crypt-clean version define EXPORTABLE_SYSTEM=true and make."
.endif

sup: ${sup_OFILES} ${LIBS}
	${CC} ${CFLAGS} -o sup ${sup_OFILES} ${LIBS} ${NETBSD_LIBS}

supfilesrv: ${supfilesrv_OFILES} ${supfilesrv_LIBS}
	${CC} ${CFLAGS} -o supfilesrv ${supfilesrv_OFILES} ${supfilesrv_LIBS} ${NETBSD_LIBS}

supscan:  ${supscan_OFILES} ${LIBS}
	${CC} ${CFLAGS} -o supscan ${supscan_OFILES} ${LIBS} ${NETBSD_LIBS}

libextra.a: ${EXTRA}
	ar r libextra.a $?
	ranlib libextra.a

clean:
	rm -f ${PROGRAMS} libextra.a netcrypt.c *.o core a.out

install:
	install -cs -m 555 -o bin -g bin ${PROGRAMS} ${NETBSD_BINDIR}
	install -c -m 444 -o bin -g bin ${MAN1} ${NETBSD_MAN1}
	install -c -m 444 -o bin -g bin ${MAN8} ${NETBSD_MAN8}

package: install
	pkg_create -c pkg/+ONELINE -d pkg/+DESC -f pkg/+PLIST sup_bin

netcrypt.c: crypt.diffs
.if defined(USE_CRYPT)
	ed - crypt/crypt.c < crypt.diffs
.else
	@echo "[ Using netcryptvoid.c ]"
	cp netcryptvoid.c netcrypt.c
.endif

scan.o:	sup.h
scm.o:	sup.h
scmio.o: sup.h supmsg.h
stree.o: sup.h
supcmain.o: sup.h supmsg.h supcdefs.h
supcmeat.o: sup.h supmsg.h supcdefs.h
supcmisc.o: sup.h supmsg.h supcdefs.h
supcname.o: sup.h supmsg.h supcdefs.h
supcparse.o: sup.h supmsg.h supcdefs.h
supfilesrv.o: sup.h supmsg.h
supmsg.o: sup.h supmsg.h
supscan.o: sup.h
netcryptvoid.o: sup.h supmsg.h
netcrypt.o: sup.h supmsg.h
