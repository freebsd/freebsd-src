# $FreeBSD$

.include "../Buildfile.inc"

NCURSES_DIR = ${BSDSRCTOP}/contrib/ncurses

CFLAGS += -I.

# for ${NCURSES_CFG_H}
CFLAGS += -I${BSDSRCTOP}/lib/ncurses
CFLAGS += -I${NCURSES_DIR}/include
CFLAGS += -I${NCURSES_DIR}/ncurses
CFLAGS += -Wall
CFLAGS += -DNDEBUG
CFLAGS += -DHAVE_CONFIG_H

# everyone needs this
.PATH: ${NCURSES_DIR}/include

AWK ?= awk
