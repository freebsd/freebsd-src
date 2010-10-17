$AT = '@';
print <<END
.text
	.type _start,${AT}function
_start:

	pmpyshr2 r4 = r5, r6, 0
	pmpyshr2.u r4 = r5, r6, 16

	pmpy2.r r4 = r5, r6
	pmpy2.l r4 = r5, r6

	mix1.r r4 = r5, r6
	mix2.r r4 = r5, r6
	mix4.r r4 = r5, r6
	mix1.l r4 = r5, r6
	mix2.l r4 = r5, r6
	mix4.l r4 = r5, r6

	pack2.uss r4 = r5, r6
	pack2.sss r4 = r5, r6
	pack4.sss r4 = r5, r6

	unpack1.h r4 = r5, r6
	unpack2.h r4 = r5, r6
	unpack4.h r4 = r5, r6
	unpack1.l r4 = r5, r6
	unpack2.l r4 = r5, r6
	unpack4.l r4 = r5, r6

	pmin1.u r4 = r5, r6
	pmax1.u r4 = r5, r6

	pmin2 r4 = r5, r6
	pmax2 r4 = r5, r6

	psad1 r4 = r5, r6

	mux1 r4 = r5, ${AT}rev
	mux1 r4 = r5, ${AT}mix
	mux1 r4 = r5, ${AT}shuf
	mux1 r4 = r5, ${AT}alt
	mux1 r4 = r5, ${AT}brcst

	mux2 r4 = r5, 0
	mux2 r4 = r5, 0xff
	mux2 r4 = r5, 0xaa

	pshr2 r4 = r5, r6
	pshr2 r4 = r5, 0
	pshr2 r4 = r5, 8
	pshr2 r4 = r5, 31

	pshr4 r4 = r5, r6
	pshr4 r4 = r5, 0
	pshr4 r4 = r5, 8
	pshr4 r4 = r5, 31

	pshr2.u r4 = r5, r6
	pshr2.u r4 = r5, 0
	pshr2.u r4 = r5, 8
	pshr2.u r4 = r5, 31

	pshr4.u r4 = r5, r6
	pshr4.u r4 = r5, 0
	pshr4.u r4 = r5, 8
	pshr4.u r4 = r5, 31

	shr r4 = r5, r6
	shr.u r4 = r5, r6

	pshl2 r4 = r5, r6
	pshl2 r4 = r5, 0
	pshl2 r4 = r5, 8
	pshl2 r4 = r5, 31

	pshl4 r4 = r5, r6
	pshl4 r4 = r5, 0
	pshl4 r4 = r5, 8
	pshl4 r4 = r5, 31

	shl r4 = r5, r6

	popcnt r4 = r5

	shrp r4 = r5, r6, 0
	shrp r4 = r5, r6, 12
	shrp r4 = r5, r6, 63

	extr r4 = r5, 0, 16
	extr r4 = r5, 0, 63
	extr r4 = r5, 10, 40
	
	extr.u r4 = r5, 0, 16
	extr.u r4 = r5, 0, 63
	extr.u r4 = r5, 10, 40
	
	dep.z r4 = r5, 0, 16
	dep.z r4 = r5, 0, 63
	dep.z r4 = r5, 10, 40
	dep.z r4 = 0, 0, 16
	dep.z r4 = 127, 0, 63
	dep.z r4 = -128, 5, 50
	dep.z r4 = 0x55, 10, 40

	dep r4 = 0, r5, 0, 16
	dep r4 = -1, r5, 0, 63
// Insert padding NOPs to force the same template selection as IAS.
	nop.m 0
	nop.f 0
	dep r4 = r5, r6, 10, 7

	movl r4 = 0
	movl r4 = 0xffffffffffffffff
	movl r4 = 0x1234567890abcdef

	break.i 0
	break.i 0x1fffff

	nop.i 0
	nop.i 0x1fffff

	chk.s.i r4, _start

	mov r4 = b0
	mov b0 = r4

	mov pr = r4, 0
	mov pr = r4, 0x1234
	mov pr = r4, 0x1ffff

	mov pr.rot = 0
// ??? This was originally 0x3ffffff, but that generates an assembler warning
// that the testsuite infrastructure isn't set up to ignore.
	mov pr.rot = 0x3ff0000
	mov pr.rot = -0x4000000

	zxt1 r4 = r5
	zxt2 r4 = r5
	zxt4 r4 = r5

	sxt1 r4 = r5
	sxt2 r4 = r5
	sxt4 r4 = r5

	czx1.l r4 = r5
	czx2.l r4 = r5
	czx1.r r4 = r5
	czx2.r r4 = r5

END
;

@ctype = ( "", ".unc", ".and", ".or", ".or.andcm", ".orcm",
	   ".andcm", ".and.orcm" );

$i = 0;
foreach $z ( ".z", ".nz" ) {
  foreach $c (@ctype) {
    print "\ttbit${z}${c} p2, p3 = r4, $i\n";
    ++$i;
  }
}
print "\n";

foreach $z ( ".z", ".nz" ) {
  foreach $c (@ctype) {
    print "\ttnat${z}${c} p2, p3 = r4\n";
  }
}
print "\n";


@mwh = ( "", ".sptk", ".dptk" );
@ih = ( "", ".imp" );

$LAB = 1;

foreach $b ("", ".ret") {
  foreach $w (@mwh) {
    foreach $i (@ih) {
      print "\tmov${b}${w}${i} b3 = r4, .L${LAB}\n";
    }
    print ".space 240\n";
    print ".L${LAB}:\n";
    ++$LAB;
  }
  print "\n";
}
