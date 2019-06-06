# /****************************************************************
# Copyright (C) Lucent Technologies 1997
# All Rights Reserved
# 
# Permission to use, copy, modify, and distribute this software and
# its documentation for any purpose and without fee is hereby
# granted, provided that the above copyright notice appear in all
# copies and that both that the copyright notice and this
# permission notice and warranty disclaimer appear in supporting
# documentation, and that the name Lucent Technologies or any of
# its entities not be used in advertising or publicity pertaining
# to distribution of the software without specific, written prior
# permission.
# 
# LUCENT DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
# INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.
# IN NO EVENT SHALL LUCENT OR ANY OF ITS ENTITIES BE LIABLE FOR ANY
# SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER
# IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
# ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF
# THIS SOFTWARE.
# ****************************************************************/

CFLAGS = -g
CFLAGS =
CFLAGS = -O2

# compiler options
#CC = gcc -Wall -g -Wwrite-strings
#CC = gcc -O4 -Wall -pedantic -fno-strict-aliasing
#CC = gcc -fprofile-arcs -ftest-coverage # then gcov f1.c; cat f1.c.gcov
HOSTCC = gcc -g -Wall -pedantic 
CC = $(HOSTCC)  # change this is cross-compiling.

# yacc options.  pick one; this varies a lot by system.
#YFLAGS = -d -S
YACC = bison -d -y
#YACC = yacc -d
#		-S uses sprintf in yacc parser instead of sprint

OFILES = b.o main.o parse.o proctab.o tran.o lib.o run.o lex.o

SOURCE = awk.h ytab.c ytab.h proto.h awkgram.y lex.c b.c main.c \
	maketab.c parse.c lib.c run.c tran.c proctab.c 

LISTING = awk.h proto.h awkgram.y lex.c b.c main.c maketab.c parse.c \
	lib.c run.c tran.c 

SHIP = README LICENSE FIXES $(SOURCE) ytab[ch].bak makefile  \
	 awk.1

a.out:	ytab.o $(OFILES)
	$(CC) $(CFLAGS) ytab.o $(OFILES) $(ALLOC)  -lm

$(OFILES):	awk.h ytab.h proto.h

#Clear dependency for parallel build: (make -j)
#YACC generated y.tab.c and y.tab.h at the same time
#this needs to be a static pattern rules otherwise multiple target
#are mapped onto multiple executions of yacc, which overwrite 
#each others outputs.
y%.c y%.h:	awk.h proto.h awkgram.y
	$(YACC) $(YFLAGS) awkgram.y
	mv y.$*.c y$*.c
	mv y.$*.h y$*.h

ytab.h:	ytab.c

proctab.c:	maketab
	./maketab ytab.h >proctab.c

maketab:	ytab.h maketab.c
	$(HOSTCC) $(CFLAGS) maketab.c -o maketab

bundle:
	@cp ytab.h ytabh.bak
	@cp ytab.c ytabc.bak
	@bundle $(SHIP)

tar:
	@cp ytab.h ytabh.bak
	@cp ytab.c ytabc.bak
	@bundle $(SHIP) >awk.shar
	@tar cf awk.tar $(SHIP)
	gzip awk.tar
	ls -l awk.tar.gz
	@zip awk.zip $(SHIP)
	ls -l awk.zip

gitadd:
	git add README LICENSE FIXES \
           awk.h proto.h awkgram.y lex.c b.c main.c maketab.c parse.c \
	   lib.c run.c tran.c \
	   makefile awk.1 awktest.tar

gitpush:
	# only do this once: 
	# git remote add origin https://github.com/onetrueawk/awk.git
	git push -u origin master

names:
	@echo $(LISTING)

clean:
	rm -f a.out *.o *.obj maketab maketab.exe *.bb *.bbg *.da *.gcov *.gcno *.gcda # proctab.c

cleaner:
	rm -f a.out *.o *.obj maketab maketab.exe *.bb *.bbg *.da *.gcov *.gcno *.gcda proctab.c ytab*
