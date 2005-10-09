# Makefile for sntp
# N.M. Maclaren, October 2000.

# Take a look at README for the various preprocessor symbols, but they are
# extremely unlikely to be needed on newer systems.  You may prefer to change
# LOCKNAME and SAVENAME to use /var/run (or even /tmp) rather than /etc.  Note
# that not all of the following system settings have been tested recently.

AUTOMAKE_OPTIONS = foreign

# Compiling this sort of ANSI C under SunOS 4.1 is a mug's game, because Sun's
# Unix headers make GNU C vomit even in compatibility mode, but the following
# will compile main.c and unix.c.  At least two people have got it to work.
# CC = gcc -ansi
# CFLAGS = -O -DNONBLOCK_BROKEN
# LDFLAGS =
# LIBS = -lm

# The following settings can be used under Linux.  While adjtime is present,
# it is completely broken (i.e. it will work only if xntp is running), so it
# is a good idea to add -DADJTIME_MISSING.
# CC = cc -DADJTIME_MISSING
# CFLAGS = -O
# LDFLAGS = 
# LIBS = -lm

bin_PROGRAMS = sntp
sntp_SOURCES = \
	header.h \
	internet.c \
	internet.h \
	kludges.h \
	main.c \
	socket.c \
	timing.c \
	unix.c \
	$(EMPTY)

