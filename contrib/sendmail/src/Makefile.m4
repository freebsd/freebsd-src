#
#  This Makefile is designed to work on any reasonably current version of
#  "make" program.
#
#	@(#)Makefile.m4	8.23 (Berkeley) 6/16/98
#

# C compiler
CC=	confCC

# Shell
SHELL=	confSHELL

# use O=-O (usual) or O=-g (debugging)
O=	ifdef(`confOPTIMIZE', `confOPTIMIZE', `-O')

# location of sendmail source directory
SRCDIR=	.

# define the database mechanisms available for map & alias lookups:
#	-DNDBM -- use new DBM
#	-DNEWDB -- use new Berkeley DB
#	-DNIS -- include NIS support
# The really old (V7) DBM library is no longer supported.
# See README for a description of how these flags interact.
#
MAPDEF=	ifdef(`confMAPDEF', `confMAPDEF')

# environment definitions (e.g., -D_AIX3)
ENVDEF=	ifdef(`confENVDEF', `confENVDEF')

# see also conf.h for additional compilation flags

# include directories
INCDIRS=confINCDIRS

# loader options
LDOPTS=	ifdef(`confLDOPTS', `confLDOPTS')

# library directories
LIBDIRS=confLIBDIRS

# libraries required on your system
#  delete -l44bsd if you are not running BIND 4.9.x
LIBS=	ifdef(`confLIBS', `confLIBS')

# location of sendmail binary (usually /usr/sbin or /usr/lib)
BINDIR=	${DESTDIR}ifdef(`confMBINDIR', `confMBINDIR', `/usr/sbin')

# location of "user" binaries (usually /usr/bin or /usr/ucb)
UBINDIR=${DESTDIR}ifdef(`confUBINDIR', `confUBINDIR', `/usr/bin')

# location of sendmail.st file (usually /var/log or /usr/lib)
STDIR=	${DESTDIR}ifdef(`confSTDIR', `confSTDIR', `/var/log')

# location of sendmail.hf file (usually /usr/share/misc or /usr/lib)
HFDIR=	${DESTDIR}ifdef(`confHFDIR', `confHFDIR', `/usr/share/misc')

# additional .o files needed
OBJADD=	ifdef(`confOBJADD', `confOBJADD') ifdef(`confSMOBJADD', `confSMOBJADD')

undivert(1)

###################  end of user configuration flags  ######################

BUILDBIN=confBUILDBIN
COPTS=	-I. ${INCDIRS} ${MAPDEF} ${ENVDEF}
CFLAGS=	$O ${COPTS}

BEFORE= confBEFORE
OBJS=	alias.o arpadate.o clock.o collect.o conf.o convtime.o daemon.o \
	deliver.o domain.o envelope.o err.o headers.o macro.o main.o \
	map.o mci.o mime.o parseaddr.o queue.o readcf.o recipient.o \
	safefile.o savemail.o snprintf.o srvrsmtp.o stab.o stats.o \
	sysexits.o trace.o udb.o usersmtp.o util.o version.o ${OBJADD}

LINKS=	ifdef(`confLINKS', `confLINKS',
	`${UBINDIR}/newaliases \
	${UBINDIR}/mailq \
	${UBINDIR}/hoststat \
	${UBINDIR}/purgestat')

NROFF=	ifdef(`confNROFF', `confNROFF', `groff -Tascii')
MANDOC=	ifdef(`confMANDOC', `confMANDOC', `-mandoc')

INSTALL=ifdef(`confINSTALL', `confINSTALL', `install')
BINOWN=	ifdef(`confSBINOWN', `confSBINOWN', `root')
BINGRP=	ifdef(`confSBINGRP', `confSBINGRP', `kmem')
BINMODE=ifdef(`confSBINMODE', `confSBINMODE', `4555')

MANOWN=	ifdef(`confMANOWN', `confMANOWN', `bin')
MANGRP=	ifdef(`confMANGRP', `confMANGRP', `bin')
MANMODE=ifdef(`confMANMODE', `confMANMODE', `444')

MANROOT=${DESTDIR}ifdef(`confMANROOT', `confMANROOT', `/usr/share/man/cat')
MAN1=	${MANROOT}ifdef(`confMAN1', `confMAN1', `1')
MAN1EXT=ifdef(`confMAN1EXT', `confMAN1EXT', `1')
MAN1SRC=ifdef(`confMAN1SRC', `confMAN1SRC', `0')
MAN5=	${MANROOT}ifdef(`confMAN5', `confMAN5', `5')
MAN5EXT=ifdef(`confMAN5EXT', `confMAN5EXT', `5')
MAN5SRC=ifdef(`confMAN5SRC', `confMAN5SRC', `0')
MAN8=	${MANROOT}ifdef(`confMAN8', `confMAN8', `8')
MAN8EXT=ifdef(`confMAN8EXT', `confMAN8EXT', `8')
MAN8SRC=ifdef(`confMAN8SRC', `confMAN8SRC', `0')

ALL=	sendmail aliases.${MAN5SRC} mailq.${MAN1SRC} newaliases.${MAN1SRC} sendmail.${MAN8SRC}

all: ${ALL}

sendmail: ${BEFORE} ${OBJS}
	${CC} -o sendmail ${LDOPTS} ${LIBDIRS} ${OBJS} ${LIBS}
	cp /dev/null sendmail.st

undivert(3)

aliases.${MAN5SRC}: aliases.5
	${NROFF} ${MANDOC} aliases.5 > aliases.${MAN5SRC}

mailq.${MAN1SRC}: mailq.1
	${NROFF} ${MANDOC} mailq.1 > mailq.${MAN1SRC}

newaliases.${MAN1SRC}: newaliases.1
	${NROFF} ${MANDOC} newaliases.1 > newaliases.${MAN1SRC}

sendmail.${MAN8SRC}: sendmail.8
	${NROFF} ${MANDOC} sendmail.8 > sendmail.${MAN8SRC}

install: install-sendmail install-docs

install-sendmail: sendmail
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} sendmail ${BINDIR}
	for i in ${LINKS}; do rm -f $$i; ln -s ${BINDIR}/sendmail $$i; done
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 444 sendmail.hf ${HFDIR}
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m 644 sendmail.st \
	    ${STDIR}/sendmail.st

install-docs: aliases.${MAN5SRC} mailq.${MAN1SRC} newaliases.${MAN1SRC} sendmail.${MAN8SRC}
ifdef(`confNO_MAN_INSTALL', `dnl',
`	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} sendmail.${MAN8SRC} ${MAN8}/sendmail.${MAN8EXT}
	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} aliases.${MAN5SRC} ${MAN5}/aliases.${MAN5EXT}
	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} mailq.${MAN1SRC} ${MAN1}/mailq.${MAN1EXT}
	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} newaliases.${MAN1SRC} ${MAN1}/newaliases.${MAN1EXT}')

clean:
	rm -f ${OBJS} sendmail aliases.${MAN5SRC} mailq.${MAN1SRC} newaliases.${MAN1SRC} sendmail.${MAN8SRC}

################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE', `generic').m4)dnl
################  End of dependency scripts
