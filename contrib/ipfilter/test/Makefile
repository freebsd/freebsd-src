#
# (C)opyright 1993-1996 by Darren Reed.
#
# See the IPFILTER.LICENCE file for details on licencing.
#
POOLDEP=../ip_lookup.c ../ip_lookup.h ../ip_pool.c ../ip_pool.h \
	../ip_htable.c ../ip_htable.h ../ip_dstlist.c ../ip_dstlist.h \
	../tools/ippool_y.y
BINDEST=/usr/local/bin
SBINDEST=/sbin
MANDIR=/usr/share/man
all: expected.d results tests

expected.d:
	(cd expected; make)

results:
	mkdir -p results

tests: ipf nat logtests ipv6 pools

ipf: patests ftests

nat: intests ntests nitests

first:
	-mkdir -p results

# Filtering tests
ftests: f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f12 f13 f14 f15 f16 f17 f18 f19 f20 f21 f22 f24 f25 f26 f27 f28 f29 f30

# Rule parsing tests
patests: i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11 i12 i13 i14 i15 i16 i17 i18 i19 \
	 i20 i21 i22 i23

ntests: n1 n2 n3 n4 n5 n6 n7 n8 n9 n10 n11 n12 n13 n14 n15 n16 n17 n18 n100 n101 n102 n103 n104 n105 n106 n200

ntests6: n1_6 n2_6 n4_6 n5_6 n6_6 n7_6 n8_6 n9_6 n11_6 n12_6 n15_6

nitests: ni1 ni2 ni3 ni4 ni5 ni6 ni7 ni8 ni9 ni10 ni11 ni12 ni13 ni14 ni15 \
	 ni16 ni17 ni18 ni19 ni20 ni21 ni23

intests: in1 in2 in3 in4 in5 in6 in7 in8 in100 in101 in102

logtests: l1

pools: p1 p2 p3 p4 p5 p6 p7 p9 p10 p11 p12 p13 ip1 ip2 ip3

ipv6: ipv6.1 ipv6.2 ipv6.3 ipv6.4 ipv6.5 ipv6.6 ntests6

bpf: bpf1 bpf-f1

f1: expected/f1 input/f1 regress/f1
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f2: expected/f2 input/f2 regress/f2
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f3: expected/f3 input/f3 regress/f3
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f4: expected/f4 input/f4 regress/f4
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f5: expected/f5 input/f5 regress/f5
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f6: expected/f6 input/f6 regress/f6
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f7: expected/f7 input/f7 regress/f7
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f8: expected/f8 input/f8 regress/f8
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f9: expected/f9 input/f9 regress/f9
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f10: expected/f10 input/f10 regress/f10
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f11: expected/f11 input/f11 regress/f11
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f12: expected/f12 input/f12 regress/f12
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f13: expected/f13 input/f13 regress/f13
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f14: expected/f14 input/f14 regress/f14
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f19: expected/f15 input/f15 regress/f15
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f15: expected/f15 input/f15 regress/f15
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f16: expected/f16 input/f16 regress/f16
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f17: expected/f17 input/f17 regress/f17
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f18: expected/f18 input/f18 regress/f18
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f20: expected/f20 input/f20 regress/f20
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f21: expected/f21 input/f21 regress/f21
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f22: expected/f22 input/f22 regress/f22
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f24: expected/f24 input/f24 regress/f24
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f25: expected/f25 input/f25 regress/f25
	@/bin/sh ./mtest `awk "/^$@ / { print; } " test.format`

f26: expected/f26 input/f26 regress/f26
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f27: expected/f27 input/f27 regress/f27
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

f28: expected/f28 input/f28 regress/f28.ipf regress/f28.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

f29: expected/f29 input/f29 regress/f29.ipf regress/f29.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

f30: expected/f30 input/f30 regress/f30
	@/bin/sh ./dotest `awk "/^$@ / { print; } " test.format`

i1: expected/i1 regress/i1
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i2: expected/i2 regress/i2
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i3: expected/i3 regress/i3
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i4: expected/i4 regress/i4
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i5: expected/i5 regress/i5
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i6: expected/i6 regress/i6
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i7: expected/i7 regress/i7
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i8: expected/i8 regress/i8
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i9: expected/i9 regress/i9
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i10: expected/i10 regress/i10
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i11: expected/i11 regress/i11
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i12: expected/i12 regress/i12
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i13: expected/i13 regress/i13
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i14: expected/i14 regress/i14
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i15: expected/i15 regress/i15
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i16: expected/i16 regress/i16
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i17: expected/i17 regress/i17
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i18: expected/i18 regress/i18
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i19: expected/i19 regress/i19
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i20: expected/i20 regress/i20
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i21: expected/i21 regress/i21
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i22: expected/i22 regress/i22
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

i23: expected/i23 regress/i23
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

bpf1: expected/bpf1 regress/bpf1
	@/bin/sh ./itest `awk "/^$@ / { print; } " test.format`

n1: expected/n1 regress/n1 input/n1
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n2: expected/n2 regress/n2 input/n2
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n3: expected/n3 regress/n3 input/n3
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n4: expected/n4 regress/n4 input/n4
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n5: expected/n5 regress/n5 input/n5
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n6: expected/n6 regress/n6 input/n6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n7: expected/n7 regress/n7 input/n7
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n8: expected/n8 regress/n8 input/n8
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n9: expected/n9 regress/n9 input/n9
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n10: expected/n10 regress/n10 input/n10
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n11: expected/n11 regress/n11 input/n11
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n12: expected/n12 regress/n12 input/n12
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n13: expected/n13 regress/n13 input/n13
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n14: expected/n14 regress/n14 input/n14
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n15: expected/n15 regress/n15 input/n15
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n16: expected/n16 regress/n16 input/n16
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n17: expected/n17 regress/n17 input/n17
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n18: expected/n18 regress/n18 input/n18
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n100: expected/n100 regress/n100 input/n100
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n101: expected/n101 regress/n101 input/n101
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n102: expected/n102 regress/n102 input/n102
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n103: expected/n103 regress/n103 input/n103
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n104: expected/n104 regress/n104 input/n104
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n105: expected/n105 regress/n105 input/n105
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n106: expected/n106 regress/n106 input/n106
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n200: expected/n200 regress/n200 input/n200
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n1_6: expected/n1_6 regress/n1_6 input/n1_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n2_6: expected/n2_6 regress/n2_6 input/n2_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n4_6: expected/n4_6 regress/n4_6 input/n4_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n5_6: expected/n5_6 regress/n5_6 input/n5_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n6_6: expected/n6_6 regress/n6_6 input/n6_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n7_6: expected/n7_6 regress/n7_6 input/n7_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n8_6: expected/n8_6 regress/n8_6 input/n8_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n9_6: expected/n9_6 regress/n9_6 input/n9_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n11_6: expected/n11_6 regress/n11_6 input/n11_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n12_6: expected/n12_6 regress/n12_6 input/n12_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

n15_6: expected/n15_6 regress/n15_6 input/n15_6
	@/bin/sh ./nattest `awk "/^$@ / { print; } " test.format`

ni2: expected/ni2 input/ni2 regress/ni2.nat regress/ni2.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni3: expected/ni3 input/ni3 regress/ni3.nat regress/ni3.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni4: expected/ni4 input/ni4 regress/ni4.nat regress/ni4.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni5: expected/ni5 input/ni5 regress/ni5.nat regress/ni5.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni7: expected/ni7 input/ni7 regress/ni7.nat regress/ni7.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni8: expected/ni8 input/ni8 regress/ni8.nat regress/ni8.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni9: expected/ni9 input/ni9 regress/ni9.nat regress/ni9.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni10: expected/ni10 input/ni10 regress/ni10.nat regress/ni10.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni11: expected/ni11 input/ni11 regress/ni11.nat regress/ni11.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni12: expected/ni12 input/ni12 regress/ni12.nat regress/ni12.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni13: expected/ni13 input/ni13 regress/ni13.nat regress/ni13.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni14: expected/ni14 input/ni14 regress/ni14.nat regress/ni14.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni15: expected/ni15 input/ni15 regress/ni15.nat regress/ni15.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni16: expected/ni16 input/ni16 regress/ni16.nat regress/ni16.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni19: expected/ni19 input/ni19 regress/ni19.nat regress/ni19.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni20: expected/ni20 input/ni20 regress/ni20.nat regress/ni20.ipf
	@/bin/sh ./natipftest single `awk "/^$@ / { print; } " test.format`

ni1: expected/ni1 input/ni1 regress/ni1.nat regress/ni1.ipf
	@/bin/sh ./natipftest multi `awk "/^$@ / { print; } " test.format`

ni6: expected/ni6 input/ni6 regress/ni6.nat regress/ni6.ipf
	@/bin/sh ./natipftest multi `awk "/^$@ / { print; } " test.format`

ni17: expected/ni17 input/ni17 regress/ni17.nat regress/ni17.ipf
	@/bin/sh ./natipftest multi `awk "/^$@ / { print; } " test.format`

ni18: expected/ni18 input/ni18 regress/ni18.nat regress/ni18.ipf
	@/bin/sh ./natipftest multi `awk "/^$@ / { print; } " test.format`

ni21: expected/ni21 input/ni21 regress/ni21.nat regress/ni21.ipf
	@/bin/sh ./natipftest multi `awk "/^$@ / { print; } " test.format`

ni23: expected/ni23 input/ni23 regress/ni23.nat regress/ni23.ipf
	@/bin/sh ./natipftest multi `awk "/^$@ / { print; } " test.format`

in1: expected/in1 regress/in1
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in2: expected/in2 regress/in2
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in3: expected/in3 regress/in3
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in4: expected/in4 regress/in4
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in5: expected/in5 regress/in5
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in6: expected/in6 regress/in6
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in7: expected/in7 regress/in7
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in8: expected/in8 regress/in8
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in100: expected/in100 regress/in100
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in101: expected/in101 regress/in101
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

in102: expected/in102 regress/in102
	@/bin/sh ./intest `awk "/^$@ / { print; } " test.format`

l1:
	@/bin/sh ./logtest `awk "/^$@ / { print; } " test.format`

ipv6.1: expected/ipv6.1 input/ipv6.1 regress/ipv6.1
	@/bin/sh ./dotest6 `awk "/^$@ / { print; } " test.format`

ipv6.2: expected/ipv6.2 input/ipv6.2 regress/ipv6.2
	@/bin/sh ./dotest6 `awk "/^$@ / { print; } " test.format`

ipv6.3: expected/ipv6.3 input/ipv6.3 regress/ipv6.3
	@/bin/sh ./dotest6 `awk "/^$@ / { print; } " test.format`

ipv6.4: expected/ipv6.4 input/ipv6.4 regress/ipv6.4
	@/bin/sh ./dotest6 `awk "/^$@ / { print; } " test.format`

ipv6.5: expected/ipv6.5 input/ipv6.5 regress/ipv6.5
	@/bin/sh ./dotest6 `awk "/^$@ / { print; } " test.format`

ipv6.6: expected/ipv6.6 input/ipv6.6 regress/ipv6.6
	@/bin/sh ./dotest6 `awk "/^$@ / { print; } " test.format`

p1: expected/p1 input/p1 regress/p1.ipf regress/p1.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p2: expected/p2 input/p2 regress/p2.ipf $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p3: expected/p3 input/p3 regress/p3.ipf regress/p3.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p4: expected/p4 input/p4 regress/p4.nat regress/p4.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p5: expected/p5 input/p5 regress/p5.ipf regress/p5.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p6: expected/p6 input/p6 regress/p6.ipf regress/p6.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p7: expected/p7 input/p7 regress/p7.nat regress/p7.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p9: expected/p9 input/p9 regress/p9.nat regress/p9.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p10: expected/p10 input/p10 regress/p10.nat regress/p10.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p11: expected/p11 input/p11 regress/p11.nat regress/p11.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p12: expected/p12 input/p12 regress/p12.nat regress/p12.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

p13: expected/p13 input/p13 regress/p13.ipf regress/p13.pool $(POOLDEP)
	@/bin/sh ./ptest `awk "/^$@ / { print; } " test.format`

ip1: expected/ip1 regress/ip1 $(POOLDEP)
	@/bin/sh ./iptest `awk "/^$@ / { print; } " test.format`

ip2: expected/ip2 input/ip2.data regress/ip2 $(POOLDEP)
	@/bin/sh ./iptest `awk "/^$@ / { print; } " test.format`

ip3: expected/ip3 regress/ip3 $(POOLDEP)
	@/bin/sh ./iptest `awk "/^$@ / { print; } " test.format`

bpf-f1: expected/bpf-f1 regress/bpf-f1
	/bin/sh ./bpftest `awk "/^$@ / { print; } " test.format`

clean:
	/bin/rm -f f1 f2 f3 f4 f5 f6 f7 f8 f9 f10 f11 f12 f13 f14 f15 f16 f17
	/bin/rm -f f18 f19 f20 f21 f22 f24 f25 f26 f27 f28 f29
	/bin/rm -f i1 i2 i3 i4 i5 i6 i7 i8 i9 i10 i11 i12 i13 i14 i15 i16 i17
	/bin/rm -f i18 i19 i20 i21 i22 i23
	/bin/rm -f n1 n2 n3 n4 n5 n6 n7 n8 n9 n10 n11 n12 n13 n14 n15 n16 n17 n18 n100 n101 n102 n103 n104 n105 n106 n200
	/bin/rm -f n1_6 n2_6 n4_6 n5_6 n6_6 n7_6 n8_6 n9_6 n11_6 n12_6 n15_6
	/bin/rm -f ni1 ni2 ni3 ni4 ni5 ni6 ni7 ni8 ni9
	/bin/rm -f ni10 ni11 ni12 ni13 ni14 ni15 ni16 ni17 ni18 ni19 ni20 ni21 ni23
	/bin/rm -f in1 in2 in3 in4 in5 in6 in7 in100 in101 in102
	/bin/rm -f p1 p2 p3 p4 p5 p6 p7 p9 p10 p11 p12 p13 ip1 ip2 ip3
	/bin/rm -f l1
	/bin/rm -f ipv6.1 ipv6.2 ipv6.3 ipv6.4 ipv6.5 ipv6.6
	/bin/rm -f bpf1 bpf-f1
	/bin/rm -f results/* logout
	(cd expected; make clean)

diffs:
	-cd expected; for i in *; do if [ -f $$i -a ! -f ../$$i -a -f ../results/$$i ] ; then  diff -c $$i ../results/$$i >> ../diff.out; fi done

n6s:
	for i in 1 2 4 5 6 7 11 13 14 15; do \
		sh i4to6 < input/n$${i} > input/n$${i}_6; \
		sh e4to6 < regress/n$${i} > regress/n$${i}_6; \
		sh e4to6 < expected/n$${i} > expected/n$${i}_6; \
	done
	for i in 8 9 10 12 17; do \
		sh e4to6 < regress/n$${i} > regress/n$${i}_6; \
		perl h4to6 < input/n$${i} > input/n$${i}_6; \
	done
