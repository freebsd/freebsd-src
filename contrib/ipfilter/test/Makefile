#
# (C)opyright 1993-1996 by Darren Reed.
#
# Redistribution and use in source and binary forms are permitted
# provided that this notice is preserved and due credit is given
# to the original author and the contributors.
#
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/share/man

tests: first 0 ftests ptests

first:
	-mkdir -p results

# Filtering tests
ftests: 1 2 3 4 5 6 7 8 9 10 11 12 14

# Rule parsing tests
ptests: i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11

0:
	@(cd ..; make ipftest; )

1 2 3 4 5 6 7 8 9 10 11 14:
	@/bin/sh ./dotest $@

12:
	@/bin/sh ./hextest $@

i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11:
	@/bin/sh ./itest $@

clean:
	/bin/rm -f 1 2 3 4 5 6 7 8 9 10 11 12 results/*
	/bin/rm -f i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11
