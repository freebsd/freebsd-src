#
#  This Makefile is designed to work on the old "make" program.
#
#	@(#)Makefile.m4	8.21	(Berkeley)	6/4/98
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

# location of mail.local binary (usually /usr/sbin or /usr/etc)
BINDIR=	${DESTDIR}ifdef(`confEBINDIR', `confEBINDIR', `/usr/libexec')

# additional .o files needed
OBJADD=	ifdef(`confOBJADD', `confOBJADD')

undivert(1)

###################  end of user configuration flags  ######################

BUILDBIN=confBUILDBIN
COPTS=	-I. ${INCDIRS} ${ENVDEF}
CFLAGS=	$O ${COPTS}

BEFORE=	snprintf.c confBEFORE
OBJS=	mail.local.o snprintf.o ${OBJADD}

NROFF=	ifdef(`confNROFF', `confNROFF', `groff -Tascii')
MANDOC=	ifdef(`confMANDOC', `confMANDOC', `-mandoc')

INSTALL=ifdef(`confINSTALL', `confINSTALL', `install')
BINOWN=	ifdef(`confSBINOWN', `confSBINOWN', `root')
BINGRP=	ifdef(`confSBINGRP', `confSBINGRP', `bin')
BINMODE=ifdef(`confSBINMODE', `confSBINMODE', `4555')

MANOWN=	ifdef(`confMANOWN', `confMANOWN', `bin')
MANGRP=	ifdef(`confMANGRP', `confMANGRP', `bin')
MANMODE=ifdef(`confMANMODE', `confMANMODE', `444')

MANROOT=${DESTDIR}ifdef(`confMANROOT', `confMANROOT', `/usr/share/man/cat')
MAN8=	${MANROOT}ifdef(`confMAN8', `confMAN8', `8')
MAN8EXT=ifdef(`confMAN8EXT', `confMAN8EXT', `8')
MAN8SRC=ifdef(`confMAN8SRC', `confMAN8SRC', `0')

ALL=	mail.local mail.local.${MAN8SRC}

all: ${ALL}

mail.local: ${BEFORE} ${OBJS}
	${CC} -o mail.local ${LDOPTS} ${OBJS} ${LIBDIRS} ${LIBS}

snprintf.c: ${SRCDIR}/snprintf.c
	-ln -s ${SRCDIR}/snprintf.c snprintf.c

undivert(3)

mail.local.${MAN8SRC}: mail.local.8
	${NROFF} ${MANDOC} mail.local.8 > mail.local.${MAN8SRC}

install:
	@echo "NOTE: This version of mail.local is not suited for some operating"
	@echo "      systems such as HP-UX and Solaris.  Please consult the"
	@echo "      README file in the mail.local directory.  You can force"
	@echo "      the install using '${MAKE} force-install'."

force-install: install-mail.local install-docs

install-mail.local: mail.local
	${INSTALL} -c -o ${BINOWN} -g ${BINGRP} -m ${BINMODE} mail.local ${BINDIR}

install-docs: mail.local.${MAN8SRC}
ifdef(`confNO_MAN_INSTALL', `dnl',
`	${INSTALL} -c -o ${MANOWN} -g ${MANGRP} -m ${MANMODE} mail.local.${MAN8SRC} ${MAN8}/mail.local.${MAN8EXT}')

clean:
	rm -f ${OBJS} mail.local mail.local.${MAN8SRC}

################  Dependency scripts
include(confBUILDTOOLSDIR/M4/depend/ifdef(`confDEPEND_TYPE', `confDEPEND_TYPE', 
`generic').m4)dnl
################  End of dependency scripts
