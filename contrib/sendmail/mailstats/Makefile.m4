#
#  This Makefile is designed to work on the old "make" program.
#
#	@(#)Makefile.m4	8.14	(Berkeley)	6/4/98
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

# see also conf.h for additional compilation flags

# include directories
INCDIRS=-I${SRCDIR} confINCDIRS

# loader options
LDOPTS=	ifdef(`confLDOPTS', `confLDOPTS')

# library directories
LIBDIRS=confLIBDIRS

# libraries required on your system
LIBS=	ifdef(`confLIBS', `confLIBS')

# location of mailstats binary (usually /usr/sbin or /usr/etc)
BINDIR=	${DESTDIR}ifdef(`confSBINDIR', `confSBINDIR', `/usr/sbin')

# additional .o files needed
OBJADD=	ifdef(`confOBJADD', `confOBJADD')

undivert(1)

###################  end of user configuration flags  ######################

BUILDBIN=confBUILDBIN
COPTS=	-I. ${INCDIRS} ${ENVDEF}
CFLAGS=	$O ${COPTS}

BEFORE= confBEFORE
OBJS=	mailstats.o ${OBJADD}

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

ALL=	mailstats mailstats.${MAN8SRC}

all: ${ALL}

mailstats: ${BEFORE} ${OBJS}
	${CC} -o mailstats ${LDOPTS} ${OBJS} ${LIBDIRS} ${LIBS}

undivert(3)

mailstats.${MAN8SRC}: mailstats.8
	${NROFF} ${MANDOC} mailstats.8 > mailstats.${MAN8SRC}

install: install-mailstats install-docs

install-mailstats: mailstats
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} mailstats ${BINDIR}

install-docs: mailstats.${MAN8SRC}
ifdef(`confNO_MAN_INSTALL', `dnl',
`	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} mailstats.${MAN8SRC} ${MAN8}/mailstats.${MAN8EXT}')

clean:
	rm -f ${OBJS} mailstats mailstats.${MAN8SRC}

################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE',
`generic').m4)dnl
################  End of dependency scripts
