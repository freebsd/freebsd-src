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

GPERF = gperf
CC = gcc

all: test

test:
	@echo "performing some tests of the perfect hash generator"   
	$(CC) -c -O test.c
	$(GPERF) -p -c -l -S1 -C -o c.gperf > cinset.c
	$(CC) -O -o cout cinset.c test.o
	@echo "testing ANSI C reserved words, all items should be found in the set"
	./cout -v < c.gperf 
	$(GPERF) -k1,4,'$$' ada.gperf > adainset.c
# double '$$' is only there since make gets confused; program wants only 1 '$'
	$(CC) -O -o aout adainset.c test.o
	@echo "testing Ada reserved words,all items should be found in the set"
	./aout -v < ada.gperf 
	$(GPERF) -p -D -S1 -k1,'$$' -s 2 -o adapredefined.gperf > preinset.c
	$(CC) -O -o preout preinset.c test.o
	@echo "testing Ada predefined words, all items should be found in the set"
	./preout -v < adapredefined.gperf 
	$(GPERF) -k1,2,'$$' -o modula3.gperf > m3inset.c
	$(CC) -O -o m3out m3inset.c test.o
	@echo "testing Modula3 reserved words, all items should be found in the set"
	./m3out -v < modula3.gperf 
	$(GPERF) -o -S1 -p < pascal.gperf > pinset.c
	$(CC) -O -o pout pinset.c test.o
	@echo "testing Pascal reserved words, all items should be found in the set"
	./pout -v < pascal.gperf 	
	$(GPERF) -o -S2 -j1 -D -p -t < c++.gperf > c++inset.c  
	$(CC) -O -o c++out c++inset.c test.o
	@echo "testing C++ reserved words, all items should be found in the set"
	tail -47 c++.gperf | ./c++out -v 	
# these next 5 are demos that show off the generated code
	$(GPERF) -p -j1 -g -o -t -N is_reserved_word -k1,3,'$$' c-parse.gperf
	$(GPERF) -n -k1-8 -l modula2.gperf 
	$(GPERF) -p -j 1 -o -a -g -t -k1,4,$$ gplus.gperf 
	$(GPERF) -D -p -t < c-parse.gperf 
	$(GPERF) -g -o -j1 -t -p -N is_reserved_word gpc.gperf
# prints out the help message
	-$(GPERF) -h 
	@echo "only if, do, for, case, goto, else, while, and return should be found "
	./aout -v < c.gperf 

clean: 
	-rm -f *.o core *~ *inset.c *out #*#
