#PROG: strip
#source: group.s
#readelf: -Sg --wide
#name: strip with section group 1

#...
  \[[ 0-9]+\] \.text.*[ \t]+PROGBITS[ \t0-9a-f]+AX[ \t]+.*
#...
  \[[ 0-9]+\] \.data.*[ \t]+PROGBITS[ \t0-9a-f]+WA[ \t]+.*
#...
There are no section groups in this file.
