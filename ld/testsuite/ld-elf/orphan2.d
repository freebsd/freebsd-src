#source: orphan2.s
#ld: -r
#readelf: -S --wide

#...
  \[[ 0-9]+\] \.text[ \t]+PROGBITS[ \t0-9a-f]+AX?.*
#...
  \[[ 0-9]+\] \.modinfo[ \t]+PROGBITS[ \t0-9a-f]+A.*
#pass
