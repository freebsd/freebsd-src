#$FreeBSD$
#	@(#)Makefile	8.1 (Berkeley) 6/2/93

PROG=	rescue
BINDIR?= /rescue

# Uncomment to exclude tcsh
#NO_TCSH=1

# Shell scripts need #! line to be edited from /bin/sh to /rescue/sh
SCRIPTS= nextboot_FIXED
SCRIPTSNAME_nextboot_FIXED= nextboot.sh
nextboot_FIXED: ../../sbin/reboot/nextboot.sh
	sed '1s/\/bin\//\/rescue\//' ${.ALLSRC} > ${.TARGET}
CLEANFILES+= nextboot_FIXED

SCRIPTS+= dhclient_FIXED
SCRIPTSNAME_dhclient_FIXED= dhclient-script
dhclient_FIXED: ../../contrib/isc-dhcp/client/scripts/freebsd
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
CRUNCH_SRCDIRS+=$(.CURDIR)/../../bin $(.CURDIR)/../../usr.bin
CRUNCH_PROGS=cat chflags chio chmod cp date dd df domainname echo ed	\
	 expr getfacl hostname kenv kill ln ls mkdir mv pax ps pwd 	\
	 realpath rm rmdir setfacl sh sleep stty sync test
CRUNCH_LIBS+=-lcrypt -lcrypto -ledit -lkvm -ll -lm -ltermcap -lutil

# Additional options for specific programs
CRUNCH_ALIAS_test= [
CRUNCH_ALIAS_sh= -sh
# The -sh alias shouldn't appear in /rescue as a hard link
CRUNCH_SUPPRESS_LINK_-sh=1
CRUNCH_ALIAS_ln= link
CRUNCH_ALIAS_rm= unlink
CRUNCH_ALIAS_ed= red

.if !defined(NO_RCMNDS)
CRUNCH_PROGS+= rcp
.endif

.if !defined(NO_TCSH)
CRUNCH_PROGS+= csh
CRUNCH_ALIAS_csh= -csh tcsh -tcsh
CRUNCH_SUPPRESS_LINK_-csh=1
CRUNCH_SUPPRESS_LINK_-tcsh=1
.endif

#Is rmail of any use at all here?  I think not.
#CRUNCH_PROGS+= rmail  

###################################################################
# Programs from standard /sbin
#
# WARNING: Changing this list may require adjusting
# /usr/include/paths.h as well!  You were warned!
#
# Note that mdmfs and shutdown have their own private 'pathnames.h'
# headers in addition to the standard 'paths.h' header.
#
CRUNCH_SRCDIRS+=$(.CURDIR)/../../sbin
CRUNCH_PROGS+=atm adjkerntz atacontrol badsect camcontrol ccdconfig	\
	clri comcontrol conscontrol devfs disklabel dmesg dump		\
	dumpfs dumpon fdisk fore_dnld fsck fsck_ffs fsck_msdosfs fsdb	\
	fsirand gbde growfs ifconfig ilmid init ip6fw ipf ipfs ipfstat	\
	ipfw ipmon ipnat kldconfig kldload kldstat kldunload ldconfig 	\
	md5 mdconfig mdmfs mknod mount mount_cd9660 mount_ext2fs	\
	mount_msdosfs mount_nfs mount_ntfs mount_nullfs	mount_portalfs	\
	mount_std mount_udf mount_umapfs mount_unionfs natd newfs	\
	newfs_msdos nfsiod nos-tun ping ping6 quotacheck raidctl reboot	\
	restore rcorder route routed rtquery rtsol savecore shutdown	\
	slattach spppcontrol startslip swapon sysctl tunefs umount vinum

# crunchgen does not like C++ programs; this should be fixed someday
# CRUNCH_PROGS+= devd

CRUNCH_LIBS+=-lalias -latm -lbsdxml -lcam -lcurses -ldevstat -lipsec -lipx \
	-lgeom -lmd -lncp -lreadline -lsbuf -lsmb -lufs -lz 

.if ${MACHINE_ARCH} == "i386"
CRUNCH_PROGS+= cxconfig mount_nwfs mount_smbfs
.endif

.if ${MACHINE} == "pc98"
CRUNCH_PROGS+= fdisk_pc98
.endif

.if ${MACHINE_ARCH} == "ia64"
CRUNCH_PROGS+= mca gpt
.endif

.if ${MACHINE_ARCH} == "sparc64"
.endif

.if ${MACHINE_ARCH} == "alpha"
.endif

CRUNCH_SRCDIR_atm=$(.CURDIR)/../../sbin/atm/atm
CRUNCH_SRCDIR_fore_dnld=$(.CURDIR)/../../sbin/atm/fore_dnld
CRUNCH_SRCDIR_ilmid=$(.CURDIR)/../../sbin/atm/ilmid
CRUNCH_SRCDIR_rtquery=$(.CURDIR)/../../sbin/routed/rtquery
CRUNCH_ALIAS_reboot= fastboot halt fasthalt
CRUNCH_ALIAS_restore=rrestore
CRUNCH_ALIAS_dump= rdump
CRUNCH_ALIAS_fsck_ffs=fsck_4.2bsd fsck_ufs
CRUNCH_ALIAS_mount_std= mount_devfs mount_fdescfs mount_linprocfs mount_procfs

# dhclient has historically been troublesome...
CRUNCH_PROGS+=dhclient
CRUNCH_BUILDOPTS_dhclient=-DRELEASE_CRUNCH -Dlint

##################################################################
# Programs from stock /usr/bin
# 
CRUNCH_SRCDIRS+=$(.CURDIR)/../../usr.bin
CRUNCH_SRCDIRS+=$(.CURDIR)/../../gnu/usr.bin

CRUNCH_PROGS+=wall

CRUNCH_PROGS+=gzip
CRUNCH_ALIAS_gzip=gunzip gzcat zcat

CRUNCH_PROGS+=bzip2
CRUNCH_ALIAS_bzip2=bunzip2 bzcat
CRUNCH_LIBS+=-lbz2

CRUNCH_PROGS+=tar
CRUNCH_PROGS+=vi
CRUNCH_ALIAS_vi=ex

##################################################################
#  The following is pretty nearly a generic crunchgen-handling makefile
#

CONF=	$(PROG).conf
OUTMK=	$(PROG).mk
OUTC=   $(PROG).c
OUTPUTS= $(OUTMK) $(OUTC) $(PROG).cache
CRUNCHOBJS= ${.OBJDIR}
.if defined(MAKEOBJDIRPREFIX)
CANONICALOBJDIR:=${MAKEOBJDIRPREFIX}${.CURDIR}
.else
CANONICALOBJDIR:=/usr/obj${.CURDIR}
.endif

NOMAN=	true
CLEANFILES+= $(CONF) *.o *.lo *.c *.mk *.cache *.a *.h

# Program names and their aliases contribute hardlinks to 'rescue' executable,
# except for those that get suppressed.
.for P in $(CRUNCH_PROGS)
.ifndef CRUNCH_SUPPRESS_LINK_${P}
LINKS += $(BINDIR)/$(PROG) $(BINDIR)/$(P)
.endif
.for A in $(CRUNCH_ALIAS_$(P))
.ifndef CRUNCH_SUPPRESS_LINK_${A}
LINKS += $(BINDIR)/$(PROG) $(BINDIR)/$(A)
.endif
.endfor
.endfor

all: $(PROG)
exe: $(PROG)

$(CONF): Makefile
	echo \# Auto-generated, do not edit >$(.TARGET)
.for D in $(CRUNCH_SRCDIRS)
	echo srcdirs $(D) >>$(.TARGET)
.endfor
.ifdef CRUNCH_BUILDOPTS
	echo buildopts $(CRUNCH_BUILDOPTS) >>$(.TARGET)
.endif
.ifdef CRUNCH_LIBS
	echo libs $(CRUNCH_LIBS) >>$(.TARGET)
.endif
.for P in $(CRUNCH_PROGS)
	echo progs $(P) >>$(.TARGET)
.ifdef CRUNCH_SRCDIR_${P}
	echo special $(P) srcdir $(CRUNCH_SRCDIR_${P}) >>$(.TARGET)
.endif
.ifdef CRUNCH_BUILDOPTS_${P}
	echo special $(P) buildopts $(CRUNCH_BUILDOPTS_${P}) >>$(.TARGET)
.endif
.for A in $(CRUNCH_ALIAS_$(P))
	echo ln $(P) $(A) >>$(.TARGET)
.endfor
.endfor


$(OUTPUTS): $(CONF)
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} crunchgen -q -m $(OUTMK) -c $(OUTC) $(CONF)

# -m here forces make to treat the bsd.prog.mk and bsd.lib.mk in
# this directory as overrides for the standard shared ones.
$(PROG): $(OUTPUTS)
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} make -f $(OUTMK) 

objs:
	MAKEOBJDIRPREFIX=${CRUNCHOBJS} make -f $(OUTMK) objs

# Use a separate build tree to hold files compiled for this crunchgen binary
# Yes, this does seem to partly duplicate bsd.subdir.mk, but I can't
# get that to cooperate with bsd.prog.mk.  Besides, many of the standard
# targets should NOT be propagated into the components.
cleandepend cleandir obj objlink:
.for D in $(CRUNCH_SRCDIRS)
	cd ${D} && MAKEOBJDIRPREFIX=${CANONICALOBJDIR} make ${.TARGET}
.endfor

clean:
	rm -f ${CLEANFILES}
	if [ -e ${.OBJDIR}/$(OUTMK) ]; then		\
		MAKEOBJDIRPREFIX=${CRUNCHOBJS} make -f $(OUTMK) clean;	\
	fi
.for D in $(CRUNCH_SRCDIRS) $(EXTRA_SRCDIRS)
	cd ${D} && MAKEOBJDIRPREFIX=${CRUNCHOBJS} make clean
.endfor

.include <bsd.prog.mk>
