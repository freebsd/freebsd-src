#source: unknown2.s
#ld: -shared
#readelf: -S
#target: *-*-linux*

#...
  \[[ 0-9]+\] \.note.foo[ \t]+NOTE[ \t]+.*
#pass
