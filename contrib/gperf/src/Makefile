# Copyright (C) 1989 Free Software Foundation, Inc.
# written by Douglas C. Schmidt (schmidt@ics.uci.edu)
# 
# This file is part of GNU GPERF.
# 
# GNU GPERF is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 1, or (at your option)
# any later version.
# 
# GNU GPERF is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with GNU GPERF; see the file COPYING.  If not, write to
# the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA. 

CC = gcc
DFLAGS= -DLO_CAL -DGATHER_STATISTICS #-DRLIMIT_STACK 
OFLAGS= -O -p -g -fstrength-reduce -fomit-frame-pointer -fdelayed-branch -finline-functions # gcc options 
CFLAGS= $(DFLAGS) $(OFLAGS)
OBJS = options.o iterator.o main.o perfect.o keylist.o listnode.o xmalloc.o \
       hashtable.o boolarray.o readline.o stderr.o version.o getopt.o
SOURCES = options.c iterator.c main.c perfect.c keylist.c listnode.c xmalloc.c \
       hashtable.c boolarray.c readline.c stderr.c version.c getopt.c

all: gperf

gperf: $(OBJS) 
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LIBS)

clean: 
	-rm -f *.o core *~ #*#

realclean: clean
	-rm -f gperf

# dependencies
# DO NOT DELETE THIS LINE -- mkdep uses it.
# DO NOT PUT ANYTHING AFTER THIS LINE, IT WILL GO AWAY.

boolarray.o: boolarray.c /usr/include/stdio.h boolarray.h prototype.h options.h
boolarray.o: /usr/include/stdio.h prototype.h
getopt.o: getopt.c /usr/include/stdio.h
hashtable.o: hashtable.c /usr/include/stdio.h hashtable.h keylist.h
hashtable.o: /usr/include/stdio.h listnode.h prototype.h prototype.h options.h
hashtable.o: /usr/include/stdio.h prototype.h
iterator.o: iterator.c /usr/include/stdio.h /usr/include/ctype.h iterator.h
iterator.o: prototype.h
keylist.o: keylist.c /usr/include/assert.h /usr/include/stdio.h options.h
keylist.o: /usr/include/stdio.h prototype.h readline.h prototype.h keylist.h
keylist.o: /usr/include/stdio.h listnode.h prototype.h hashtable.h keylist.h
keylist.o: prototype.h stderr.h prototype.h /usr/include/varargs.h
listnode.o: listnode.c /usr/include/stdio.h options.h /usr/include/stdio.h
listnode.o: prototype.h listnode.h prototype.h stderr.h prototype.h
listnode.o: /usr/include/varargs.h
main.o: main.c /usr/include/stdio.h stderr.h prototype.h /usr/include/varargs.h
main.o: options.h /usr/include/stdio.h prototype.h perfect.h prototype.h
main.o: keylist.h /usr/include/stdio.h listnode.h prototype.h boolarray.h
main.o: prototype.h
options.o: options.c /usr/include/stdio.h /usr/include/assert.h options.h
options.o: /usr/include/stdio.h prototype.h iterator.h prototype.h stderr.h
options.o: prototype.h /usr/include/varargs.h
perfect.o: perfect.c /usr/include/stdio.h /usr/include/assert.h
perfect.o: /usr/include/ctype.h options.h /usr/include/stdio.h prototype.h
perfect.o: perfect.h prototype.h keylist.h /usr/include/stdio.h listnode.h
perfect.o: prototype.h boolarray.h prototype.h stderr.h prototype.h
perfect.o: /usr/include/varargs.h
readline.o: readline.c /usr/include/stdio.h readline.h prototype.h
stderr.o: stderr.c /usr/include/stdio.h stderr.h prototype.h
stderr.o: /usr/include/varargs.h
version.o: version.c
xmalloc.o: xmalloc.c /usr/include/stdio.h

# IF YOU PUT ANYTHING HERE IT WILL GO AWAY
