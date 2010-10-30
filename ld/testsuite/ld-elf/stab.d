#source: start.s
#as: -gstabs
#readelf: -S --wide
#ld:
#notarget: ia64-*-*

#...
  \[[0-9 ][0-9]\] \.stab +PROGBITS +0+ [0-9a-f]+ [0-9a-f]+ [0-9a-f]+ +[1-9]+ +0.*
#...
  \[[0-9 ][0-9]\] \.stabstr +STRTAB +0+ [0-9a-f]+ [0-9a-f]+ 00 +0 +0.*
#...
