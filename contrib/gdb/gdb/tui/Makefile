# Generated automatically from Makefile.in by configure.
# Copyright 1998 Free Software Foundation, Inc.

# This file is part of GDB.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.

all: libtui.a

srcdir=.

SHELL = /bin/sh

CC=gcc
CFLAGS=-g -O2
AR=ar
RANLIB=ranlib

# Host and target-dependent makefile fragments come in here.

# Host: Sun 4 or Sparcstation, running SunOS 4
XDEPFILES= ser-tcp.o
XM_FILE= xm-sun4os4.h
NAT_FILE= nm-sun4os4.h
NATDEPFILES= fork-child.o infptrace.o inftarg.o corelow.o sparc-nat.o
HOST_IPC=-DBSD_IPC
GDBSERVER_DEPFILES= low-sparc.o
# Setting XM_CLIBS=-lresolv would let us use the DNS, but that would screw
# anyone who wants to use NIS, which includes at least one Cygnus customer
# (PR 3593).  So leave it this way until/unless we find a resolver which can
# get names from either DNS or NIS from the same GDB binary.

# Target: Sun 4 or Sparcstation, running SunOS 4
TDEPFILES= sparc-tdep.o solib.o
TM_FILE= tm-sun4os4.h
# End of host and target-dependent makefile fragments

# Where is our "include" directory?  Typically $(srcdir)/../include.
# This is essentially the header file directory for the library
# routines in libiberty.
INCLUDE_DIR =  $(srcdir)/../../include
INCLUDE_CFLAGS = -I$(INCLUDE_DIR)

# Configured by the --with-mmalloc option to configure.
MMALLOC = 
MMALLOC_CFLAGS = 

# Where is the BFD library?  Typically in ../bfd.
BFD_DIR = ../../bfd
BFD_SRC = $(srcdir)/$(BFD_DIR)
BFD_CFLAGS = -I$(BFD_DIR) -I$(BFD_SRC)

# Where is the READLINE library?  Typically in ../readline.
READLINE_DIR = ../../readline
READLINE_SRC = $(srcdir)/$(READLINE_DIR)
READLINE_CFLAGS = -I$(READLINE_SRC)

# Where is the INTL library?  Typically in ../intl.
INTL_DIR = ../../intl
INTL_SRC = $(srcdir)/$(INTL_DIR)
INTL_CFLAGS = -I$(INTL_DIR) -I$(INTL_SRC)

# Where is the TCL library?  Typically in ../tcl.
TCL_CFLAGS = @TCLHDIR@

# Where is the TK library?  Typically in ../tk.
TK_CFLAGS = @TKHDIR@ @TK_BUILD_INCLUDES@

# Where is Itcl?  Typically in ../itcl.
ITCL_CFLAGS = @ITCLHDIR@

# Where is Tix?  Typically in ../tix.
TIX_CFLAGS = @TIXHDIR@

X11_CFLAGS = @TK_XINCLUDES@

ENABLE_IDE= @ENABLE_IDE@

GUI_CFLAGS_X = -I$(srcdir)/../../libgui/src

IDE_CFLAGS_X = -I$(srcdir)/../../libidetcl/src -I$(srcdir)/../../libide/src \
  `if [ x"$(ENABLE_IDE)" != x ] ; then \
    echo -DIDE -I$(srcdir)/../../ilu/runtime/mainloop;\
  fi`

IDE_CFLAGS=$(GUI_CFLAGS_X) $(IDE_CFLAGS_X)

ENABLE_CFLAGS= 

# -I. for config files.
# -I$(srcdir) for gdb internal headers and possibly for gnu-regex.h also.
# -I$(srcdir)/config for more generic config files.

# It is also possible that you will need to add -I/usr/include/sys if
# your system doesn't have fcntl.h in /usr/include (which is where it
# should be according to Posix).
DEFS = -DHAVE_CONFIG_H
GDB_CFLAGS = -I. -I.. -I$(srcdir) -I$(srcdir)/.. -I$(srcdir)/../config $(DEFS)

# M{H,T}_CFLAGS, if defined, have host- and target-dependent CFLAGS
# from the config directory.
GLOBAL_CFLAGS = $(MT_CFLAGS) $(MH_CFLAGS)
#PROFILE_CFLAGS = -pg

# INTERNAL_CFLAGS is the aggregate of all other *CFLAGS macros.
INTERNAL_CFLAGS = $(CFLAGS) $(GLOBAL_CFLAGS) $(PROFILE_CFLAGS) \
	$(GDB_CFLAGS) $(READLINE_CFLAGS) $(BFD_CFLAGS) \
	$(MMALLOC_CFLAGS) $(INCLUDE_CFLAGS) $(INTL_CFLAGS) \
	$(ENABLE_CFLAGS)

HEADERS = tuiIO.h tuiData.h tuiGeneralWin.h tuiLayout.h tuiStack.h \
	  tuiSource.h tuiCommand.h tuiWin.h tuiDisassem.h \
	  tuiSourceWin.h tuiRegs.h tuiDataWin.h

SOURCES = tui.c tuiData.c tuiSource.c tuiStack.c tuiIO.c \
	  tuiGeneralWin.c tuiLayout.c tuiWin.c tuiCommand.c \
	  tuiDisassem.c tuiSourceWin.c tuiRegs.c tuiDataWin.c

OBJECTS = tui.o tuiData.o tuiSource.o tuiStack.o tuiIO.o \
	  tuiGeneralWin.o tuiLayout.o tuiWin.o tuiCommand.o \
	  tuiDisassem.o tuiSourceWin.o tuiRegs.o tuiDataWin.o \
	  tuiInit.o


# Prevent Sun make from putting in the machine type.  Setting
# TARGET_ARCH to nothing works for SunOS 3, 4.0, but not for 4.1.
.c.o:
	$(CC) -c $(INTERNAL_CFLAGS) $<
.SUFFIXES: .cpp
.c.cpp:
	$(CC) -E $(INTERNAL_CFLAGS) $< > $@

libtui.a: $(OBJECTS)
	rm -f libtui.a
	$(AR) rc libtui.a $(OBJECTS)
	$(RANLIB) libtui.a

tui.o: tui.c tui.h tuiData.h tuiLayout.h tuiIO.h tuiRegs.h tuiWin.h
tuiCommand.o: tui.h tuiData.h tuiWin.h tuiIO.h
tuiData.o: tui.h tuiData.h
tuiDataWin.o: tui.h tuiData.h tuiRegs.h
tuiDisassem.o: tui.h tuiData.h tuiLayout.h tuiSourceWin.h tuiStack.h
tuiGeneralWin.o: tui.h tuiData.h tuiGeneralWin.h
tuiIO.o: tui.h tuiData.h tuiIO.h tuiCommand.h tuiWin.h
tuiLayout.o: tui.h tuiData.h tuiGeneralWin.h tuiStack.h tuiRegs.h \
	tuiDisassem.h
tuiRegs.o: tui.h tuiData.h tuiLayout.h tuiWin.h
tuiSource.o: tui.h tuiData.h tuiStack.h tuiSourceWin.h tuiSource.h
tuiSourceWin.o: tui.h tuiData.h tuiStack.h tuiSourceWin.h tuiSource.h \
	tuiDisassem.h
tuiStack.o: tui.h tuiData.h tuiStack.h tuiSourceWin.h
tuiWin.o: tui.h tuiData.h tuiGeneralWin.h tuiStack.h tuiSourceWin.h \
	tuiDataWin.h

tuiInit.o: tuiInit.c
tuiInit.c: $(SOURCES)
	@echo Making tuiInit.c
	@rm -f init.c-tmp
	@echo '/* Do not modify this file.  */' >init.c-tmp
	@echo '/* It is created automatically by the Makefile.	*/'>>init.c-tmp
	@echo '#include "ansidecl.h"' >>init.c-tmp
	@echo 'extern void initialize_tui_files PARAMS ((void));' >>init.c-tmp
	@echo 'void initialize_tui_files PARAMS ((void)) {' >>init.c-tmp
	@-( cd $(srcdir) ; grep '^_initialize_[a-z_0-9A-Z]* *(' $(SOURCES) ) 2>/dev/null \
	| sed -e 's/^.*://' -e 's/^\([a-z_0-9A-Z]*\).*/  {extern void \1 PARAMS ((void)); \1 ();}/' >>init.c-tmp
	@echo '}' >>init.c-tmp
	@mv init.c-tmp tuiInit.c

clean:
	rm -f *.o *.a
