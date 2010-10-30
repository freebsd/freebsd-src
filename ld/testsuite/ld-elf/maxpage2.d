#source: maxpage1.s
#ld: -z max-page-size=0x100000
#readelf: -l --wide
#target: *-*-linux*

#...
  LOAD+.*0x100000
  LOAD+.*0x100000
#pass
