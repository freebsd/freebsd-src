#$FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

NO_MAN=

.include <bsd.own.mk>

PROG=	rescue
BINDIR?=/rescue

# Shell scripts need #! line to be edited from /bin/sh to /rescue/sh
SCRIPTS= nextboot_FIXED
SCRIPTSNAME_nextboot_FIXED= nextboot
nextboot_FIXED: ../../sbin/reboot/nextboot.sh
	sed '1s/\/bin\//\/rescue\//' ${.ALLSRC} > ${.TARGET}
CLEANFILES+= nextboot_FIXED

SCRIPTS+= dhclient_FIXED
SCRIPTSNAME_dhclient_FIXED= dhclient-script
dhclient_FIXED: ../../sbin/dhclient/dhclient-script
	sed '1s/\/bin\//\/rescue\//' ${.ALLSRC} > ${.TARGET}
CLEANFILES+= dhclient_FIXED

#################################################################
#
# General notes:
#
# A number of Make variables are used to generate the crunchgen config file.
#
#  CRUNCH_SRCDIRS: lists directories to search for included programs
#  CRUNCH_PROGS:  lists programs to be included
#  CRUNCH_LIBS:  libraries to link with
#  CRUNCH_BUILDOPTS: generic build options to be added to every program
#
# Special options can be specified for individual programs
#  CRUNCH_SRCDIR_$(P): base source directory for program $(P)
#  CRUNCH_BUILDOPTS_$(P): additional build options for $(P)
#  CRUNCH_ALIAS_$(P): additional names to be used for $(P)
#
# By default, any name appearing in CRUNCH_PROGS or CRUNCH_ALIAS_${P}
# will be used to generate a hard link to the resulting binary.
# Specific links can be suppressed by setting
# CRUNCH_SUPPRESS_LINK_$(NAME) to 1.
#

# Define Makefile variable RESCUE
CRUNCH_BUILDOPTS+= -DRESCUE
# Define compile-time RESCUE symbol when compiling components
CRUNCH_BUILDOPTS+= CRUNCH_CFLAGS=-DRESCUE

# An experiment that failed: try overriding bsd.lib.mk and bsd.prog.mk
# rather than incorporating rescue-specific logic into standard files.
#MAKEFLAGS= -m ${.CURDIR} ${.MAKEFLAGS}

# Hackery:  'librescue' exists merely as a tool for appropriately
# recompiling specific library entries.  We _know_ they're needed, and
# regular archive searching creates ugly library ordering problems.
# Easiest fix: tell the linker to include them into the executable
# first, so they are guaranteed to override the regular lib entries.
# Note that if 'librescue' hasn't been compiled, we'll just get the
# regular lib entries from libc and friends.
CRUNCH_LIBS+= ${.OBJDIR}/../librescue/*.o

###################################################################
# Programs from stock /bin
#
# WARNING: Changing this list may require adjusting
# /usr/include/paths.h as well!  You were warned!
#
CRUNCH_SRCDIRS+= bin
CRUNCH_PROGS_bin= cat chflags chio chmod cp date dd df echo 	\
	 ed expr getfacl hostname kenv kill ln ls mkdir mv pax ps pwd 	\
	 realpath rm rmdir setfacl sh stty sync test
CRUNCH_LIBS+= -lcrypt -ledit -lkvm -ll -lm -ltermcap -lutil
.if ${MK_OPENSSL} != "no"
CRUNCH_LIBS+= -lcrypto
.endif

# Additional options for specific programs
CRUNCH_ALIAS_test= [
CRUNCH_ALIAS_sh= -sh
# The -sh alias shouldn't appear in /rescue as a hard link
CRUNCH_SUPPRESS_LINK_-sh= 1
CRUNCH_ALIAS_ln= link
CRUNCH_ALIAS_rm= unlink
CRUNCH_ALIAS_ed= red

.if ${MK_RCMDS} != "no"
CRUNCH_PROGS_bin+= rcp
.endif

.if ${MK_TCSH} != "no"
CRUNCH_PROGS_bin+= csh
CRUNCH_ALIAS_csh= -csh tcsh -tcsh
CRUNCH_SUPPRESS_LINK_-csh= 1
CRUNCH_SUPPRESS_LINK_-tcsh= 1
.endif

###################################################################
# Programs from standard /sbin
#
# WARNING: Changing this list may require adjusting
# /usr/include/paths.h as well!  You were warned!
#
# Note that mdmfs have their own private 'pathnames.h'
# headers in addition to the standard 'paths.h' header.
#
CRUNCH_SRCDIRS+= sbin
CRUNCH_PROGS_sbin= atacontrol badsect bsdlabel		\
	camcontrol ccdconfig clri devfs dmesg dump			\
	dumpfs dumpon fsck fsck_ffs fsck_msdosfs fsdb		\
	fsirand gbde ifconfig init 				\
	kldconfig kldload kldstat kldunload ldconfig 			\
	md5 mdconfig mdmfs mknod mount mount_cd9660	\
	mount_msdosfs mount_nfs mount_ntfs mount_nullfs			\
	mount_udf mount_umapfs mount_unionfs newfs		\
	newfs_msdos nos-tun ping reboot			\
	restore rcorder route routed rtquery rtsol savecore 		\
	slattach spppcontrol startslip swapon sysctl tunefs umount 

.if ${MK_ATM} != "no"
CRUNCH_PROGS_sbin+= atm atmconfig fore_dnld ilmid
CRUNCH_LIBS+= -latm
.endif

.if ${MK_INET6_SUPPORT} != "no"
CRUNCH_PROGS_sbin+= ping6
.endif

.if ${MK_IPFILTER} != "no"
CRUNCH_PROGS_sbin+= ipf
.endif

# crunchgen does not like C++ programs; this should be fixed someday
# CRUNCH_PROGS+= devd

CRUNCH_LIBS+= -lalias -lcam -lcurses -ldevstat -lipsec
.if ${MK_IPX} != "no"
CRUNCH_LIBS+= -lipx
.endif
CRUNCH_LIBS+= -lgeom -lbsdxml -lkiconv -lmd -lreadline -lsbuf -lufs -lz

.if ${MACHINE_ARCH} == "i386"
CRUNCH_PROGS_sbin+= sconfig fdisk
CRUNCH_ALIAS_bsdlabel= disklabel
#.if ${MK_NCP} != "no"
#CRUNCH_PROGS+= mount_nwfs
#CRUNCH_LIBS+= -lncp
#.endif
#CRUNCH_PROGS+= mount_smbfs
#CRUNCH_LIBS+= -lsmb
.endif

.if ${MACHINE} == "pc98"
CRUNCH_SRCDIR_fdisk= $(.CURDIR)/../../sbin/fdisk_pc98
.endif

.if ${MACHINE_ARCH} == "ia64"
CRUNCH_PROGS_sbin+= mca gpt fdisk
.endif

.if ${MACHINE_ARCH} == "sparc64"
CRUNCH_PROGS_sbin+= sunlabel
.endif

.if ${MACHINE_ARCH} == "amd64"
CRUNCH_PROGS_sbin+= fdisk
CRUNCH_ALIAS_bsdlabel= disklabel
.endif

CRUNCH_SRCDIR_atm= $(.CURDIR)/../../sbin/atm/atm
CRUNCH_SRCDIR_atmconfig= $(.CURDIR)/../../sbin/atm/atmconfig
CRUNCH_SRCDIR_fore_dnld= $(.CURDIR)/../../sbin/atm/fore_dnld
CRUNCH_SRCDIR_ilmid= $(.CURDIR)/../../sbin/atm/ilmid
CRUNCH_SRCDIR_rtquery= $(.CURDIR)/../../sbin/routed/rtquery
CRUNCH_SRCDIR_ipf= $(.CURDIR)/../../sbin/ipf/ipf
CRUNCH_ALIAS_reboot= fastboot halt fasthalt
CRUNCH_ALIAS_restore= rrestore
CRUNCH_ALIAS_dump= rdump
CRUNCH_ALIAS_fsck_ffs= fsck_4.2bsd fsck_ufs

# dhclient has historically been troublesome...
CRUNCH_PROGS_sbin+= dhclient
CRUNCH_BUILDOPTS_dhclient= -DRELEASE_CRUNCH -Dlint

##################################################################
# Programs from stock /usr/bin
# 
CRUNCH_SRCDIRS+= usr.bin
CRUNCH_SRCDIRS+= gnu/usr.bin

CRUNCH_PROGS_gnu/usr.bin+= gzip
CRUNCH_ALIAS_gzip= gunzip gzcat zcat

CRUNCH_PROGS_usr.bin+= bzip2
CRUNCH_ALIAS_bzip2= bunzip2 bzcat
CRUNCH_LIBS+= -lbz2

CRUNCH_PROGS_usr.bin+= tar
CRUNCH_LIBS+= -larchive

CRUNCH_PROGS_usr.bin+= vi
CRUNCH_ALIAS_vi= ex

CRUNCH_PROGS_usr.bin+= id
CRUNCH_ALIAS_id= groups whoami

##################################################################
# Programs from stock /usr/sbin
# 
CRUNCH_SRCDIRS+= usr.sbin

CRUNCH_PROGS_usr.sbin+= chroot

##################################################################
#  The following is pretty nearly a generic crunchgen-handling makefile
#

CONF=	$(PROG).conf
OUTMK=	$(PROG).mk
OUTC=   $(PROG).c
OUTPUTS=$(OUTMK) $(OUTC) $(PROG).cache
CRUNCHOBJS= ${.OBJDIR}
.if defined(MAKEOBJDIRPREFIX)
CANONICALOBJDIR:= ${MAKEOBJDIRPREFIX}${.CURDIR}
.else
CANONICALOBJDIR:= /usr/obj${.CURDIR}
.endif

CLEANFILES+= $(CONF) *.o *.lo *.c *.mk *.cache *.a *.h

# Program names and their aliases contribute hardlinks to 'rescue' executable,
# except for those that get suppressed.
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
$(OUTPUTS): $(CRUNCH_SRCDIR_${P})/Makefile
.else
$(OUTPUTS): $(.CURDIR)/../../$(D)/$(P)/Makefile
.endif
.ifndef CRUNCH_SUPPRESS_LINK_${P}
LINKS+= $(BINDIR)/$(PROG) $(BINDIR)/$(P)
.endif
.for A in $(CRUNCH_ALIAS_$(P))
.ifndef CRUNCH_SUPPRESS_LINK_${A}
LINKS+= $(BINDIR)/$(PROG) $(BINDIR)/$(A)
.endif
.endfor
.endfor
.endfor

all: $(PROG)
exe: $(PROG)

$(CONF): Makefile
	echo \# Auto-generated, do not edit >$(.TARGET)
.ifdef CRUNCH_BUILDOPTS
	echo buildopts $(CRUNCH_BUILDOPTS) >>$(.TARGET)
.endif
.ifdef CRUNCH_LIBS
	echo libs $(CRUNCH_LIBS) >>$(.TARGET)
.endif
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
	echo progs $(P) >>$(.TARGET)
.ifdef CRUNCH_SRCDIR_${P}
	echo special $(P) srcdir $(CRUNCH_SRCDIR_${P}) >>$(.TARGET)
.else
	echo special $(P) srcdir $(.CURDIR)/../../$(D)/$(P) >>$(.TARGET)
.endif
.ifdef CRUNCH_BUILDOPTS_${P}
	echo special $(P) buildopts DIRPRFX=${DIRPRFX}${P}/ \
	    $(CRUNCH_BUILDOPTS_${P}) >>$(.TARGET)
.else
	echo special $(P) buildopts DIRPRFX=${DIRPRFX}${P}/ >>$(.TARGET)
.endif
.for A in $(CRUNCH_ALIAS_$(P))
	echo ln $(P) $(A) >>$(.TARGET)
.endfor
.endfor
.endfor

# XXX Make sure we don't pass -P to crunchgen(1).
.MAKEFLAGS:= ${.MAKEFLAGS:N-P}
.ORDER: $(OUTPUTS) objs
$(OUTPUTS): $(CONF)
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} crunchgen -fq -m $(OUTMK) \
	    -c $(OUTC) $(CONF)

$(PROG): $(OUTPUTS) objs
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) exe

objs: $(OUTMK)
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) objs

# <sigh> Someone should replace the bin/csh and bin/sh build-tools with
# shell scripts so we can remove this nonsense.
build-tools:
.for _tool in bin/csh bin/sh 
	cd $(.CURDIR)/../../${_tool}; \
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} obj; \
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} build-tools
.endfor

# Use a separate build tree to hold files compiled for this crunchgen binary
# Yes, this does seem to partly duplicate bsd.subdir.mk, but I can't
# get that to cooperate with bsd.prog.mk.  Besides, many of the standard
# targets should NOT be propagated into the components.
cleandepend cleandir obj objlink:
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
	cd ${CRUNCH_SRCDIR_$(P)} && \
	    MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.else
	cd $(.CURDIR)/../../${D}/${P} && \
	    MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.endif
.endfor
.endfor

clean:
	rm -f ${CLEANFILES}
	if [ -e ${.OBJDIR}/$(OUTMK) ]; then				\
		MAKEOBJDIRPREFIX=${CRUNCHOBJS} ${MAKE} -f $(OUTMK) clean;	\
	fi
.for D in $(CRUNCH_SRCDIRS)
.for P in $(CRUNCH_PROGS_$(D))
.ifdef CRUNCH_SRCDIR_${P}
	cd ${CRUNCH_SRCDIR_$(P)} && \
	    MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.else
	cd $(.CURDIR)/../../${D}/${P} && \
	    MAKEOBJDIRPREFIX=${CANONICALOBJDIR} ${MAKE} \
	    DIRPRFX=${DIRPRFX}${P}/ ${CRUNCH_BUILDOPTS} ${.TARGET}
.endif
.endfor
.endfor

.include <bsd.prog.mk>
