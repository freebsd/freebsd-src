#	@(#)Makefile	5.12 (Berkeley) 5/30/93

# EXTRA
#	Incorporates code to gather additional performance statistics
# SYSV
#	Use system III/V (instead of V7) type ioctl calls
# BSD
#	Use BSD specific features (mostly timer and signal stuff)
# BSD4.1
#	Use BSD4.1 to avoid some 4.2 dependencies (must be used with
#	BSD above; do not mix with SYSV)
# HIDEBYLINK
#	If defined, the program attempts to hide from ps
# DOCHECKPOINTS
#	If not defined, checkpoint files are periodically written by the
#	larn process (no forking) if enabled in the .larnopts description
#	file.  Checkpointing is handy on an unreliable system, but takes
#	CPU. Inclusion of DOCHECKPOINTS will cause fork()ing to perform the
#	checkpoints (again if enabled in the .larnopts file).  This usually
#	avoids pauses in larn while the checkpointing is being done (on
#	large machines).
# VER
#	This is the version of the software, example:  12
# SUBVER
#	This is the revision of the software, example:  1
# FLUSHNO=#
#	Set the input queue excess flushing threshold (default 5)
# NOVARARGS
#	Define for systems that don't have varargs (a default varargs will
#	be used).
# MACRORND
#	Define to use macro version of rnd() and rund() (fast and big)
# UIDSCORE
#	Define to use user id's to manage scoreboard.  Leaving this out will
#	cause player id's from the file ".playerids" to be used instead.
#	(.playerids is created upon demand).  Only one entry per id # is
#	allowed in each scoreboard (winning & non-winning).
#  VT100
#	Compile for using vt100 family of terminals.  Omission of this
#	define will cause larn to use termcap, but it will be MUCH slower
#	due to an extra layer of output interpretation.  Also, only VT100
#	mode allows 2 different standout modes, inverse video, and bold video.
#	And only in VT100 mode is the scrolling region of the terminal used
#	(much nicer than insert/delete line sequences to simulate it, if
#	VT100 is omitted).
# NONAP
#	This causes napms() to return immediately instead of delaying n
#	milliseconds.  This define may be needed on some systems if the nap
#	stuff does not work correctly (possible hang).  nap() is primarilly
#	used to delay for effect when casting missile type spells.
# NOLOG
#	Turn off logging.

PROG=	larn
MAN6=	larn.6
CFLAGS+=-DBSD -DVER=12 -DSUBVER=0 -DNONAP -DUIDSCORE -fwritable-strings
SRCS=	main.c object.c create.c tok.c display.c global.c data.c io.c \
	monster.c store.c diag.c help.c config.c nap.c bill.c scores.c \
	signal.c moreobj.c movem.c regen.c fortune.c savelev.c
DPADD=	${LIBTERMCAP} ${LIBCOMPAT}
LDADD=	-ltermcap -lcompat
HIDEGAME=hidegame

beforeinstall:
	(cd ${.CURDIR}/datfiles; ${INSTALL} -c -o ${BINOWN} -g ${BINGRP} \
	    -m 444 larnmaze larnopts larn.help \
	    ${DESTDIR}/usr/share/games/larn)

.include <bsd.prog.mk>
