print ".text\n\t.type _start,@", "function\n_start:\n\n";

@ldhint = ( "", ".nt1", ".nta" );
@ldspec = ( "", ".s", ".a", ".sa", ".c.clr", ".c.nc" );
@sthint = ( "", ".nta" );

$i = 0;

# Integer Load

foreach $s ( "1", "2", "4", "8" ) {
  foreach $e (@ldspec, ".bias", ".acq", ".c.clr.acq") {
    foreach $l (@ldhint) {
      print "\tld${s}${e}${l} r4 = [r5]\n";
      print "\tld${s}${e}${l} r4 = [r5], r6\n";
      print "\tld${s}${e}${l} r4 = [r5], ", $i - 256, "\n";
      $i = ($i + 13) % 512;
    }
    print "\n";
  }
}

# Integer Fill

for $l (@ldhint) {
  print "\tld8.fill${l} r4 = [r5]\n";
  print "\tld8.fill${l} r4 = [r5], r6\n";
  print "\tld8.fill${l} r4 = [r5], ", $i - 256, "\n";
  $i = ($i + 13) % 512;
}
print "\n";

# Integer Store

foreach $s ("1", "2", "4", "8", "1.rel", "2.rel", "4.rel", "8.rel", "8.spill") {
  for $l (@sthint) {
    print "\tst${s}${l} [r4] = r5\n";
    print "\tst${s}${l} [r4] = r5, ", $i - 256, "\n";
    $i = ($i + 13) % 512;
  }
  print "\n";
}

# Floating Point Load

foreach $s ( "fs", "fd", "f8", "fe" ) {
  foreach $e (@ldspec) {
    foreach $l (@ldhint) {
      print "\tld${s}${e}${l} f4 = [r5]\n";
      print "\tld${s}${e}${l} f4 = [r5], r6\n";
      print "\tld${s}${e}${l} f4 = [r5], ", $i - 256, "\n";
      $i = ($i + 13) % 512;
    }
    print "\n";
  }
}

# Floating Point Fill

for $l (@ldhint) {
  print "\tldf.fill${l} f4 = [r5]\n";
  print "\tldf.fill${l} f4 = [r5], r6\n";
  print "\tldf.fill${l} f4 = [r5], ", $i - 256, "\n";
  $i = ($i + 13) % 512;
}
print "\n";

# Floating Point Store

foreach $s ( "fs", "fd", "f8", "fe", "f.spill" ) {
  for $l (@sthint) {
    print "\tst${s}${l} [r4] = f5\n";
    print "\tst${s}${l} [r4] = f5, ", $i - 256, "\n";
    $i = ($i + 13) % 512;
  }
  print "\n";
}

# Floating Point Load Pair

foreach $s ( "fps", "fpd", "fp8" ) {
  foreach $e (@ldspec) {
    foreach $l (@ldhint) {
      print "\tld${s}${e}${l} f4, f5 = [r5]\n";
      print "\tld${s}${e}${l} f4, f5 = [r5], ", ($s eq "fps" ? 8 : 16), "\n";
    }
    print "\n";
  }
}

# Line Prefetch

@lfhint = ( "", ".nt1", ".nt2", ".nta" );

foreach $e ( "", ".excl" ) {
  foreach $f ( "", ".fault" ) {
    foreach $h (@lfhint) {
      print "\tlfetch${f}${e}${h} [r4]\n";
      print "\tlfetch${f}${e}${h} [r4], r5\n";
      print "\tlfetch${f}${e}${h} [r4], ", $i - 256, "\n";
      $i = ($i + 13) % 512;
    }
    print "\n";
  }
}

# Compare and Exchange

foreach $s ( "1", "2", "4", "8" ) {
  foreach $e ( ".acq", ".rel" ) {
    foreach $h (@ldhint) {
      print "\tcmpxchg${s}${e}${h} r4 = [r5], r6, ar.ccv\n";
    }
    print "\n";
  }
}

# Exchange

foreach $s ( "1", "2", "4", "8" ) {
  foreach $h (@ldhint) {
    print "\txchg${s}${h} r4 = [r5], r6\n";
  }
  print "\n";
}

# Fetch and Add

$i = 0;
@inc3 = ( -16, -8, -4, -1, 1, 4, 8, 16 );
foreach $s ( "4.acq", "8.acq", "4.rel", "8.rel" ) {
  foreach $h (@ldhint) {
    print "\tfetchadd${s}${h} r4 = [r5], ", $inc3[$i], "\n";
    $i = ($i + 1) % 8;
  }
  print "\n";
}

# Get/Set FR

foreach $e ( ".sig", ".exp", ".s", ".d" ) {
  print "\tsetf${e} f4 = r5\n";
}
print "\n";

foreach $e ( ".sig", ".exp", ".s", ".d" ) {
  print "\tgetf${e} r4 = f5\n";
}
print "\n";

# Speculation and Advanced Load Checkso

print <<END
	chk.s.m r4, _start
	chk.s f4, _start
	chk.a.nc r4, _start
	chk.a.clr r4, _start
	chk.a.nc f4, _start
	chk.a.clr f4, _start

	invala
	fwb
	mf
	mf.a
	srlz.d
	srlz.i
	sync.i
	nop.m 0
	nop.i 0

	{ .mii; alloc r4 = ar.pfs, 2, 10, 16, 16 }

	{ .mii; flushrs }
	{ .mii; loadrs }

	invala.e r4
	invala.e f4

	fc r4
	ptc.e r4

	break.m 0
	break.m 0x1ffff

	nop.m 0
	break.m 0x1ffff

	probe.r r4 = r5, r6
	probe.w r4 = r5, r6

	probe.r r4 = r5, 0
	probe.w r4 = r5, 1

	probe.r.fault r3, 2
	probe.w.fault r3, 3
	probe.rw.fault r3, 0

	itc.d r8
	itc.i r9

	sum 0x1234
	rum 0x5aaaaa
	ssm 0xffffff
	rsm 0x400000

	ptc.l r4, r5
	ptc.g r4, r5
	ptc.ga r4, r5
	ptr.d r4, r5
	ptr.i r4, r5

	thash r4 = r5
	ttag r4 = r5
	tpa r4 = r5
	tak r4 = r5

END
;
