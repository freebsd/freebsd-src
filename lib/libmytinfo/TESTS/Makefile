# Makefile for libmytinfo tests
# $FreeBSD$

LIB=    mytinfo

TESTS=  ttest tputs.test tparm.test readcaps.test caps clear.tcap \
	clear.tinfo tset tput

CFLAGS+= -Wall -I${.CURDIR}/..

CLEANFILES+= $(TESTS) quit.o

all: $(TESTS)

ttest: ttest.c quit.o
	$(CC) $(CFLAGS) -o ttest $(LDFLAGS) ${.CURDIR}/ttest.c quit.o -l$(LIB)

caps: caps.c quit.o
	$(CC) $(CFLAGS) -o caps $(LDFLAGS) ${.CURDIR}/caps.c quit.o -l$(LIB)

tputs.test: ${.CURDIR}/../tputs.c
	$(CC) -o tputs.test $(LDFLAGS) $(CFLAGS) -DTEST ${.CURDIR}/../tputs.c -l$(LIB)

tparm.test: ${.CURDIR}/../tparm.c
	$(CC) -o tparm.test $(LDFLAGS) $(CFLAGS) -DTEST ${.CURDIR}/../tparm.c -l$(LIB)

readcaps.test: ${.CURDIR}/../readcaps.c
	$(CC) -o readcaps.test $(LDFLAGS) $(CFLAGS) -DTEST ${.CURDIR}/../readcaps.c -l$(LIB)

clear.tcap: clear.c
	$(CC) $(LDFLAGS) $(CFLAGS) -o clear.tcap ${.CURDIR}/clear.c -l$(LIB)

clear.tinfo: clear.c
	$(CC) $(LDFLAGS) $(CFLAGS) -DUSE_TERMINFO -o clear.tinfo ${.CURDIR}/clear.c -l$(LIB)

tset: tset.c quit.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o tset ${.CURDIR}/tset.c quit.o -l$(LIB)

tput: tput.c quit.o
	$(CC) $(CFLAGS) $(LDFLAGS) -o tput ${.CURDIR}/tput.c quit.o -l$(LIB)

.include <bsd.prog.mk>
