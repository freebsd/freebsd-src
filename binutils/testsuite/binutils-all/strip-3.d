#PROG: strip
#source: empty.s
#strip: -R .text -R .data -R .bss -R .ARM.attributes -R .reginfo -R .pdr -R .xtensa.info
#readelf: -S --wide
#name: strip empty file
#target: *-*-linux*

#...
  \[[ 0]+\][ \t]+NULL[ \t]+.*
  \[[ 1]+\] \.shstrtab.*[ \t]+STRTAB[ \t]+.*
#pass
