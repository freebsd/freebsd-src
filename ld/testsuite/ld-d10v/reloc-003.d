#source: reloc-001.s
#ld: -T $srcdir/$subdir/reloc-003.ld
#error: relocation truncated to fit: R_D10V_10_PCREL_L foo$

# Test 10 bit pc rel reloc bad boundary.