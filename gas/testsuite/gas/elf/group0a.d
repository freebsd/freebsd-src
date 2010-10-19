#readelf: -SW
#name: group section
#source: group0.s

#...
[ 	]*\[.*\][ 	]+\.foo_group[ 	]+GROUP.*
#...
[ 	]*\[.*\][ 	]+\.foo[ 	]+PROGBITS.*[ 	]+AXG[ 	]+.*
[ 	]*\[.*\][ 	]+\.bar[ 	]+PROGBITS.*[ 	]+AG[ 	]+.*
#pass
