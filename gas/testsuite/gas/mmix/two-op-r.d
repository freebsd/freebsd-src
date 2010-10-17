# objdump: -dr
# as: -linkrelax
# source: two-op.s
.*:     file format elf64-mmix

Disassembly of section \.text:

0000000000000000 <Main>:
   0:	e0175840 	seth \$23,0x5840
   4:	e12d5840 	setmh \$45,0x5840
   8:	e8171ed4 	orh \$23,0x1ed4
   c:	e92d3039 	ormh \$45,0x3039
  10:	e2175840 	setml \$23,0x5840
  14:	e32d5840 	setl \$45,0x5840
  18:	ea171ed4 	orml \$23,0x1ed4
  1c:	eb2d3039 	orl \$45,0x3039
  20:	e42d3039 	inch \$45,0x3039
  24:	e5171ed4 	incmh \$23,0x1ed4
  28:	ec2d5840 	andnh \$45,0x5840
  2c:	ed175840 	andnmh \$23,0x5840
  30:	e6175840 	incml \$23,0x5840
  34:	e72d5840 	incl \$45,0x5840
  38:	ee171ed4 	andnml \$23,0x1ed4
  3c:	ef2d3039 	andnl \$45,0x3039
