#source: start.s
#readelf: -d 
#ld: -shared --hash-style=gnu
#target: *-*-linux*
#notarget: mips*-*-*

#...
[ 	]*0x[0-9a-z]+[ 	]+\(GNU_HASH\)[ 	]+0x[0-9a-z]+
#...
