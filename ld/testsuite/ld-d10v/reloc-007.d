#source: reloc-005.s
#ld: -T $srcdir/$subdir/reloc-007.ld
#objdump: -D
#error: relocation truncated to fit: R_D10V_18_PCREL foo$

# Test 18 bit pc rel reloc bad boundary

