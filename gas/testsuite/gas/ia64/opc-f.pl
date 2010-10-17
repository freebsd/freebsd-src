print ".text\n\t.type _start,@", "function\n_start:\n\n";

@sf = ( "", ".s0", ".s1", ".s2", ".s3" );

# Arithmetic

foreach $i ( "fma", "fma.s", "fma.d", "fpma",
	     "fms", "fms.s", "fms.d", "fpms",
	     "fnma", "fnma.s", "fnma.d", "fpnma" ) {
  foreach $s (@sf) {
    print "\t${i}${s} f4 = f5, f6, f7\n";
  }
  print "\n";
}

foreach $i ( "fmpy", "fmpy.s", "fmpy.d", "fpmpy",
	     "fadd", "fadd.s", "fadd.d",
	     "fsub", "fsub.s", "fsub.d",
	     "fnmpy", "fnmpy.s", "fnmpy.d", "fpnmpy" ) {
  foreach $s (@sf) {
    print "\t${i}${s} f4 = f5, f6\n";
  }
  print "\n";
}

foreach $i ( "fnorm", "fnorm.s", "fnorm.d" ) { 
  foreach $s (@sf) {
    print "\t${i}${s} f4 = f5\n";
  }
  print "\n";
}

# Fixed Point Multiply Add

foreach $s ( ".l", ".lu", ".h", ".hu" ) {
  print "\txma${s} f4 = f5, f6, f7\n";
}
print "\n";

foreach $s ( ".l", ".lu", ".h", ".hu" ) {
  print "\txmpy${s} f4 = f5, f6\n";
}
print "\n";

# Parallel Floating Point Select

print "\tfselect f4 = f5, f6, f7\n\n";

# Floating Point Compare

@cmp = ( ".eq", ".lt", ".le", ".unord", ".gt", ".ge", ".neq", ".nlt", 
	 ".nle", ".ngt", ".nge", ".ord" );

@fctype = ( "", ".unc" );

foreach $c (@cmp) {
  foreach $u (@fctype) {
    foreach $s (@sf) {
      print "\tfcmp${c}${u}${s} p3, p4 = f4, f5\n";
    }
  }
  print "\n";
}

# Floating Point Class

foreach $u (@fctype) {
  foreach $c ( '@nat', '@qnan', '@snan', '@pos', '@neg', '@unorm',
	       '@norm', '@inf', '0x1ff' ) {
    foreach $m ( ".m", ".nm" ) {
      print "\tfclass${m}${u} p3, p4 = f4, $c\n";
    }
  }
  print "\n";
}

# Approximation

foreach $i ( "frcpa", "fprcpa" ) {
  foreach $s (@sf) {
    print "\t${i}${s} f4, p5 = f6, f7\n";
  }
  print "\n";
}

foreach $i ( "frsqrta", "fprsqrta" ) {
  foreach $s (@sf) {
    print "\t${i}${s} f4, p5 = f6\n";
  }
  print "\n";
}

# Min/Max

foreach $i ( "fmin", "fmax", "famin", "famax",
	     "fpmin", "fpmax", "fpamin", "fpamax" ) {
  foreach $s (@sf) {
    print "\t${i}${s} f4 = f5, f6\n";
  }
  print "\n";
}

# Parallel Compare

foreach $c (@cmp) {
  foreach $s (@sf) {
    print "\tfpcmp${c}${s} f3 = f4, f5\n";
  }
  print "\n";
}

# Merge and Logical

foreach $i ( "fmerge.s", "fmerge.ns", "fmerge.se", "fmix.lr", "fmix.r",
	     "fmix.l", "fsxt.l", "fpack", "fswap", "fswap.nl", "fswap.nr",
	     "fand", "fandcm", "for", "fxor", "fpmerge.s", "fpmerge.ns",
	     "fpmerge.se" ) {
  print "\t$i f4 = f5, f6\n";
}
print "\n";

foreach $i ( "fabs", "fneg", "fnegabs", "fpabs", "fpneg", "fpnegabs" ) {
  print "\t$i f4 = f5\n";
}
print "\n";

# Convert Floating to Fixed

foreach $b ( "fcvt", "fpcvt" ) {
  foreach $f ( ".fx", ".fxu" ) {
    foreach $t ( "", ".trunc" ) {
      foreach $s (@sf) {
	print "\t${b}${f}${t}${s} f4 = f5\n";
      }
      print "\n";
    }
  }
}

# Convert Fixed to Floating

foreach $e ( ".xf", ".xuf" ) {
  print "\tfcvt$e f4 = f5\n";
}
print "\n";

# Set Controls

foreach $s (@sf) {
  print "\tfsetc$s 0, 0\n";
  print "\tfsetc$s 0x3f, 0x3f\n";
}
print "\n";

# Clear flags

foreach $s (@sf) {
  print "\tfclrf$s\n";
}
print "\n";

# Check flags

foreach $s (@sf) {
  print "\tfchkf$s _start\n";
}
print "\n";

# Misc

print "\tbreak.f 0\n";
print "\tnop.f 0;;\n";
print "\n";

