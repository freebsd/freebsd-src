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

tests: first 0 ftests ptests ntests nitests logtests ipv6 intests

first:
	-mkdir -p results

# Filtering tests
ftests: f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f12 f13 f14 f15 f16 f17

# Rule parsing tests
ptests: i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11 i12

ntests: n1 n2 n3 n4 n5 n6 n7

nitests: ni1 ni2 ni3 ni4 ni5

intests: in1 in2 in3 in4

logtests: l1

ipv6: ipv6.1 ipv6.2 ipv6.3

0:
	@(cd ..; make ipftest; )

f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f14:
	@/bin/sh ./dotest $@

f12 f13:
	@/bin/sh ./hextest $@

f15 f16:
	@/bin/sh ./mtest $@

f17:
	@/bin/sh ./mhtest $@

i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11 i12:
	@/bin/sh ./itest $@

n1 n2 n3 n4 n5 n6 n7:
	@/bin/sh ./nattest $@

ni1 ni2 ni3 ni4 ni5 ni7 ni8 ni10 ni11:
	@/bin/sh ./natipftest $@

in1 in2 in3 in4:
	@/bin/sh ./intest $@

l1:
	@/bin/sh ./logtest $@

ipv6.1 ipv6.2 ipv6.3:
	@/bin/sh ./dotest6 $@

clean:
	/bin/rm -f f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f13 f12 f14 f15 f16 f17
	/bin/rm -f i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11 i12
	/bin/rm -f n1 n2 n3 n4 n5 n6 n7
	/bin/rm -f ni1 ni2 ni3 ni4 ni5
	/bin/rm -f in1 in2 in3 in4
	/bin/rm -f l1
	/bin/rm -f ipv6.1 ipv6.2 ipv6.3
	/bin/rm -f results/*
