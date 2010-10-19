#readelf: -SW
#name: group section with multiple sections of same name
#source: group1.s

#...
[ 	]*\[.*\][ 	]+\.foo_group[ 	]+GROUP.*
#...
[ 	]*\[.*\][ 	]+\.text[ 	]+PROGBITS.*[ 	]+AX[ 	]+.*
#...
[ 	]*\[.*\][ 	]+\.text[ 	]+PROGBITS.*[ 	]+AXG[ 	]+.*
#pass
