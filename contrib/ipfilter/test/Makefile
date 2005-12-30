#
# (C)opyright 1993-1996 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/share/man
all: results tests

results:
	mkdir -p results

tests: ipf nat logtests ipv6 pools bpf

ipf: ftests ptests

nat: ntests nitests intests

first:
	-mkdir -p results

# Filtering tests
ftests: f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f12 f13 f14 f15 f16 f17 f18 f19

# Rule parsing tests
ptests: i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11 i12 i13 i14 i15 i16 i17 i18 i19 \
	i20 i21

ntests: n1 n2 n3 n4 n5 n6 n7 n8 n9 n10 n11 n12 n13 n14

nitests: ni1 ni2 ni3 ni4 ni5 ni6 ni7 ni8 ni9 ni10 ni11 ni12 ni13 ni14 ni15 ni16

intests: in1 in2 in3 in4 in5 in6

logtests: l1

pools: p1 p2 p3 ip1

ipv6: ipv6.1 ipv6.2 ipv6.3

bpf: bpf1 bpf-f1

f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f12 f13 f14 f19:
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f15 f16 f17 f18:
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11 i12 i13 i14 i15 i16 i17 i18 i19 i20 i21 bpf1:
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

n1 n2 n3 n4 n5 n6 n7 n8 n9 n10 n11 n12 n13 n14:
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

ni1 ni2 ni3 ni4 ni5 ni7 ni8 ni9 ni10 ni11 ni12 ni13 ni14 ni15 ni16:
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni6:
	@/bin/sh ./natipftest multi `awk "/^$@ / { print; } " test.format`

in1 in2 in3 in4 in5 in6:
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

l1:
	@/bin/sh ./logtest `awk "/^$@ / { print; } " test.format`

ipv6.1 ipv6.2 ipv6.3:
	@/bin/sh ./dotest6 `awk "/^$@ / { print; } " test.format`

p1 p2 p3:
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

ip1:
	@/bin/sh ./iptest `awk "/^$@ / { print; } " test.format`

bpf-f1:
	/bin/sh ./bpftest `awk "/^$@ / { print; } " test.format`

clean:
	/bin/rm -f f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f12 f13 f14 f15 f16 f17 f18 f19
	/bin/rm -f i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11 i12 i13 i14 i15 i16 i17 i18 i19 i20 i21
	/bin/rm -f n1 n2 n3 n4 n5 n6 n7 n8 n9 n10 n11 n12 n13 n14
	/bin/rm -f ni1 ni2 ni3 ni4 ni5 ni6 ni7 ni8 ni9
	/bin/rm -f ni10 ni11 ni12 ni13 ni14 ni15 ni16
	/bin/rm -f in1 in2 in3 in4 in5 in6
	/bin/rm -f p1 p2 p3 ip1
	/bin/rm -f l1
	/bin/rm -f ipv6.1 ipv6.2 ipv6.3
	/bin/rm -f bpf1 bpf-f1
	/bin/rm -f results/* logout

diffs:
	-cd expected; for i in *; do if [ -f $$i -a ! -f ../$$i -a -f ../results/$$i ] ; then  diff -c $$i ../results/$$i >> ../diff.out; fi done
