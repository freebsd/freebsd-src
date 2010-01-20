# /****************************************************************
# Copyright (C) 1998, 2000 by Lucent Technologies
# All Rights Reserved
#
# Permission to use, copy, modify, and distribute this software and
# its documentation for any purpose and without fee is hereby
# granted, provided that the above copyright notice appear in all
# copies and that both that the copyright notice and this
# permission notice and warranty disclaimer appear in supporting
# documentation, and that the name of Lucent or any of its entities
# not be used in advertising or publicity pertaining to
# distribution of the software without specific, written prior
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
#
# ****************************************************************/

.SUFFIXES: .c .o
CC = cc
CFLAGS = -g -I..
A = ../gdtoa.a

.c.o:
	$(CC) -c $(CFLAGS) $*.c

all: dt dItest ddtest dtest ftest Qtest xLtest xtest ddtestsi dItestsi tests

dt = dt.o $A
dt: $(dt)
	$(CC) -o dt $(dt)

dItest = dItest.o getround.o $A
dItest: $(dItest)
	$(CC) -o dItest $(dItest)

ddtest = ddtest.o getround.o $A
ddtest: $(ddtest)
	$(CC) -o ddtest $(ddtest)

dtest = dtest.o getround.o $A
dtest: $(dtest)
	$(CC) -o dtest $(dtest)

ftest = ftest.o getround.o $A
ftest: $(ftest)
	$(CC) -o ftest $(ftest)

Qtest = Qtest.o getround.o $A
Qtest: $(Qtest)
	$(CC) -o Qtest $(Qtest)

xtest = xtest.o getround.o $A
xtest: $(xtest)
	$(CC) -o xtest $(xtest)

xLtest = xLtest.o getround.o $A
xLtest: $(xLtest)
	$(CC) -o xLtest $(xLtest)

strtopddSI.o: strtopddSI.c ../strtopdd.c

strtorddSI.o: strtorddSI.c ../strtordd.c

strtodISI.o: strtodISI.c ../strtodI.c

strtoIddSI.o: strtoIddSI.c ../strtoIdd.c

strtoIdSI.o: strtoIdSI.c ../strtoId.c

ddtestsi = ddtest.o strtopddSI.o strtorddSI.o strtoIddSI.o getround.o $A
ddtestsi: $(ddtestsi)
	$(CC) -o ddtestsi $(ddtestsi)

dItestsi = dItest.o strtodISI.o strtoIdSI.o getround.o $A
dItestsi: $(dItestsi)
	$(CC) -o dItestsi $(dItestsi)

strtodt = strtodt.o $A
strtodt: $(strtodt)
	$(CC) -o strtodt $(strtodt)

# xQtest generates cp commands that depend on sizeof(long double).
# See the source for details.  If you know better, create Q.out,
# x.out and xL.out by copying the relevant *.ou0 or *.ou1 files
# to the corresponding .out files.  In short, the *.ou0 files are
# for long double == double; x.ou1 and xL.ou1 are for
# long double == extended (a la 80x87 and MC680x0), and Q.ou1 is
# for long double == quad.

Q.out x.out xL.out:
	$(CC) -o xQtest xQtest.c
	./xQtest | sh
	rm -f xQtest xQtest.o

## The rmdir below will fail if any test results differ.

tests: Q.out x.out xL.out dt dItest ddtest dtest ftest Qtest xLtest xtest ddtestsi dItestsi strtodt
	mkdir bad
	cat testnos testnos1 | ./dt >zap 2>&1
	cmp dtst.out zap || mv zap bad/dtst.out
	./dItest <testnos >zap 2>&1
	cmp dI.out zap || mv zap bad/dI.out
	./dItestsi <testnos >zap 2>&1
	cmp dIsi.out zap || mv zap bad/dIsi.out
	./ddtestsi <testnos >zap 2>&1
	cmp ddsi.out zap || mv zap bad/ddsi.out
	for i in dd d f x xL Q; do cat testnos rtestnos | \
	 ./"$$i"test >zap 2>&1;\
	cmp $$i.out zap || mv zap bad/$$i.out; done
	./strtodt testnos3 >bad/strtodt.out && rm bad/strtodt.out || \
		cat bad/strtodt.out
	rmdir bad
	touch tests

xs0 = README Qtest.c dItest.c ddtest.c dtest.c dt.c ftest.c getround.c \
	strtoIdSI.c strtoIddSI.c strtodISI.c strtodt.c strtopddSI.c \
	strtorddSI.c xLtest.c xQtest.c xtest.c rtestnos testnos testnos1 \
	testnos3 dI.out dIsi.out ddsi.out dd.out dtst.out d.out f.out \
	x.ou0 xL.ou0 x.ou1 xL.ou1 Q.ou0 Q.ou1 makefile

xsum.out: xsum0.out $(xs0)
	xsum $(xs0) >xsum1.out
	cmp xsum0.out xsum1.out && mv xsum1.out xsum.out || diff xsum[01].out

clean:
	rm -f *.[ao] dt *test *testsi strtodt xsum.out xsum1.out tests zap x.out xL.out Q.out
	rm -rf bad
