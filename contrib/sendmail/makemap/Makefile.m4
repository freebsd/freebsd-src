#
#  This Makefile is designed to work on the old "make" program.
#
#	@(#)Makefile.m4	8.21	(Berkeley)	7/12/1998
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
ENVDEF= -DNOT_SENDMAIL ifdef(`confENVDEF', `confENVDEF')

# see also conf.h for additional compilation flags

# include directories
INCDIRS=-I${SRCDIR} confINCDIRS

# loader options
LDOPTS=	ifdef(`confLDOPTS', `confLDOPTS')

# library directories
LIBDIRS=confLIBDIRS

# libraries required on your system
LIBS=	ifdef(`confLIBS', `confLIBS')

# location of makemap binary (usually /usr/sbin or /usr/etc)
SBINDIR=${DESTDIR}ifdef(`confSBINDIR', `confSBINDIR', `/usr/sbin')

# additional .o files needed
OBJADD=	ifdef(`confOBJADD', `confOBJADD')

undivert(1)

###################  end of user configuration flags  ######################

BUILDBIN=confBUILDBIN
COPTS=	-I. ${INCDIRS} ${MAPDEF} ${ENVDEF}
CFLAGS=	$O ${COPTS}

BEFORE=	confBEFORE safefile.c snprintf.c
OBJS=	makemap.o safefile.o snprintf.o ${OBJADD}

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

ALL=	makemap makemap.${MAN8SRC}

all: ${ALL}

makemap: ${BEFORE} ${OBJS}
	${CC} -o makemap ${LDOPTS} ${OBJS} ${LIBDIRS} ${LIBS}

safefile.c: ${SRCDIR}/safefile.c
	-ln -s ${SRCDIR}/safefile.c safefile.c

snprintf.c: ${SRCDIR}/snprintf.c
	-ln -s ${SRCDIR}/snprintf.c snprintf.c

undivert(3)

makemap.${MAN8SRC}: makemap.8
	${NROFF} ${MANDOC} makemap.8 > makemap.${MAN8SRC}

install: install-makemap install-docs

install-makemap: makemap
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} makemap ${SBINDIR}

install-docs: makemap.${MAN8SRC}
ifdef(`confNO_MAN_INSTALL', `dnl',
`	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} makemap.${MAN8SRC} ${MAN8}/makemap.${MAN8EXT}')

clean:
	rm -f ${OBJS} makemap makemap.${MAN8SRC}

################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE', 
`generic').m4)dnl
################  End of dependency scripts
