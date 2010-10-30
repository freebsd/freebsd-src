#source: noload-1.s
#ld: -T noload-1.t
#readelf: -S --wide

#...
  \[[ 0-9]+\] TEST[ \t]+NOBITS[ \t0-9a-f]+WA.*
#pass
