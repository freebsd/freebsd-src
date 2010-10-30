#source: maxpage1.s
#ld: -T maxpage3.t -z max-page-size=0x10000000
#readelf: -lS --wide
#target: x86_64-*-linux*

#...
  \[[ 0-9]+\] \.data[ \t]+PROGBITS[ \t]+0*200000[ \t]+[ \t0-9a-f]+WA?.*
#...
  LOAD+.*0x10000000
#pass
