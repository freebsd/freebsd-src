#ld: -Tdefined2.t
#nm: -B
#source: phdrs.s

# Check that arithmetic on DEFINED works.
# Matching both A and T accounts for formats that can't tell a .text
# symbol from an absolute symbol (mmo), but matches whatever section that
# contains an address matching the value.  The symbol sym1 is supposed to
# be in the .text section for all targets, though.

#...
0+1 [AT] defined1
#...
0+11 A defined2
#...
0+100 A defined3
#...
0+1ff A defined4
#...
0+3 T sym1
#pass
