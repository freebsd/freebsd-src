#
# Makefile to install the BIND 4.9 manual entries.
#
# Default Configuration:
# 	There are a set of default assignments immediately following this 
#	note. These defaults are for BSD4.4, BSD/386, other net2-alikes, 
#	and will install manual entries with following characteristics:
#	o  They will be catable (i.e., passed through nroff)
#	o  They will be installed in the directories
#		/usr/share/man/catN, where N is 1, 3, 5, 7, 8
#	o  They will have an extension of `.0'
#
# Don't change these defaults. Instead, following the default configuration
# are sets of commented values for particular systems that can be used
# to override the default values.
#

#
# Target directory for the manual directory tree. Eg., may be used to 
# specify the path of an NFS-mounted directory for common files.
#
DESTDIR=

#
# Default location for manual section directories.
#
DESTMAN= /usr/share/man

#
# Install manuals in ${MANDIR}N. For systems that generate catable manual 
# entries on the fly, use
#	MANDIR = man
#
MANDIR = cat

#
# Default extension for manual entries. To install the manual entries under
# their `real' extensions use 
#	CATEXT = $$N
#
CATEXT = 0

#
# Command to install manual entries
#
INSTALL= install

#
# `install' options to set Owner and Group for manual entries. Eg. for 
# BSD `install' use
# 	MAN_OWNER = -o bin
#	MAN_GROUP = -g bin
#
MAN_OWNER =
MAN_GROUP =

SHELL= /bin/sh

INDOT=
XFER_INDOT=
#
# Uppercase versions of the above variables (`INDOT_U' and `XFER_INDOT_U')
# are defined for use in `.TH' lines.
#

#
# Command used to generate a manual entry. By default this produces catable
# manual entries.
#
# For systems that store manuals in source form (eg SunOS 4.x and SunOS 5.x)
# and generate catable manual entries on the fly the following assignment
# can be used. 
#	MANROFF = cat
#
MANROFF = ( tbl | nroff -man )

#
# Default extensions for installed manual entries. The following variables 
# have been defined to allow BIND's manual entries to be installed in the 
# right place for a given platform.
#
# 	CMD_EXT = extension for user commands (eg, dig)
# 	LIB_NETWORK_EXT = extension for network library routines (eg, 
#			gethostbyname)
# 	FORMAT_EXT = extension for files describing file formats
#			(eg, resolver)
# 	DESC_EXT = extension for descriptive files (eg, mailaddr)
# 	SYS_OPS_EXT =  extension system operation and maintenance commands 
#			and applications. (eg, named, named-xfer, syslog)
#
# Associated with each variable is an additional variable with the suffix
# `_DIR' that specifies the suffix to ${MANDIR}. It's needed because on
# some systems, eg., Ultrix, multiple subsections (eg 3x, 3m 3n) are 
# stored in generic manual section directories (eg., man3).
#
# Associated with each variable is an additional variable with the suffix
# `_U' which gives the upper case form of the variable for use in `.TH'
# commands. Useful for platforms (such as Solaris 2) that include letters
# in manual sections.
#
CMD_EXT = 1
CMD_EXT_DIR = ${CMD_EXT}
LIB_NETWORK_EXT = 3
LIB_NETWORK_EXT_DIR = ${LIB_NETWORK_EXT}
FORMAT_EXT = 5
FORMAT_EXT_DIR = ${FORMAT_EXT}
DESC_EXT = 7
DESC_EXT_DIR = ${DESC_EXT}
SYS_OPS_EXT = 8
SYS_OPS_EXT_DIR = ${SYS_OPS_EXT}

#
# Additional variables are defined for cross-references within manual
# entries:
#	SYSCALL_EXT = extension for system calls
#	BSD_SYSCALL_EXT = extension for BSD-specifc system calls. On some
#			systems (eg Ultrix) these appear in section 2.
#			On other system (eg SunOS 5) these are implemented
#			via a BSD-compatibility library and appear in 
#			section 3.
#	LIB_C_EXT = extension for C library routines (eg, signal)
#
SYSCALL_EXT = 2
SYSCALL_EXT_DIR = ${SYSCALL_EXT}
BSD_SYSCALL_EXT = 2
BSD_SYSCALL_EXT_DIR = ${BSD_SYSCALL_EXT}
LIB_C_EXT = 3
LIB_C_EXT_DIR = ${LIB_C_EXT}

#
# Platform specific assignments start here:
#

#
# (CRAY)
#

#
# (DEC AXP OSF/1)
#
#DESTMAN= /usr/share/man
#MANDIR = man
#CATEXT = $$N
#MAN_OWNER = -o root
#MAN_GROUP = -g root
#INSTALL = installbsd
#MANROFF = cat
## Extensions for DEC AXP OSF/1 manual entries
#CMD_EXT = 1
#SYS_OPS_EXT = 8
#LIB_NETWORK_EXT = 3
#FORMAT_EXT = 4
#DESC_EXT = 5
#
#SYSCALL_EXT = 2
#BSD_SYSCALL_EXT = 2
#LIB_C_EXT = 3

#
# (irix4)
#

#
# (irix5)
#

#
# (sunos4.x)
#

#
# (ULTRIX, sunos, other 4.[23]bsd-alikes)
#
#DESTMAN= /usr/man
#MANDIR = man
#CATEXT = $$N
#MAN_OWNER = -o root
#MAN_GROUP = -g root
#INSTALL = install
#MANROFF = cat
## Extensions for ULTRIX, sunos, other 4.[23]bsd-alikes manual entries
#CMD_EXT = 1
#SYS_OPS_EXT = 8
#LIB_NETWORK_EXT = 3n
#LIB_NETWORK_EXT_DIR = 3
#FORMAT_EXT = 5
#DESC_EXT = 7
#
#SYSCALL_EXT = 2
#BSD_SYSCALL_EXT = 2
#LIB_C_EXT = 3

#
# SunOS 5.x (Solaris 2.x)
#
#DESTMAN= /usr/share/man
#MANDIR = man
#CATEXT = $$N
#MAN_OWNER = -o bin
#MAN_GROUP = -g bin
#INSTALL = /usr/ucb/install 
#MANROFF = cat
#INDOT = in.
#XFER_INDOT =
## Extensions for Solaris 2.x manual entries
#CMD_EXT = 1
#SYS_OPS_EXT = 1m
#LIB_NETWORK_EXT = 3n
#FORMAT_EXT = 4
#DESC_EXT = 5
#
#SYSCALL_EXT = 2
#BSD_SYSCALL_EXT = 3b
#LIB_C_EXT = 3c

#
# (hpux9.0)
#

#
# (apollo domainos)
#

#
# (AIX3)
#

#
# (ConvexOS-10.x)
#

#
# (NEC EWS4800 EWS-UX/V Rel4.0/Rel4.2)
#

#
# SCO Unix 3.4.2 / ODT 3.0
#

#
# (NeXTstep 2.1 and 3.0)
#

#
# (Sequent Dynix/PTX)
#

######################################################################
#
# No user changes needed past this point.
#
######################################################################
#
# This sed command is used to update the manual entries so they refer to 
# the appropriate section of the manual for a given platform.
#
EXT_SED_CMD =	INDOT_U=`echo "${INDOT}"|tr "[a-z]" "[A-Z]"`; \
		export INDOT_U; \
		XFER_INDOT_U=`echo "${XFER_INDOT}"|tr "[a-z]" "[A-Z]"`; \
		export XFER_INDOT_U; \
		CMD_EXT_U=`echo "${CMD_EXT}"|tr "[a-z]" "[A-Z]"`; \
		export CMD_EXT_U; \
		SYS_OPS_EXT_U=`echo "${SYS_OPS_EXT}"|tr "[a-z]" "[A-Z]"`; \
		export SYS_OPS_EXT_U; \
		LIB_NETWORK_EXT_U=`echo "${LIB_NETWORK_EXT}"|tr "[a-z]" "[A-Z]"`; \
		export LIB_NETWORK_EXT_U; \
		FORMAT_EXT_U=`echo "${FORMAT_EXT}"|tr "[a-z]" "[A-Z]"`; \
		export FORMAT_EXT_U; \
		DESC_EXT_U=`echo "${DESC_EXT}"|tr "[a-z]" "[A-Z]"`; \
		export DESC_EXT_U; \
		SYSCALL_EXT_U=`echo "${SYSCALL_EXT}"|tr "[a-z]" "[A-Z]"`; \
		export SYSCALL_EXT_U; \
		BSD_SYSCALL_EXT_U=`echo "${BSD_SYSCALL_EXT}"|tr "[a-z]" "[A-Z]"`; \
		export BSD_SYSCALL_EXT_U; \
		LIB_C_EXT_U=`echo "${LIB_C_EXT}"|tr "[a-z]" "[A-Z]"`; \
		export LIB_C_EXT_U; \
		sed -e "s/@INDOT@/${INDOT}/g" \
		-e "s/@INDOT_U@/$${INDOT_U}/g" \
		-e "s/@XFER_INDOT@/${XFER_INDOT}/g" \
		-e "s/@XFER_INDOT_U@/$${XFER_INDOT_U}/g" \
		-e "s/@CMD_EXT@/${CMD_EXT}/g" \
		-e "s/@CMD_EXT_U@/$${CMD_EXT_U}/g" \
		-e "s/@LIB_NETWORK_EXT@/${LIB_NETWORK_EXT}/g" \
		-e "s/@LIB_NETWORK_EXT_U@/$${LIB_NETWORK_EXT_U}/g" \
		-e "s/@FORMAT_EXT@/${FORMAT_EXT}/g" \
		-e "s/@FORMAT_EXT_U@/$${FORMAT_EXT_U}/g" \
		-e "s/@DESC_EXT@/${DESC_EXT}/g" \
		-e "s/@DESC_EXT_U@/$${DESC_EXT_U}/g" \
		-e "s/@SYS_OPS_EXT@/${SYS_OPS_EXT}/g" \
		-e "s/@SYS_OPS_EXT_U@/$${SYS_OPS_EXT_U}/g" \
		-e "s/@SYSCALL_EXT@/${SYSCALL_EXT}/g" \
		-e "s/@SYSCALL_EXT_U@/$${SYSCALL_EXT_U}/g" \
		-e "s/@BSD_SYSCALL_EXT@/${BSD_SYSCALL_EXT}/g" \
		-e "s/@BSD_SYSCALL_EXT_U@/$${BSD_SYSCALL_EXT_U}/g" \
		-e "s/@LIB_C_EXT@/${LIB_C_EXT}/g" \
		-e "s/@LIB_C_EXT_U@/$${LIB_C_EXT_U}/g"

#
# Command used to produce manual entries
#
MK_MANFILE = ( ${EXT_SED_CMD} | ${MANROFF} )

#
# Extensions for the generated manual entries
#
CMD_OUT_EXT = out${CMD_EXT}
LIB_NETWORK_OUT_EXT = out${LIB_NETWORK_EXT}
FORMAT_OUT_EXT = out${FORMAT_EXT}
DESC_OUT_EXT = out${DESC_EXT}
SYS_OPS_OUT_EXT = out${SYS_OPS_EXT}

#
# User command manual entries
#
CMD_BASE = 	dig host dnsquery
CMD_SRC_EXT = 	1
CMD_SRC = 	dig.${CMD_SRC_EXT} host.${CMD_SRC_EXT} dnsquery.${CMD_SRC_EXT}
CMD_OUT =	dig.${CMD_OUT_EXT} host.${CMD_OUT_EXT} dnsquery.${CMD_OUT_EXT}

#
# named manual entries
#
NAMED_BASE =	named named.reload named.restart ndc
SYS_OPS_SRC_EXT = 8
NAMED_SRC = 	named.${SYS_OPS_SRC_EXT} named.reload.${SYS_OPS_SRC_EXT} \
		named.restart.${SYS_OPS_SRC_EXT} ndc.${SYS_OPS_SRC_EXT}
NAMED_OUT = 	named.${SYS_OPS_OUT_EXT} named.reload.${SYS_OPS_OUT_EXT} \
		named.restart.${SYS_OPS_OUT_EXT} ndc.${SYS_OPS_OUT_EXT}

#
# named-xfer manual entry
#
NAMED_XFER_BASE = named-xfer
NAMED_XFER_SRC = named-xfer.${SYS_OPS_SRC_EXT}
NAMED_XFER_OUT = named-xfer.${SYS_OPS_OUT_EXT}

#
# nslookup manual entry
#
NSLOOKUP_BASE = nslookup
NSLOOKUP_SRC = 	nslookup.${SYS_OPS_SRC_EXT}
NSLOOKUP_OUT = 	nslookup.${SYS_OPS_OUT_EXT}

#
# Network library routines manual entries
#
LIB_NETWORK_BASE =	gethostbyname resolver getnetent
LIB_NETWORK_SRC_EXT =	3
LIB_NETWORK_SRC = 	gethostbyname.${LIB_NETWORK_SRC_EXT} \
			resolver.${LIB_NETWORK_SRC_EXT} \
			getnetent.${LIB_NETWORK_SRC_EXT}
LIB_NETWORK_OUT =	gethostbyname.${LIB_NETWORK_OUT_EXT} \
			resolver.${LIB_NETWORK_OUT_EXT} \
			getnetent.${LIB_NETWORK_OUT_EXT}

#
# File format manual entries
#
FORMAT_BASE =	resolver
FORMAT_SRC_EXT = 5
FORMAT_SRC = 	resolver.${FORMAT_SRC_EXT}
FORMAT_OUT = 	resolver.${FORMAT_OUT_EXT}

#
# Feature Description manual entries
#
DESC_BASE =	hostname mailaddr
DESC_SRC_EXT = 	7
DESC_SRC = 	hostname.${DESC_SRC_EXT} mailaddr.${DESC_SRC_EXT}
DESC_OUT = 	hostname.${DESC_OUT_EXT} mailaddr.${DESC_OUT_EXT}

.SUFFIXES:	.${CMD_SRC_EXT} .${CMD_OUT_EXT} \
		.${SYS_OPS_SRC_EXT} .${SYS_OPS_OUT_EXT} \
		.${LIB_NETWORK_SRC_EXT} .${LIB_NETWORK_OUT_EXT} \
		.${FORMAT_SRC_EXT} .${FORMAT_OUT_EXT} \
		.${DESC_SRC_EXT} .${DESC_OUT_EXT} 

.${CMD_SRC_EXT}.${CMD_OUT_EXT}:
	${MK_MANFILE} <$*.${CMD_SRC_EXT} >$*.${CMD_OUT_EXT}

.${SYS_OPS_SRC_EXT}.${SYS_OPS_OUT_EXT}:
	${MK_MANFILE} <$*.${SYS_OPS_SRC_EXT} >$*.${SYS_OPS_OUT_EXT}

.${LIB_NETWORK_SRC_EXT}.${LIB_NETWORK_OUT_EXT}:
	${MK_MANFILE} <$*.${LIB_NETWORK_SRC_EXT} >$*.${LIB_NETWORK_OUT_EXT}

.${FORMAT_SRC_EXT}.${FORMAT_OUT_EXT}:
	${MK_MANFILE} <$*.${FORMAT_SRC_EXT} >$*.${FORMAT_OUT_EXT}

.${DESC_SRC_EXT}.${DESC_OUT_EXT}:
	${MK_MANFILE} <$*.${DESC_SRC_EXT} >$*.${DESC_OUT_EXT}

OUTFILES =	${CMD_OUT} ${NAMED_OUT} ${NAMED_XFER_OUT} ${NSLOOKUP_OUT} \
		${LIB_NETWORK_OUT} ${FORMAT_OUT} ${DESC_OUT}

all:	${OUTFILES}

install:	${OUTFILES} \
		${DESTDIR}${DESTMAN}/${MANDIR}${CMD_EXT_DIR} \
		${DESTDIR}${DESTMAN}/${MANDIR}${SYS_OPS_EXT_DIR} \
		${DESTDIR}${DESTMAN}/${MANDIR}${LIB_NETWORK_EXT_DIR} \
		${DESTDIR}${DESTMAN}/${MANDIR}${FORMAT_EXT_DIR} \
		${DESTDIR}${DESTMAN}/${MANDIR}${DESC_EXT_DIR}
	@set -x; N=${CMD_EXT}; for f in ${CMD_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${CMD_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${CMD_EXT_DIR}/$${f}.${CATEXT}; \
	done
	@set -x; N=${SYS_OPS_EXT}; for f in ${NAMED_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${SYS_OPS_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${SYS_OPS_EXT_DIR}/${INDOT}$${f}.${CATEXT}; \
	done
	@set -x; N=${SYS_OPS_EXT}; for f in ${NAMED_XFER_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${SYS_OPS_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${SYS_OPS_EXT_DIR}/${XFER_INDOT}$${f}.${CATEXT}; \
	done
	@set -x; N=${SYS_OPS_EXT}; for f in ${NSLOOKUP_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${SYS_OPS_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${SYS_OPS_EXT_DIR}/$${f}.${CATEXT}; \
	done
	@set -x; N=${LIB_NETWORK_EXT}; for f in ${LIB_NETWORK_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${LIB_NETWORK_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${LIB_NETWORK_EXT_DIR}/$${f}.${CATEXT}; \
	done
	@set -x; N=${FORMAT_EXT}; for f in ${FORMAT_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${FORMAT_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${FORMAT_EXT_DIR}/$${f}.${CATEXT}; \
	done
	@set -x; N=${DESC_EXT}; for f in ${DESC_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${DESC_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${DESC_EXT_DIR}/$${f}.${CATEXT}; \
	done

${DESTDIR}${DESTMAN}/${MANDIR}${CMD_EXT_DIR} \
${DESTDIR}${DESTMAN}/${MANDIR}${SYS_OPS_EXT_DIR} \
${DESTDIR}${DESTMAN}/${MANDIR}${LIB_NETWORK_EXT_DIR} \
${DESTDIR}${DESTMAN}/${MANDIR}${FORMAT_EXT_DIR} \
${DESTDIR}${DESTMAN}/${MANDIR}${DESC_EXT_DIR}:
	mkdir $@

depend:

clean:
	rm -f *~ *.BAK *.CKP *.orig
	rm -f ${OUTFILES}
