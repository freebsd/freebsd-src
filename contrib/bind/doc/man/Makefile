## Portions Copyright (c) 1993 by Digital Equipment Corporation.
## 
## Permission to use, copy, modify, and distribute this software for any
## purpose with or without fee is hereby granted, provided that the above
## copyright notice and this permission notice appear in all copies, and that
## the name of Digital Equipment Corporation not be used in advertising or
## publicity pertaining to distribution of the document or software without
## specific, written prior permission.
## 
## THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
## WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
## CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
## PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
## ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.

## Portions Copyright (c) 1996,1999 by Internet Software Consortium
##
## Permission to use, copy, modify, and distribute this software for any
## purpose with or without fee is hereby granted, provided that the above
## copyright notice and this permission notice appear in all copies.
##
## THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM DISCLAIMS
## ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES
## OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL INTERNET SOFTWARE
## CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
## DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
## PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
## ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
## SOFTWARE.

#
# Makefile to install the BIND manual entries.
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
# For systems that store manuals in MDOC form (eg modern BSD systems) and
# can generate catable manual entries on the fly the following assignment
# can be used. 
#	MANROFF = cat
#
MANROFF = ( tbl | nroff -mandoc )

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
OUT_EXT = lst
CMD_OUT_EXT = ${OUT_EXT}${CMD_EXT}
LIB_NETWORK_OUT_EXT = ${OUT_EXT}${LIB_NETWORK_EXT}
FORMAT_OUT_EXT = ${OUT_EXT}${FORMAT_EXT}
DESC_OUT_EXT = ${OUT_EXT}${DESC_EXT}
SYS_OPS_OUT_EXT = ${OUT_EXT}${SYS_OPS_EXT}

#
# User command manual entries
#
CMD_BASE = 	dig host dnsquery dnskeygen
CMD_SRC_EXT = 	1
CMD_SRC = 	dig.${CMD_SRC_EXT} \
		host.${CMD_SRC_EXT} \
		dnsquery.${CMD_SRC_EXT} \
		dnskeygen.${CMD_SRC_EXT}
CMD_OUT =	dig.${CMD_OUT_EXT} \
		host.${CMD_OUT_EXT} \
		dnsquery.${CMD_OUT_EXT} \
		dnskeygen.${CMD_OUT_EXT}

#
# named manual entries
#
NAMED_BASE =	named ndc
SYS_OPS_SRC_EXT = 8
NAMED_SRC = 	named.${SYS_OPS_SRC_EXT} ndc.${SYS_OPS_SRC_EXT}
NAMED_OUT = 	named.${SYS_OPS_OUT_EXT} ndc.${SYS_OPS_OUT_EXT}

#
# named-xfer manual entry
#
NAMED_XFER_BASE = named-xfer
NAMED_XFER_SRC = named-xfer.${SYS_OPS_SRC_EXT}
NAMED_XFER_OUT = named-xfer.${SYS_OPS_OUT_EXT}

#
# named-bootconf manual entry
#
NAMED_BOOTCONF_BASE = named-bootconf
NAMED_BOOTCONF_SRC = named-bootconf.${SYS_OPS_SRC_EXT}
NAMED_BOOTCONF_OUT = named-bootconf.${SYS_OPS_OUT_EXT}

#
# nslookup manual entry
#
NSLOOKUP_BASE = nslookup
NSLOOKUP_SRC = 	nslookup.${SYS_OPS_SRC_EXT}
NSLOOKUP_OUT = 	nslookup.${SYS_OPS_OUT_EXT}

#
# nsupdate manual entry
#
NSUPDATE_BASE = nsupdate
NSUPDATE_SRC = 	nsupdate.${SYS_OPS_SRC_EXT}
NSUPDATE_OUT = 	nsupdate.${SYS_OPS_OUT_EXT}

#
# Network library routines manual entries
#
LIB_NETWORK_BASE =	gethostbyname inet_cidr resolver hesiod getnetent \
			tsig getaddrinfo getnameinfo getipnodebyname
LIB_NETWORK_SRC_EXT =	3
LIB_NETWORK_SRC = 	gethostbyname.${LIB_NETWORK_SRC_EXT} \
			inet_cidr.${LIB_NETWORK_SRC_EXT} \
			resolver.${LIB_NETWORK_SRC_EXT} \
			hesiod.${LIB_NETWORK_SRC_EXT} \
			getnetent.${LIB_NETWORK_SRC_EXT} \
			tsig.${LIB_NETWORK_SRC_EXT} \
			getaddrinfo.${LIB_NETWORK_SRC_EXT} \
			getnameinfo.${LIB_NETWORK_SRC_EXT} \
			getipnodebyname.${LIB_NETWORK_SRC_EXT}
LIB_NETWORK_OUT =	gethostbyname.${LIB_NETWORK_OUT_EXT} \
			inet_cidr.${LIB_NETWORK_OUT_EXT} \
			resolver.${LIB_NETWORK_OUT_EXT} \
			hesiod.${LIB_NETWORK_OUT_EXT} \
			getnetent.${LIB_NETWORK_OUT_EXT} \
			tsig.${LIB_NETWORK_OUT_EXT} \
			getaddrinfo.${LIB_NETWORK_OUT_EXT} \
			getnameinfo.${LIB_NETWORK_OUT_EXT} \
			getipnodebyname.${LIB_NETWORK_OUT_EXT}

#
# File format manual entries
#
FORMAT_BASE =	resolver irs.conf named.conf
FORMAT_SRC_EXT = 5
FORMAT_SRC = 	resolver.${FORMAT_SRC_EXT} \
		irs.conf.${FORMAT_SRC_EXT} \
		named.conf.${FORMAT_SRC_EXT}
FORMAT_OUT = 	resolver.${FORMAT_OUT_EXT} \
		irs.conf.${FORMAT_OUT_EXT} \
		named.conf.${FORMAT_OUT_EXT}

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
	@echo "$*.${CMD_SRC_EXT} -> $*.${CMD_OUT_EXT}"
	@${MK_MANFILE} <$*.${CMD_SRC_EXT} >$*.${CMD_OUT_EXT}

.${SYS_OPS_SRC_EXT}.${SYS_OPS_OUT_EXT}:
	@echo "$*.${SYS_OPS_SRC_EXT} -> $*.${SYS_OPS_OUT_EXT}"
	@${MK_MANFILE} <$*.${SYS_OPS_SRC_EXT} >$*.${SYS_OPS_OUT_EXT}

.${LIB_NETWORK_SRC_EXT}.${LIB_NETWORK_OUT_EXT}:
	@echo "$*.${LIB_NETWORK_SRC_EXT} -> $*.${LIB_NETWORK_OUT_EXT}"
	@${MK_MANFILE} <$*.${LIB_NETWORK_SRC_EXT} >$*.${LIB_NETWORK_OUT_EXT}

.${FORMAT_SRC_EXT}.${FORMAT_OUT_EXT}:
	@echo "$*.${FORMAT_SRC_EXT} -> $*.${FORMAT_OUT_EXT}"
	@${MK_MANFILE} <$*.${FORMAT_SRC_EXT} >$*.${FORMAT_OUT_EXT}

.${DESC_SRC_EXT}.${DESC_OUT_EXT}:
	@echo "$*.${DESC_SRC_EXT} -> $*.${DESC_OUT_EXT}"
	@${MK_MANFILE} <$*.${DESC_SRC_EXT} >$*.${DESC_OUT_EXT}

OUTFILES =	${CMD_OUT} ${NAMED_OUT} ${NAMED_XFER_OUT} ${NSLOOKUP_OUT} \
		${NSUPDATE_OUT} ${LIB_NETWORK_OUT} ${FORMAT_OUT} ${DESC_OUT} \
		${NAMED_BOOTCONF_OUT}

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
	@set -x; N=${SYS_OPS_EXT}; for f in ${NAMED_BOOTCONF_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${SYS_OPS_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${SYS_OPS_EXT_DIR}/${XFER_INDOT}$${f}.${CATEXT}; \
	done
	@set -x; N=${SYS_OPS_EXT}; for f in ${NSLOOKUP_BASE}; do \
		${INSTALL} -c -m 444 ${MAN_OWNER} ${MAN_GROUP} \
		$${f}.${SYS_OPS_OUT_EXT} \
		${DESTDIR}${DESTMAN}/${MANDIR}${SYS_OPS_EXT_DIR}/$${f}.${CATEXT}; \
	done
	@set -x; N=${SYS_OPS_EXT}; for f in ${NSUPDATE_BASE}; do \
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

links: FRC
	@set -ex; ln -s SRC/*.[0-9] .

depend:

clean:
	rm -f *~ *.BAK *.CKP *.orig
	rm -f ${OUTFILES}

FRC:
