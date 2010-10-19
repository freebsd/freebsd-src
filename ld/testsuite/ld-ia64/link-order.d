#source: ../../../binutils/testsuite/binutils-all/link-order.s
#ld: -r
#readelf: -S --wide

#...
  \[[ ]+1\] \.text.*[ \t]+PROGBITS[ \t0-9a-f]+AX.*
#...
  \[[ 0-9]+\] \.IA_64.unwind[ \t]+IA_64_UNWIND[ \t0-9a-f]+AL[ \t]+1[ \t]+1[ \t]+8
#pass
