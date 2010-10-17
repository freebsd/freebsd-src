$AT = '@';
print <<END
.text
	.type _start,${AT}function
_start:

	add r101 = r102, r103
(p1)	add r104 = r105, r106
	add r107 = r108, r109, 1
(p2)	add r110 = r111, r112, 1

	adds r20 = 0, r10
(p1)	adds r21 = 1, r10
	adds r22 = -1, r10
	adds r23 = -0x2000, r10
(p2)	adds r24 = 0x1FFF, r10

	addl r30 = 0, r1
	addl r31 = 1, r1
(p1)	addl r32 = -1, r1
	addl r33 = -0x2000, r1
	addl r34 = 0x1FFF, r1
	addl r35 = -0x200000, r1
	addl r36 = 0x1FFFFF, r1

	add r11 = 0, r10
	add r12 = 0x1234, r10
	add r13 = 0x1234, r1
	add r14 = 0x12345, r1

	addp4 r20 = r3, r10
(p1)	addp4 r21 = 1, r10
	addp4 r22 = -1, r10

	sub r101 = r102, r103
(p2)	sub r110 = r111, r112, 1
	sub r120 = 0, r3
	sub r121 = 1, r3
	sub r122 = -1, r3
	sub r123 = -128, r3
	sub r124 = 127, r3

	and r8 = r9, r10
(p3)	and r11 = -128, r12

(p4)	or r8 = r9, r10
	or r11 = -128, r12

	xor r8 = r9, r10
	xor r11 = -128, r12

	andcm r8 = r9, r10
	andcm r11 = -128, r12

	shladd r8 = r30, 1, r31
	shladd r9 = r30, 2, r31
	shladd r10 = r30, 3, r31
	shladd r11 = r30, 4, r31

	shladdp4 r8 = r30, 1, r31
	shladdp4 r9 = r30, 2, r31
	shladdp4 r10 = r30, 3, r31
	shladdp4 r11 = r30, 4, r31

	padd1 r10 = r30, r31
	padd1.sss r11 = r30, r31
	padd1.uus r12 = r30, r31
	padd1.uuu r13 = r30, r31
	padd2 r14 = r30, r31
	padd2.sss r15 = r30, r31
	padd2.uus r16 = r30, r31
	padd2.uuu r17 = r30, r31
	padd4 r18 = r30, r31

	psub1 r10 = r30, r31
	psub1.sss r11 = r30, r31
	psub1.uus r12 = r30, r31
	psub1.uuu r13 = r30, r31
	psub2 r14 = r30, r31
	psub2.sss r15 = r30, r31
	psub2.uus r16 = r30, r31
	psub2.uuu r17 = r30, r31
	psub4 r18 = r30, r31

	pavg1 r10 = r30, r31
	pavg1.raz r10 = r30, r31
	pavg2 r10 = r30, r31
	pavg2.raz r10 = r30, r31

	pavgsub1 r10 = r30, r31
	pavgsub2 r10 = r30, r31

	pcmp1.eq r10 = r30, r31
	pcmp2.eq r10 = r30, r31
	pcmp4.eq r10 = r30, r31
	pcmp1.gt r10 = r30, r31
	pcmp2.gt r10 = r30, r31
	pcmp4.gt r10 = r30, r31

	pshladd2 r10 = r11, 1, r12
	pshladd2 r10 = r11, 3, r12

	pshradd2 r10 = r11, 1, r12
	pshradd2 r10 = r11, 2, r12

END
;

@cmp2 = ( ".eq", ".ne" );
@cmp6 = ( @cmp2, ".lt", ".le", ".gt", ".ge" );
@cmp10 = ( @cmp6, ".ltu", ".leu", ".gtu", ".geu" );

@ctype = ( ".and", ".or", ".or.andcm", ".orcm", ".andcm", ".and.orcm" );

foreach $C ( "cmp", "cmp4" ) {
  foreach $u ( "", ".unc" ) {
    foreach $i (@cmp10) {
      print "\t${C}${i}${u} p2, p3 = r3, r4\n";
      print "\t${C}${i}${u} p2, p3 = 3, r4\n";
    }
    print "\n";
  }
  
  foreach $i (@cmp2) {
    foreach $c (@ctype) {
      print "\t${C}${i}${c} p2, p3 = r3, r4\n";
      print "\t${C}${i}${c} p2, p3 = 3, r4\n";
    }
    print "\n";
  }
      
  foreach $i (@cmp6) {
    foreach $c (@ctype) {
      print "\t${C}${i}${c} p2, p3 = r0, r4\n";
      print "\t${C}${i}${c} p2, p3 = r4, r0\n";
    }
    print "\n";
  }
}

# Pad to a bundle boundary with known nops.
print "nop.i 0; nop.i 0\n";
