#
#  This Makefile is designed to work on the old "make" program.
#
#	@(#)Makefile.m4	8.16	(Berkeley)	7/12/1998
#

# C compiler
CC=	confCC

# Shell
SHELL=	confSHELL

# use O=-O (usual) or O=-g (debugging)
O=	ifdef(`confOPTIMIZE', `confOPTIMIZE', `-O')

# location of sendmail source directory
SRCDIR=	ifdef(`confSRCDIR', `confSRCDIR', `../../src')

# define the database mechanisms available for map & alias lookups:
#	-DNDBM -- use new DBM
#	-DNEWDB -- use new Berkeley DB
# The really old (V7) DBM library is no longer supported.
#
MAPDEF=	ifdef(`confMAPDEF', `confMAPDEF')

# environment definitions (e.g., -D_AIX3)
ENVDEF=	ifdef(`confENVDEF', `confENVDEF')

# see also conf.h for additional compilation flags

# include directories
INCDIRS=-I${SRCDIR} confINCDIRS

# loader options
LDOPTS=	ifdef(`confLDOPTS', `confLDOPTS')

# library directories
LIBDIRS=confLIBDIRS

# libraries required on your system
LIBS=	ifdef(`confLIBS', `confLIBS')

# location of praliases binary (usually /usr/sbin or /usr/etc)
SBINDIR=${DESTDIR}ifdef(`confSBINDIR', `confSBINDIR', `/usr/sbin')

# additional .o files needed
OBJADD=	ifdef(`confOBJADD', `confOBJADD')

undivert(1)

###################  end of user configuration flags  ######################

BUILDBIN=confBUILDBIN
COPTS=	-I. ${INCDIRS} ${MAPDEF} ${ENVDEF}
CFLAGS=	$O ${COPTS}

BEFORE= confBEFORE
OBJS=	praliases.o ${OBJADD}

NROFF=	ifdef(`confNROFF', `confNROFF', `groff -Tascii')
MANDOC=	ifdef(`confMANDOC', `confMANDOC', `-mandoc')

INSTALL=ifdef(`confINSTALL', `confINSTALL', `install')
BINOWN=	ifdef(`confUBINOWN', `confUBINOWN', `bin')
BINGRP=	ifdef(`confUBINGRP', `confUBINGRP', `bin')
BINMODE=ifdef(`confUBINMODE', `confUBINMODE', `555')

MANOWN=	ifdef(`confMANOWN', `confMANOWN', `bin')
MANGRP=	ifdef(`confMANGRP', `confMANGRP', `bin')
MANMODE=ifdef(`confMANMODE', `confMANMODE', `444')

MANROOT=${DESTDIR}ifdef(`confMANROOT', `confMANROOT', `/usr/share/man/cat')
MAN8=	${MANROOT}ifdef(`confMAN8', `confMAN8', `8')
MAN8EXT=ifdef(`confMAN8EXT', `confMAN8EXT', `8')
MAN8SRC=ifdef(`confMAN8SRC', `confMAN8SRC', `0')

ALL=	praliases praliases.${MAN8SRC}

all: ${ALL}

praliases: ${BEFORE} ${OBJS}
	${CC} -o praliases ${LDOPTS} ${OBJS} ${LIBDIRS} ${LIBS}

undivert(3)

praliases.${MAN8SRC}: praliases.8
	${NROFF} ${MANDOC} praliases.8 > praliases.${MAN8SRC}

install: install-praliases install-docs

install-praliases: praliases
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} praliases ${SBINDIR}

install-docs: praliases.${MAN8SRC}
ifdef(`confNO_MAN_INSTALL', `dnl',
`	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} praliases.${MAN8SRC} ${MAN8}/praliases.${MAN8EXT}')

clean:
	rm -f ${OBJS} praliases praliases.${MAN8SRC}

################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE',
`generic').m4)dnl
################  End of dependency scripts
