#
# (C)opyright 1993-1996 by Darren Reed.
#
# This code may be freely distributed as long as it retains this notice
# and is not changed in any way.  The author accepts no responsibility
# for the use of this software.  I hate legaleese, don't you ?
#
# where to put things.
#
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/share/man

tests: first 0 ftests ptests

first:
	-mkdir -p results

# Filtering tests
ftests: 1 2 3 4 5 6 7 8 9 10 11 12

# Rule parsing tests
ptests: i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11

0:
	@(cd ..; make ipftest; )

1 2 3 4 5 6 7 8 9 10 11:
	@./dotest $@

12:
	@./hextest $@

i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11:
	@./itest $@

clean:
	/bin/rm -f 1 2 3 4 5 6 7 8 9 10 11 12 results/*
	/bin/rm -f i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11
