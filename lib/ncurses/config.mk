# $FreeBSD$

# This Makefile is shared by libncurses, libform, libmenu, libpanel.

NCURSES_DIR=	${SRCTOP}/contrib/ncurses
NCURSES_MAJOR=	6
NCURSES_MINOR=	2
NCURSES_PATCH=	20210220

CFLAGS+=	-D_XOPEN_SOURCE_EXTENDED
NCURSES_CFG_H=	${.CURDIR}/ncurses_cfg.h

CFLAGS+=	-I.
CFLAGS+=	-I${.CURDIR:H}/tinfo

# for ${NCURSES_CFG_H}
CFLAGS+=	-I${.CURDIR:H}/ncurses

CFLAGS+=	-I${NCURSES_DIR}/include
CFLAGS+=	-I${NCURSES_DIR}/ncurses
CFLAGS+=	-I${.OBJDIR:H}/tinfo/

CFLAGS+=	-Wall

CFLAGS+=	-DNDEBUG

CFLAGS+=	-DHAVE_CONFIG_H

# everyone needs this
.PATH:		${NCURSES_DIR}/include
.PATH:		${.OBJDIR:H}/tinfo/

# tools and directories
AWK?=		awk
TERMINFODIR?=	${SHAREDIR}/misc

# Generate headers
ncurses_def.h:	MKncurses_def.sh ncurses_defs
	AWK=${AWK} sh ${NCURSES_DIR}/include/MKncurses_def.sh \
	    ${NCURSES_DIR}/include/ncurses_defs > ncurses_def.h

# Manual pages filter
MANFILTER=	sed -e 's%@TERMINFO@%${TERMINFODIR}/terminfo%g' \
		    -e 's%@DATADIR@%/usr/share%g' \
		    -e 's%@NCURSES_OSPEED@%${NCURSES_OSPEED}%g' \
		    -e 's%@NCURSES_MAJOR@%${NCURSES_MAJOR}%g' \
		    -e 's%@NCURSES_MINOR@%${NCURSES_MINOR}%g' \
		    -e 's%@NCURSES_PATCH@%${NCURSES_PATCH}%g' \
		    -e 's%@TPUT@%tput%g' \
		    -e 's%@TSET@%tset%g' \
		    -e 's%@RESET@%reset%g' \
		    -e 's%@CLEAR@%clear%g' \
		    -e 's%@TABS@%tabs%g' \
		    -e 's%@TIC@%tic%g' \
		    -e 's%@TOE@%toe%g' \
		    -e 's%@INFOCMP@%infocmp%g' \
		    -e 's%@CAPTOINFO@%captoinfo%g' \
		    -e 's%@INFOTOCAP@%infotocap%g'
