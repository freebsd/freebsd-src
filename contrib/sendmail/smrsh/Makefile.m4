#
#  This Makefile is designed to work on the old "make" program.
#
#	@(#)Makefile.m4	8.14	(Berkeley)	7/12/1998
#

# C compiler
CC=	confCC

# Shell
SHELL=	confSHELL

# use O=-O (usual) or O=-g (debugging)
O=	ifdef(`confOPTIMIZE', `confOPTIMIZE', `-O')

# location of sendmail source directory
SRCDIR=	ifdef(`confSRCDIR', `confSRCDIR', `../../src')

# environment definitions (e.g., -D_AIX3)
ENVDEF=	ifdef(`confENVDEF', `confENVDEF')

# include directories
INCDIRS=-I${SRCDIR} confINCDIRS

# loader options
LDOPTS=	ifdef(`confLDOPTS', `confLDOPTS')

# library directories
LIBDIRS=confLIBDIRS

# libraries required on your system
LIBS=	ifdef(`confLIBS', `confLIBS')

# location of smrsh binary (usually /usr/libexec or /usr/etc)
EBINDIR=${DESTDIR}ifdef(`confEBINDIR', `confEBINDIR', `/usr/libexec')

# additional .o files needed
OBJADD=	ifdef(`confOBJADD', `confOBJADD')

undivert(1)

###################  end of user configuration flags  ######################

BUILDBIN=confBUILDBIN
COPTS=	-I. ${INCDIRS} ${ENVDEF}
CFLAGS=	$O ${COPTS}

BEFORE= confBEFORE
OBJS=	smrsh.o ${OBJADD}

# Which *roff program has -mandoc support
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

ALL=	smrsh smrsh.${MAN8SRC}

all: ${ALL}

smrsh: ${BEFORE} ${OBJS}
	${CC} -o smrsh ${LDOPTS} ${OBJS} ${LIBDIRS} ${LIBS}

undivert(3)

smrsh.${MAN8SRC}: smrsh.8
	${NROFF} ${MANDOC} smrsh.8 > smrsh.${MAN8SRC}

install: install-smrsh install-docs

install-smrsh: smrsh
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} smrsh ${EBINDIR}

install-docs: smrsh.${MAN8SRC}
ifdef(`confNO_MAN_INSTALL', `dnl',
`	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} smrsh.${MAN8SRC} ${MAN8}/smrsh.${MAN8EXT}')

clean:
	rm -f ${OBJS} smrsh smrsh.${MAN8SRC}

################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE',
`generic').m4)dnl
################  End of dependency scripts
