#source: multibss1.s
#ld: -e 0
#readelf: -l --wide
#target: *-*-linux*

#...
 +LOAD +0x[^ ]+ +0x[^ ]+ +0x[^ ]+ +0x[^ ]+ +0x500000 .*
#       p_offset p_vaddr  p_paddr  p_filesz
#pass
