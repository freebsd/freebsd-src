#source: noload-1.s
#ld: -T noload-1.t -z max-page-size=0x200000
#readelf: -Sl --wide
#target: *-*-linux*

#...
 +LOAD +0x200000 +0x0+ +0x0+ +0x0+ +0x0+1 +RW +0x200000
#pass
