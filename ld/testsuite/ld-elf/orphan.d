#source: orphan.s
#ld: -T orphan.ld
#readelf: -S --wide

#...
  \[[ 0-9]+\] \.(text|notbad)[ \t]+PROGBITS[ \t0-9a-f]+AX?.*
  \[[ 0-9]+\] \.(text|notbad)[ \t]+PROGBITS[ \t0-9a-f]+AX?.*
  \[[ 0-9]+\] \.data[ \t]+PROGBITS[ \t0-9a-f]+WA.*
#...
  \[[ 0-9]+\] \.note[ \t]+NOTE[ \t0-9a-f]+A.*
  \[[ 0-9]+\] \.note.bar[ \t]+NOTE[ \t0-9a-f]+A.*
#pass
