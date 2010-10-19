print ".text\n";
print "\t.type _start,@","function\n";
print "_start:\n\n";

print "// Fixed and stacked integer registers.\n";
for ($i = 1; $i < 128; ++$i) {
  print "\t{ .mii; mov r$i = r0; nop.i 0; nop.i 0;; }\n";
}
print "\n";

print "// Alternate names for input registers\n";
print "\t.regstk 96, 0, 0, 0\n";
for ($i = 0; $i < 96; ++$i) {
  print "\t{ .mii; mov in$i = r0; nop.i 0; nop.i 0;; }\n";
}
print "\n";

print "// Alternate names for output registers\n";
print "\t.regstk 0, 0, 96, 0\n";
for ($i = 0; $i < 96; ++$i) {
  print "\t{ .mii; mov out$i = r0; nop.i 0; nop.i 0;; }\n";
}
print "\n";

print "// Alternate names for local registers\n";
print "\t.regstk 0, 96, 0, 0\n";
for ($i = 0; $i < 96; ++$i) {
  print "\t{ .mii; mov loc$i = r0; nop.i 0; nop.i 0;; }\n";
}
print "\n";

print "// Return value registers\n";
for ($i = 0; $i < 4; ++$i) {
  print "\t{ .mii; mov ret$i = r0; nop.i 0; nop.i 0;; }\n";
}
print "\n";

print "\t{ .mii;\n";
print "\tmov gp = r0\n";
print "\tmov sp = r0\n";
print "\tmov tp = r0;; }\n\n";

print "// Floating point registers\n";
for ($i = 2; $i < 128; ++$i) {
  print "\t{ .mfi; mov f$i = f0 ;; }\n";
}
print "\n";

print "// Floating point argument registers\n";
for ($i = 0; $i < 8; ++$i) {
  print "\t{ .mfi; mov farg$i = f1 ;; }\n";
}
print "\n";

print "// Floating point return value registers\n";
for ($i = 0; $i < 8; ++$i) {
  print "\t{ .mfi; mov fret$i = f1 ;; }\n";
}
print "\n";

print "// Predicate registers\n";
for ($i = 0; $i < 64; ++$i) {
  print "\t{ .mii; (p$i)\tmov r", $i+1, " = r0; nop.i 0; nop.i 0;; }\n";
}
print "\n";

print "// Predicates as a unit\n";
print "\t{ .mmi; nop.m 0; mov r1 = pr ;; }\n";
print "//\tmov r2 = pr.rot\n";
print "\n";

print "// Branch registers.\n";
for ($i = 0; $i < 8; ++$i) {
  print "\t{ .mmi; mov b$i = r0;; }\n";
}
print "\n";

print "\t{ .mmi; mov rp = r0;; }\n";
print "\n";

print "// Application registers\n";
@reserved = ( 8..15, 20, 22..23, 31, 33..35, 37..39, 41..47, 67..111 );
%reserved = ();
foreach $i (@reserved) {
  $reserved{$i} = 1;
}
for ($i = 0; $i < 128; ++$i) {
  print "//" if $reserved{$i};
  print "\t{ .mmi; nop.m 0; mov r1 = ar$i ;; }";
  print "\t\t// reserved" if $reserved{$i};
  print "\n";
}
print "\n";

print "// Application registers by name\n";
for ($i = 0; $i < 8; ++$i) {
  print "\t{ .mmi; nop.m 0; mov r1 = ar.k$i ;;}\n";
}

@regs = ( "rsc", "bsp", "bspstore", "rnat", "ccv", "unat", "fpsr", "itc",
	  "pfs", "lc", "ec" );
foreach $i (@regs) {
  print "\t{ .mmi; nop.m 0; mov r1 = ar.$i ;; }\n";
}
print "\n";

print "// Control registers\n";
@reserved = ( 3..7, 10..15, 18, 26..63, 75..79, 82..127 );
%reserved = ();
foreach $i (@reserved) {
  $reserved{$i} = 1;
}
for ($i = 0; $i < 128; ++$i) {
  print "//" if $reserved{$i};
  print "\t{ .mfb; mov r1 = cr$i ;; }";
  print "\t\t// reserved" if $reserved{$i};
  print "\n";
}
print "\n";

print "// Control registers by name\n";
@regs = ( "dcr", "itm", "iva", "pta", "ipsr", "isr", "iip",
	  "iipa", "ifs", "iim", "iha", "lid", "ivr",
	  "tpr", "eoi", "irr0", "irr1", "irr2", "irr3", "itv", "pmv",
	  "lrr0", "lrr1", "cmcv" );
# ias doesn't accept these, despite documentation to the contrary.
# push @regs, "ida", "idtr", "iitr"
foreach $i (@regs) {
  print "\t{ .mfb; mov r1 = cr.$i ;; }\n";
}
print "\n";


print "// Other registers\n";
print "\t{ .mfb; mov r1 = psr ;; }\n";
print "//\t{ .mfb; mov r1 = psr.l ;; }\n";
print "\t{ .mfb; mov r1 = psr.um ;; }\n";
print "\t{ .mmi; mov r1 = ip ;; }\n";
print "\n";

print "// Indirect register files\n";
@regs = ("pmc", "pmd", "pkr", "rr", "ibr", "dbr", "CPUID", "cpuid");
# ias doesn't accept these, despite documentation to the contrary.
# push @regs, "itr", "dtr";
foreach $i (@regs) {
  print "\t{ .mmi\n";
  print "\tmov r1 = ${i}[r3]\n";
  print "\tmov r2 = ${i}[r4]\n";
  print "\tnop.i 0;; }\n";
}
