# Generated automatically from Makefile.in by configure.
# Un*x Makefile for GNU tar program.
# Copyright (C) 1991, 1992, 1993 Free Software Foundation, Inc.

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#### Start of system configuration section. ####

srcdir = .
VPATH = .

# If you use gcc, you should either run the fixincludes script that
# comes with it or else use gcc with the -traditional option.  Otherwise
# ioctl calls will be compiled incorrectly on some systems.
CC = gcc
YACC = bison -y
INSTALL = /usr/local/bin/install -c
INSTALL_PROGRAM = $(INSTALL)
INSTALL_DATA = $(INSTALL) -m 644

# Things you might add to DEFS:
# -DSTDC_HEADERS	If you have ANSI C headers and libraries.
# -DHAVE_UNISTD_H	If you have unistd.h.
# -DHAVE_STRING_H	If you don't have ANSI C headers but have string.h.
# -DHAVE_LIMITS_H	If you have limits.h.
# -DBSD42		If you have sys/dir.h (unless you use -DPOSIX),
#			sys/file.h, and st_blocks in `struct stat'.
# -DDIRENT		If you have dirent.h.
# -DSYSNDIR		Old Xenix systems (sys/ndir.h).
# -DSYSDIR		Old BSD systems (sys/dir.h).
# -DNDIR		Old System V systems (ndir.h).
# -DMAJOR_IN_MKDEV	If major, minor, makedev defined in sys/mkdev.h.
# -DMAJOR_IN_SYSMACROS	If major, minor, makedev defined in sys/sysmacros.h.
# -DRETSIGTYPE=int	If your signal handlers return int, not void.
# -DHAVE_SYS_MTIO_H	If you have sys/mtio.h (magtape ioctls).
# -DHAVE_SYS_GENTAPE_H	If you have sys/gentape.h (ISC magtape ioctls).
# -DHAVE_NETDB_H	To use rexec for remote tape operations
#			instead of forking rsh or remsh.
# -DNO_REMOTE		If you have neither a remote shell nor rexec.
# -DHAVE_VPRINTF	If you have vprintf function.
# -DHAVE_DOPRNT		If you have _doprnt function (but lack vprintf).
# -DHAVE_FTIME		If you have ftime system call.
# -DHAVE_STRSTR		If you have strstr function.
# -DHAVE_VALLOC		If you have valloc function.
# -DHAVE_MKDIR		If you have mkdir and rmdir system calls.
# -DHAVE_MKNOD		If you have mknod system call.
# -DHAVE_RENAME 	If you have rename system call.
# -DHAVE_GETCWD		If not POSIX.1 but have getcwd function.
# -DHAVE_FTRUNCATE	If you have ftruncate system call.
# -DV7			On Version 7 Unix (not tested in a long time).
# -DEMUL_OPEN3		If you lack a 3-argument version of open, and want
#			to emulate it with system calls you do have.
# -DNO_OPEN3		If you lack the 3-argument open and want to
#			disable the tar -k option instead of emulating open.
# -DXENIX		If you have sys/inode.h and need it to be included.

DEF_AR_FILE = /dev/rst0
DEFBLOCKING = 20
DEFS =  -DRETSIGTYPE=void -DDIRENT=1 -DHAVE_SYS_MTIO_H=1 -DHAVE_UNISTD_H=1 -DHAVE_GETGRGID=1 -DHAVE_GETPWUID=1 -DHAVE_STRING_H=1 -DHAVE_LIMITS_H=1 -DHAVE_STRSTR=1 -DHAVE_VALLOC=1 -DHAVE_MKDIR=1 -DHAVE_MKNOD=1 -DHAVE_RENAME=1 -DHAVE_FTRUNCATE=1 -DHAVE_GETCWD=1 -DHAVE_VPRINTF=1 -DDEF_AR_FILE=\"$(DEF_AR_FILE)\" -DDEFBLOCKING=$(DEFBLOCKING)

# Set this to rtapelib.o unless you defined NO_REMOTE, in which case
# make it empty.
RTAPELIB = rtapelib.o
LIBS = 

CFLAGS = -g
LDFLAGS = -g

prefix = /usr/bin
exec_prefix = $(prefix)

# Prefix for each installed program, normally empty or `g'.
binprefix = 

# The directory to install tar in.
bindir = $(exec_prefix)/bin

# Where to put the rmt executable.
libdir = /sbin

# The directory to install the info files in.
infodir = $(prefix)/info

#### End of system configuration section. ####

SHELL = /bin/sh

SRC1 =	tar.c create.c extract.c buffer.c getoldopt.c update.c gnu.c mangle.c
SRC2 =  version.c list.c names.c diffarch.c port.c fnmatch.c getopt.c malloc.c
SRC3 =  getopt1.c regex.c getdate.y getdate.c alloca.c
SRCS =	$(SRC1) $(SRC2) $(SRC3)
OBJ1 =	tar.o create.o extract.o buffer.o getoldopt.o update.o gnu.o mangle.o
OBJ2 =	version.o list.o names.o diffarch.o port.o fnmatch.o getopt.o 
OBJ3 =  getopt1.o regex.o getdate.o $(RTAPELIB) 
OBJS =	$(OBJ1) $(OBJ2) $(OBJ3)
AUX =   README INSTALL NEWS COPYING ChangeLog Makefile.in makefile.pc \
	configure configure.in \
	tar.h fnmatch.h pathmax.h port.h open3.h getopt.h regex.h \
	rmt.h rmt.c rtapelib.c \
	msd_dir.h msd_dir.c tcexparg.c \
	level-0 level-1 backup-specs dump-remind getpagesize.h
#	tar.texinfo tar.info* texinfo.tex \

all:	tar rmt 
# tar.info

.c.o:
	$(CC) -c $(CFLAGS) $(CPPFLAGS) $(DEFS) -I$(srcdir) -I. $<

tar:	$(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS) $(LIBS)

rmt:	rmt.c
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(srcdir)/rmt.c $(LIBS)

tar.info: tar.texinfo
	makeinfo $(srcdir)/tar.texinfo

install: all
	$(INSTALL_PROGRAM) tar $(bindir)/$(binprefix)tar
	-test ! -f rmt || $(INSTALL_PROGRAM) rmt $(libdir)/rmt
#	for file in $(srcdir)/tar.info*; \
#	do $(INSTALL_DATA) $$file $(infodir)/$$file; \
#	done

uninstall:
	rm -f $(bindir)/$(binprefix)tar	$(infodir)/tar.info*
	-rm -f $(libdir)/rmt

$(OBJS): tar.h pathmax.h port.h
regex.o buffer.o tar.o: regex.h
tar.o fnmatch.o: fnmatch.h

getdate.c: getdate.y
	$(YACC) $(srcdir)/getdate.y
	mv y.tab.c getdate.c
# getdate.y has 8 shift/reduce conflicts.

TAGS:	$(SRCS)
	etags $(SRCS)

clean:
	rm -f *.o tar rmt core
mostlyclean: clean

distclean: clean
	rm -f Makefile config.status

realclean: distclean
	rm -f TAGS *.info* getdate.c y.tab.c

shar: $(SRCS) $(AUX)
	shar $(SRCS) $(AUX) | gzip > tar-`sed -e '/version_string/!d' -e 's/[^0-9.]*\([0-9.]*\).*/\1/' -e q version.c`.shar.z

dist: $(SRCS) $(AUX)
	echo tar-`sed -e '/version_string/!d' -e 's/[^0-9.]*\([0-9.]*\).*/\1/' -e q version.c` > .fname
	-rm -rf `cat .fname`
	mkdir `cat .fname`
	for file in $(SRCS) $(AUX); do \
          ln $$file `cat .fname` || cp $$file `cat .fname`; done
	tar chzf `cat .fname`.tar.z `cat .fname`
	-rm -rf `cat .fname` .fname

tar.zoo: $(SRCS) $(AUX)
	-rm -rf tmp.dir
	-mkdir tmp.dir
	-rm tar.zoo
	for X in $(SRCS) $(AUX) ; do echo $$X ; sed 's/$$//' $$X > tmp.dir/$$X ; done
	cd tmp.dir ; zoo aM ../tar.zoo *
	-rm -rf tmp.dir

# Prevent GNU make v3 from overflowing arg limit on SysV.
.NOEXPORT:
