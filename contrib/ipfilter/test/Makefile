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

tests: first 0 ftests ptests ntests nitests logtests

first:
	-mkdir -p results

# Filtering tests
ftests: f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f12 f13 f14 f15 f16

# Rule parsing tests
ptests: i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11

ntests: n1 n2 n3 n4 n5 n6 n7

nitests: ni1 ni2

logtests: l1

0:
	@(cd ..; make ipftest; )

f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f14:
	@/bin/sh ./dotest $@

f12 f13:
	@/bin/sh ./hextest $@

f15 f16:
	@/bin/sh ./mtest $@

i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11:
	@/bin/sh ./itest $@

n1 n2 n3 n4 n5 n6 n7:
	@/bin/sh ./nattest $@

ni1 ni2:
	@/bin/sh ./natipftest $@

l1:
	@/bin/sh ./logtest $@

clean:
	/bin/rm -f f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f13 f12 f14 f15 f16
	/bin/rm -f i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11
	/bin/rm -f n1 n2 n3 n4 n5 n6 n7
	/bin/rm -f ni1 ni2
	/bin/rm -f l1
	/bin/rm -f results/*
